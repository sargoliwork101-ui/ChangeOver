/**
 * @file    measurement.h
 * @brief   [EN] Converts ADC counts to engineering units (mV / mA) and keeps
 *              the latest snapshot for the other tasks. Pure software: the
 *              ADC+DMA hardware fills the raw buffer autonomously (bsp_adc),
 *              this module only converts.
 *          [FA] شمارش ADC را به واحد مهندسی (‎mV / mA)‎ تبدیل می‌کند و آخرین
 *              نمونه را برای تسک‌های دیگر نگه می‌دارد. منطق این ماژول
 *              مستقل از برد است؛ ADC خام از BSP می‌آید و کالیبراسیون برد
 *              در `bsp_measurement` انجام می‌شود.
 */

#ifndef MEASUREMENT_H
#define MEASUREMENT_H

/* ==================== Includes ==================== */
#include "app_types.h"

/* ==================== Defines ==================== */

/* [EN] MEASUREMENT TASK PERIOD - change the sample rate HERE. The BSP
 *      returns only completed normalized frames, so this period is
 *      independent of the ADC clock. 1 ms keeps the synchronized current
 *      samples at most 1 ms old when the control task reads them.
 * [FA] دورهٔ تسک اندازه‌گیری - نرخ نمونه‌برداری همین‌جا عوض می‌شود. BSP
 *      فقط فریم‌های کامل را می‌دهد، پس دوره مستقل از کلاک ADC است. ۱ms
 *      یعنی نمونه‌های سنکرون جریان حداکثر ۱ms کهنه‌اند. */
#define MEASUREMENT_PERIOD_MS      1u

/* ==================== Current filter switches / کلیدهای فیلتر جریان ==================== */

/* [EN] Median-of-3 prefilter on each charge-current channel (user order
 *      2026-09-22): kills single-sample jumps of the synchronized mid-ON
 *      reading with zero added lag. Set to 1u to enable, 0u to compile it
 *      out completely; the sample then passes through unchanged.
 * [FA] پیش‌فیلتر مدین-۳ روی هر کانال جریان شارژ (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۲): پرش‌های تک‌نمونه‌ای خوانش سنکرون وسط ON را بدون هیچ
 *      تأخیری حذف می‌کند. ۱u فعال و ۰u کامپایل‌نشده؛ در حالت خاموش نمونه
 *      بدون تغییر عبور می‌کند. */
#define MEASUREMENT_CURRENT_MEDIAN3_ENABLE   1u

/* [EN] Moving average over the last MEASUREMENT_CURRENT_AVERAGE_WINDOW
 *      current samples (user order 2026-09-22: window of 10). At the 1 ms
 *      measurement cadence this smooths ~10 ms of history and reacts to a
 *      real current step within one window. Set to 1u to enable, 0u to
 *      compile it out completely.
 * [FA] میانگین متحرک روی آخرین MEASUREMENT_CURRENT_AVERAGE_WINDOW نمونهٔ
 *      جریان (دستور کاربر ۲۰۲۶-۰۹-۲۲: پنجرهٔ ۱۰تایی). با دورهٔ ۱ms اندازه‌گیری
 *      حدود ۱۰ms تاریخچه را صاف می‌کند و به پلهٔ واقعی جریان در حد یک پنجره
 *      واکنش می‌دهد. ۱u فعال و ۰u کامپایل‌نشده. */
#define MEASUREMENT_CURRENT_AVERAGE_ENABLE   1u

/* [EN] The bench calibration tables (channel LUTs + battery-voltage
        compensation) moved to calibration.h (user order 2026-09-25: one
        separate file next to this one, easy to amend; three tables live
        there and missing points get appended later). calibration.h is
        included ONLY by measurement.c - the tables are static.
   [FA] جدول‌های کالیبراسیون بنچ (LUT کانال‌ها + جبران ولتاژ باتری) به
        calibration.h منتقل شدند (دستور کاربر ۲۰۲۶-۰۹-۲۵: یک فایل جدا کنار
        همین فایل برای اصلاح راحت؛ سه جدول آنجاست و نقاط ناقص بعداً اضافه
        می‌شوند). calibration.h فقط توسط measurement.c اینکلود می‌شود. */


/* [EN] Compiled ring size / hard ceiling of the ESP-adjustable runtime
 *      moving-average window, per channel. WHY 300: TLM streams at 10 Hz,
 *      so at W <= 100 two panel samples share almost no filter history -
 *      the filter worked but was INVISIBLE on the panel; W = 200..300
 *      spans 2..3 TLM frames and the smoothing becomes observable.
 *      CAVEAT: the auto-mode charger regulates at 100 Hz on this filtered
 *      value - keep W <= ~50 in AUTO mode; large W is for MANUAL-duty
 *      bench watching. Ring RAM cost: 2 x 300 x 4 B.
 * [FA] اندازهٔ حلقهٔ کامپایل / سقف پنجرهٔ میانگین متحرکِ قابل‌تنظیم از ESP
 *      (هر کانال). چرا ۳۰۰: TLM با ۱۰Hz می‌رود، پس در W≤۱۰۰ دو نمونهٔ پنل
 *      تاریخچهٔ مشترکی ندارند و صاف‌کردن روی پنل دیده نمی‌شود؛ W=۲۰۰..۳۰۰
 *      روی ۲..۳ فریم TLM می‌ایستد. هشدار: شارژر مود خودکار با ۱۰۰Hz روی
 *      همین مقدار تنظیم می‌کند - در مود خودکار W را ~۵۰ یا کمتر نگه
 *      دارید؛ W بزرگ برای تماشای بنچ با دیوتی دستی است. RAM: ۲×۳۰۰×۴B. */
#define MEASUREMENT_CURRENT_AVERAGE_WINDOW   300u

/* [EN] Startup default of the runtime moving-average window (today's
 *      behavior). [FA] پیش‌فرض بوت پنجرهٔ میانگین (رفتار فعلی). */
#define MEASUREMENT_CURRENT_AVERAGE_WINDOW_DEFAULT 10u

/* [EN] Hard ceiling of the ESP-adjustable runtime median window (v1.4,
 *      user order 2026-09-25: the panel can set ANY value 1..15 - even
 *      sizes allowed, no odd rounding anymore; 1..2 = bypass, 3..15 =
 *      active insertion-sort median). The history arrays are sized by
 *      this constant.
 * [FA] سقف قطعی پنجرهٔ مدینِ قابل‌تنظیم از ESP (v1.4، دستور کاربر
 *      ۲۰۲۶-۰۹-۲۵: پنل هر مقدار ۱..۱۵ را می‌گذارد - اندازهٔ زوج هم
 *      مجاز است و دیگر به فرد گرد نمی‌شود؛ ۱..۲ = عبور مستقیم،
 *      ۳..۱۵ = مدین فعال با مرتب‌سازی درجی). آرایه‌های تاریخچه با
 *      همین ثابت اندازه می‌گیرند. */
#define MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX  15u

/* [EN] Clamp limit of the ESP-adjustable runtime voltage calibration
 *      offsets in mV (user order 2026-09-22): the panel can trim each
 *      voltage channel by at most +/-2 V; default 0 keeps today's behavior.
 * [FA] حد گیرهٔ آفست‌های کالیبراسیون ولتاژِ قابل‌تنظیم از ESP بر حسب mV
 *      (دستور کاربر ۲۰۲۶-۰۹-۲۲): پنل حداکثر ±۲ ولت هر کانال ولتاژ را
 *      جابه‌جا می‌کند؛ پیش‌فرض ۰ همان رفتار فعلی است. */
/* [EN] v1.10 (user order 2026-09-25): raised 2000 -> 5000 mV. The pack
        divider error alone was ~2.3 V at 24 V, beyond the old range, so the
        runtime offset could not even express it. The divider itself is now
        corrected at the source (BSP battery-24 factor); the wider range
        keeps future divider/resistor drift correctable from the panel.
   [FA] نسخه ۱٫۱۰ (دستور کاربر ۲۰۲۶-۰۹-۲۵): از ۲۰۰۰ به ۵۰۰۰mV بالا رفت.
        خطای مقسم پک به‌تنهایی ~2.3V در ۲۴V بود و از بازهٔ قدیمی بیرون؛
        یعنی آفست زمان اجرا اصلاً نمی‌توانست آن را بنویسد. خود مقسم حالا
        در BSP اصلاح شده؛ بازهٔ پهن‌تر برای جبران رانش‌های آینده از پنل است. */
#define MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV  5000u

/* ==================== Globals (shared values) ==================== */
/* [EN] Shared engineering values, written ONLY by the measurement task
 *      (Run). Any module/task can read them (#include "measurement.h");
 *      check BOOL__G__MeasDataValid first. Named Meas* on purpose to
 *      avoid a link collision with the manual test globals in task_ui.c.
 * [FA] مقادیر مهندسی مشترک، فقط توسط تسک measurement نوشته می‌شوند؛ هر
 *      ماژول می‌تواند بخواند (پیش از اعتماد، BOOL__G__MeasDataValid را
 *      چک کنید). پیشوند Meas* عمدی است تا با متغیرهای تست task_ui.c
 *      تداخل لینک نگیرد. */
extern volatile uint32_t UINT32_T__G__MeasInputVoltageMv;   /* [EN] Logical 24 V input, mV / ورودی منطقی ۲۴ ولت، mV */
extern volatile uint32_t UINT32_T__G__MeasBattery24Mv;      /* [EN] Logical pack monitor only, mV / فقط مانیتور پک، mV */
extern volatile uint32_t UINT32_T__G__MeasBattery12Mv;      /* [EN] Middle-node/low-battery monitor, mV / مانیتور MID/باتری پایین، mV */
extern volatile uint32_t UINT32_T__G__MeasBatteryLowMv;     /* [EN] VLOW = MID-GND, mV / باتری پایین، mV */
extern volatile uint32_t UINT32_T__G__MeasBatteryHighMv;    /* [EN] VHIGH = V24-MID, mV / باتری بالا، mV */
/* [EN] Measured ADC reference (VDDA), mV; 0 = unavailable.
 * [FA] مرجع ADC اندازه‌گیری‌شده، mV؛ صفر یعنی در دسترس نیست. */
extern volatile uint32_t UINT32_T__G__MeasVddaMv;
/* [EN] RAW ADC counts of the three voltage channels and the internal
 *      reference, straight off the DMA frame - before the divider maths,
 *      before the runtime offsets, before the V12 bench compensation.
 *      They exist so a calibration can be built from FIRST PRINCIPLES: counts
 *      are the only number on this board that no coefficient can distort, so
 *      a sweep that logs counts next to a DMM lets every scale be recomputed
 *      offline without trusting any constant currently in the firmware.
 *      Deliberately NOT gated behind a mode - see measurement.c.
 * [FA] شمارش خام ADC سه کانال ولتاژ و مرجع داخلی، مستقیم از فریم DMA - پیش از
 *      ریاضیات مقسم، پیش از آفست‌های زمان اجرا و پیش از جبران بنچ ۱۲ولت.
 *      هستند تا کالیبراسیون از پایه ساخته شود: شمارش تنها عددی روی این برد
 *      است که هیچ ضریبی نمی‌تواند خرابش کند، پس سوییپی که شمارش را کنار
 *      مولتی‌متر ثبت کند اجازه می‌دهد هر مقیاس آفلاین و بدون اعتماد به هیچ
 *      ثابت فعلی بازمحاسبه شود. عمداً پشت هیچ مودی قفل نشده‌اند. */
extern volatile uint32_t UINT32_T__G__MeasVinRawCounts;
extern volatile uint32_t UINT32_T__G__MeasV24RawCounts;
extern volatile uint32_t UINT32_T__G__MeasV12RawCounts;
extern volatile uint32_t UINT32_T__G__MeasVrefintRawCounts;
extern volatile uint32_t UINT32_T__G__MeasCurrent1Ma;       /* [EN] Logical charge current 1, mA / جریان منطقی شارژ ۱، mA */
extern volatile uint32_t UINT32_T__G__MeasCurrent2Ma;       /* [EN] Logical charge current 2, mA / جریان منطقی شارژ ۲، mA */
/* [EN] Unfiltered single-frame current-chain diagnostics (user order
 *      2026-09-22): raw ADC counts, pure-hardware shunt voltage (no offset,
 *      no trim) and unfiltered single-frame mA of the last frame, per channel.
 * [FA] دیاگ تک‌فریمیِ فیلترنشدهٔ زنجیرهٔ جریان (دستور کاربر ۲۰۲۶-۰۹-۲۲):
 *      شمارش خام ADC، ولتاژ شانت فقط-سخت‌افزاری (بدون آفست و اصلاح) و
 *      mA تک‌فریمِ بدون فیلترِ آخرین فریم، برای هر کانال. */
extern volatile uint32_t UINT32_T__G__MeasCurrent1RawCounts;  /* [EN] Raw ADC counts ch1 / شمارش خام کانال ۱ */
extern volatile uint32_t UINT32_T__G__MeasCurrent1ShuntUv;    /* [EN] Shunt voltage ch1, uV / ولتاژ شانت کانال ۱ */
extern volatile uint32_t UINT32_T__G__MeasCurrent1MaUnfiltered; /* [EN] Unfiltered single-frame mA ch1 / mA تک‌فریم بدون فیلتر کانال ۱ */
extern volatile uint32_t UINT32_T__G__MeasCurrent2RawCounts;  /* [EN] Raw ADC counts ch2 / شمارش خام کانال ۲ */
extern volatile uint32_t UINT32_T__G__MeasCurrent2ShuntUv;    /* [EN] Shunt voltage ch2, uV / ولتاژ شانت کانال ۲ */
extern volatile uint32_t UINT32_T__G__MeasCurrent2MaUnfiltered; /* [EN] Unfiltered single-frame mA ch2 / mA تک‌فریم بدون فیلتر کانال ۲ */
extern volatile bool BOOL__G__MeasInputPresent;           /* [EN] Logical 24 V input present / حضور منطقی ورودی ۲۴ ولت */
extern volatile bool BOOL__G__MeasDataValid;              /* [EN] true after ADC warm-up frames / پس از فریم‌های warm-up ADC true */

/* ==================== Measurement Init ==================== */

/**
 * @brief  [EN] Zero the last snapshot (valid = false).
 *         [FA] آخرین نمونه را صفر می‌کند (‎valid = false)‎.
 */
void func__Measurement_Init(void);

/* ==================== Measurement Run ==================== */

/**
 * @brief  [EN] Pull one raw frame from the BSP and convert every channel
 *              into the shared snapshot (mV / mA + input_present + valid).
 *         [FA] یک فریم خام از BSP می‌گیرد و همهٔ کانال‌ها را در snapshot
 *              مشترک تبدیل می‌کند (‎mV / mA + input_present + valid)‎.
 * @note   [EN] Three completed stable ADC frames are required before this
 *              function publishes valid data (unit: completed ADC frame);
 *              input voltage / input presence do not affect ADC validity.
 *         [FA] سه فریم کامل و پایدار ADC لازم است تا دادهٔ معتبر منتشر
 *              شود (واحد: فریم کامل ADC)؛ ولتاژ/حضور ورودی روی اعتبار
 *              ADC اثر ندارند.
 */
#define MEASUREMENT_WARMUP_FRAME_COUNT 3u
void func__Measurement_Run(void);

/* ==================== Measurement Get Snapshot ==================== */

/**
 * @brief  [EN] Copy the last snapshot. Returns false if not valid yet.
 *         [FA] آخرین نمونه را کپی می‌کند. اگر هنوز معتبر نباشد false.
 * @param  measurement_snapshot_t__out [EN] Output pointer, must not be NULL /
 *                                          اشاره‌گر خروجی
 * @return bool [EN] true if a valid snapshot was copied / اگر نمونهٔ معتبر
 *                   کپی شد true
 */
bool func__Measurement_GetSnapshot(measurement_snapshot_t *measurement_snapshot_t__out);

/* ==================== Counts To Mv ==================== */

/**
 * @brief  [EN] Convert normalized ADC counts to millivolts using the board
 *              calibration port.
 *         [FA] شمارش استاندارد ADC را با استفاده از پورت کالیبراسیون برد
 *              به میلی‌ولت تبدیل می‌کند.
 * @‎param  uint16_t__counts [EN] Raw ADC count, 0..4095‎ / شمارش خام ADC
 * @‎return uint32_t [EN] Voltage in mV, 0..3300‎ / ولتاژ بر حسب mV
 */
uint32_t func__Measurement_CountsToMv(uint16_t uint16_t__counts);

/* ==================== V24 Counts To Mv ==================== */

/**
 * @brief  [EN] Convert normalized 24 V channels to source mV through the
 *              board calibration port.
 *         [FA] کانال‌های استاندارد ۲۴ ولت را از طریق پورت کالیبراسیون برد
 *              به میلی‌ولت منبع تبدیل می‌کند.
 * @‎param  uint16_t__counts [EN] Raw ADC count, 0..4095‎ / شمارش خام ADC
 * @‎return uint32_t [EN] Source voltage in mV, 0..~37000‎ / ولتاژ منبع mV
 */
uint32_t func__Measurement_V24CountsToMv(uint16_t uint16_t__counts);

/**
 * @brief  [EN] Convert the battery-PACK 24 V channel with the USER divider
 *              factor (attenuation 6.8k/69.2k, 2026-09-25) - the input net
 *              keeps func__Measurement_V24CountsToMv.
 *         [FA] کانال باتری‌پک ۲۴ ولت با ضریب مقسم «کاربر» (تضعیف
 *              ‎6.8k/69.2k‎، ۲۰۲۶-۰۹-۲۵) - نت ورودی روی V24CountsToMv می‌ماند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Pack voltage in mV / ولتاژ پک mV
 */
uint32_t func__Measurement_Battery24CountsToMv(uint16_t uint16_t__counts);

/* ==================== V12 Counts To Mv ==================== */

/**
 * @brief  [EN] Convert the normalized 12 V battery channel to source mV
 *              through the board calibration port.
 *         [FA] کانال استاندارد باتری ۱۲ ولت را از طریق پورت کالیبراسیون برد
 *              به میلی‌ولت منبع تبدیل می‌کند.
 * @‎param  uint16_t__counts [EN] Raw ADC count, 0..4095‎ / شمارش خام ADC
 * @‎return uint32_t [EN] Source voltage in mV, 0..~20000‎ / ولتاژ منبع mV
 */
uint32_t func__Measurement_V12CountsToMv(uint16_t uint16_t__counts);

/* ==================== Current Counts To Ma ==================== */

/**
 * @brief  [EN] Convert a normalized current channel to milliamps through the
 *              board calibration port.
 *         [FA] کانال استاندارد جریان را از طریق پورت کالیبراسیون برد به
 *              میلی‌آمپر تبدیل می‌کند.
 * @‎param  uint16_t__counts [EN] Raw ADC count, 0..4095‎ / شمارش خام ADC
 * @‎return uint32_t [EN] Current in mA, 0..~3300‎ / جریان بر حسب mA
 */
uint32_t func__Measurement_Current1CountsToMa(uint16_t uint16_t__counts);
uint32_t func__Measurement_Current2CountsToMa(uint16_t uint16_t__counts);

#ifdef MEASUREMENT_HOST_TEST
/**
 * @brief  [EN] Host-only exact arithmetic probe for the runtime LUT math.
 *         [FA] probeٔ دقیق arithmetic LUT، فقط برای تست هاست.
 * @param  uint32_t__a [EN] Multiplicand / عامل اول
 * @param  uint32_t__b [EN] Multiplicand / عامل دوم
 * @param  uint32_t__divisor [EN] Nonzero divisor / مقسوم‌علیه ناصفر
 * @return uint32_t [EN] Saturated quotient / خارج‌قسمت اشباع‌شده
 */
uint32_t func__Measurement_HostTest_MulDivU32(uint32_t uint32_t__a,
                                              uint32_t uint32_t__b,
                                              uint32_t uint32_t__divisor);
#endif

/* [EN] Legacy generic wrapper = channel-2 calibration (kept for old callers;
 *      new code must pick the per-channel wrapper above).
 * [FA] wrapper عمومی قدیمی = کانال ۲؛ کد جدید wrapper پر-کانال. */
uint32_t func__Measurement_CurrentCountsToMa(uint16_t uint16_t__counts);

/**
 * @brief  [EN] Raw current counts to the pure-hardware shunt voltage in uV
 *              (no offset, no trim) - live diagnostic for the current-chain
 *              review (user order 2026-09-22).
 *         [FA] شمارش خام جریان به ولتاژ شانتِ فقط-سخت‌افزاری بر حسب uV
 *              (بدون آفست و اصلاح) - دیاگ زندهٔ بررسی زنجیرهٔ جریان
 *              (دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint16_t__counts [EN] ADC count / شمارش ADC
 * @return uint32_t [EN] Shunt voltage in uV / ولتاژ شانت بر حسب uV
 */
uint32_t func__Measurement_CurrentCountsToShuntUv(uint16_t uint16_t__counts);

/* ==================== Runtime config API (ESP panel) / API پیکربندی زمان اجرا ==================== */

/**
 * @brief  [EN] Set the runtime median window size of the current filter:
 *              any size 1..MAX (even sizes too, 1..2 bypass); capability-
 *              gated by the compiled switch; flash-persisted (NVM id 7).
 *         [FA] اندازهٔ پنجرهٔ مدین فیلتر جریان در زمان اجرا: هر اندازهٔ
 *              ۱..MAX (زوج هم، ۱..۲ عبور مستقیم)؛ با کلید کامپایل؛ روی
 *              فلش می‌ماند (شناسهٔ NVM ۷).
 * @param  uint8_t__medianSize [EN] Requested size / اندازهٔ درخواستی
 * @return uint8_t [EN] Applied size / اندازهٔ اعمال‌شده
 */
uint8_t func__Measurement_SetFilterMedianSize(uint8_t uint8_t__medianSize);

/**
 * @brief  [EN] Set the runtime moving-average window, clamped to
 *              1..MEASUREMENT_CURRENT_AVERAGE_WINDOW; the measurement task
 *              resets the filter state after a change.
 *         [FA] پنجرهٔ میانگین متحرک در زمان اجرا، گیرهٔ
 *              ۱..MEASUREMENT_CURRENT_AVERAGE_WINDOW؛ تسک اندازه‌گیری بعد
 *              از تغییر وضعیت فیلتر را ریست می‌کند.
 * @‎param  uint32_t__windowSamples [EN] Requested window, clamped before narrowing (u8 storage would slice 256..300 to 0..44)‎ / پنجرهٔ درخواستی
 * @return uint16_t [EN] Applied window / پنجرهٔ اعمال‌شده
 */
uint16_t func__Measurement_SetFilterAverageWindow(uint32_t uint32_t__windowSamples);

/**
 * @brief  [EN] Read the live median window size of the current filter.
 *         [FA] اندازهٔ زندهٔ پنجرهٔ مدین.
 * @return uint8_t [EN] 1, 3 or 5 / اندازهٔ فعال
 */
uint8_t func__Measurement_GetFilterMedianSize(void);

/**
 * @brief  [EN] Read the live moving-average window size.
 *         [FA] اندازهٔ زندهٔ پنجرهٔ میانگین.
 * @return uint16_t [EN] Window in samples / پنجره بر حسب نمونه
 */
uint16_t func__Measurement_GetFilterAverageWindow(void);

/**
 * @brief  [EN] Set one runtime voltage calibration offset, clamped to
 *              +/-MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV; flash-persisted
 *              (NVM ids 4/5/6). Default 0 = today's behavior.
 *         [FA] یک آفست کالیبراسیون ولتاژ زمان اجرا، گیرهٔ
 *              ±MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV؛ روی فلش می‌ماند
 *              (شناسه‌های NVM ۴/۵/۶). پیش‌فرض ۰ همان رفتار فعلی است.
 * @‎param  uint8_t__channelIndex [EN] 0 = VIN, 1 = V24, 2 = V12‎ / اندیس
 * @param  int32_t__offsetMv [EN] Requested offset, mV / آفست درخواستی
 * @return int32_t [EN] Applied offset, mV / آفست اعمال‌شده
 */
int32_t func__Measurement_SetVoltageOffsetMv(uint8_t uint8_t__channelIndex,
                                             int32_t int32_t__offsetMv);

/**
 * @brief  [EN] Read one runtime voltage calibration offset.
 *         [FA] خواندن یک آفست کالیبراسیون ولتاژ زمان اجرا.
 * @‎param  uint8_t__channelIndex [EN] 0 = VIN, 1 = V24, 2 = V12‎ / اندیس
 * @return int32_t [EN] Live offset, mV / آفست زنده
 */
int32_t func__Measurement_GetVoltageOffsetMv(uint8_t uint8_t__channelIndex);

#endif /* MEASUREMENT_H */
