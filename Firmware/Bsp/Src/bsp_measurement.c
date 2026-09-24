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
#define BSP_MEASUREMENT_CURRENT_DIV_BOTTOM_OHMS  10000u
/* [EN] Unit scales of the current formula, each ONE concept (no shared
 *      magic 1000): milliamps per ampere for the shunt stage, and permille
 *      for the bench gain trim stage.
 *      [FA] مقیاس‌های واحد فرمول جریان، هر کدام یک مفهوم (بدون ۱۰۰۰ جادویی
 *      مشترک): میلی‌آمپر بر آمپر برای مرحلهٔ شانت، و پرمیل برای مرحلهٔ
 *      اصلاح گین بنچ. */
#define BSP_MEASUREMENT_MA_PER_A              1000u
#define BSP_MEASUREMENT_PERMILLE_SCALE        1000u

/* [EN] Microvolts per millivolt - scale factor of the pure-hardware
   shunt-voltage diagnostic converter (user order 2026-09-22).
   [FA] میکروولت بر میلی‌ولت - ضریب مقیاس مبدل تشخیصیِ فقط-سخت‌افزاریِ
   ولتاژ شانت (دستور کاربر ۲۰۲۶-۰۹-۲۲). */
#define BSP_MEASUREMENT_UV_PER_MV             1000u
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
 *      2026-09-24 post-fix bench (DMM in series, both channels running) left
 *      ch2 unstable and self-contradictory (D=10%: 185/200/208 vs 185 true;
 *      D=15%: 320/354 vs 425 true) - no single permille fits both, so 1085
 *      stays until a stable SOLO ch2 point (ch1 parked) is recorded.
 * [FA] کانال ۲ (Trans2/Shunt2): زوج تأییدشدهٔ بنچ؛ نرم‌افزار ۳۳۰ می‌خواند،
 *      اسکوپ ۳۵۸ واقعی؛ ضریب ۱۰۸۵ پرمیل تا خوانش = جریان فیزیکی اولیه.
 *      بنچ ۲۰۲۶-۰۹-۲۴ پس از فیکس: ch2 ناپایدار و متناقض ماند (D=10%:
 *      185/200/208 در برابر 185 واقعی؛ D=15%: 320/354 در برابر 425) — یک
 *      پرمیل هر دو را پوشش نمی‌دهد؛ ۱۰۸۵ می‌ماند تا نقطهٔ تکیِ پایدار ch2
 *      (با پارک ch1) ثبت شود. */
#define BSP_MEASUREMENT_CURRENT2_OFFSET_COUNTS 8u
#define BSP_MEASUREMENT_CURRENT2_GAIN_PERMILLE 1085u
/* [EN] Channel 1 (Trans1 / Shunt1 -> PA1): bench-calibrated 2026-09-24,
 *      after the dual-channel drop cleared (user order: bake it and push).
 *      DMM in series with the 24 V input, true vs displayed: D=15% 423 vs
 *      436/438/442 mA, D=10% 185 vs 185/189 mA. Gain = 1085 * 423/438.7 =
 *      1046 permille, set at D=15% (closest to the ~650 mA AUTO point).
 *      Offset stays 8 counts - off-state display reads 0 mA.
 * [FA] کانال ۱ (Trans1/Shunt1): کالیبرهٔ بنچ ۲۰۲۶-۰۹-۲۴ پس از رفع افت
 *      دوکاناله (دستور کاربر: بپز و پوش کن). مولتی‌متر سری با ورودی ۲۴V؛
 *      واقعی در برابر نمایش: D=15% → 423 در برابر 436/438/442؛ D=10% → 185
 *      در برابر 185/189. گین = 1085×423÷438.7 = ۱۰۴۶ پرمیل، تنظیم در D=15%
 *      (نزدیک‌ترین به نقطهٔ کار ~650mA در AUTO). آفست 8 ماند (خاموش = 0mA). */
#define BSP_MEASUREMENT_CURRENT1_OFFSET_COUNTS 8u
#define BSP_MEASUREMENT_CURRENT1_GAIN_PERMILLE 1046u

/* [EN] Runtime clamp limits for the ESP-adjustable current calibration
 *      (user order 2026-09-22: the ESP command panel must never be able to
 *      push a calibration value into a nonsense region).
 * [FA] حدود گیرهٔ زمان اجرا برای کالیبراسیون قابل‌تنظیم از ESP (دستور
 *      کاربر ۲۰۲۶-۰۹-۲۲): پنل ESP هرگز نباید مقدار کالیبراسیون را به
 *      ناحیهٔ بی‌معنا ببرد. */
#define BSP_MEASUREMENT_CURRENT_OFFSET_MAX_COUNTS 255u
#define BSP_MEASUREMENT_CURRENT_GAIN_MIN_PERMILLE 100u
#define BSP_MEASUREMENT_CURRENT_GAIN_MAX_PERMILLE 3000u

/* [EN] Runtime copies of the per-channel current calibration, initialized
 *      from the compiled bench defaults above and writable at runtime by
 *      the ESP link (RAM only - a reboot returns to the compiled defaults).
 *      Written from the EspLink task, read in the measurement task; both
 *      are aligned 32-bit values, atomic on Cortex-M3.
 * [FA] نسخهٔ زمان اجرای کالیبراسیون جریان هر کانال: مقدار اولیه از
 *      پیش‌فرض‌های بنچ بالا و نوشتن در زمان اجرا توسط لینک ESP (فقط RAM -
 *      ری‌استارت به پیش‌فرض کامپایل برمی‌گردد). نوشتن از تسک EspLink و
 *      خواندن در تسک اندازه‌گیری؛ هر دو ۳۲ بیتی تراز شده‌اند و روی
 *      Cortex-M3 اتمیک‌اند. */
static volatile uint32_t UINT32_T__G__Current1OffsetCounts =
    BSP_MEASUREMENT_CURRENT1_OFFSET_COUNTS;
static volatile uint32_t UINT32_T__G__Current1GainPermille =
    BSP_MEASUREMENT_CURRENT1_GAIN_PERMILLE;
static volatile uint32_t UINT32_T__G__Current2OffsetCounts =
    BSP_MEASUREMENT_CURRENT2_OFFSET_COUNTS;
static volatile uint32_t UINT32_T__G__Current2GainPermille =
    BSP_MEASUREMENT_CURRENT2_GAIN_PERMILLE;

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
 * @brief  [EN] Shared current-formula shape for both channels, one stage per
 *              schematic element (user order 2026-09-22: coefficients from
 *              the actual resistor values): counts -> ADC pin mV ->
 *              undo the R41/R42 MCU divider -> undo the LM358 gain ->
 *              undo the shunt mOhms -> mA, then subtract the per-channel
 *              zero offset and apply the per-channel bench gain permille.
 *              Every multiply carries its own numerator/denominator stage
 *              and only ONE division runs at the very end, so no
 *              intermediate truncation accumulates.
 *         [FA] قالب فرمول مشترک هر دو کانال، یک مرحله برای هر المان شماتیک
 *              (دستور کاربر ۲۰۲۶-۰۹-۲۲: ضرایب از مقدار واقعی مقاومت‌ها):
 *              شمارش -> mV پایه ADC -> خنثی‌کردن تقسیم R41/R42 -> خنثی‌کردن
 *              گین LM358 -> خنثی‌کردن mΩ شانت -> mA، سپس کم‌کردن آفست صفر
 *              پر-کانال و اعمال گین پرمیل بنچ. هر ضرب مرحلهٔ صورت/مخرج خودش
 *              را جابه‌جا می‌کند و فقط یک تقسیم در انتها اجرا می‌شود تا
 *              خطای گردشدن میانی جمع نشود.
 * @param  uint16_t__counts           [EN] ADC count / شمارش ADC
 * @param  uint32_t__offsetCounts     [EN] zero-current offset, counts / آفست صفر
 * @param  uint32_t__gainPermille     [EN] bench gain permille / ضریب گین بنچ
 * @return uint32_t [EN] Current in mA / جریان بر حسب mA
 */
static uint32_t func__BspMeasurement_ConvertCurrent(uint16_t uint16_t__counts,
                                                    uint32_t uint32_t__offsetCounts,
                                                    uint32_t uint32_t__gainPermille)
{
    uint64_t uint64_t__chainNumerator;
    uint64_t uint64_t__chainDenominator;
    uint32_t uint32_t__calibratedCounts;
    uint32_t uint32_t__chainCurrentMa;

    /* [EN] Stage 0 - per-channel zero offset, in raw counts (bench value).
       [FA] مرحلهٔ ۰ - آفست صفر پر-کانال، بر حسب شمارش خام (مقدار بنچ). */
    if ((uint32_t)uint16_t__counts > uint32_t__offsetCounts)
    {
        uint32_t__calibratedCounts =
            (uint32_t)uint16_t__counts - uint32_t__offsetCounts;
    }
    else
    {
        uint32_t__calibratedCounts = 0u;
    }

    uint64_t__chainNumerator = (uint64_t)uint32_t__calibratedCounts;
    uint64_t__chainDenominator = 1u;

    /* [EN] Stage 1 - counts to ADC-pin voltage: 12-bit ADC with the 3300 mV
       reference (the ADC pin is the CURRENTx net behind R42).
       [FA] مرحلهٔ ۱ - شمارش به ولتاژ پایه ADC: ADC دوازده‌بیتی با مرجع
       ۳۳۰۰mV (پایه ADC = نت CURRENTx پشت R42). */
    uint64_t__chainNumerator *= BSP_MEASUREMENT_VREF_MV;
    uint64_t__chainDenominator *= BSP_MEASUREMENT_ADC_FULL_SCALE;

    /* [EN] Stage 2 - ADC-pin voltage to LM358 output: undo the permanent
       R41(1k)/R42(10k) MCU-input divider (multiply by 11/10).
       [FA] مرحلهٔ ۲ - ولتاژ پایه ADC به خروجی LM358: خنثی‌کردن تقسیم
       دائمی R41(1k)/R42(10k) ورودی MCU (ضرب در ۱۱/۱۰). */
    uint64_t__chainNumerator *=
        (uint64_t)(BSP_MEASUREMENT_CURRENT_DIV_TOP_OHMS +
                   BSP_MEASUREMENT_CURRENT_DIV_BOTTOM_OHMS);
    uint64_t__chainDenominator *= BSP_MEASUREMENT_CURRENT_DIV_BOTTOM_OHMS;

    /* [EN] Stage 3 - LM358 output to shunt voltage: undo the non-inverting
       gain of 101 (Rf/Rg of the current-sense amplifier).
       [FA] مرحلهٔ ۳ - خروجی LM358 به ولتاژ شانت: خنثی‌کردن گین
       غیروارونگر ۱۰۱ (Rf/Rg تقویت‌کنندهٔ سنجش جریان). */
    uint64_t__chainDenominator *= BSP_MEASUREMENT_AMP_GAIN;

    /* [EN] Stage 4 - shunt voltage in mV to primary current in mA:
       V[mV] = I[A] x R[mOhm] x 1000, therefore mA = mV x 1000 / mOhm.
       [FA] مرحلهٔ ۴ - ولتاژ شانت بر حسب mV به جریان اولیه بر حسب mA:
       V[mV] = I[A] x R[mΩ] x 1000، پس mA = mV x 1000 / mΩ. */
    uint64_t__chainNumerator *= BSP_MEASUREMENT_MA_PER_A;
    uint64_t__chainDenominator *= BSP_MEASUREMENT_SHUNT_MOHMS;

    uint32_t__chainCurrentMa =
        (uint32_t)(uint64_t__chainNumerator / uint64_t__chainDenominator);

    /* [EN] Stage 5 - per-channel bench gain trim in permille (ch1 1046 =
       the DMM-calibrated 2026-09-24 point; ch2 1085 = the measured 1.085x
       of the Trans2 bench point, kept pending a stable solo re-check).
       [FA] مرحلهٔ ۵ - اصلاح گین بنچ پر-کانال بر حسب پرمیل (کانال ۱: ۱۰۴۶ =
       کالیبرهٔ مولتی‌متری ۲۰۲۶-۰۹-۲۴؛ کانال ۲: ۱۰۸۵ یعنی ۱٫۰۸۵ برابر نقطهٔ
       بنچ Trans2 — تا تست تکیِ مجدد همان می‌ماند). */
    uint32_t__chainCurrentMa =
        (uint32_t)(((uint64_t)uint32_t__chainCurrentMa *
                    (uint64_t)uint32_t__gainPermille) /
                   BSP_MEASUREMENT_PERMILLE_SCALE);

    return uint32_t__chainCurrentMa;
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
                                               UINT32_T__G__Current1OffsetCounts,
                                               UINT32_T__G__Current1GainPermille);
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
                                               UINT32_T__G__Current2OffsetCounts,
                                               UINT32_T__G__Current2GainPermille);
}

/* ==================== BspMeasurement current calibration setters/getters ==================== */

/**
 * @brief  [EN] Set the zero-current offset (raw counts) of one current
 *              channel at runtime, clamped to 0..255. Channel 0 = the
 *              Trans1/Shunt1 chain, channel 1 = Trans2/Shunt2. The value
 *              lives in RAM only; a reboot restores the compiled bench
 *              default (ESP panel, user order 2026-09-22).
 *         [FA] آفست جریان صفر (شمارش خام) یک کانال را در زمان اجرا تنظیم
 *              می‌کند، گیره در ۰..۲۵۵. کانال ۰ = زنجیرهٔ Trans1/Shunt1 و
 *              کانال ۱ = Trans2/Shunt2. مقدار فقط در RAM است؛ ری‌استارت
 *              پیش‌فرض بنچ کامپایل را برمی‌گرداند (پنل ESP، دستور کاربر
 *              ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = channel 1, 1 = channel 2 / ۰ یا ۱
 * @param  uint32_t__offsetCounts [EN] Requested offset in counts / آفست
 * @return uint32_t [EN] Actually applied offset / آفست اعمال‌شده
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
 *              2026-09-22; RAM only).
 *         [FA] ضریب گین بنچ (پرمیل) یک کانال را در زمان اجرا تنظیم می‌کند،
 *              گیره در ۱۰۰..۳۰۰۰ (پنل ESP، دستور کاربر ۲۰۲۶-۰۹-۲۲؛ فقط RAM).
 * @param  uint8_t__channelIndex [EN] 0 = channel 1, 1 = channel 2 / ۰ یا ۱
 * @param  uint32_t__gainPermille [EN] Requested gain permille / گین پرمیل
 * @return uint32_t [EN] Actually applied gain permille / گین اعمال‌شده
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
        /* [EN] Value already inside the window. [FA] مقدار داخل پنجره است. */
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

/**
 * @brief  [EN] Read the live zero-current offset of one current channel.
 *         [FA] آفست جریان صفرِ زندهٔ یک کانال را می‌خواند.
 * @param  uint8_t__channelIndex [EN] 0 = channel 1, 1 = channel 2 / ۰ یا ۱
 * @return uint32_t [EN] Offset in counts / آفست بر حسب شمارش
 */
uint32_t func__BspMeasurement_GetCurrentOffsetCounts(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return UINT32_T__G__Current1OffsetCounts;
    }

    return UINT32_T__G__Current2OffsetCounts;
}

/**
 * @brief  [EN] Read the live bench gain trim of one current channel.
 *         [FA] ضریب گین بنچِ زندهٔ یک کانال را می‌خواند.
 * @param  uint8_t__channelIndex [EN] 0 = channel 1, 1 = channel 2 / ۰ یا ۱
 * @return uint32_t [EN] Gain permille / گین پرمیل
 */
uint32_t func__BspMeasurement_GetCurrentGainPermille(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return UINT32_T__G__Current1GainPermille;
    }

    return UINT32_T__G__Current2GainPermille;
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

/* ==================== BspMeasurement Current Counts To Shunt Uv ==================== */

/**
 * @brief  [EN] Pure hardware chain only: raw current counts of either
 *              channel to the voltage across the sense shunt, in
 *              microvolts. Applies exactly the three hardware stages -
 *              ADC reference, R41(1k)/R42(10k) input divider, amplifier
 *              gain - and deliberately NO zero offset and NO bench trim,
 *              so the value can be checked directly against a scope probe
 *              on the LM358 output (mV = uV x 101 / 1000). Live
 *              diagnostic for the current-chain review, user order
 *              2026-09-22.
 *         [FA] فقط زنجیرهٔ سخت‌افزاری: شمارش خام جریان هر کانال به ولتاژ
 *              دو سر شانت بر حسب میکروولت. دقیقاً سه مرحلهٔ سخت‌افزاری را
 *              اعمال می‌کند - مرجع ADC، مقسم ورودی R41(1k)/R42(10k)، گین
 *              تقویت‌کننده - و عمداً نه آفست صفر و نه اصلاح بنچ، تا مقدار
 *              مستقیم با پروب اسکوپ روی خروجی LM358 قابل مقایسه باشد
 *              (mV = uV x 101 / 1000). دیاگ زندهٔ بررسی زنجیرهٔ جریان،
 *              دستور کاربر ۲۰۲۶-۰۹-۲۲.
 * @param  uint16_t__counts [EN] Raw ADC count of a current channel /
 *                              شمارش خام ADC یک کانال جریان
 * @return uint32_t [EN] Shunt voltage in uV / ولتاژ شانت بر حسب uV
 */
uint32_t func__BspMeasurement_CurrentCountsToShuntUv(uint16_t uint16_t__counts)
{
    uint64_t uint64_t__numerator;
    uint64_t uint64_t__denominator;

    uint64_t__numerator = (uint64_t)uint16_t__counts;
    uint64_t__denominator = 1u;

    /* [EN] Stage 1 - counts to ADC-pin voltage in mV (12-bit, 3300 mV).
       [FA] مرحلهٔ ۱ - شمارش به ولتاژ پایهٔ ADC بر حسب mV (۱۲ بیتی، ۳۳۰۰mV). */
    uint64_t__numerator *= BSP_MEASUREMENT_VREF_MV;
    uint64_t__denominator *= BSP_MEASUREMENT_ADC_FULL_SCALE;

    /* [EN] Stage 2 - ADC pin to LM358 output: undo the permanent R41(1k)/
       R42(10k) MCU-input divider (multiply by 11/10), still in mV.
       [FA] مرحلهٔ ۲ - پایهٔ ADC به خروجی LM358: خنثی‌کردن مقسم دائمی
       R41(1k)/R42(10k) ورودی MCU (ضرب در ۱۱/۱۰)، هنوز بر حسب mV. */
    uint64_t__numerator *=
        (uint64_t)(BSP_MEASUREMENT_CURRENT_DIV_TOP_OHMS +
                   BSP_MEASUREMENT_CURRENT_DIV_BOTTOM_OHMS);
    uint64_t__denominator *= BSP_MEASUREMENT_CURRENT_DIV_BOTTOM_OHMS;

    /* [EN] Stage 3 - LM358 output to shunt voltage: undo the non-inverting
       gain of 101 and scale mV up to uV (one division at the end).
       [FA] مرحلهٔ ۳ - خروجی LM358 به ولتاژ شانت: خنثی‌کردن گین غیروارونگر
       ۱۰۱ و بزرگ‌کردن mV به uV (یک تقسیم در انتها). */
    uint64_t__numerator *= BSP_MEASUREMENT_UV_PER_MV;
    uint64_t__denominator *= BSP_MEASUREMENT_AMP_GAIN;

    return (uint32_t)(uint64_t__numerator / uint64_t__denominator);
}
