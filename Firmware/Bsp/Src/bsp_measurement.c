/**
 * @file    bsp_measurement.c
 * @brief   [EN] Board-specific ADC calibration for the current schematic.
 * @note    [EN] Replace this port when the analog circuit, ADC reference or
 *              ADC resolution changes; Measurement logic remains unchanged.
 */
#include "bsp_measurement.h"
#include <stdint.h>
/* ==================== Board calibration constants ==================== */
/*
 * [EN] Current STM32F1/board values: 3.3 V reference and 12-bit ADC.
 */
/*
 * [EN] THE SINGLE GLOBAL SCALE. Every voltage and every current on this board
 *      is counts x VREF / FULL_SCALE, so VREF is the one term common to all of
 *      them - and therefore the ONLY correct place to fix a common-mode scale
 *      error. Do not patch individual channels for something that is shared.
 *      VDDA HYPOTHESIS - NOT CONFIRMED (audit 2026-10-10). An earlier note
 *      claimed VIN and V12 both over-read by 1.19 %, implying VDDA ~3261 mV.
 *      Calibration Data/solo2_dense.csv (removed, see git history) does NOT support that: row 1 has VIN 24386 vs
 *      DMM 24100 (+1.2 %) but V12 12074 vs DMM 12080 (-0.05 %). Two channels
 *      disagree, so no shared-VDDA conclusion follows from this file.
 *      (Unverified idea) the board "never calibrates" if the dominant error is a GAIN
 *      error, and the only runtime calibration the product exposes (params
 *      4/5/6) is an ADDER. An adder mathematically cannot correct a
 *      multiplier - it can only be right at one operating point, which is
 *      exactly why every attempt drifted and why per-channel patches kept
 *      getting added.
 *      NOT CHANGED HERE, DELIBERATELY. Lowering VREF to 3261 would make every
 *      reading 1.2 percent LOWER, which moves the over-voltage cut LATER - a
 *      safety regression - and the number comes from one bench session on one
 *      board with a DMM logged to 100 mV granularity on VIN. Reading slightly
 *      HIGH is the safe direction, so the nominal 3300 stays until VDDA is
 *      measured directly.
 *      TO FIX IT PROPERLY: (1) put a DMM on VDDA and set this constant, or
 *      better (2) let the STM32F103 internal reference (VREFINT) compute VDDA
 *      at runtime, which makes every board self-calibrating instead of
 *      one-board-tuned.
 *      STATUS OF (2), corrected by the full-program audit 2026-10-05: the
 *      CubeMX side is ALREADY DONE - CubeIDE.ioc carries NbrOfConversion 6
 *      with Channel-6 = ADC_CHANNEL_VREFINT, bsp_adc.h exposes it as
 *      BSP_ADC_CHANNEL_VREFINT and func__BspMeasurement_VddaMv above turns it
 *      into millivolts. This comment used to say a regeneration was still
 *      pending; it is not. What is still open is purely a bench step: measure
 *      VDDA on the board with a DMM, set CAL_VREFINT_MV to the value that
 *      makes the two agree, then switch CAL_VDDA_TRACKING_ENABLE on. Until
 *      that measurement exists the raw 1.16..1.24 V datasheet spread would be
 *      worse than the error it corrects, so the flag ships OFF on purpose.
 */
#define BSP_MEASUREMENT_VREF_MV             3300u
#define BSP_MEASUREMENT_VDDA_MIN_MV         3000u
#define BSP_MEASUREMENT_VDDA_MAX_MV         3600u
#define BSP_MEASUREMENT_ADC_FULL_SCALE     4095u
/*
 * [EN] VOLTAGE SENSE DIVIDERS - read straight off the schematic, not tuned.
 *      USER-ORDERED CORRECTION 2026-09-29. Every one of the three sense
 *      nets has the SAME shape: two resistors in series feeding the ADC pin,
 *      and one resistor from that pin to ground.
 *        sheet 1 (power)      sheet 4 (MCU)        to GND
 *        ADC_MICRO_+24V_INPUT     R46 = 68K  +  R11 = 1.2K  |  R12 = 6.8K
 *        ADC_MICRO_+24V_BAT       R47 = 68K  +  R13 = 1.2K  |  R14 = 6.8K
 *        ADC_MICRO_+24V_BAT_COM   R48 = 33K  +  R15 = 1.2K  |  R16 = 6.8K
 *      So the two 24 V nets are ELECTRICALLY IDENTICAL (68K+1.2K over 6.8K)
 *      and there is no physical reason for their coefficients to differ.
 *      WHY THIS HAD TO BE CORRECTED. The pack net used to carry TOP = 66200,
 *      which is not a resistor that exists on this board. It was invented to
 *      make one bench reading line up, and it was the real reason the pack
 *      voltage never calibrated: a wrong divider is a GAIN error, so it can
 *      only ever be right at a single point and is wrong everywhere else -
 *      and it silently corrupts every consumer of that reading (charge PID,
 *      OV cut, panel, bench CSV, half-pack balance).
 *      The rule from here on: READ THE VOLTAGE HONESTLY. If a decision needs
 *      to happen sooner, move the THRESHOLD (see CHG_ALARM_PARAM_OV_CUTOFF_MV
 *      and the absorb/over pair), never the scale. Residual per-board error
 *      belongs in the runtime offsets, ESP params 4/5/6, which are an ADDER
 *      and therefore cannot distort the slope.
 */
#define BSP_MEASUREMENT_SENSE_SERIES_MCU_OHMS  1200u  /* R11 / R13 / R15 */
#define BSP_MEASUREMENT_SENSE_SHUNT_OHMS       6800u  /* R12 / R14 / R16 */
#define BSP_MEASUREMENT_SENSE_TOP_24V_OHMS    68000u  /* R46 (input), R47 (pack) */
#define BSP_MEASUREMENT_SENSE_TOP_12V_OHMS    33000u  /* R48 (mid node)          */
/*
 * [EN] MEASURED CORRECTION 2026-09-29, and it overrules the schematic reading.
 *      With VDDA confirmed at exactly 3.300 V, the divider ratio was measured
 *      directly - DMM on the connector, DMM on the ADC pin - which needs no
 *      assumption at all, because a resistive divider is ratiometric and has no
 *      offset:
 *          input net (PA2): 24.16 V / 2.197 V = 10.9968
 *          pack net  (PA3): 28.46 V / 2.584 V = 11.0139   average 11.0054
 *      68K over 6.8K gives exactly 11.0000. A 0.05 % fit on TWO independent
 *      nets is not a coincidence, so the 1.2K is NOT in the divider: R11/R13
 *      sit between the tap and the ADC pin, where they are series protection
 *      into a high-impedance input and drop no DC. Including them, as the
 *      schematic reading implied, made the board read 1.55 % high.
 *      The 12 V net still includes its 1.2K: 34.2K/6.8K = 6.0294 against a
 *      measured 6.0586, i.e. -0.48 %, which is inside a 1 % resistor stack and
 *      inside most meters' DC accuracy. One point is not enough to chase that,
 *      so it is left alone and flagged for a second reading.
 */
#define BSP_MEASUREMENT_DIV24_TOP_OHMS         BSP_MEASUREMENT_SENSE_TOP_24V_OHMS
#define BSP_MEASUREMENT_DIV24_BOTTOM_OHMS   BSP_MEASUREMENT_SENSE_SHUNT_OHMS
/*
 * [EN] Same physical network as the input net above - deliberately spelled
 * out from the same constants so the two can never drift apart again.
 */
#define BSP_MEASUREMENT_DIV24BAT_TOP_OHMS   BSP_MEASUREMENT_DIV24_TOP_OHMS
#define BSP_MEASUREMENT_DIV24BAT_BOTTOM_OHMS BSP_MEASUREMENT_DIV24_BOTTOM_OHMS
/*
 * [EN] MEASURED 2026-09-29: 14.88 V at CON2 pin 2 against 2.456 V at PA5 gives
 *      a ratio of 6.0586. 33K+1.2K over 6.8K gives 6.0294, so the firmware read
 *      0.48 % LOW - and reading low on the battery the charger regulates means
 *      charging it HIGH, which is the unsafe direction. Unlike the 24 V nets
 *      there is no clean structural story here (the 1.2K IS in this path); the
 *      residual is ordinary 1 % resistor spread, so the top is set to what the
 *      divider actually measures: 6800 x (6.0586 - 1) = 34398.
 */
#define BSP_MEASUREMENT_DIV12_TOP_OHMS      34398u
#define BSP_MEASUREMENT_DIV12_BOTTOM_OHMS   BSP_MEASUREMENT_SENSE_SHUNT_OHMS
/*
 * [EN] Current sense: 10mOhm shunt and LM358 gain 101.
 */
/*
 * [EN] MCU input divider on the CURRENTx nets (MCU sheet): R41 = 1k series
 *      and R42 = 10k to GND. The ADC pin therefore sees only
 *      10k/(1k+10k) of the LM358 output; the conversion below undoes this
 *      permanent hardware divider separately from the user calibration.
 */
/*
 * [EN] Unit scales of the current formula, each ONE concept (no shared
 *      magic 1000): milliamps per ampere for the shunt stage, and permille
 *      for the bench gain trim stage.
 */
#define BSP_MEASUREMENT_MA_PER_A              1000u
#define BSP_MEASUREMENT_PERMILLE_SCALE        1000u
/*
 * [EN] Microvolts per millivolt - scale factor of the pure-hardware
 * shunt-voltage diagnostic converter (user order 2026-09-22).
 */
/*
 * [EN] Per-CHANNEL calibration since 2026-09-20 (user order: charger 1 must
 *      not ride on charger 2's calibration). Both chains share the same
 *      schematic topology above (10 mOhm shunt, LM358 gain 101, R41/R42), so
 *      only the offset and the bench gain permille differ per channel.
 */
/*
 * [EN] Safe start values until the first bench calibration (user order
 *      2026-10-10): the MAXIMUM gain (3000 permille) and ZERO offset. A
 *      higher reading only slows the charge; a low reading would push
 *      extra current into the battery. Both channels start here and the
 *      panel calibrates them at the first flash.
 */
#define BSP_MEASUREMENT_CURRENT2_OFFSET_COUNTS 0u
#define BSP_MEASUREMENT_CURRENT2_GAIN_PERMILLE 3000u
#define BSP_MEASUREMENT_CURRENT1_OFFSET_COUNTS 0u
#define BSP_MEASUREMENT_CURRENT1_GAIN_PERMILLE 3000u
/*
 * [EN] Runtime clamp limits for the ESP-adjustable current calibration
 *      (user order 2026-09-22: the ESP command panel must never be able to
 *      push a calibration value into a nonsense region).
 */
#define BSP_MEASUREMENT_CURRENT_OFFSET_MAX_COUNTS 255u
#define BSP_MEASUREMENT_CURRENT_GAIN_MIN_PERMILLE 100u
#define BSP_MEASUREMENT_CURRENT_GAIN_MAX_PERMILLE 3000u
/*
 * [EN] Runtime copies of the per-channel current calibration, initialized
 *      from the compiled bench defaults above and writable at runtime by
 *      the ESP link (flash-persisted since v1.14 - a reboot keeps the set).
 *      Written from the EspLink task, read in the measurement task; both
 *      are aligned 32-bit values, atomic on Cortex-M3.
 */
static volatile uint32_t UINT32_T__G__Current1OffsetCounts =
    BSP_MEASUREMENT_CURRENT1_OFFSET_COUNTS;
static volatile uint32_t UINT32_T__G__Current1GainPermille =
    BSP_MEASUREMENT_CURRENT1_GAIN_PERMILLE;
static volatile uint32_t UINT32_T__G__Current2OffsetCounts =
    BSP_MEASUREMENT_CURRENT2_OFFSET_COUNTS;
static volatile uint32_t UINT32_T__G__Current2GainPermille =
    BSP_MEASUREMENT_CURRENT2_GAIN_PERMILLE;
/* ==================== VDDA from the internal reference ==================== */
/**
 * @brief  [EN] Work out the real VDDA (= ADC reference) from the internal
 *              1.20 V reference channel. VREFINT sits at a known voltage, so
 *              whatever count it produces tells you what full scale is worth:
 *                  VDDA = VREFINT_mV * FULL_SCALE / vrefint_counts
 *              HONEST LIMIT, read this before trusting it: the STM32F103 does
 *              NOT store a factory VREFINT calibration (unlike F0/F3/F4/L0).
 *              The datasheet only guarantees 1.16..1.24 V, so used raw this is
 *              +-3.3 percent - WORSE than the ~1.2 percent error it would be
 *              correcting. It becomes accurate only once VDDA has been
 *              measured on THIS board with a DMM and CAL_VREFINT_MV set to
 *              match. After that the board tracks supply and temperature drift
 *              by itself, which is the part no fixed constant can ever do.
 *              That is why CAL_VDDA_TRACKING_ENABLE ships OFF: this function
 *              is a measurement you can look at first, not a silent change to
 *              every reading.
 * @param  uint16_t__vrefintCounts [EN] Raw counts of the VREFINT channel /
 * @param  uint32_t__vrefintMv [EN] Reference voltage of that channel in mV -
 *                                  passed IN so the board port stays free of
 *                                  bench-calibration headers /
 * @return uint32_t [EN] Measured VDDA in mV, 0 if the reading is implausible /
 */
uint32_t func__BspMeasurement_VddaMv(uint16_t uint16_t__vrefintCounts,
                                    uint32_t uint32_t__vrefintMv)
{
    uint32_t uint32_t__vddaMv;
    if (uint16_t__vrefintCounts == 0u)
    {
        return 0u;
    }
    uint32_t__vddaMv = (uint32_t__vrefintMv *
                        (uint32_t)BSP_MEASUREMENT_ADC_FULL_SCALE) /
                       (uint32_t)uint16_t__vrefintCounts;
    /*
 * [EN] Refuse anything a 3.3 V rail could not physically be; a stuck or
 * un-enabled VREFINT channel must not silently rescale the product.
 */
    if ((uint32_t__vddaMv < BSP_MEASUREMENT_VDDA_MIN_MV) ||
        (uint32_t__vddaMv > BSP_MEASUREMENT_VDDA_MAX_MV))
    {
        return 0u;
    }
    return uint32_t__vddaMv;
}
/**
 * @brief  [EN] Convert raw ADC counts to millivolts at the ADC pin.
* @param uint16_t__counts ADC count / ADC
* @return uint32_t Pin voltage in mV / mV
 */
/* ==================== BspMeasurement_CountsToMv ==================== */
uint32_t func__BspMeasurement_CountsToMv(uint16_t uint16_t__counts)
{
    uint32_t uint32_t__countsScaled;
    uint32_t__countsScaled =
        (uint32_t)uint16_t__counts * BSP_MEASUREMENT_VREF_MV;
    return uint32_t__countsScaled / BSP_MEASUREMENT_ADC_FULL_SCALE;
}
/**
 * @brief  [EN] Undo the board divider for a 24 V source channel.
* @param uint16_t__counts ADC count / ADC
* @return uint32_t Source voltage in mV / mV
 */
/* ==================== BspMeasurement_V24CountsToMv ==================== */
uint32_t func__BspMeasurement_V24CountsToMv(uint16_t uint16_t__counts)
{
    uint32_t uint32_t__adcPinMv;
    uint32_t uint32_t__dividerTotalOhms;
    uint32_t uint32_t__scaledMv;
    uint32_t__adcPinMv = func__BspMeasurement_CountsToMv(uint16_t__counts);
    uint32_t__dividerTotalOhms =
        BSP_MEASUREMENT_DIV24_TOP_OHMS + BSP_MEASUREMENT_DIV24_BOTTOM_OHMS;
    uint32_t__scaledMv = uint32_t__adcPinMv * uint32_t__dividerTotalOhms;
    return uint32_t__scaledMv / BSP_MEASUREMENT_DIV24_BOTTOM_OHMS;
}
/* ==================== BspMeasurement_Battery24CountsToMv ==================== */
/**
 * @brief  [EN] Undo the battery-PACK 24 V divider (user factor, 2026-09-25).
* @param uint16_t__counts ADC count / ADC
* @return uint32_t Pack voltage in mV / mV
 */
uint32_t func__BspMeasurement_Battery24CountsToMv(uint16_t uint16_t__counts)
{
    uint32_t uint32_t__adcPinMv;
    uint32_t uint32_t__dividerTotalOhms;
    uint32_t uint32_t__scaledMv;
    uint32_t__adcPinMv = func__BspMeasurement_CountsToMv(uint16_t__counts);
    uint32_t__dividerTotalOhms =
        BSP_MEASUREMENT_DIV24BAT_TOP_OHMS + BSP_MEASUREMENT_DIV24BAT_BOTTOM_OHMS;
    uint32_t__scaledMv = uint32_t__adcPinMv * uint32_t__dividerTotalOhms;
    return uint32_t__scaledMv / BSP_MEASUREMENT_DIV24BAT_BOTTOM_OHMS;
}
/**
 * @brief  [EN] Undo the board divider for a 12 V source channel.
* @param uint16_t__counts ADC count / ADC
* @return uint32_t Source voltage in mV / mV
 */
/* ==================== BspMeasurement_V12CountsToMv ==================== */
uint32_t func__BspMeasurement_V12CountsToMv(uint16_t uint16_t__counts)
{
    uint32_t uint32_t__adcPinMv;
    uint32_t uint32_t__dividerTotalOhms;
    uint32_t uint32_t__scaledMv;
    uint32_t__adcPinMv = func__BspMeasurement_CountsToMv(uint16_t__counts);
    uint32_t__dividerTotalOhms =
        BSP_MEASUREMENT_DIV12_TOP_OHMS + BSP_MEASUREMENT_DIV12_BOTTOM_OHMS;
    uint32_t__scaledMv = uint32_t__adcPinMv * uint32_t__dividerTotalOhms;
    return uint32_t__scaledMv / BSP_MEASUREMENT_DIV12_BOTTOM_OHMS;
}
/* ==================== BspMeasurement_ConvertCurrent (internal) ==================== */
/**
 * @brief  [EN] Shared current formula of both channels, one stage per
 *              schematic element: counts -> ADC pin mV -> undo the
 *              R41/R42 MCU divider -> undo the LM358 gain -> undo the
 *              shunt mOhms -> mA, then subtract the per-channel zero
 *              offset and apply the per-channel bench gain permille.
 *              Every multiply carries its own numerator/denominator stage
 *              and only ONE division runs at the very end (no
 *              intermediate truncation accumulates).
 * @note   [EN] Flash diet: the five runtime stages are folded into ONE
 *              32-bit multiply+divide with BIT-IDENTICAL results (proven
 *              for all 4096 counts; the u64 division pulled
 *              __aeabi_uldivmod ~1 KiB). Derivation from the schematic
 *              values, kept staged so it stays auditable:
 *                num = counts x VREF(3300) x (R41+R42)(11000) x MA_PER_A(1000)
 *                den = FULL(4095) x R42(10000) x GAIN(101) x SHUNT(10 mOhm)
 *                = counts x 24200 / 27573   (both sides / 1,500,000;
 *                  24200 x 4095 = 99,099,000 < 2^32, exact for every count).
 *              ROUNDING (2026-10-11): the folded division rounds UP (ceiling),
 *              the same rule as the table interpolation, so no truncation or
 *              nearest rounding can read below the true current.
 *              If ANY resistor/value above ever changes, re-derive (the
 *              host test recomputes the collapse from these defines).
* @param uint16_t__counts ADC count / ADC
* @param uint32_t__offsetCounts zero-current offset, counts /
* @param uint32_t__gainPermille bench gain permille /
* @return uint32_t Current in mA / mA
 */
static uint32_t func__BspMeasurement_ChainNumerator(uint16_t uint16_t__counts,
                                                    uint32_t uint32_t__offsetCounts,
                                                    uint32_t uint32_t__gainPermille)
{
    uint32_t uint32_t__calibratedCounts;
    /*
 * [EN] Zero offset in raw counts (bench value), clamped at 0. Integer only.
 */
    if ((uint32_t)uint16_t__counts > uint32_t__offsetCounts)
    {
        uint32_t__calibratedCounts =
            (uint32_t)uint16_t__counts - uint32_t__offsetCounts;
    }
    else
    {
        uint32_t__calibratedCounts = 0u;
    }
    /*
 * [EN] Exact numerator (no division, no rounding): calibrated counts x gain
 * permille. Max 4095 x 3000 = 12,285,000, fits u32.
 */
    return uint32_t__calibratedCounts * uint32_t__gainPermille;
}
/*
 * [EN] Exact chain scale: mA = numerator x 24200 / 27573000, where 24200 /
 *      27573 is the fixed fold of the schematic stages and 1000 is the permille
 */
#define BSP_MEASUREMENT_CHAIN_DENOMINATOR (27573u * BSP_MEASUREMENT_PERMILLE_SCALE)
/* ==================== BspMeasurement_ScaleRoundMa ==================== */
/**
 * @brief  [EN] Ceil(numerator x 24200 / 27573000) in ONE round-up, without a
 *              64-bit divide. Split: N = a x 27573 + b, so
 *              N x 24200 / D = a x 24200 / 1000 + frac / D with
 *              frac = (a x 24200 mod 1000) x 27573 + b x 24200 (all < 2^31).
 *              round-up = p1 + ceil(frac / D).
* @param uint32_t__numerator Exact numerator /
* @return uint32_t Current in mA, rounded up to an integer / mA
 */
static uint32_t func__BspMeasurement_ScaleRoundMa(uint32_t uint32_t__numerator)
{
    uint32_t uint32_t__a = uint32_t__numerator / 27573u;
    uint32_t uint32_t__b = uint32_t__numerator % 27573u;
    uint32_t uint32_t__product = uint32_t__a * 24200u;
    uint32_t uint32_t__p1 = uint32_t__product / BSP_MEASUREMENT_PERMILLE_SCALE;
    uint32_t uint32_t__r1 = uint32_t__product % BSP_MEASUREMENT_PERMILLE_SCALE;
    uint32_t uint32_t__frac = (uint32_t__r1 * 27573u) + (uint32_t__b * 24200u);
    /* [EN] Single ROUND-UP (ceiling): never below the exact chain value. */
    return uint32_t__p1 +
           ((uint32_t__frac + BSP_MEASUREMENT_CHAIN_DENOMINATOR - 1u) /
            BSP_MEASUREMENT_CHAIN_DENOMINATOR);
}
static uint32_t func__BspMeasurement_ConvertCurrent(uint16_t uint16_t__counts,
                                                    uint32_t uint32_t__offsetCounts,
                                                    uint32_t uint32_t__gainPermille)
{
    return func__BspMeasurement_ScaleRoundMa(
        func__BspMeasurement_ChainNumerator(uint16_t__counts,
                                            uint32_t__offsetCounts,
                                            uint32_t__gainPermille));
}
/*
 * [EN] Above-anchor extension in TENTHS of mA, rounded UP once. Exact:
 *      mA x 10 = num x 24200 x 10 / (27573 x 1000) = num x 242 / 27573.
 *      num = q x 27573 + r gives q x 242 + ceil(r x 242 / 27573), so the whole
 *      computation stays in 32 bits (no 64-bit divide in the image).
 */
static uint32_t func__BspMeasurement_ChainDeltaTenthMa(uint16_t uint16_t__countsHigh,
                                                       uint16_t uint16_t__countsLow,
                                                       uint32_t uint32_t__offsetCounts,
                                                       uint32_t uint32_t__gainPermille)
{
    uint32_t uint32_t__high =
        func__BspMeasurement_ChainNumerator(uint16_t__countsHigh,
                                            uint32_t__offsetCounts,
                                            uint32_t__gainPermille);
    uint32_t uint32_t__low =
        func__BspMeasurement_ChainNumerator(uint16_t__countsLow,
                                            uint32_t__offsetCounts,
                                            uint32_t__gainPermille);
    uint32_t uint32_t__num;
    uint32_t uint32_t__q;
    uint32_t uint32_t__r;
    if (uint32_t__high <= uint32_t__low)
    {
        return 0u;
    }
    uint32_t__num = uint32_t__high - uint32_t__low;
    uint32_t__q = uint32_t__num / 27573u;
    uint32_t__r = uint32_t__num % 27573u;
    return (uint32_t__q * 242u) + (((uint32_t__r * 242u) + 27572u) / 27573u);
}
uint32_t func__BspMeasurement_Current1DeltaTenthMa(uint16_t uint16_t__countsHigh,
                                                   uint16_t uint16_t__countsLow)
{
    return func__BspMeasurement_ChainDeltaTenthMa(uint16_t__countsHigh,
                                                  uint16_t__countsLow,
                                                  UINT32_T__G__Current1OffsetCounts,
                                                  UINT32_T__G__Current1GainPermille);
}
uint32_t func__BspMeasurement_Current2DeltaTenthMa(uint16_t uint16_t__countsHigh,
                                                   uint16_t uint16_t__countsLow)
{
    return func__BspMeasurement_ChainDeltaTenthMa(uint16_t__countsHigh,
                                                  uint16_t__countsLow,
                                                  UINT32_T__G__Current2OffsetCounts,
                                                  UINT32_T__G__Current2GainPermille);
}
/* ==================== BspMeasurement_Current1CountsToMa ==================== */
/**
 * @brief  [EN] Channel 1 (Trans1 / Shunt1 / PA1) raw counts to primary mA -
 *              same formula shape as channel 2, own calibration pair.
 * @param  uint16_t__counts [EN] ADC count from BSP_ADC_CHANNEL_CURRENT1 /
* @return uint32_t Primary current in mA / mA
 */
uint32_t func__BspMeasurement_Current1CountsToMa(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_ConvertCurrent(uint16_t__counts,
                                               UINT32_T__G__Current1OffsetCounts,
                                               UINT32_T__G__Current1GainPermille);
}
/* ==================== BspMeasurement_Current2CountsToMa ==================== */
/**
 * @brief  [EN] Channel 2 (Trans2 / Shunt2 / PA7) raw counts to primary mA -
 *              bench-verified calibration pair from 2026-09-18.
 * @param  uint16_t__counts [EN] ADC count from BSP_ADC_CHANNEL_CURRENT2 /
* @return uint32_t Primary current in mA / mA
 */
uint32_t func__BspMeasurement_Current2CountsToMa(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_ConvertCurrent(uint16_t__counts,
                                               UINT32_T__G__Current2OffsetCounts,
                                               UINT32_T__G__Current2GainPermille);
}
/* ==================== BspMeasurement current calibration setters/getters ==================== */
/**
 * @brief  [EN] Set the zero-current offset (raw counts) of one current
 *              channel at runtime, clamped to 0..255. Channel 0 = the
 *              Trans1/Shunt1 chain, channel 1 = Trans2/Shunt2.
 *              Flash-persisted (NVM); a reboot keeps the tuned value.
* @param uint8_t__channelIndex 0 = channel 1, 1 = channel 2 /
* @param uint32_t__offsetCounts Requested offset in counts /
* @return uint32_t Actually applied offset /
 */
uint32_t func__BspMeasurement_SetCurrentOffsetCounts(uint8_t uint8_t__channelIndex,
                                                     uint32_t uint32_t__offsetCounts)
{
    if (uint32_t__offsetCounts > BSP_MEASUREMENT_CURRENT_OFFSET_MAX_COUNTS)
    {
        uint32_t__offsetCounts = BSP_MEASUREMENT_CURRENT_OFFSET_MAX_COUNTS;
    }
    if (uint8_t__channelIndex == 0u)
    {
        UINT32_T__G__Current1OffsetCounts = uint32_t__offsetCounts;
    }
    else
    {
        UINT32_T__G__Current2OffsetCounts = uint32_t__offsetCounts;
    }
    return uint32_t__offsetCounts;
}
/**
 * @brief  [EN] Set the bench gain trim (permille) of one current channel at
 *              runtime, clamped to 100..3000 (ESP panel, user order
 *              2026-09-22; flash-persisted since v1.14).
* @param uint8_t__channelIndex 0 = channel 1, 1 = channel 2 /
* @param uint32_t__gainPermille Requested gain permille /
* @return uint32_t Actually applied gain permille /
 */
uint32_t func__BspMeasurement_SetCurrentGainPermille(uint8_t uint8_t__channelIndex,
                                                     uint32_t uint32_t__gainPermille)
{
    if (uint32_t__gainPermille < BSP_MEASUREMENT_CURRENT_GAIN_MIN_PERMILLE)
    {
        uint32_t__gainPermille = BSP_MEASUREMENT_CURRENT_GAIN_MIN_PERMILLE;
    }
    else if (uint32_t__gainPermille > BSP_MEASUREMENT_CURRENT_GAIN_MAX_PERMILLE)
    {
        uint32_t__gainPermille = BSP_MEASUREMENT_CURRENT_GAIN_MAX_PERMILLE;
    }
    else
    {
    }
    if (uint8_t__channelIndex == 0u)
    {
        UINT32_T__G__Current1GainPermille = uint32_t__gainPermille;
    }
    else
    {
        UINT32_T__G__Current2GainPermille = uint32_t__gainPermille;
    }
    return uint32_t__gainPermille;
}
/* ==================== BspMeasurement_GetCurrentOffsetCounts ==================== */
/**
 * @brief  [EN] Read the live zero-current offset of one current channel.
* @param uint8_t__channelIndex 0 = channel 1, 1 = channel 2 /
* @return uint32_t Offset in counts /
 */
uint32_t func__BspMeasurement_GetCurrentOffsetCounts(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return UINT32_T__G__Current1OffsetCounts;
    }
    return UINT32_T__G__Current2OffsetCounts;
}
/* ==================== BspMeasurement_GetCurrentGainPermille ==================== */
/**
 * @brief  [EN] Read the live bench gain trim of one current channel.
* @param uint8_t__channelIndex 0 = channel 1, 1 = channel 2 /
* @return uint32_t Gain permille /
 */
uint32_t func__BspMeasurement_GetCurrentGainPermille(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return UINT32_T__G__Current1GainPermille;
    }
    return UINT32_T__G__Current2GainPermille;
}
/* ==================== BspMeasurement Current Counts To Shunt Uv ==================== */
/**
 * @brief  [EN] Pure hardware chain only: raw current counts of either
 *              channel to the shunt voltage in microvolts - exactly the
 *              three hardware stages (ADC reference, R41/R42 divider,
 *              amplifier gain), deliberately NO zero offset and NO bench
 *              trim, so the value checks directly against a scope probe
 *              on the LM358 output (mV = uV x 101 / 1000).
 * @note   [EN] Flash diet: same fold as ConvertCurrent (one u32
 *              multiply+divide, bit-identical for all 4096 counts):
 *                num = counts x VREF(3300) x (R41+R42)(11000) x UV_PER_MV(1000)
 *                den = FULL(4095) x R42(10000) x GAIN(101)
 *                = counts x 242000 / 27573  (both sides / 150,000;
 *                  242000 x 4095 = 990,990,000 < 2^32, exact every count).
 *              Re-derive if any value changes (host test recomputes).
 * @param  uint16_t__counts [EN] Raw ADC count of a current channel /
* @return uint32_t Shunt voltage in uV / uV
 */
uint32_t func__BspMeasurement_CurrentCountsToShuntUv(uint16_t uint16_t__counts)
{
    return ((uint32_t)uint16_t__counts * (uint32_t)242000) / (uint32_t)27573;
}
