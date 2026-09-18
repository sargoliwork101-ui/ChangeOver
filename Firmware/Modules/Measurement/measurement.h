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
 *      remains independent of the MCU ADC clock.
 * [FA] دورهٔ تسک اندازه‌گیری. برای تغییر نرخ نمونه‌برداری همین‌جا عوض شود.
 *      BSP فقط فریم‌های استانداردشدهٔ کامل را برمی‌گرداند، پس این دوره
 *      مستقل از کلاک ADC میکروکنترلر است. */
#define MEASUREMENT_PERIOD_MS      10u

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

/* ==================== Input presence thresholds / آستانه‌های حضور ورودی ==================== */

/**
 * @brief  [EN] Input voltage at or above which the shared snapshot reports the
 *              24 V input as present, in millivolts. Range 0..40000 mV; effect:
 *              snapshot.input_present and BOOL__G__MeasInputPresent latch true.
 *              The presence flag is derived from the measured input voltage,
 *              not from the PB4 level: on the current schematic PB4 sits behind
 *              the 68K/6.8K divider, so between about 9 V and 23 V of input it
 *              lies inside the STM32 undefined input band and cannot be trusted.
 *         [FA] ولتاژ ورودی که در آن یا بالاتر، snapshot حضور ورودی ۲۴ ولت را
 *              true گزارش می‌کند، بر حسب میلی‌ولت. بازه 0..40000 mV؛ اثر:
 *              snapshot.input_present و BOOL__G__MeasInputPresent روی true قفل
 *              می‌شوند. پرچم حضور از ولتاژ اندازه‌گیری‌شده ورودی ساخته می‌شود،
 *              نه از سطح PB4: در شماتیک فعلی PB4 پشت تقسیم 68K/6.8K است و بین
 *              حدود 9V تا 23V ورودی داخل بازهٔ تعریف‌نشدهٔ ورودی STM32 می‌افتد
 *              و قابل اعتماد نیست.
 */
#define MEASUREMENT_INPUT_PRESENT_ON_MV   21000u

/**
 * @brief  [EN] Input voltage at or below which the shared snapshot reports the
 *              24 V input as absent, in millivolts. Range 0..40000 mV; effect:
 *              snapshot.input_present and BOOL__G__MeasInputPresent latch false.
 *              The 1 V gap to MEASUREMENT_INPUT_PRESENT_ON_MV is the hysteresis
 *              band that keeps a sagging input from chattering the flag.
 *         [FA] ولتاژ ورودی که در آن یا پایین‌تر، snapshot حضور ورودی ۲۴ ولت را
 *              false گزارش می‌کند، بر حسب میلی‌ولت. بازه 0..40000 mV؛ اثر:
 *              snapshot.input_present و BOOL__G__MeasInputPresent روی false قفل
 *              می‌شوند. فاصلهٔ ۱ ولتی با MEASUREMENT_INPUT_PRESENT_ON_MV بازهٔ
 *              هیسترزیس است و از پرپرزدن پرچم هنگام افت ورودی جلوگیری می‌کند.
 */
#define MEASUREMENT_INPUT_PRESENT_OFF_MV  20000u

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
uint32_t func__Measurement_CurrentCountsToMa(uint16_t uint16_t__counts);

#endif /* MEASUREMENT_H */
