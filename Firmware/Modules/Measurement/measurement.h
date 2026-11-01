/**
 * @file    measurement.h
 * @brief   [EN] Converts ADC counts to engineering units (mV / mA) and keeps
 *              the latest snapshot for the other tasks. Pure software: the
 *              ADC+DMA hardware fills the raw buffer autonomously (bsp_adc),
 *              this module only converts.
 */
#ifndef MEASUREMENT_H
#define MEASUREMENT_H
/* ==================== Includes ==================== */
#include "app_types.h"
/* ==================== Defines ==================== */
/*
 * [EN] MEASUREMENT TASK PERIOD - change the sample rate HERE. The BSP
 *      returns only completed normalized frames, so this period is
 *      independent of the ADC clock. 1 ms keeps the synchronized current
 *      samples at most 1 ms old when the control task reads them.
 */
#define MEASUREMENT_PERIOD_MS      1u
/* ==================== Current filter switches ==================== */
/*
 * [EN] Median-of-3 prefilter on each charge-current channel (user order
 *      2026-09-22): kills single-sample jumps of the synchronized mid-ON
 *      reading with zero added lag. Set to 1u to enable, 0u to compile it
 *      out completely; the sample then passes through unchanged.
 */
#define MEASUREMENT_CURRENT_MEDIAN3_ENABLE   1u
/*
 * [EN] Moving average over the last MEASUREMENT_CURRENT_AVERAGE_WINDOW
 *      current samples (user order 2026-09-22: window of 10). At the 1 ms
 *      measurement cadence this smooths ~10 ms of history and reacts to a
 *      real current step within one window. Set to 1u to enable, 0u to
 *      compile it out completely.
 */
#define MEASUREMENT_CURRENT_AVERAGE_ENABLE   1u
/*
 * [EN] calibration.h holds only the VREFINT constant and the VDDA tracking
 * switch. The bench tables are NOT compiled in: they live in flash
 * (CalLut) and the gain and offset line is the fallback.
 */
/*
 * [EN] Compiled ring size / hard ceiling of the ESP-adjustable runtime
 *      moving-average window, per channel. WHY 300: TLM streams at 10 Hz,
 *      so at W <= 100 two panel samples share almost no filter history -
 *      the filter worked but was INVISIBLE on the panel; W = 200..300
 *      spans 2..3 TLM frames and the smoothing becomes observable.
 *      CAVEAT: the auto-mode charger regulates at 100 Hz on this filtered
 *      value - keep W <= ~50 in AUTO mode; large W is for MANUAL-duty
 *      bench watching. Ring RAM cost: 2 x 300 x 4 B.
 */
#define MEASUREMENT_CURRENT_AVERAGE_WINDOW   300u
/*
 * [EN] Startup default of the runtime moving-average window (today's
 */
#define MEASUREMENT_CURRENT_AVERAGE_WINDOW_DEFAULT 10u
/*
 * [EN] Hard ceiling of the ESP-adjustable runtime median window (v1.4,
 *      user order 2026-09-25: the panel can set ANY value 1..15 - even
 *      sizes allowed, no odd rounding anymore; 1..2 = bypass, 3..15 =
 *      active insertion-sort median). The history arrays are sized by
 *      this constant.
 */
#define MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX  15u
/*
 * [EN] Clamp limit of the ESP-adjustable runtime voltage calibration
 *      offsets in mV (user order 2026-09-22): the panel can trim each
 *      voltage channel by at most +/-2 V; default 0 keeps today's behavior.
 */
/*
 * [EN] v1.10 (user order 2026-09-25): raised 2000 -> 5000 mV. The pack
 * divider error alone was ~2.3 V at 24 V, beyond the old range, so the
 * runtime offset could not even express it. The divider itself is now
 * corrected at the source (BSP battery-24 factor); the wider range
 * keeps future divider/resistor drift correctable from the panel.
 */
#define MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV  5000u
/*
 * [EN] Silent ADC1+DMA self-healing cadence and two-stage escalation window
 *      (user order 2026-10-25): attempt in-place recovery for 60 s before
 *      issuing one MCU software reset, then attempt in-place recovery for
 *      another 60 s after that reset before latching FAULT_ADC.
 */
#define MEASUREMENT_ADC_RECOVER_RETRY_MS     500u
#define MEASUREMENT_ADC_RECOVER_WINDOW_MS    60000u
/* ==================== Globals (shared values) ==================== */
/*
 * [EN] Shared engineering values, written ONLY by the measurement task
 *      (Run). Any module/task can read them (#include "measurement.h");
 *      check BOOL__G__MeasDataValid first. Named Meas* on purpose to
 *      avoid a link collision with the manual test globals in task_ui.c.
 */
extern volatile uint32_t UINT32_T__G__MeasInputVoltageMv;
extern volatile uint32_t UINT32_T__G__MeasBattery24Mv;
extern volatile uint32_t UINT32_T__G__MeasBattery12Mv;
extern volatile uint32_t UINT32_T__G__MeasBatteryLowMv;
extern volatile uint32_t UINT32_T__G__MeasBatteryHighMv;
/*
 * [EN] Measured ADC reference (VDDA), mV; 0 = unavailable.
 */
extern volatile uint32_t UINT32_T__G__MeasVddaMv;
/*
 * [EN] RAW ADC counts of the three voltage channels and the internal
 *      reference, straight off the DMA frame - before the divider maths,
 *      before the runtime offsets, before the V12 bench compensation.
 *      They exist so a calibration can be built from FIRST PRINCIPLES: counts
 *      are the only number on this board that no coefficient can distort, so
 *      a sweep that logs counts next to a DMM lets every scale be recomputed
 *      offline without trusting any constant currently in the firmware.
 *      Deliberately NOT gated behind a mode - see measurement.c.
 */
extern volatile uint32_t UINT32_T__G__MeasVinRawCounts;
extern volatile uint32_t UINT32_T__G__MeasV24RawCounts;
extern volatile uint32_t UINT32_T__G__MeasV12RawCounts;
extern volatile uint32_t UINT32_T__G__MeasVrefintRawCounts;
extern volatile uint32_t UINT32_T__G__MeasCurrent1Ma;
extern volatile uint32_t UINT32_T__G__MeasCurrent2Ma;
/*
 * [EN] Unfiltered single-frame current-chain diagnostics (user order
 *      2026-09-22): raw ADC counts, pure-hardware shunt voltage (no offset,
 *      no trim) and unfiltered single-frame mA of the last frame, per channel.
 */
extern volatile uint32_t UINT32_T__G__MeasCurrent1RawCounts;
extern volatile uint32_t UINT32_T__G__MeasCurrent1ShuntUv;
extern volatile uint32_t UINT32_T__G__MeasCurrent1MaUnfiltered;
extern volatile uint32_t UINT32_T__G__MeasCurrent2RawCounts;
extern volatile uint32_t UINT32_T__G__MeasCurrent2ShuntUv;
extern volatile uint32_t UINT32_T__G__MeasCurrent2MaUnfiltered;
extern volatile bool BOOL__G__MeasInputPresent;
extern volatile bool BOOL__G__MeasDataValid;
/* ==================== Measurement Init ==================== */
/**
 * @brief  [EN] Zero the last snapshot (valid = false).
 */
void func__Measurement_Init(void);
/* ==================== Measurement Run ==================== */
/**
 * @brief  [EN] Pull one raw frame from the BSP and convert every channel
 *              into the shared snapshot (mV / mA + input_present + valid).
 * @note   [EN] Three completed stable ADC frames are required before this
 *              function publishes valid data (unit: completed ADC frame);
 *              input voltage / input presence do not affect ADC validity.
 */
#define MEASUREMENT_WARMUP_FRAME_COUNT 3u
void func__Measurement_Run(void);
/* ==================== Measurement Get Snapshot ==================== */
/**
 * @brief  [EN] Copy the last snapshot. Returns false if not valid yet.
 * @param  measurement_snapshot_t__out [EN] Output pointer, must not be NULL /
* @return bool true if a valid snapshot was copied /
 */
bool func__Measurement_GetSnapshot(measurement_snapshot_t *measurement_snapshot_t__out);
/* ==================== Counts To Mv ==================== */
/**
 * @brief  [EN] Convert normalized ADC counts to millivolts using the board
 *              calibration port.
* @param uint16_t__counts Raw ADC count, 0..4095 / ADC
* @return uint32_t Voltage in mV, 0..3300 / mV
 */
uint32_t func__Measurement_CountsToMv(uint16_t uint16_t__counts);
/* ==================== V24 Counts To Mv ==================== */
/**
 * @brief  [EN] Convert normalized 24 V channels to source mV through the
 *              board calibration port.
* @param uint16_t__counts Raw ADC count, 0..4095 / ADC
* @return uint32_t Source voltage in mV, 0..~37000 / mV
 */
uint32_t func__Measurement_V24CountsToMv(uint16_t uint16_t__counts);
/**
 * @brief  [EN] Convert the battery-PACK 24 V channel with the USER divider
 *              factor (attenuation 6.8k/69.2k, 2026-09-25) - the input net
 *              keeps func__Measurement_V24CountsToMv.
* @param uint16_t__counts Normalized ADC count / ADC
* @return uint32_t Pack voltage in mV / mV
 */
uint32_t func__Measurement_Battery24CountsToMv(uint16_t uint16_t__counts);
/* ==================== V12 Counts To Mv ==================== */
/**
 * @brief  [EN] Convert the normalized 12 V battery channel to source mV
 *              through the board calibration port.
* @param uint16_t__counts Raw ADC count, 0..4095 / ADC
* @return uint32_t Source voltage in mV, 0..~20000 / mV
 */
uint32_t func__Measurement_V12CountsToMv(uint16_t uint16_t__counts);
/* ==================== Current Counts To Ma ==================== */
/**
 * @brief  [EN] Convert a normalized current channel to milliamps through the
 *              board calibration port.
* @param uint16_t__counts Raw ADC count, 0..4095 / ADC
* @return uint32_t Current in mA, 0..~3300 / mA
 */
uint32_t func__Measurement_Current1CountsToMa(uint16_t uint16_t__counts);
uint32_t func__Measurement_Current2CountsToMa(uint16_t uint16_t__counts);
/**
 * @brief  [EN] Report whether a channel's public current result is already
 *              battery-side by the panel's current table (or by the gain and
 *              offset line when no table is stored). When true,
 *              downstream charger code must not apply any second conversion.
 * @param  uint8_t__channelIndex [EN] Zero-based channel index (0 or 1) /
 * @return bool [EN] true for channels 0 and 1 (battery-side output) /
 */
bool func__Measurement_CurrentIsBatteryCalibrated(uint8_t uint8_t__channelIndex);
/**
 * @brief  [EN] Raw current counts to the pure-hardware shunt voltage in uV
 *              (no offset, no trim) - live diagnostic for the current-chain
 *              review (user order 2026-09-22).
* @param uint16_t__counts ADC count / ADC
* @return uint32_t Shunt voltage in uV / uV
 */
uint32_t func__Measurement_CurrentCountsToShuntUv(uint16_t uint16_t__counts);
/* ==================== Runtime config API (ESP panel) / API ==================== */
/**
 * @brief  [EN] Set the runtime median window size of the current filter:
 *              any size 1..MAX (even sizes too, 1..2 bypass); capability-
 *              gated by the compiled switch; flash-persisted (NVM id 7).
* @param uint8_t__medianSize Requested size /
* @return uint8_t Applied size /
 */
uint8_t func__Measurement_SetFilterMedianSize(uint8_t uint8_t__medianSize);
/**
 * @brief  [EN] Set the runtime moving-average window, clamped to
 *              1..MEASUREMENT_CURRENT_AVERAGE_WINDOW; the measurement task
 *              resets the filter state after a change.
* @param uint32_t__windowSamples Requested window, clamped before narrowing (u8 storage would slice 256..300 to 0..44) /
* @return uint16_t Applied window /
 */
uint16_t func__Measurement_SetFilterAverageWindow(uint32_t uint32_t__windowSamples);
/**
 * @brief  [EN] Read the live median window size of the current filter.
* @return uint8_t 1, 3 or 5 /
 */
uint8_t func__Measurement_GetFilterMedianSize(void);
/**
 * @brief  [EN] Read the live moving-average window size.
* @return uint16_t Window in samples /
 */
uint16_t func__Measurement_GetFilterAverageWindow(void);
/**
 * @brief  [EN] Set one runtime voltage calibration offset, clamped to
 *              +/-MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV; flash-persisted
 *              (NVM ids 4/5/6). Default 0 = today's behavior.
* @param uint8_t__channelIndex 0 = VIN, 1 = V24, 2 = V12 /
* @param int32_t__offsetMv Requested offset, mV /
* @return int32_t Applied offset, mV /
 */
int32_t func__Measurement_SetVoltageOffsetMv(uint8_t uint8_t__channelIndex,
                                             int32_t int32_t__offsetMv);
/**
 * @brief  [EN] Read one runtime voltage calibration offset.
* @param uint8_t__channelIndex 0 = VIN, 1 = V24, 2 = V12 /
* @return int32_t Live offset, mV /
 */
int32_t func__Measurement_GetVoltageOffsetMv(uint8_t uint8_t__channelIndex);
/* ==================== Measurement_SuperviseAdc ==================== */
/**
 * @brief  [EN] Supervise ADC1+DMA liveness on every measurement pass: silently
 *              retry in-place hardware recovery for 60 s, perform at most one
 *              MCU software reset, retry silently for another 60 s after that
 *              reset, and latch FAULT_ADC only if still unrecovered.
* @param uint32_t__nowMs Monotonic kernel time in milliseconds /
 */
void func__Measurement_SuperviseAdc(uint32_t uint32_t__nowMs);
#endif /* MEASUREMENT_H */
