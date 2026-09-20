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
/* [EN] MCU input divider on the CURRENTx nets (MCU sheet): R41 = 1k series
 *      and R42 = 10k to GND. The ADC pin therefore sees only
 *      10k/(1k+10k) of the LM358 output; the conversion below undoes this
 *      permanent hardware divider separately from the user calibration.
 *      [FA] تقسیم ورودی MCU روی نت‌های CURRENTx (شیت MCU): R41 برابر 1k سری
 *      و R42 برابر 10k به زمین. پایه ADC فقط 10k/(1k+10k) خروجی LM358 را
 *      می‌بیند؛ تبدیل پایین این تقسیم دائمی سخت‌افزاری را جدا از کالیبراسیون
 *      کاربر خنثی می‌کند. */
#define BSP_MEASUREMENT_CURRENT_DIV_TOP_OHMS     1000u
#define BSP_MEASUREMENT_CURRENT_DIV_BOTTOM_OHMS 10000u
#define BSP_MEASUREMENT_CURRENT_MA_SCALE         1000u
/* [EN] Per-CHANNEL calibration since 2026-09-20 (user order: charger 1 must
 *      not ride on charger 2's calibration). Both chains share the same
 *      schematic topology above (10 mOhm shunt, LM358 gain 101, R41/R42), so
 *      only the offset and the bench gain permille differ per channel.
 * [FA] کالیبراسیون پر-کانال (دستور کاربر): توپولوژی هر دو زنجیره یکی است و
 *      فقط آفست و ضریب گینِ بنچ هر کانال جدا تنظیم می‌شود. */
/* [EN] Channel 2 (Trans2 / Shunt2 -> PA7): the bench-verified pair
 *      (2026-09-18, fixed 15% duty: firmware read 330 mA while scope MEAN at
 *      the LM358 output gave 362/1.01 = 358 mA true primary; 358/330 = 1085
 *      permille). After this the mA readout equals the physical primary
 *      current; the efficiency target lives in CHG_FLYBACK_EFFICIENCY_PERMILLE.
 * [FA] کانال ۲ (Trans2/Shunt2): زوج تأییدشدهٔ بنچ؛ نرم‌افزار ۳۳۰ می‌خواند،
 *      اسکوپ ۳۵۸ واقعی؛ ضریب ۱۰۸۵ پرمیل تا خوانش = جریان فیزیکی اولیه. */
#define BSP_MEASUREMENT_CURRENT2_OFFSET_COUNTS 8u
#define BSP_MEASUREMENT_CURRENT2_GAIN_PERMILLE 1085u
/* [EN] Channel 1 (Trans1 / Shunt1 -> PA1): provisional copies of the
 *      channel-2 bench values - this chain was never calibrated on the
 *      board (bring-up started 2026-09-20). Replace after recording the
 *      zero-current count and one known-current point on Trans1; the values
 *      are intentional duplicates, NOT a shared concept.
 * [FA] کانال ۱ (Trans1/Shunt1): کپی موقت از مقادیر کانال ۲ - این زنجیره هنوز
 *      روی برد کالیبره نشده؛ بعد از ثبت صفر و نقطهٔ جریان معلوم روی Trans1
 *      به‌روزرسانی می‌شود (کپی عمدی است، مفهوم مشترک نیست). */
#define BSP_MEASUREMENT_CURRENT1_OFFSET_COUNTS 8u
#define BSP_MEASUREMENT_CURRENT1_GAIN_PERMILLE 1085u

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
/* ==================== BspMeasurement_ConvertCurrent (internal) ==================== */

/**
 * @brief  [EN] Shared current-formula shape for both channels: undo the
 *              permanent R41/R42 MCU-input divider, subtract the per-channel
 *              zero offset and apply the per-channel bench gain permille.
 *         [FA] قالب فرمول مشترک هر دو کانال؛ فقط آفست و گین ورودی پر-کانال.
 * @param  uint16_t__counts           [EN] ADC count / شمارش ADC
 * @param  uint32_t__offsetCounts     [EN] zero-current offset, counts / آفست صفر
 * @param  uint32_t__gainPermille     [EN] bench gain permille / ضریب گین بنچ
 * @return uint32_t [EN] Current in mA / جریان بر حسب mA
 */
static uint32_t func__BspMeasurement_ConvertCurrent(uint16_t uint16_t__counts,
                                                    uint32_t uint32_t__offsetCounts,
                                                    uint32_t uint32_t__gainPermille)
{
    uint64_t uint64_t__adcVoltageNumerator;
    uint64_t uint64_t__currentNumerator;
    uint64_t uint64_t__currentDenominator;
    uint32_t uint32_t__calibratedCounts;
    uint32_t uint32_t__currentMa;

    if ((uint32_t)uint16_t__counts > uint32_t__offsetCounts)
    {
        uint32_t__calibratedCounts =
            (uint32_t)uint16_t__counts - uint32_t__offsetCounts;
    }
    else
    {
        uint32_t__calibratedCounts = 0u;
    }

    uint64_t__adcVoltageNumerator =
        (uint64_t)uint32_t__calibratedCounts * BSP_MEASUREMENT_VREF_MV;
    uint64_t__currentNumerator =
        uint64_t__adcVoltageNumerator * BSP_MEASUREMENT_CURRENT_MA_SCALE *
        (uint64_t)(BSP_MEASUREMENT_CURRENT_DIV_TOP_OHMS +
                   BSP_MEASUREMENT_CURRENT_DIV_BOTTOM_OHMS);
    uint64_t__currentDenominator =
        (uint64_t)BSP_MEASUREMENT_ADC_FULL_SCALE *
        BSP_MEASUREMENT_AMP_GAIN *
        BSP_MEASUREMENT_SHUNT_MOHMS *
        BSP_MEASUREMENT_CURRENT_DIV_BOTTOM_OHMS;

    uint32_t__currentMa = (uint32_t)(uint64_t__currentNumerator /
                                     uint64_t__currentDenominator);
    uint32_t__currentMa =
        (uint32_t)(((uint64_t)uint32_t__currentMa *
                    uint32_t__gainPermille) /
                   BSP_MEASUREMENT_CURRENT_MA_SCALE);

    return uint32_t__currentMa;
}

/* ==================== BspMeasurement_Current1CountsToMa ==================== */

/**
 * @brief  [EN] Channel 1 (Trans1 / Shunt1 / PA1) raw counts to primary mA -
 *              same formula shape as channel 2, own calibration pair.
 *         [FA] تبدیل شمارش کاal ۱ به mA اولیه با کالیبراسیون مستقل.
 * @param  uint16_t__counts [EN] ADC count from BSP_ADC_CHANNEL_CURRENT1 /
 *                              شمارش ADC کانال جریان ۱
 * @return uint32_t [EN] Primary current in mA / جریان اولیه mA
 */
uint32_t func__BspMeasurement_Current1CountsToMa(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_ConvertCurrent(uint16_t__counts,
                                               BSP_MEASUREMENT_CURRENT1_OFFSET_COUNTS,
                                               BSP_MEASUREMENT_CURRENT1_GAIN_PERMILLE);
}

/* ==================== BspMeasurement_Current2CountsToMa ==================== */

/**
 * @brief  [EN] Channel 2 (Trans2 / Shunt2 / PA7) raw counts to primary mA -
 *              bench-verified calibration pair from 2026-09-18.
 *         [FA] تبدیل شمارش کانال ۲ به mA اولیه با کالیبراسیون تأییدشدهٔ بنچ.
 * @param  uint16_t__counts [EN] ADC count from BSP_ADC_CHANNEL_CURRENT2 /
 *                              شمارش ADC کانال جریان ۲
 * @return uint32_t [EN] Primary current in mA / جریان اولیه mA
 */
uint32_t func__BspMeasurement_Current2CountsToMa(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_ConvertCurrent(uint16_t__counts,
                                               BSP_MEASUREMENT_CURRENT2_OFFSET_COUNTS,
                                               BSP_MEASUREMENT_CURRENT2_GAIN_PERMILLE);
}

/* ==================== BspMeasurement_CurrentCountsToMa (legacy) ==================== */

/**
 * @brief  [EN] Legacy generic converter, kept for the old public wrapper:
 *              identical to func__BspMeasurement_Current2CountsToMa (the
 *              bench-calibrated chain). New code must pick the per-channel
 *              function instead.
 *         [FA] نسخهٔ قدیمی عمومی = کانال ۲؛ کد جدید از تابع پر-کانال استفاده کند.
 * @param  uint16_t__counts [EN] ADC count / شمارش ADC
 * @return uint32_t [EN] Primary current in mA / جریان اولیه mA
 */
uint32_t func__BspMeasurement_CurrentCountsToMa(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_Current2CountsToMa(uint16_t__counts);
}
