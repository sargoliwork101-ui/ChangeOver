/**
 * @file    measurement.h
 * @brief   [EN] Converts ADC counts to engineering units (mV / mA) and keeps
 *              the latest snapshot for the other tasks. Pure software: the
 *              ADC+DMA hardware fills the raw buffer autonomously (bsp_adc),
 *              this module only converts.
 *          [FA] شمارش ADC را به واحد مهندسی (mV / mA) تبدیل می‌کند و آخرین
 *              نمونه را برای تسک‌های دیگر نگه می‌دارد. منطق این ماژول
 *              مستقل از برد است؛ ADC خام از BSP می‌آید و کالیبراسیون برد
 *              در `bsp_measurement` انجام می‌شود.
 */

#ifndef MEASUREMENT_H
#define MEASUREMENT_H

/* ==================== Includes ==================== */
#include "app_types.h"

/* ==================== Defines ==================== */

/* [EN] MEASUREMENT TASK PERIOD. CHANGE HERE to change the sample rate.
 *      The BSP returns only completed normalized frames, so this task period
 *      remains independent of the MCU ADC clock. Since 2026-09-22 (user
 *      order) the period is 1 ms so the synchronized charge-current samples
 *      in the snapshot are at most 1 ms old when the control task reads
 *      them; the voltage channels only get fresher.
 * [FA] دورهٔ تسک اندازه‌گیری. برای تغییر نرخ نمونه‌برداری همین‌جا عوض شود.
 *      BSP فقط فریم‌های استانداردشدهٔ کامل را برمی‌گرداند، پس این دوره
 *      مستقل از کلاک ADC میکروکنترلر است. از ۲۰۲۶-۰۹-۲۲ (دستور کاربر)
 *      دوره ۱ms است تا نمونه‌های سنکرون جریان شارژ داخل snapshot حداکثر
 *      ۱ms عمر داشته باشند وقتی تسک کنترل می‌خواندشان؛ ولتاژها هم فقط
 *      تازه‌تر می‌شوند. */
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

/* [EN] Number of current samples in the moving-average window, per channel.
 *      Range: 1..255 samples; the two channels keep separate windows.
 * [FA] تعداد نمونه‌های جریان در پنجرهٔ میانگین متحرک، به ازای هر کانال.
 *      بازهٔ ۱ تا ۲۵۵ نمونه؛ دو کانال پنجرهٔ جدا دارند. */
#define MEASUREMENT_CURRENT_AVERAGE_WINDOW   10u

/* ==================== Globals (shared values) ==================== */
/* [EN] Shared engineering values, written ONLY by the measurement task
 *      (Run). Any module/task can read them: #include "measurement.h" and
 *      use. Check BOOL__G__MeasDataValid before trusting the numbers.
 * [FA] مقادیر مهندسی مشترک، فقط توسط تسک measurement (Run) نوشته
 *      می‌شوند. هر ماژول/تسک می‌تواند بخواند: کافی است measurement.h را
 *      include کنید. قبل از اعتماد به اعداد، BOOL__G__MeasDataValid را
 *      چک کنید.
 * @note [EN] Named Meas* on purpose: the UI test globals in task_ui.c use
 *          InputVoltageMv/BatteryVoltageMv, so Meas* avoids a link
 *          collision. When the UI switches to the real values, the manual
 *          test globals are deleted in that stage.
 *      [FA] عمداً با پیشوند Meas*: متغیرهای تست UI در task_ui.c از
 *          InputVoltageMv/BatteryVoltageMv استفاده می‌کنند و Meas* از
 *          تداخل لینک جلوگیری می‌کند. وقتی UI به مقدار واقعی وصل شد،
 *          متغیرهای تست دستی در همان مرحله حذف می‌شوند. */
extern volatile uint32_t UINT32_T__G__MeasInputVoltageMv;   /* [EN] Logical 24 V input, mV / ورودی منطقی ۲۴ ولت، mV */
extern volatile uint32_t UINT32_T__G__MeasBattery24Mv;      /* [EN] Logical pack monitor only, mV / فقط مانیتور پک، mV */
extern volatile uint32_t UINT32_T__G__MeasBattery12Mv;      /* [EN] Middle-node/low-battery monitor, mV / مانیتور MID/باتری پایین، mV */
extern volatile uint32_t UINT32_T__G__MeasBatteryLowMv;     /* [EN] VLOW = MID-GND, mV / باتری پایین، mV */
extern volatile uint32_t UINT32_T__G__MeasBatteryHighMv;    /* [EN] VHIGH = V24-MID, mV / باتری بالا، mV */
extern volatile uint32_t UINT32_T__G__MeasCurrent1Ma;       /* [EN] Logical charge current 1, mA / جریان منطقی شارژ ۱، mA */
extern volatile uint32_t UINT32_T__G__MeasCurrent2Ma;       /* [EN] Logical charge current 2, mA / جریان منطقی شارژ ۲، mA */
extern volatile bool BOOL__G__MeasInputPresent;           /* [EN] Logical 24 V input present / حضور منطقی ورودی ۲۴ ولت */
extern volatile bool BOOL__G__MeasDataValid;              /* [EN] true after ADC warm-up frames / پس از فریم‌های warm-up ADC true */

/* ==================== Measurement Init ==================== */

/**
 * @brief  [EN] Zero the last snapshot (valid = false).
 *         [FA] آخرین نمونه را صفر می‌کند (valid = false).
 */
void func__Measurement_Init(void);

/* ==================== Measurement Run ==================== */

/**
 * @brief  [EN] Pull one raw frame from the BSP and convert every channel into
 *              the shared snapshot (mV / mA + input_present + valid).
 *         [FA] یک فریم خام از BSP می‌گیرد و همهٔ کانال‌ها را در snapshot
 *              مشترک تبدیل می‌کند (mV / mA + input_present + valid).
 * @note   [EN] Three completed stable ADC frames are required before this
 *              function publishes valid data. Unit: completed ADC frame.
 *         [FA] پیش از انتشار دادهٔ معتبر توسط این تابع، سه فریم کامل و پایدار
 *              ADC لازم است. واحد: فریم کامل ADC.
 *         [EN] Input voltage and input presence do not affect ADC validity.
 *         [FA] ولتاژ ورودی و حضور ورودی روی اعتبار ADC اثر ندارند.
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
 * @param  uint16_t__counts [EN] Raw ADC count, 0..4095 / شمارش خام ADC
 * @return uint32_t [EN] Voltage in mV, 0..3300 / ولتاژ بر حسب mV
 */
uint32_t func__Measurement_CountsToMv(uint16_t uint16_t__counts);

/* ==================== V24 Counts To Mv ==================== */

/**
 * @brief  [EN] Convert normalized 24 V channels to source mV through the
 *              board calibration port.
 *         [FA] کانال‌های استاندارد ۲۴ ولت را از طریق پورت کالیبراسیون برد
 *              به میلی‌ولت منبع تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Raw ADC count, 0..4095 / شمارش خام ADC
 * @return uint32_t [EN] Source voltage in mV, 0..~37000 / ولتاژ منبع mV
 */
uint32_t func__Measurement_V24CountsToMv(uint16_t uint16_t__counts);

/* ==================== V12 Counts To Mv ==================== */

/**
 * @brief  [EN] Convert the normalized 12 V battery channel to source mV
 *              through the board calibration port.
 *         [FA] کانال استاندارد باتری ۱۲ ولت را از طریق پورت کالیبراسیون برد
 *              به میلی‌ولت منبع تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Raw ADC count, 0..4095 / شمارش خام ADC
 * @return uint32_t [EN] Source voltage in mV, 0..~20000 / ولتاژ منبع mV
 */
uint32_t func__Measurement_V12CountsToMv(uint16_t uint16_t__counts);

/* ==================== Current Counts To Ma ==================== */

/**
 * @brief  [EN] Convert a normalized current channel to milliamps through the
 *              board calibration port.
 *         [FA] کانال استاندارد جریان را از طریق پورت کالیبراسیون برد به
 *              میلی‌آمپر تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Raw ADC count, 0..4095 / شمارش خام ADC
 * @return uint32_t [EN] Current in mA, 0..~3300 / جریان بر حسب mA
 */
uint32_t func__Measurement_Current1CountsToMa(uint16_t uint16_t__counts);
uint32_t func__Measurement_Current2CountsToMa(uint16_t uint16_t__counts);
/* [EN] Legacy generic wrapper = channel-2 calibration (kept for old callers;
 *      new code must pick the per-channel wrapper above).
 * [FA] wrapper عمومی قدیمی = کانال ۲؛ کد جدید wrapper پر-کانال. */
uint32_t func__Measurement_CurrentCountsToMa(uint16_t uint16_t__counts);

#endif /* MEASUREMENT_H */
