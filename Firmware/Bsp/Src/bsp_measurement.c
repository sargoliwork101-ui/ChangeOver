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

/* [EN] VOLTAGE SENSE DIVIDERS - read straight off the schematic, not tuned.
 *      USER-ORDERED CORRECTION 2026-09-29. Every one of the three sense
 *      nets has the SAME shape: two resistors in series feeding the ADC pin,
 *      and one resistor from that pin to ground.
 *
 *        sheet 1 (power)      sheet 4 (MCU)        to GND
 *        ADC_MICRO_+24V_INPUT     R46 = 68K  +  R11 = 1.2K  |  R12 = 6.8K
 *        ADC_MICRO_+24V_BAT       R47 = 68K  +  R13 = 1.2K  |  R14 = 6.8K
 *        ADC_MICRO_+24V_BAT_COM   R48 = 33K  +  R15 = 1.2K  |  R16 = 6.8K
 *
 *      So the two 24 V nets are ELECTRICALLY IDENTICAL (68K+1.2K over 6.8K)
 *      and there is no physical reason for their coefficients to differ.
 *
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
 * [FA] مقسم‌های حس ولتاژ - مستقیم از شماتیک خوانده شده‌اند، نه تنظیم‌شده.
 *      اصلاح به دستور کاربر ۲۰۲۶-۰۹-۲۹. هر سه نت حس یک شکل دارند: دو مقاومت
 *      سری تا پایهٔ ADC و یک مقاومت از همان پایه به زمین (R46/R47=68K و
 *      R48=33K روی شیت قدرت، به‌اضافهٔ R11/R13/R15=1.2K و R12/R14/R16=6.8K
 *      روی شیت MCU). پس دو نت ۲۴ولت از نظر الکتریکی یکی‌اند و هیچ دلیل فیزیکی
 *      برای تفاوت ضرایبشان وجود ندارد.
 *      چرا اصلاح لازم بود: نت پک قبلاً TOP=66200 داشت که هیچ مقاومتی روی این
 *      برد نیست؛ ساخته شده بود تا یک خواندن بنچ جور دربیاید، و دقیقاً همین
 *      دلیل کالیبره‌نشدن ولتاژ پک بود. مقسم غلط یعنی خطای گین: فقط در یک نقطه
 *      درست است و در بقیهٔ نقاط غلط، و بی‌صدا همهٔ مصرف‌کننده‌های آن عدد را
 *      خراب می‌کند. قاعده از این به بعد: ولتاژ را صادقانه بخوان؛ اگر تصمیمی
 *      باید زودتر گرفته شود، آستانه را جابه‌جا کن نه مقیاس را. خطای باقی‌ماندهٔ
 *      هر برد جای آفست‌های زمان اجرا (پارامترهای ۴/۵/۶) است که جمع‌شونده‌اند و
 *      نمی‌توانند شیب را خراب کنند. */
#define BSP_MEASUREMENT_SENSE_SERIES_MCU_OHMS  1200u  /* R11 / R13 / R15 */
#define BSP_MEASUREMENT_SENSE_SHUNT_OHMS       6800u  /* R12 / R14 / R16 */
#define BSP_MEASUREMENT_SENSE_TOP_24V_OHMS    68000u  /* R46 (input), R47 (pack) */
#define BSP_MEASUREMENT_SENSE_TOP_12V_OHMS    33000u  /* R48 (mid node)          */

#define BSP_MEASUREMENT_DIV24_TOP_OHMS \
    (BSP_MEASUREMENT_SENSE_TOP_24V_OHMS + BSP_MEASUREMENT_SENSE_SERIES_MCU_OHMS)
#define BSP_MEASUREMENT_DIV24_BOTTOM_OHMS   BSP_MEASUREMENT_SENSE_SHUNT_OHMS

/* [EN] Same physical network as the input net above - deliberately spelled
        out from the same constants so the two can never drift apart again.
   [FA] دقیقاً همان شبکهٔ فیزیکی نت ورودی - عمداً از همان ثابت‌ها ساخته شده تا
        دیگر هرگز از هم جدا نشوند. */
#define BSP_MEASUREMENT_DIV24BAT_TOP_OHMS   BSP_MEASUREMENT_DIV24_TOP_OHMS
#define BSP_MEASUREMENT_DIV24BAT_BOTTOM_OHMS BSP_MEASUREMENT_DIV24_BOTTOM_OHMS

#define BSP_MEASUREMENT_DIV12_TOP_OHMS \
    (BSP_MEASUREMENT_SENSE_TOP_12V_OHMS + BSP_MEASUREMENT_SENSE_SERIES_MCU_OHMS)
#define BSP_MEASUREMENT_DIV12_BOTTOM_OHMS   BSP_MEASUREMENT_SENSE_SHUNT_OHMS

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
/* [EN] Channel 2 (Trans2 / Shunt2 -> PA7): bench-calibrated pair. The
 *      2026-09-18 scope run gave 1085 permille (330 read vs 358 mA true
 *      primary at fixed 15% duty); the 2026-09-24 user order (calibrate
 *      from the given bench numbers, no further tests) re-set the gain at
 *      the D=15% point: 1085 x 425/354 = 1303 permille (displayed 354 vs
 *      425 mA DMM true). The chain stays non-linear (D=10%: reads
 *      ~222-250 vs 185 true) - a solo hardware re-check stays on the
 *      bench list; the LUT above the chain is the real correction.
 * [FA] کانال ۲ (Trans2/Shunt2): زوج کالیبره‌شدهٔ بنچ. ران اسکوپ ۲۰۲۶-۰۹-۱۸
 *      ۱۰۸۵ پرمیل داد؛ دستور ۲۰۲۶-۰۹-۲۴ (کالیبره از همین اعداد، بدون تست
 *      بیشتر) گین را در نقطهٔ D=15% گذاشت: 1085×425÷354 = ۱۳۰۳ پرمیل.
 *      زنجیره هنوز غیرخطی است (D=10%: ~222-250 در برابر 185 واقعی) — تست
 *      تکی سخت‌افزاری در فهرست بنچ می‌ماند؛ اصلاح واقعی LUT روی زنجیره است. */
#define BSP_MEASUREMENT_CURRENT2_OFFSET_COUNTS 8u
#define BSP_MEASUREMENT_CURRENT2_GAIN_PERMILLE 1303u
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
 *      the ESP link (flash-persisted since v1.14 - a reboot keeps the set).
 *      Written from the EspLink task, read in the measurement task; both
 *      are aligned 32-bit values, atomic on Cortex-M3.
 * [FA] نسخهٔ زمان اجرای کالیبراسیون جریان هر کانال: مقدار اولیه از
 *      پیش‌فرض‌های بنچ بالا و نوشتن در زمان اجرا توسط لینک ESP (روی فلش
 *      می‌ماند از نسخهٔ ۱.۱۴ - ری‌استارت مجموعه را نگه می‌دارد). نوشتن از تسک EspLink و
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
/* ==================== BspMeasurement_Battery24CountsToMv ==================== */

/**
 * @brief  [EN] Undo the battery-PACK 24 V divider (user factor, 2026-09-25).
 *         [FA] خنثی‌کردن مقسم نت باتری‌پک ۲۴V (ضریب کاربر، ۲۰۲۶-۰۹-۲۵).
 * @param  uint16_t__counts [EN] ADC count / شمارش ADC
 * @return uint32_t [EN] Pack voltage in mV / ولتاژ پک بر حسب mV
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
 * @brief  [EN] Shared current formula of both channels, one stage per
 *              schematic element: counts -> ADC pin mV -> undo the
 *              R41/R42 MCU divider -> undo the LM358 gain -> undo the
 *              shunt mOhms -> mA, then subtract the per-channel zero
 *              offset and apply the per-channel bench gain permille.
 *              Every multiply carries its own numerator/denominator stage
 *              and only ONE division runs at the very end (no
 *              intermediate truncation accumulates).
 *         [FA] قالب فرمول مشترک هر دو کانال، یک مرحله برای هر المان
 *              شماتیک: شمارش -> mV پایه ADC -> خنثی‌کردن تقسیم R41/R42 ->
 *              خنثی‌کردن گین LM358 -> خنثی‌کردن mΩ شانت -> mA، سپس آفست
 *              صفر پر-کانال و گین پرمیل بنچ. هر ضرب مرحلهٔ صورت/مخرج خودش
 *              را جابه‌جا می‌کند و فقط یک تقسیم در انتها اجرا می‌شود.
 * @note   [EN] Flash diet: the five runtime stages are folded into ONE
 *              32-bit multiply+divide with BIT-IDENTICAL results (proven
 *              for all 4096 counts; the u64 division pulled
 *              __aeabi_uldivmod ~1 KiB). Derivation from the schematic
 *              values, kept staged so it stays auditable:
 *                num = counts x VREF(3300) x (R41+R42)(11000) x MA_PER_A(1000)
 *                den = FULL(4095) x R42(10000) x GAIN(101) x SHUNT(10 mOhm)
 *                = counts x 24200 / 27573   (both sides / 1,500,000;
 *                  24200 x 4095 = 99,099,000 < 2^32, exact for every count).
 *              If ANY resistor/value above ever changes, re-derive (the
 *              host test recomputes the collapse from these defines).
 *         [FA] رژیم فلش: پنج مرحله در یک ضرب+تقسیم ۳۲بیتی با نتیجهٔ
 *              بیت‌به‌بیت یکسان جمع شد (اثبات برای هر ۴۰۹۶ شمارش؛ تقسیم
 *              ۶۴بیتی ~۱KB می‌خواست). اگر مقاومتی عوض شد دوباره اشتقاق
 *              بگیر (تست هاست از همین دیفاین‌ها بازمحاسبه می‌کند).
 * @param  uint16_t__counts           [EN] ADC count / شمارش ADC
 * @param  uint32_t__offsetCounts     [EN] zero-current offset, counts / آفست صفر
 * @param  uint32_t__gainPermille     [EN] bench gain permille / ضریب گین بنچ
 * @return uint32_t [EN] Current in mA / جریان بر حسب mA
 */
static uint32_t func__BspMeasurement_ConvertCurrent(uint16_t uint16_t__counts,
                                                    uint32_t uint32_t__offsetCounts,
                                                    uint32_t uint32_t__gainPermille)
{
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

    /* [EN] Stages 1..4 folded (see the @note derivation): counts x 24200 /
       27573 - exact for every count, fits u32 (99,099,000 < 2^32).
       [FA] مراحل ۱..۴ جمع‌شده (اشتقاق در @note): دقیق برای هر شمارش. */
    uint32_t__chainCurrentMa =
        (uint32_t__calibratedCounts * (uint32_t)24200) / (uint32_t)27573;

    /* [EN] Stage 5 - per-channel bench gain trim in permille (ch1 1046,
       ch2 1303 = the DMM-calibrated 2026-09-24 points at D=15%).
       u32 is exact here: mA <= 3594 (full-scale chain) x gain <= 3000
       (setter clamp) = 10,782,000 < 2^32.
       [FA] مرحلهٔ ۵ - اصلاح گین بنچ پر-کانال بر حسب پرمیل. ضرب ۳۲بیتی
       دقیق است (حداکثر ~۱۰٫۸میلیون < ۲^۳۲). */
    return (uint32_t__chainCurrentMa * uint32_t__gainPermille) /
           BSP_MEASUREMENT_PERMILLE_SCALE;
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
 *              Trans1/Shunt1 chain, channel 1 = Trans2/Shunt2.
 *              Flash-persisted (NVM); a reboot keeps the tuned value.
 *         [FA] آفست جریان صفر (شمارش خام) یک کانال را در زمان اجرا تنظیم
 *              می‌کند، گیره در ۰..۲۵۵. کانال ۰ = زنجیرهٔ Trans1/Shunt1 و
 *              کانال ۱ = Trans2/Shunt2. روی فلش می‌ماند؛ ری‌استارت مقدار
 *              تنظیم‌شده را نگه می‌دارد.
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
 *              2026-09-22; flash-persisted since v1.14).
 *         [FA] ضریب گین بنچ (پرمیل) یک کانال را در زمان اجرا تنظیم می‌کند،
 *              گیره در ۱۰۰..۳۰۰۰ (پنل ESP، دستور کاربر ۲۰۲۶-۰۹-۲۲؛ روی فلش
 *              می‌ماند از نسخهٔ ۱.۱۴).
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
 *              channel to the shunt voltage in microvolts - exactly the
 *              three hardware stages (ADC reference, R41/R42 divider,
 *              amplifier gain), deliberately NO zero offset and NO bench
 *              trim, so the value checks directly against a scope probe
 *              on the LM358 output (mV = uV x 101 / 1000).
 *         [FA] فقط زنجیرهٔ سخت‌افزاری: شمارش خام جریان هر کانال به ولتاژ
 *              دو سر شانت بر حسب میکروولت - دقیقاً سه مرحلهٔ سخت‌افزاری،
 *              عمداً بدون آفست صفر و بدون اصلاح بنچ، تا مستقیم با پروب
 *              اسکوپ روی خروجی LM358 قابل مقایسه باشد (mV = uV×101÷1000).
 * @note   [EN] Flash diet: same fold as ConvertCurrent (one u32
 *              multiply+divide, bit-identical for all 4096 counts):
 *                num = counts x VREF(3300) x (R41+R42)(11000) x UV_PER_MV(1000)
 *                den = FULL(4095) x R42(10000) x GAIN(101)
 *                = counts x 242000 / 27573  (both sides / 150,000;
 *                  242000 x 4095 = 990,990,000 < 2^32, exact every count).
 *              Re-derive if any value changes (host test recomputes).
 *         [FA] رژیم فلش: همان تا‌کردن ConvertCurrent (یک ضرب+تقسیم u32،
 *              بیت‌به‌بیت یکسان برای هر ۴۰۹۶ شمارش). با تغییر هر مقدار
 *              دوباره اشتقاق بگیر (تست هاست بازمحاسبه می‌کند).
 * @param  uint16_t__counts [EN] Raw ADC count of a current channel /
 *                              شمارش خام ADC یک کانال جریان
 * @return uint32_t [EN] Shunt voltage in uV / ولتاژ شانت بر حسب uV
 */
uint32_t func__BspMeasurement_CurrentCountsToShuntUv(uint16_t uint16_t__counts)
{
    return ((uint32_t)uint16_t__counts * (uint32_t)242000) / (uint32_t)27573;
}
