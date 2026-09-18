/**
 * @file    bsp_measurement.c
 * @brief   [EN] Board-specific ADC calibration for the current schematic.
 *          [FA] کالیبراسیون ADC مخصوص برد و شماتیک فعلی.
 *
 * @note    [EN] Replace this port when the analog circuit, ADC reference or
 *              ADC resolution changes; Measurement logic remains unchanged.
 *          [FA] اگر مدار آنالوگ، مرجع ADC یا وضوح ADC تغییر کرد این پورت را
 *              عوض کنید؛ منطق Measurement بدون تغییر می‌ماند.
 */

#include "bsp_measurement.h"

#include <stdint.h>

/* ==================== Board calibration constants / ثابت‌های کالیبراسیون برد ==================== */
/* [EN] Current STM32F1/board values: 3.3 V reference and 12-bit ADC.
 *      [FA] مقدارهای برد فعلی STM32F1: مرجع ۳٫۳V و ADC دوازده‌بیتی. */
#define BSP_MEASUREMENT_VREF_MV             3300u
#define BSP_MEASUREMENT_ADC_FULL_SCALE     4095u

/* [EN] Schematic dividers: 24V = 68K+1.2K / 6.8K, 12V = 33K+1.2K / 6.8K.
 *      [FA] تقسیم‌های شماتیک: ۲۴V برابر 68K+1.2K / 6.8K و ۱۲V برابر
 *      33K+1.2K / 6.8K. */
#define BSP_MEASUREMENT_DIV24_TOP_OHMS   69200u
#define BSP_MEASUREMENT_DIV24_BOTTOM_OHMS 6800u
#define BSP_MEASUREMENT_DIV12_TOP_OHMS   34200u
#define BSP_MEASUREMENT_DIV12_BOTTOM_OHMS 6800u

/* [EN] Current sense: 10mOhm shunt and LM358 gain 101.
 *      [FA] سنجش جریان: شانت ۱۰mΩ و گین LM358 برابر ۱۰۱. */
#define BSP_MEASUREMENT_SHUNT_MOHMS          10u
#define BSP_MEASUREMENT_AMP_GAIN            101u
#define BSP_MEASUREMENT_CURRENT_MA_SCALE  1000u
/* [EN] Provisional calibration until a zero-current and known-current board
 *      measurement is recorded. Do not treat these defaults as final. */
/* [FA] تا زمان ثبت اندازه‌گیری برد در جریان صفر و جریان معلوم، این کالیبراسیون
 *      موقت است و نباید نهایی فرض شود. */
#define BSP_MEASUREMENT_CURRENT_OFFSET_COUNTS 0u
#define BSP_MEASUREMENT_CURRENT_GAIN_PERMILLE 1000u

/**
 * @brief  [EN] Convert raw ADC counts to millivolts at the ADC pin.
 *         [FA] شمارش خام ADC را به میلی‌ولت روی پایهٔ ADC تبدیل می‌کند.
 * @param  uint16_t__counts [EN] ADC count / شمارش ADC
 * @return uint32_t [EN] Pin voltage in mV / ولتاژ پایه بر حسب mV
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
 *         [FA] تقسیم برد را برای کانال منبع ۲۴ ولت برمی‌گرداند.
 * @param  uint16_t__counts [EN] ADC count / شمارش ADC
 * @return uint32_t [EN] Source voltage in mV / ولتاژ منبع بر حسب mV
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

/**
 * @brief  [EN] Undo the board divider for a 12 V source channel.
 *         [FA] تقسیم برد را برای کانال منبع ۱۲ ولت برمی‌گرداند.
 * @param  uint16_t__counts [EN] ADC count / شمارش ADC
 * @return uint32_t [EN] Source voltage in mV / ولتاژ منبع بر حسب mV
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

/**
 * @brief  [EN] Convert board current-sense voltage to milliamps.
 *         [FA] ولتاژ مدار سنجش جریان برد را به میلی‌آمپر تبدیل می‌کند.
 * @param  uint16_t__counts [EN] ADC count / شمارش ADC
 * @return uint32_t [EN] Current in mA / جریان بر حسب mA
 */
/* ==================== BspMeasurement_CurrentCountsToMa ==================== */
uint32_t func__BspMeasurement_CurrentCountsToMa(uint16_t uint16_t__counts)
{
    uint64_t uint64_t__adcVoltageNumerator;
    uint64_t uint64_t__currentNumerator;
    uint64_t uint64_t__currentDenominator;
    uint32_t uint32_t__calibratedCounts;
    uint32_t uint32_t__currentMa;

    if ((uint32_t)uint16_t__counts > BSP_MEASUREMENT_CURRENT_OFFSET_COUNTS)
    {
        uint32_t__calibratedCounts =
            (uint32_t)uint16_t__counts - BSP_MEASUREMENT_CURRENT_OFFSET_COUNTS;
    }
    else
    {
        uint32_t__calibratedCounts = 0u;
    }

    uint64_t__adcVoltageNumerator =
        (uint64_t)uint32_t__calibratedCounts * BSP_MEASUREMENT_VREF_MV;
    uint64_t__currentNumerator =
        uint64_t__adcVoltageNumerator * BSP_MEASUREMENT_CURRENT_MA_SCALE;
    uint64_t__currentDenominator =
        (uint64_t)BSP_MEASUREMENT_ADC_FULL_SCALE *
        BSP_MEASUREMENT_AMP_GAIN *
        BSP_MEASUREMENT_SHUNT_MOHMS;

    uint32_t__currentMa = (uint32_t)(uint64_t__currentNumerator /
                                     uint64_t__currentDenominator);
    uint32_t__currentMa =
        (uint32_t)(((uint64_t)uint32_t__currentMa *
                    BSP_MEASUREMENT_CURRENT_GAIN_PERMILLE) /
                   BSP_MEASUREMENT_CURRENT_MA_SCALE);

    return uint32_t__currentMa;
}
