/**
 * @file    measurement.c
 * @brief   [EN] ADC counts to engineering units (mV / mA), step by step;
 *              latest snapshot shared with the other tasks. Runs in the
 *              measurement task: a few conversions + one GPIO read, then
 *              yield - no HAL_Delay anywhere. Charge currents = the PWM
 *              mid-ON synchronized samples, filtered on the RAW counts
 *              first (median + moving average), converted to mA after;
 *              battery voltages keep their median-5 spike guard on the
 *              raw counts.
 * @note    [EN] Divider/gain constants are private to bsp_measurement.c;
 *              converted values are exposed as UINT32_T__G__Meas* /
 *              BOOL__G__Meas* globals - written only by this task,
 *              readable by any module (and the debugger via Live
 *              Expressions).
 */
#include "measurement.h"
#include "modules_enable.h"
#include "calibration.h"
#include "cal_lut.h"
#include "bsp_adc.h"
#include "bsp_measurement.h"
#include "bsp_gpio.h"
#if MODULE_FAULT
#include "fault.h"
#endif
#include "cmsis_os2.h"
#include <stddef.h>
/* ==================== Static State ==================== */
/*
 * [EN] Shared snapshot for the other tasks (UI / protection / comm).
 *      Written only by the measurement task, read by GetSnapshot.
 */
static volatile measurement_snapshot_t MEASUREMENT_SNAPSHOT_T__G__Snap;
/*
 * [EN] Number of completed stable normalized ADC frames collected during
 *      startup warm-up. Unit: completed ADC frame.
 */
static uint8_t UINT8_T__G__MeasurementWarmupFrameCount;
/*
 * [EN] Median-of-5 history of the RAW counts of the two battery ADC
 * channels (0=V24_BAT, 1=V12_BAT; user order 2026-09-27: filters run on
 * the raw ADC data, conversion/derivation happen after): raised
 * 2026-09-19 from median-3 because a false buzzer
 * still fired WHILE the charger genuinely pumped (absorb mode) - that
 * means glitch bursts of two consecutive frames also cross 14.8 V, and
 * median-3 passes a 2-frame burst straight through. Median-of-5 only
 * outputs the middle sample, so any burst shorter than 3 frames is dead
 * while real voltage steps pass with ~20 ms of lag at the 10 ms frame
 * cadence - nothing the control loop can notice.
 */
static uint32_t UINT32_T__G__BatVoltageMedianHistoryCounts[2][5];
static bool BOOL__G__BatVoltageMedianSeeded[2];
/*
 * [EN] Two-stage ADC1+DMA self-healing supervisor state (60 s pre-reset +
 *      1 software reset + 60 s post-reset -> latched FAULT_ADC).
 */
static bool BOOL__G__AdcStallActive = false;
static bool BOOL__G__AdcFirstRetryDone = false;
static uint32_t UINT32_T__G__AdcStallStartMs = 0u;
static uint32_t UINT32_T__G__AdcLastRecoverMs = 0u;
static bool BOOL__G__AdcHealthyActive = false;
static uint32_t UINT32_T__G__AdcHealthyStartMs = 0u;
static bool BOOL__G__AdcPostResetUsed = false;
static bool BOOL__G__AdcFaultLatched = false;
#if (MEASUREMENT_CURRENT_MEDIAN3_ENABLE != 0u)
/*
 * [EN] Median history per current channel (0=Current1, 1=Current2), window
 *      = runtime median size 1/3/5 with default 3 (user order 2026-09-22):
 *      kills single-sample jumps of the synchronized mid-ON reading with
 *      zero added lag. Exists only when the compile-time filter switch is
 *      on.
 */
static uint32_t UINT32_T__G__CurrentMedianHistoryCounts[2][MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX];
static bool BOOL__G__CurrentMedianSeeded[2];
#endif
#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
/*
 * [EN] Moving-average window per current channel (user order 2026-09-22):
 *      the last MEASUREMENT_CURRENT_AVERAGE_WINDOW raw count samples (user
 *      order 2026-09-27: filters run on the raw ADC data), a fill counter
 *      for the startup ramp and the next slot index. Exists only when the
 *      filter switch is on.
 */
static uint32_t UINT32_T__G__CurrentAverageWindowCounts[2][MEASUREMENT_CURRENT_AVERAGE_WINDOW];
static uint16_t UINT16_T__G__CurrentAverageFillCount[2];
static uint16_t UINT16_T__G__CurrentAverageNextIndex[2];
#endif
/* ==================== Runtime filter config / voltage offsets (ESP panel) ==================== */
/*
 * [EN] Full-program audit 2026-10-05: this whole group used to sit INSIDE
 *      the MEASUREMENT_CURRENT_AVERAGE_ENABLE block above, although none of
 *      it belongs to the moving-average filter - the runtime median size and
 *      the three voltage offsets are read unconditionally by
 *      func__Measurement_Run, func__Measurement_Init and the public
 *      setters and getters. Setting that switch to 0u therefore broke the
 *      build with eight "undeclared" errors. The
 *      declarations are unconditional now; behaviour with the switch at 1u
 *      is bit-for-bit identical.
 */
/*
 * [EN] Runtime copies of the current-filter configuration (resizable
 *      live from the ESP panel; size 1 = bypass, no separate on/off
 *      switch). The compile-time switches stay the capability gates; the
 *      measurement task detects a change and resets the filter state in
 *      its own context, so no cross-task locking (written by EspLink,
 *      volatile).
 */
static volatile uint8_t UINT8_T__G__FilterMedianSize = 3u;
static volatile uint16_t UINT16_T__G__FilterAverageWindow =
    (uint16_t)MEASUREMENT_CURRENT_AVERAGE_WINDOW_DEFAULT;
/*
 * [EN] Last configuration the measurement task applied; owned by the
 *      measurement task only (change detection).
 */
static uint8_t UINT8_T__G__FilterMedianSizeApplied = 3u;
static uint16_t UINT16_T__G__FilterAverageWindowApplied =
    (uint16_t)MEASUREMENT_CURRENT_AVERAGE_WINDOW_DEFAULT;
/*
 * [EN] Runtime voltage calibration offsets in mV, default 0 = today's
 *      behavior; applied AFTER the divider conversion, before the
 *      low/high derivation. Flash-persisted since v1.14 (NVM ids 4/5/6).
 */
static volatile int32_t INT32_T__G__VoltageInOffsetMv = 0;
static volatile int32_t INT32_T__G__Voltage24OffsetMv = 0;
static volatile int32_t INT32_T__G__Voltage12OffsetMv = 0;
/* ==================== Measurement_Median5 ==================== */
/*
 * [EN] Median-of-5 for the battery voltage channel prefilter: plain
 * insertion sort of a LOCAL copy keeps the live history untouched; the
 * middle element is the spike-immune sample. Five-element sorting network
 * avoided on purpose - clarity beats cycle-pinching at a 10 ms cadence.
 */
static uint32_t func__Measurement_Median5(uint32_t *uint32_t__samples)
{
    uint32_t uint32_t__sorted[5u];
    uint8_t  uint8_t__index;
    uint8_t  uint8_t__pass;
    for (uint8_t__index = 0u; uint8_t__index < 5u; uint8_t__index++)
    {
        uint32_t__sorted[uint8_t__index] = uint32_t__samples[uint8_t__index];
    }
    for (uint8_t__pass = 1u; uint8_t__pass < 5u; uint8_t__pass++)
    {
        uint32_t uint32_t__key = uint32_t__sorted[uint8_t__pass];
        int32_t  int32_t__slot = (int32_t)uint8_t__pass - 1;
        while ((int32_t__slot >= 0) &&
               (uint32_t__sorted[int32_t__slot] > uint32_t__key))
        {
            uint32_t__sorted[int32_t__slot + 1] = uint32_t__sorted[int32_t__slot];
            int32_t__slot--;
        }
        uint32_t__sorted[int32_t__slot + 1] = uint32_t__key;
    }
    return uint32_t__sorted[2u];
}
#if (MEASUREMENT_CURRENT_MEDIAN3_ENABLE != 0u)
/* ==================== Measurement Current Median3 ==================== */
/**
 * @brief  [EN] Shift one new raw current count sample into the channel's
 *              median history and return the middle value. Window size =
 *              the runtime median size (any 1..15, even sizes too); a
 *              single-sample jump of the synchronized reading is discarded
 *              with zero added lag. Insertion sort of a LOCAL copy keeps
 *              the live history untouched (same pattern as the voltage
 *              median-5).
* @param uint8_t__channelIndex Current channel 0 or 1 /
* @param uint32_t__sampleCounts New raw count sample /
* @param uint8_t__medianSize Active median window (1..15) /
* @return uint32_t Median-filtered counts /
 */
static uint32_t func__Measurement_CurrentMedian(uint8_t uint8_t__channelIndex,
                                                uint32_t uint32_t__sampleCounts,
                                                uint8_t uint8_t__medianSize)
{
    uint32_t UINT32_T__A__Sorted[MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX];
    uint32_t *uint32_t__historyCounts;
    uint8_t uint8_t__index;
    uint8_t uint8_t__pass;
    if ((uint8_t__channelIndex >= 2u) ||
        (uint8_t__medianSize < 1u) ||
        (uint8_t__medianSize > (uint8_t)MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX))
    {
        return uint32_t__sampleCounts;
    }
    if (uint8_t__medianSize == 1u)
    {
        return uint32_t__sampleCounts;
    }
    uint32_t__historyCounts = UINT32_T__G__CurrentMedianHistoryCounts[uint8_t__channelIndex];
    if (BOOL__G__CurrentMedianSeeded[uint8_t__channelIndex] == false)
    {
        for (uint8_t__index = 0u; uint8_t__index < uint8_t__medianSize; uint8_t__index++)
        {
            uint32_t__historyCounts[uint8_t__index] = uint32_t__sampleCounts;
        }
        BOOL__G__CurrentMedianSeeded[uint8_t__channelIndex] = true;
    }
    else
    {
        for (uint8_t__index = (uint8_t)(uint8_t__medianSize - 1u);
             uint8_t__index > 0u;
             uint8_t__index--)
        {
            uint32_t__historyCounts[uint8_t__index] =
                uint32_t__historyCounts[uint8_t__index - 1u];
        }
        uint32_t__historyCounts[0] = uint32_t__sampleCounts;
    }
    for (uint8_t__index = 0u; uint8_t__index < uint8_t__medianSize; uint8_t__index++)
    {
        UINT32_T__A__Sorted[uint8_t__index] = uint32_t__historyCounts[uint8_t__index];
    }
    for (uint8_t__pass = 1u; uint8_t__pass < uint8_t__medianSize; uint8_t__pass++)
    {
        uint32_t uint32_t__key = UINT32_T__A__Sorted[uint8_t__pass];
        int32_t int32_t__slot = (int32_t)uint8_t__pass - 1;
        while ((int32_t__slot >= 0) &&
               (UINT32_T__A__Sorted[int32_t__slot] > uint32_t__key))
        {
            UINT32_T__A__Sorted[int32_t__slot + 1] = UINT32_T__A__Sorted[int32_t__slot];
            int32_t__slot--;
        }
        UINT32_T__A__Sorted[int32_t__slot + 1] = uint32_t__key;
    }
    return UINT32_T__A__Sorted[uint8_t__medianSize / 2u];
}
#endif
#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
/* ==================== Measurement Current Moving Average ==================== */
/**
 * @brief  [EN] Push one new raw current count sample into the channel's
 *              moving-average window and return the average. Until the
 *              window fills after a restart, the average runs over the
 *              collected samples only (no zero-drag from empty slots).
* @param uint8_t__channelIndex Current channel 0 or 1 /
* @param uint32_t__sampleCounts New raw count sample /
* @return uint32_t Moving-average counts /
 */
static uint32_t func__Measurement_CurrentMovingAverage(uint8_t uint8_t__channelIndex,
                                                       uint32_t uint32_t__sampleCounts)
{
    uint32_t uint32_t__windowSumCounts;
    uint32_t uint32_t__windowSlot;
    uint32_t uint32_t__averageCounts;
    if (uint8_t__channelIndex >= 2u)
    {
        return uint32_t__sampleCounts;
    }
    /*
 * [EN] Replace the oldest slot, then advance the ring index by hand (no
 * modulo, keeps the index inside the window under MISRA rules).
 */
    UINT32_T__G__CurrentAverageWindowCounts[uint8_t__channelIndex]
        [UINT16_T__G__CurrentAverageNextIndex[uint8_t__channelIndex]] = uint32_t__sampleCounts;
    UINT16_T__G__CurrentAverageNextIndex[uint8_t__channelIndex]++;
    if (UINT16_T__G__CurrentAverageNextIndex[uint8_t__channelIndex] >=
        UINT16_T__G__FilterAverageWindow)
    {
        UINT16_T__G__CurrentAverageNextIndex[uint8_t__channelIndex] = 0u;
    }
    if (UINT16_T__G__CurrentAverageFillCount[uint8_t__channelIndex] <
        UINT16_T__G__FilterAverageWindow)
    {
        UINT16_T__G__CurrentAverageFillCount[uint8_t__channelIndex]++;
    }
    /*
 * [EN] Sum only the filled slots and divide once by the fill count.
 */
    uint32_t__windowSumCounts = 0u;
    for (uint32_t__windowSlot = 0u;
         uint32_t__windowSlot < (uint32_t)UINT16_T__G__CurrentAverageFillCount[uint8_t__channelIndex];
         uint32_t__windowSlot++)
    {
        uint32_t__windowSumCounts +=
            UINT32_T__G__CurrentAverageWindowCounts[uint8_t__channelIndex][uint32_t__windowSlot];
    }
    uint32_t__averageCounts =
        uint32_t__windowSumCounts / (uint32_t)UINT16_T__G__CurrentAverageFillCount[uint8_t__channelIndex];
    return uint32_t__averageCounts;
}
#endif
/* ==================== Measurement ApplyCurrentFilters ==================== */
/**
 * @brief  [EN] Run the enabled current-filter chain of one channel in the
 *              fixed order: median first (kill single jumps), then the
 *              moving average (smooth) - exactly as the two compile-time
 *              switches select. Both off = pass-through unchanged.
* @param uint8_t__channelIndex Current channel 0 or 1 /
* @param uint32_t__sampleCounts Raw count sample /
* @return uint32_t Filtered counts /
 */
static uint32_t func__Measurement_ApplyCurrentFilters(uint8_t uint8_t__channelIndex,
                                                      uint32_t uint32_t__sampleCounts)
{
    (void)uint8_t__channelIndex;
#if (MEASUREMENT_CURRENT_MEDIAN3_ENABLE != 0u)
    /*
 * [EN] Runtime window (ESP panel): the compiled switch is the
 * capability, the size is the live setting (v1.4: 1..2 = bypass,
 * 3..15 = active, any value).
 */
    if (UINT8_T__G__FilterMedianSize >= 3u)
    {
        uint32_t__sampleCounts =
            func__Measurement_CurrentMedian(uint8_t__channelIndex,
                                            uint32_t__sampleCounts,
                                            UINT8_T__G__FilterMedianSize);
    }
#endif
#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
    /*
 * [EN] Runtime window (ESP panel): window 1 = bypass, >= 2 = active.
 */
    if (UINT16_T__G__FilterAverageWindow >= 2u)
    {
        uint32_t__sampleCounts =
            func__Measurement_CurrentMovingAverage(uint8_t__channelIndex, uint32_t__sampleCounts);
    }
#endif
    return uint32_t__sampleCounts;
}
/* ==================== Measurement ResetCurrentFilters ==================== */
/**
 * @brief  [EN] Zero both current-filter histories and restart both average
 *              ramps. Called from Init and whenever the measurement task
 *              sees that the runtime filter configuration changed, so a
 *              window resize never mixes stale slots into the average.
 *              Only the measurement task may call this.
 */
static void func__Measurement_ResetCurrentFilters(void)
{
#if (MEASUREMENT_CURRENT_MEDIAN3_ENABLE != 0u)
    for (uint32_t uint32_t__i = 0u; uint32_t__i < 2u; uint32_t__i++)
    {
        for (uint32_t uint32_t__j = 0u;
             uint32_t__j < (uint32_t)MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX;
             uint32_t__j++)
        {
            UINT32_T__G__CurrentMedianHistoryCounts[uint32_t__i][uint32_t__j] = 0u;
        }
        BOOL__G__CurrentMedianSeeded[uint32_t__i] = false;
    }
#endif
#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
    for (uint32_t uint32_t__i = 0u; uint32_t__i < 2u; uint32_t__i++)
    {
        for (uint32_t uint32_t__j = 0u; uint32_t__j < MEASUREMENT_CURRENT_AVERAGE_WINDOW; uint32_t__j++)
        {
            UINT32_T__G__CurrentAverageWindowCounts[uint32_t__i][uint32_t__j] = 0u;
        }
        UINT16_T__G__CurrentAverageFillCount[uint32_t__i] = 0u;
        UINT16_T__G__CurrentAverageNextIndex[uint32_t__i] = 0u;
    }
#endif
}
/**
 * @brief  [EN] Shift one new RAW count sample of a battery ADC channel
 *         into the median-5 history and return the filtered counts. The
 *         charger state machine and the battery-lost detector consume the
 *         voltages DERIVED from these filtered conversions, so a short
 *         spike burst cannot fake "battery gone".
* @param uint8_t__channelIndex 0 = V24_BAT raw, 1 = V12_BAT raw /
* @param uint32_t__sampleCounts New raw count sample /
* @return uint32_t Median-of-5 filtered counts /
 */
static uint32_t func__Measurement_MedianFilterVoltageSample(uint8_t uint8_t__channelIndex,
                                                            uint32_t uint32_t__sampleCounts)
{
    uint32_t *uint32_t__historyCounts;
    if (uint8_t__channelIndex >= 2u)
    {
        return uint32_t__sampleCounts;
    }
    uint32_t__historyCounts = UINT32_T__G__BatVoltageMedianHistoryCounts[uint8_t__channelIndex];
    if (BOOL__G__BatVoltageMedianSeeded[uint8_t__channelIndex] == false)
    {
        uint32_t__historyCounts[0] = uint32_t__sampleCounts;
        uint32_t__historyCounts[1] = uint32_t__sampleCounts;
        uint32_t__historyCounts[2] = uint32_t__sampleCounts;
        uint32_t__historyCounts[3] = uint32_t__sampleCounts;
        uint32_t__historyCounts[4] = uint32_t__sampleCounts;
        BOOL__G__BatVoltageMedianSeeded[uint8_t__channelIndex] = true;
    }
    else
    {
        uint32_t__historyCounts[0] = uint32_t__historyCounts[1];
        uint32_t__historyCounts[1] = uint32_t__historyCounts[2];
        uint32_t__historyCounts[2] = uint32_t__historyCounts[3];
        uint32_t__historyCounts[3] = uint32_t__historyCounts[4];
        uint32_t__historyCounts[4] = uint32_t__sampleCounts;
    }
    return func__Measurement_Median5(uint32_t__historyCounts);
}
/* ==================== Global Shared Values ==================== */
/*
 * [EN] The engineering values of the newest frame, shared to all tasks.
 *      Written ONLY by the measurement task (Run/Init); any module reads
 *      them after including measurement.h. Meas* prefix avoids a link
 *      collision with the UI manual test globals (task_ui.c).
 */
volatile uint32_t UINT32_T__G__MeasInputVoltageMv = 0u;
volatile uint32_t UINT32_T__G__MeasBattery24Mv = 0u;
volatile uint32_t UINT32_T__G__MeasBattery12Mv = 0u;
volatile uint32_t UINT32_T__G__MeasBatteryLowMv = 0u;
volatile uint32_t UINT32_T__G__MeasBatteryHighMv = 0u;
/*
 * [EN] Measured ADC reference (VDDA) in mV, from the internal 1.20 V channel.
 *      Published even while CAL_VDDA_TRACKING_ENABLE is 0, because the whole
 *      point is to LOOK at it first: compare this against a DMM on the 3.3 V
 *      rail, and the gap is the global scale error every channel carries.
 *      0 = not available / reading outside a plausible 3.0..3.6 V.
 */
volatile uint32_t UINT32_T__G__MeasVddaMv = 0u;
/*
 * [EN] Raw ADC counts of the voltage channels. WHY THEY ARE ALWAYS ON rather
 *      than behind a "calibration mode": a mode that neutralises gains and
 *      offsets would also neutralise them for the CONTROL path, so the
 *      over-voltage cut and the current fault would be judging uncalibrated
 *      numbers for as long as the mode is left on - and a mode left on by
 *      accident is a real failure mode on a bench. Publishing the raw counts
 *      alongside costs nothing, can never mis-protect, and is strictly MORE
 *      informative: from counts plus a DMM every coefficient can be rebuilt
 *      from scratch, including ones a bypass mode would still have applied.
 */
volatile uint32_t UINT32_T__G__MeasVinRawCounts = 0u;
volatile uint32_t UINT32_T__G__MeasV24RawCounts = 0u;
volatile uint32_t UINT32_T__G__MeasV12RawCounts = 0u;
volatile uint32_t UINT32_T__G__MeasVrefintRawCounts = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent1Ma = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent2Ma = 0u;
/*
 * [EN] Live current-chain diagnostics, unfiltered single-frame values of
 *      the last frame (user order 2026-09-22: raw counts -> shunt uV ->
 *      mA before any filter, so the chain can be checked against a scope
 *      and an ammeter step by step).
 */
volatile uint32_t UINT32_T__G__MeasCurrent1RawCounts = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent1ShuntUv = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent1MaUnfiltered = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent2RawCounts = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent2ShuntUv = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent2MaUnfiltered = 0u;
volatile bool BOOL__G__MeasInputPresent = false;
volatile bool BOOL__G__MeasDataValid = false;
/* ==================== Measurement Init ==================== */
/**
 * @brief  [EN] Zero the last snapshot (valid = false).
 */
void func__Measurement_Init(void)
{
    /*
 * [EN] Zero the warm-up counter, shared globals and snapshot; nothing is
 * valid until the required number of stable frames is collected.
 */
    UINT8_T__G__MeasurementWarmupFrameCount = 0u;
    BOOL__G__BatVoltageMedianSeeded[0u] = false;
    BOOL__G__BatVoltageMedianSeeded[1u] = false;
    /*
 * [EN] Clean filter state and re-sync the applied-copy of the runtime
 * filter configuration (a config that arrived before Init cannot
 * survive into the first frame).
 */
    func__Measurement_ResetCurrentFilters();
    UINT8_T__G__FilterMedianSizeApplied = UINT8_T__G__FilterMedianSize;
    UINT16_T__G__FilterAverageWindowApplied = UINT16_T__G__FilterAverageWindow;
    UINT32_T__G__MeasInputVoltageMv = 0u;
    UINT32_T__G__MeasBattery24Mv = 0u;
    UINT32_T__G__MeasBattery12Mv = 0u;
    UINT32_T__G__MeasBatteryLowMv = 0u;
    UINT32_T__G__MeasBatteryHighMv = 0u;
    UINT32_T__G__MeasVddaMv = 0u;
    UINT32_T__G__MeasVinRawCounts = 0u;
    UINT32_T__G__MeasV24RawCounts = 0u;
    UINT32_T__G__MeasV12RawCounts = 0u;
    UINT32_T__G__MeasVrefintRawCounts = 0u;
    UINT32_T__G__MeasCurrent1Ma = 0u;
    UINT32_T__G__MeasCurrent2Ma = 0u;
    UINT32_T__G__MeasCurrent1RawCounts = 0u;
    UINT32_T__G__MeasCurrent1ShuntUv = 0u;
    UINT32_T__G__MeasCurrent1MaUnfiltered = 0u;
    UINT32_T__G__MeasCurrent2RawCounts = 0u;
    UINT32_T__G__MeasCurrent2ShuntUv = 0u;
    UINT32_T__G__MeasCurrent2MaUnfiltered = 0u;
    BOOL__G__MeasInputPresent = false;
    BOOL__G__MeasDataValid = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch1_ma = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch2_ma = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
    BOOL__G__AdcStallActive = false;
    BOOL__G__AdcFirstRetryDone = false;
    UINT32_T__G__AdcStallStartMs = 0u;
    UINT32_T__G__AdcLastRecoverMs = 0u;
    BOOL__G__AdcHealthyActive = false;
    UINT32_T__G__AdcHealthyStartMs = 0u;
    BOOL__G__AdcPostResetUsed = func__BspAdc_WasRecoveryReset();
    BOOL__G__AdcFaultLatched = false;
}
/* ==================== Counts To Mv ==================== */
/**
 * @brief  [EN] Convert normalized ADC counts through the board calibration port.
* @param uint16_t__counts Normalized ADC count / ADC
* @return uint32_t Voltage in mV / mV
 */
uint32_t func__Measurement_CountsToMv(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_CountsToMv(uint16_t__counts);
}
/* ==================== V24 Counts To Mv ==================== */
/**
 * @brief  [EN] Convert a normalized 24 V channel through board calibration.
* @param uint16_t__counts Normalized ADC count / ADC
* @return uint32_t Source voltage in mV / mV
 */
uint32_t func__Measurement_V24CountsToMv(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_V24CountsToMv(uint16_t__counts);
}
/* ==================== Battery24 Counts To Mv ==================== */
/**
 * @brief  [EN] Convert the battery-PACK 24 V channel through the USER divider
 *              factor (2026-09-25): the pack sense path attenuates
 *              6.8k/69.2k to the pin, NOT the input net's 6.8k/76k.
* @param uint16_t__counts Normalized ADC count / ADC
* @return uint32_t Pack voltage in mV / mV
 */
uint32_t func__Measurement_Battery24CountsToMv(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_Battery24CountsToMv(uint16_t__counts);
}
/* ==================== V12 Counts To Mv ==================== */
/**
 * @brief  [EN] Convert the normalized 12 V channel through board calibration.
* @param uint16_t__counts Normalized ADC count / ADC
* @return uint32_t Source voltage in mV / mV
 */
uint32_t func__Measurement_V12CountsToMv(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_V12CountsToMv(uint16_t__counts);
}
/* ==================== Measurement Current Counts To Ma (bench table, current) ==================== */
/*
 * [EN] Bench correction on the COUNTS axis (user order 2026-10-10).
 *      - Table axis X = raw ADC counts (fixed fold only, no gain, no offset),
 *        so calibrating gain or offset can never change a table output
 *        inside the table range.
 *      - Between anchors: linear interpolation in counts, rounded ONCE.
 *      - Below the first anchor: Y[0] is held.
 *      - Above the last anchor: out = lastY + (BSPfull(c) - BSPfull(lastCounts)),
 *        using the gain/offset chain; the difference is rounded once.
 *        Offset cancels in this difference when lastCounts >= offset, so
 *        above the table only the gain matters.
 *      - No table in flash: out = BSPfull(c), the full gain/offset line.
 *      No intermediate rounding, no auto-inserted point, no silent drop.
 */
/**
 * @brief  [EN] Table lookup in counts; the caller supplies the above-table
 *              extension (BSPfull delta from the last anchor, one rounding).
* @param UINT32_T__A__Counts Anchor counts, strictly increasing /
* @param UINT32_T__A__BatteryTenthMa Anchor battery mA, non-decreasing / mA
* @param uint32_t__points Anchor count (>= 1) /
* @param uint16_t__counts Raw counts /
 * @param  uint32_t__aboveTenthMa     [EN] Extension used only when counts > last anchor
* @return uint32_t Battery mA / mA
 */
/**
 * @brief  [EN] Ceiling of n / d (d > 0). The table is capped by CalLut at
 *              CAL_LUT_BATTERY_TENTHS_MAX, so every numerator here fits 32 bits.
 */
static uint32_t func__Measurement_CeilDivU32(uint32_t uint32_t__numerator,
                                             uint32_t uint32_t__denominator)
{
    return (uint32_t__numerator + uint32_t__denominator - 1u) / uint32_t__denominator;
}
/**
 * @brief  [EN] Bench table lookup. The table and the above-anchor extension
 *              are in TENTHS of mA. The interpolation stays exact (no
 *              intermediate rounding) and the result is rounded UP once, to
 *              whole mA, so a reading is never below the true current.
 */
static uint32_t func__Measurement_BenchTableMa(const uint32_t *UINT32_T__A__Counts,
                                               const uint32_t *UINT32_T__A__BatteryTenthMa,
                                               uint32_t uint32_t__points,
                                               uint16_t uint16_t__counts,
                                               uint32_t uint32_t__aboveTenthMa)
{
    uint32_t uint32_t__index;
    uint32_t uint32_t__c = (uint32_t)uint16_t__counts;
    if (uint32_t__c <= UINT32_T__A__Counts[0u])
    {
        return func__Measurement_CeilDivU32(UINT32_T__A__BatteryTenthMa[0u], 10u);
    }
    for (uint32_t__index = 1u; uint32_t__index < uint32_t__points; uint32_t__index++)
    {
        uint32_t uint32_t__xLow = UINT32_T__A__Counts[uint32_t__index - 1u];
        uint32_t uint32_t__xHigh = UINT32_T__A__Counts[uint32_t__index];
        if (uint32_t__c <= uint32_t__xHigh)
        {
            uint32_t uint32_t__yLow = UINT32_T__A__BatteryTenthMa[uint32_t__index - 1u];
            uint32_t uint32_t__yHigh = UINT32_T__A__BatteryTenthMa[uint32_t__index];
            uint32_t uint32_t__span = uint32_t__xHigh - uint32_t__xLow;
            uint32_t uint32_t__step = uint32_t__c - uint32_t__xLow;
            if (uint32_t__span == 0u)
            {
                return func__Measurement_CeilDivU32(uint32_t__yHigh, 10u);
            }
            if (uint32_t__yHigh <= uint32_t__yLow)
            {
                return func__Measurement_CeilDivU32(uint32_t__yLow, 10u);
            }
            /* [EN] Exact: value_tenths = yLow + step x (yHigh - yLow) / span.
             *      mA = value_tenths / 10 = (yLow x span + step x dy) / (10 x span).
             *      With yLow, dy <= CAL_LUT_BATTERY_TENTHS_MAX and span, step <= 4095
             *      the numerator is below 2 x 200000 x 4095 < 2^32. */
            return func__Measurement_CeilDivU32(
                (uint32_t__yLow * uint32_t__span) +
                    (uint32_t__step * (uint32_t__yHigh - uint32_t__yLow)),
                uint32_t__span * 10u);
        }
    }
    /* [EN] Above the last anchor: last point + extension, both in tenths. */
    return func__Measurement_CeilDivU32(
        UINT32_T__A__BatteryTenthMa[uint32_t__points - 1u] + uint32_t__aboveTenthMa,
        10u);
}
/* ==================== Measurement_Current1CountsToMaLocked ==================== */
static uint32_t func__Measurement_Current1CountsToMaLocked(uint16_t uint16_t__counts)
{
    const uint32_t *UINT32_T__A__Counts;
    const uint32_t *UINT32_T__A__BatteryTenthMa;
    uint32_t uint32_t__points;
    uint32_t uint32_t__aboveTenthMa = 0u;
    if (func__CalLut_Active((uint8_t)CAL_LUT_CHANNEL_1) == false)
    {
        return func__BspMeasurement_Current1CountsToMa(uint16_t__counts);
    }
    UINT32_T__A__Counts = func__CalLut_Counts((uint8_t)CAL_LUT_CHANNEL_1);
    UINT32_T__A__BatteryTenthMa = func__CalLut_BatteryMa((uint8_t)CAL_LUT_CHANNEL_1);
    uint32_t__points = func__CalLut_Points((uint8_t)CAL_LUT_CHANNEL_1);
    if (uint16_t__counts > UINT32_T__A__Counts[uint32_t__points - 1u])
    {
        uint32_t__aboveTenthMa = func__BspMeasurement_Current1DeltaTenthMa(
            uint16_t__counts, (uint16_t)UINT32_T__A__Counts[uint32_t__points - 1u]);
    }
    return func__Measurement_BenchTableMa(UINT32_T__A__Counts,
                                          UINT32_T__A__BatteryTenthMa,
                                          uint32_t__points,
                                          uint16_t__counts,
                                          uint32_t__aboveTenthMa);
}
/* ==================== Measurement_Current1CountsToMa ==================== */
/**
 * @brief  [EN] Channel-1 counts to battery mA, read under the kernel lock so a
 *              CalLut commit cannot swap the table mid-lookup (race fix 2026-10-10).
 */
uint32_t func__Measurement_Current1CountsToMa(uint16_t uint16_t__counts)
{
    int32_t int32_t__savedKernelLock = osKernelLock();
    uint32_t uint32_t__ma;
    uint32_t__ma = func__Measurement_Current1CountsToMaLocked(uint16_t__counts);
    /*
 * [EN] Restore only after a lock that actually succeeded: a negative
 * return means the kernel was not running (or we are in an ISR) and
 * there is no lock state to put back.
 */
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
    return uint32_t__ma;
}
/* ==================== Measurement_Current2CountsToMaLocked ==================== */
/**
 * @brief  [EN] Channel-2 counts to battery-2 mA. Table present: bench table on
 *              the counts axis. No table: the full gain/offset line BSPfull.
* @param uint16_t__counts ADC count / ADC
* @return uint32_t Battery-2 current in mA / mA
 */
static uint32_t func__Measurement_Current2CountsToMaLocked(uint16_t uint16_t__counts)
{
    const uint32_t *UINT32_T__A__Counts;
    const uint32_t *UINT32_T__A__BatteryTenthMa;
    uint32_t uint32_t__points;
    uint32_t uint32_t__aboveTenthMa = 0u;
    if (func__CalLut_Active((uint8_t)CAL_LUT_CHANNEL_2) == false)
    {
        return func__BspMeasurement_Current2CountsToMa(uint16_t__counts);
    }
    UINT32_T__A__Counts = func__CalLut_Counts((uint8_t)CAL_LUT_CHANNEL_2);
    UINT32_T__A__BatteryTenthMa = func__CalLut_BatteryMa((uint8_t)CAL_LUT_CHANNEL_2);
    uint32_t__points = func__CalLut_Points((uint8_t)CAL_LUT_CHANNEL_2);
    if (uint16_t__counts > UINT32_T__A__Counts[uint32_t__points - 1u])
    {
        uint32_t__aboveTenthMa = func__BspMeasurement_Current2DeltaTenthMa(
            uint16_t__counts, (uint16_t)UINT32_T__A__Counts[uint32_t__points - 1u]);
    }
    return func__Measurement_BenchTableMa(UINT32_T__A__Counts,
                                          UINT32_T__A__BatteryTenthMa,
                                          uint32_t__points,
                                          uint16_t__counts,
                                          uint32_t__aboveTenthMa);
}
/* ==================== Measurement_Current2CountsToMa ==================== */
/**
 * @brief  [EN] Channel-2 counts to battery mA, read under the kernel lock so a
 *              CalLut commit cannot swap the table mid-lookup (race fix 2026-10-10).
 */
uint32_t func__Measurement_Current2CountsToMa(uint16_t uint16_t__counts)
{
    int32_t int32_t__savedKernelLock = osKernelLock();
    uint32_t uint32_t__ma;
    uint32_t__ma = func__Measurement_Current2CountsToMaLocked(uint16_t__counts);
    /*
 * [EN] Restore only after a lock that actually succeeded: a negative
 * return means the kernel was not running (or we are in an ISR) and
 * there is no lock state to put back.
 */
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
    return uint32_t__ma;
}
/* ==================== Measurement_CurrentIsBatteryCalibrated ==================== */
bool func__Measurement_CurrentIsBatteryCalibrated(uint8_t uint8_t__channelIndex)
{
    /*
 * [EN] Channels 1 and 2 always return BATTERY-side current (chain with
     *      gain and offset, then the bench table when present). Whether the
     *      table has been calibrated is a separate question for the panel.
 */
    return (uint8_t__channelIndex <= 1u);
}
/* ==================== Measurement Current Counts To Shunt Uv ==================== */
/**
 * @brief  [EN] Raw current counts to the pure-hardware shunt voltage in uV
 *              via the BSP (no zero offset, no bench trim) - live
 *              diagnostic for the current-chain review (user order
 *              2026-09-22).
* @param uint16_t__counts ADC count / ADC
* @return uint32_t Shunt voltage in uV / uV
 */
uint32_t func__Measurement_CurrentCountsToShuntUv(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_CurrentCountsToShuntUv(uint16_t__counts);
}
/* ==================== Measurement Battery12 Bench Compensation (user order 2026-09-25) ==================== */
/* ==================== Measurement ApplyVoltageOffsetMv ==================== */
/**
 * @brief  [EN] Saturating signed add of one runtime calibration offset to a
 *              voltage in mV; the result is never below 0 mV. The offset is
 *              clamped to +/-MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV by its
 *              setter, so the sum cannot overflow int32_t.
* @param uint32_t__voltageMv Converted voltage, mV /
* @param int32_t__offsetMv Runtime offset, mV /
* @return uint32_t Offset voltage, mV /
 */
static uint32_t func__Measurement_ApplyVoltageOffsetMv(uint32_t uint32_t__voltageMv,
                                                       int32_t int32_t__offsetMv)
{
    int32_t int32_t__resultMv;
    int32_t__resultMv = (int32_t)uint32_t__voltageMv + int32_t__offsetMv;
    if (int32_t__resultMv < 0)
    {
        return 0u;
    }
    return (uint32_t)int32_t__resultMv;
}
/* ==================== Measurement Run ==================== */
/**
 * @brief  [EN] Pull one stable raw frame from the BSP, convert all ADC
 *              channels and publish one coherent shared snapshot.
 */
void func__Measurement_Run(void)
{
    uint16_t uint16_t__raw[BSP_ADC_CHANNEL_COUNT];
    uint16_t uint16_t__battery24CountsFiltered;
    uint16_t uint16_t__battery12CountsFiltered;
    uint32_t uint32_t__current1CountsFiltered;
    uint32_t uint32_t__current2CountsFiltered;
    uint32_t uint32_t__current1SampleMa;
    uint32_t uint32_t__current1Ma;
    uint32_t uint32_t__current1ShuntUv;
    uint32_t uint32_t__inputVoltageMv;
    uint32_t uint32_t__battery24Mv;
    uint32_t uint32_t__battery12Mv;
    uint32_t uint32_t__batteryLowMv;
    uint32_t uint32_t__batteryHighMv;
    uint32_t uint32_t__current2SampleMa;
    uint32_t uint32_t__current2Ma;
    uint32_t uint32_t__current2ShuntUv;
    bool bool__frameCopied;
    bool bool__inputPresent;
    int32_t int32_t__savedKernelLock;
    /*
 * [EN] Once both 60 s recovery windows (pre-reset and post-reset) have
 * failed and FAULT_ADC is latched, never publish sporadic frames again in
 * this power session.
 */
    if (BOOL__G__AdcFaultLatched != false)
    {
        UINT8_T__G__MeasurementWarmupFrameCount = 0u;
        BOOL__G__MeasDataValid = false;
        MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
        return;
    }
    /*
 * [EN] GetRaw copies only a completed DMA half-frame; no ADC register
 * polling is performed here.
 */
    bool__frameCopied = func__BspAdc_GetRaw(uint16_t__raw);
    if (bool__frameCopied == false)
    {
        /*
 * [EN] A missing stable frame restarts warm-up, clears the voltage and
 * current filter histories so pre-outage samples cannot bleed into
 * recovered readings, and invalidates the ADC snapshot.
 */
        UINT8_T__G__MeasurementWarmupFrameCount = 0u;
        BOOL__G__BatVoltageMedianSeeded[0u] = false;
        BOOL__G__BatVoltageMedianSeeded[1u] = false;
        func__Measurement_ResetCurrentFilters();
        int32_t__savedKernelLock = osKernelLock();
        if (int32_t__savedKernelLock >= 0)
        {
            BOOL__G__MeasDataValid = false;
            MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
            (void)osKernelRestoreLock(int32_t__savedKernelLock);
        }
        else
        {
            BOOL__G__MeasDataValid = false;
            MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
        }
        return;
    }
    /*
 * [EN] Count only completed stable frames; saturation keeps the small
 * warm-up counter from wrapping after validity is reached.
 */
    if (UINT8_T__G__MeasurementWarmupFrameCount < MEASUREMENT_WARMUP_FRAME_COUNT)
    {
        UINT8_T__G__MeasurementWarmupFrameCount++;
    }
    /*
 * [EN] Runtime filter-config change detection (ESP panel, user order
 *          2026-09-22): when the median size or the average window moved,
 *          reset the filter state in this task's own context and take over
 *          the new configuration - a resize never mixes stale slots into
 *          the filters and no cross-task lock is needed.
 */
    if ((UINT8_T__G__FilterMedianSizeApplied != UINT8_T__G__FilterMedianSize) ||
        (UINT16_T__G__FilterAverageWindowApplied != UINT16_T__G__FilterAverageWindow))
    {
        func__Measurement_ResetCurrentFilters();
        UINT8_T__G__FilterMedianSizeApplied = UINT8_T__G__FilterMedianSize;
        UINT16_T__G__FilterAverageWindowApplied = UINT16_T__G__FilterAverageWindow;
    }
    /*
 * [EN] Convert into locals first so other tasks never observe a partly
 * updated measurement set.
 */
    /*
 * [EN] Currents: the board port delivers the PWM mid-ON synchronized
 * raw counts in the CURRENT1/CURRENT2 frame positions; this module
 * runs the switchable filter chain on the RAW counts first (median to
 * kill single-sample jumps, moving average to smooth) and converts the
 * filtered counts to mA after. Voltages keep their median-5 spike
 * guard, likewise on the raw counts.
 */
/*
 * [EN] Filters run on the RAW ADC counts (user order 2026-09-27):
 * median/average of 12-bit counts cannot exceed 4095, so the u16 casts
 * at the conversion calls are airtight. The unfiltered converted
 * samples stay for telemetry (unf) and diagnostics.
 */
    uint32_t__current1CountsFiltered =
        func__Measurement_ApplyCurrentFilters(0u,
            (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1]);
    uint32_t__current1SampleMa =
        func__Measurement_Current1CountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1]);
    uint32_t__current1ShuntUv =
        func__Measurement_CurrentCountsToShuntUv(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1]);
    uint32_t__current1Ma =
        func__Measurement_Current1CountsToMa((uint16_t)uint32_t__current1CountsFiltered);
    uint32_t__inputVoltageMv =
        func__Measurement_V24CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_24V_IN]);
    /*
 * [EN] Median-5 on the raw counts of the two battery ADC channels
 * (Vin keeps no filter, exactly as before).
 */
    uint16_t__battery24CountsFiltered =
        (uint16_t)func__Measurement_MedianFilterVoltageSample(0u,
            (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_24V_BAT]);
    uint16_t__battery12CountsFiltered =
        (uint16_t)func__Measurement_MedianFilterVoltageSample(1u,
            (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_12V_BAT]);
    uint32_t__battery24Mv =
        func__Measurement_Battery24CountsToMv(uint16_t__battery24CountsFiltered);
    uint32_t__battery12Mv =
        func__Measurement_V12CountsToMv(uint16_t__battery12CountsFiltered);
    /*
 * [EN] Channel-2 sample conversion moved BEFORE the voltage chain (user
 * order 2026-09-25): the V12 bench compensation needs the corrected
 * channel-2 current. Pure function of the raw frame - no state, so
 * the earlier position changes nothing else.
 */
    uint32_t__current2SampleMa =
        func__Measurement_Current2CountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2]);
    uint32_t__current2CountsFiltered =
        func__Measurement_ApplyCurrentFilters(1u,
            (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2]);
    uint32_t__current2ShuntUv =
        func__Measurement_CurrentCountsToShuntUv(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2]);
    /*
 * [EN] Runtime voltage calibration offsets (ESP panel, user order
 *          2026-09-22): applied after the divider conversion and before the
 *          low/high derivation, each clamped by the setter to
 *          +/-MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV; default 0 keeps today's
 *          behavior. Saturating signed add, result never below 0 mV.
 */
    uint32_t__inputVoltageMv = func__Measurement_ApplyVoltageOffsetMv(
        uint32_t__inputVoltageMv, INT32_T__G__VoltageInOffsetMv);
    uint32_t__battery24Mv = func__Measurement_ApplyVoltageOffsetMv(
        uint32_t__battery24Mv, INT32_T__G__Voltage24OffsetMv);
    uint32_t__battery12Mv = func__Measurement_ApplyVoltageOffsetMv(
        uint32_t__battery12Mv, INT32_T__G__Voltage12OffsetMv);
    uint32_t__batteryLowMv = uint32_t__battery12Mv;
    if (uint32_t__battery24Mv >= uint32_t__battery12Mv)
    {
        uint32_t__batteryHighMv = uint32_t__battery24Mv - uint32_t__battery12Mv;
    }
    else
    {
        uint32_t__batteryHighMv = 0u;
    }
    /*
 * [EN] No median here anymore: the median-5 ran on the raw V24/V12
 * counts above (user order 2026-09-27), so a single-frame ADC spike on
 * the switching node is already dead before the conversion; low/high
 * derive from the filtered conversions. Real steps pass with only a
 * few frames of lag, no moving average.
 */
    /*
 * [EN] Convert the filtered counts once per pass (counts axis, no
 * voltage input).
 */
    uint32_t__current2Ma =
        func__Measurement_Current2CountsToMa((uint16_t)uint32_t__current2CountsFiltered);
    /*
 * [EN] The BSP exposes the board input-detect signal as a logical GPIO;
 * polarity and physical pin mapping remain inside the board port.
 */
    bool__inputPresent =
        func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT);
    /*
 * [EN] Publish globals and snapshot while the RTOS scheduler is locked.
 * The snapshot valid bit is written last; this uses CMSIS-RTOS2 rather
 * than an MCU-specific interrupt instruction.
 */
    int32_t__savedKernelLock = osKernelLock();
    if (int32_t__savedKernelLock < 0)
    {
        BOOL__G__MeasDataValid = false;
        MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
        return;
    }
    UINT32_T__G__MeasCurrent1Ma = uint32_t__current1Ma;
    /*
 * [EN] Unfiltered current-chain diagnostics of this frame: raw counts,
 * pure-hardware shunt uV and pre-filter mA (user order 2026-09-22).
 */
    UINT32_T__G__MeasCurrent1RawCounts =
        (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1];
    UINT32_T__G__MeasCurrent1ShuntUv = uint32_t__current1ShuntUv;
    UINT32_T__G__MeasCurrent1MaUnfiltered = uint32_t__current1SampleMa;
    UINT32_T__G__MeasInputVoltageMv = uint32_t__inputVoltageMv;
    UINT32_T__G__MeasBattery24Mv = uint32_t__battery24Mv;
    UINT32_T__G__MeasBattery12Mv = uint32_t__battery12Mv;
    UINT32_T__G__MeasBatteryLowMv = uint32_t__batteryLowMv;
    UINT32_T__G__MeasBatteryHighMv = uint32_t__batteryHighMv;
    UINT32_T__G__MeasVddaMv = func__BspMeasurement_VddaMv(
        uint16_t__raw[BSP_ADC_CHANNEL_VREFINT], CAL_VREFINT_MV);
    UINT32_T__G__MeasVinRawCounts =
        (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_24V_IN];
    /*
 * [EN] Audit fix 2026-10-03: the V24/V12 slots used to publish the
 * median+average filtered counts under a "RawCounts" name, so the
 * calibration-ground-truth channel lied during sweeps (v1.25 esp_link
 * comment: counts BEFORE any processing). Now all three voltage slots
 * carry the true DMA frame counts, matching Vin; the filtered copy still
 * feeds the mV maths above and the control loops, which are untouched.
 */
    UINT32_T__G__MeasV24RawCounts =
        (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_24V_BAT];
    UINT32_T__G__MeasV12RawCounts =
        (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_12V_BAT];
    UINT32_T__G__MeasVrefintRawCounts =
        (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_VREFINT];
    UINT32_T__G__MeasCurrent2Ma = uint32_t__current2Ma;
    UINT32_T__G__MeasCurrent2RawCounts =
        (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2];
    UINT32_T__G__MeasCurrent2ShuntUv = uint32_t__current2ShuntUv;
    UINT32_T__G__MeasCurrent2MaUnfiltered = uint32_t__current2SampleMa;
    BOOL__G__MeasInputPresent = bool__inputPresent;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch1_ma = uint32_t__current1Ma;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv = uint32_t__inputVoltageMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = uint32_t__battery24Mv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv = uint32_t__battery12Mv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = uint32_t__batteryLowMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv = uint32_t__batteryHighMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch2_ma = uint32_t__current2Ma;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = bool__inputPresent;
    /*
 * [EN] ADC validity depends only on the warm-up count, never on input
 * voltage or input presence. Write the public flag before snapshot.valid.
 */
    if (UINT8_T__G__MeasurementWarmupFrameCount >= MEASUREMENT_WARMUP_FRAME_COUNT)
    {
        BOOL__G__MeasDataValid = true;
    }
    else
    {
        BOOL__G__MeasDataValid = false;
    }
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = BOOL__G__MeasDataValid;
    (void)osKernelRestoreLock(int32_t__savedKernelLock);
}
/* ==================== Measurement Get Snapshot ==================== */
/**
 * @brief  [EN] Copy the last snapshot. Returns false if pointer is NULL or
 *              data is not valid yet.
 * @param  measurement_snapshot_t__out [EN] Output pointer, must not be NULL /
* @return bool true if a valid snapshot was copied /
 */
bool func__Measurement_GetSnapshot(measurement_snapshot_t *measurement_snapshot_t__out)
{
    int32_t int32_t__savedKernelLock;
    bool bool__snapshotValid;
    if (measurement_snapshot_t__out == NULL)
    {
        return false;
    }
    /*
 * [EN] Prevent a task switch while copying the multi-field snapshot.
 * CMSIS-RTOS2 keeps this independent of the MCU core instructions.
 */
    int32_t__savedKernelLock = osKernelLock();
    if (int32_t__savedKernelLock < 0)
    {
        return false;
    }
    *measurement_snapshot_t__out = MEASUREMENT_SNAPSHOT_T__G__Snap;
    bool__snapshotValid = MEASUREMENT_SNAPSHOT_T__G__Snap.valid;
    (void)osKernelRestoreLock(int32_t__savedKernelLock);
    return bool__snapshotValid;
}
/* ==================== Measurement runtime config API (ESP panel) ==================== */
/**
 * @brief  [EN] Set the runtime median window size of the current filter
 *              (one size parameter per filter; 1 = bypass, so no separate
 *              on/off switch). ANY size 1..MAX is valid (even sizes too,
 *              no odd rounding; 1..2 behave as bypass). The compiled
 *              switch stays the capability gate: 0 clamps the request to
 *              1. Flash-persisted (NVM id 7).
* @param uint8_t__medianSize Requested size /
* @return uint8_t Applied size /
 */
uint8_t func__Measurement_SetFilterMedianSize(uint8_t uint8_t__medianSize)
{
#if (MEASUREMENT_CURRENT_MEDIAN3_ENABLE != 0u)
    /*
 * [EN] v1.4 (user order 2026-09-25): ANY size 1..MAX - even sizes
 * allowed, no odd rounding. 1..2 behave as bypass in the task.
 */
    if (uint8_t__medianSize < 1u)
    {
        uint8_t__medianSize = 1u;
    }
    else if (uint8_t__medianSize > (uint8_t)MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX)
    {
        uint8_t__medianSize = (uint8_t)MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX;
    }
    else
    {
        /*
 * [EN] Any value inside 1..MAX is taken as-is.
 */
    }
#else
    (void)uint8_t__medianSize;
    uint8_t__medianSize = 1u;
#endif
    UINT8_T__G__FilterMedianSize = uint8_t__medianSize;
    return uint8_t__medianSize;
}
/* ==================== Measurement_SetFilterAverageWindow ==================== */
/**
 * @brief  [EN] Set the runtime moving-average window size, clamped to
 *              1..MEASUREMENT_CURRENT_AVERAGE_WINDOW (the compiled ring
 *              size is the hard ceiling). The measurement task resets the
 *              filter state on the next frame after a change.
* @param uint32_t__windowSamples Requested window /
* @return uint16_t Applied window /
 */
uint16_t func__Measurement_SetFilterAverageWindow(uint32_t uint32_t__windowSamples)
{
    uint16_t uint16_t__appliedWindow;
    /*
 * [EN] Clamp-then-narrow (full-program audit 2026-09-26): the old u8
 * path sliced 256..300 to 0..44 AND clamped the ceiling to (u8)300 =
 * 44, so windows above 44 were unreachable. The wire/u32 value is
 * clamped first, then narrowed - the storage/index/counters are u16.
 */
    if (uint32_t__windowSamples < 1u)
    {
        uint16_t__appliedWindow = 1u;
    }
    else if (uint32_t__windowSamples > (uint32_t)MEASUREMENT_CURRENT_AVERAGE_WINDOW)
    {
        uint16_t__appliedWindow = (uint16_t)MEASUREMENT_CURRENT_AVERAGE_WINDOW;
    }
    else
    {
        uint16_t__appliedWindow = (uint16_t)uint32_t__windowSamples;
    }
    UINT16_T__G__FilterAverageWindow = uint16_t__appliedWindow;
    return uint16_t__appliedWindow;
}
/* ==================== Measurement_GetFilterMedianSize ==================== */
/**
 * @brief  [EN] Read the live median window size of the current filter.
* @return uint8_t 1, 3 or 5 /
 */
uint8_t func__Measurement_GetFilterMedianSize(void)
{
    return UINT8_T__G__FilterMedianSize;
}
/* ==================== Measurement_GetFilterAverageWindow ==================== */
/**
 * @brief  [EN] Read the live moving-average window size.
* @return uint16_t Window in samples /
 */
uint16_t func__Measurement_GetFilterAverageWindow(void)
{
    return UINT16_T__G__FilterAverageWindow;
}
/**
 * @brief  [EN] Set one runtime voltage calibration offset, clamped to
 *              +/-MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV. Index 0 = 24 V
 *              input, 1 = 24 V battery pack, 2 = 12 V (middle node)
 *              battery. Default 0 = today's behavior; flash-persisted
 *              since v1.14 (NVM ids 4/5/6).
* @param uint8_t__channelIndex 0 = VIN, 1 = V24, 2 = V12 /
* @param int32_t__offsetMv Requested offset, mV /
* @return int32_t Applied offset, mV /
 */
int32_t func__Measurement_SetVoltageOffsetMv(uint8_t uint8_t__channelIndex,
                                             int32_t int32_t__offsetMv)
{
    if (int32_t__offsetMv > (int32_t)MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV)
    {
        int32_t__offsetMv = (int32_t)MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV;
    }
    else if (int32_t__offsetMv < -(int32_t)MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV)
    {
        int32_t__offsetMv = -(int32_t)MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV;
    }
    else
    {
    }
    if (uint8_t__channelIndex == 0u)
    {
        INT32_T__G__VoltageInOffsetMv = int32_t__offsetMv;
    }
    else if (uint8_t__channelIndex == 1u)
    {
        INT32_T__G__Voltage24OffsetMv = int32_t__offsetMv;
    }
    else
    {
        INT32_T__G__Voltage12OffsetMv = int32_t__offsetMv;
    }
    return int32_t__offsetMv;
}
/* ==================== Measurement_GetVoltageOffsetMv ==================== */
/**
 * @brief  [EN] Read one runtime voltage calibration offset.
* @param uint8_t__channelIndex 0 = VIN, 1 = V24, 2 = V12 /
* @return int32_t Live offset, mV /
 */
int32_t func__Measurement_GetVoltageOffsetMv(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return INT32_T__G__VoltageInOffsetMv;
    }
    if (uint8_t__channelIndex == 1u)
    {
        return INT32_T__G__Voltage24OffsetMv;
    }
    return INT32_T__G__Voltage12OffsetMv;
}
/* ==================== Measurement_SuperviseAdc ==================== */
/**
 * @brief  [EN] Supervise ADC1+DMA liveness on every measurement pass: silently
 *              retry in-place hardware recovery for 60 s, perform at most one
 *              MCU software reset, retry silently for another 60 s after that
 *              reset, and latch FAULT_ADC only if still unrecovered.
* @param uint32_t__nowMs Monotonic kernel time in milliseconds /
 */
void func__Measurement_SuperviseAdc(uint32_t uint32_t__nowMs)
{
    uint32_t uint32_t__stallElapsedMs;
    uint32_t uint32_t__healthyElapsedMs;
    uint32_t uint32_t__retryElapsedMs;
    if (BOOL__G__AdcFaultLatched != false)
    {
#if MODULE_FAULT
        func__Fault_Set(FAULT_ADC);
#endif
        return;
    }
    if (BOOL__G__MeasDataValid != false)
    {
        BOOL__G__AdcStallActive = false;
        BOOL__G__AdcFirstRetryDone = false;
        /*
 * [EN] Re-arm the single-reset budget only after 60 s of continuous
 * healthy operation so an intermittent short-lived boot can never loop.
 */
        if (BOOL__G__AdcPostResetUsed != false)
        {
            if (BOOL__G__AdcHealthyActive == false)
            {
                BOOL__G__AdcHealthyActive = true;
                UINT32_T__G__AdcHealthyStartMs = uint32_t__nowMs;
            }
            else
            {
                uint32_t__healthyElapsedMs =
                    uint32_t__nowMs - UINT32_T__G__AdcHealthyStartMs;
                if (uint32_t__healthyElapsedMs >= MEASUREMENT_ADC_RECOVER_WINDOW_MS)
                {
                    BOOL__G__AdcPostResetUsed = false;
                    BOOL__G__AdcHealthyActive = false;
                }
            }
        }
        return;
    }
    BOOL__G__AdcHealthyActive = false;
    if (BOOL__G__AdcStallActive == false)
    {
        BOOL__G__AdcStallActive = true;
        BOOL__G__AdcFirstRetryDone = false;
        UINT32_T__G__AdcStallStartMs = uint32_t__nowMs;
        UINT32_T__G__AdcLastRecoverMs = uint32_t__nowMs;
    }
    uint32_t__stallElapsedMs = uint32_t__nowMs - UINT32_T__G__AdcStallStartMs;
    if (uint32_t__stallElapsedMs >= MEASUREMENT_ADC_RECOVER_WINDOW_MS)
    {
        if (BOOL__G__AdcPostResetUsed == false)
        {
            BOOL__G__AdcPostResetUsed = true;
            BOOL__G__AdcStallActive = false;
            BOOL__G__AdcFirstRetryDone = false;
            func__BspAdc_TriggerRecoveryReset();
            return;
        }
        BOOL__G__AdcFaultLatched = true;
#if MODULE_FAULT
        func__Fault_Set(FAULT_ADC);
#endif
        return;
    }
    /*
 * [EN] While warm-up frames are actively arriving (1..2 frames after boot
 * or after a successful Recover), let warm-up finish without resetting ADC.
 */
    if (UINT8_T__G__MeasurementWarmupFrameCount > 0u)
    {
        return;
    }
    uint32_t__retryElapsedMs = uint32_t__nowMs - UINT32_T__G__AdcLastRecoverMs;
    if ((BOOL__G__AdcFirstRetryDone == false) ||
        (uint32_t__retryElapsedMs >= MEASUREMENT_ADC_RECOVER_RETRY_MS))
    {
        BOOL__G__AdcFirstRetryDone = true;
        UINT32_T__G__AdcLastRecoverMs = uint32_t__nowMs;
        (void)func__BspAdc_Recover();
    }
}
