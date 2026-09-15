/**
 * @file    ui_led.h
 * @brief   [EN] UI LED scenarios - green/red/yellow, battery percent, InputOk/Charging/BatteryRun.
 *          Split from UI into LED and BUZZER per user request. Constants for LED in its own header.
 *          RTOS simple readable, non-linear formulas, markers above each function in h and c.
 *          [FA] سناریوهای LED ماژول UI - ثابت‌های LED در هدر خودش، فرمول غیرخطی، RTOS ساده.
 *
 * @note    [EN] LED defaults live in this header; app_config.c copies them into const APP_CONFIG, and runtime logic reads APP_CONFIG. Naming __ after type, func__ prefix.
 *          RTOS: vTaskDelay allowed, HAL_Delay forbidden. Formulas broken into steps.
 *          [FA] پیش‌فرض‌های LED در این هدر هستند؛ app_config.c آن‌ها را به APP_CONFIG ثابت منتقل می‌کند و منطق زمان اجرا از APP_CONFIG می‌خواند.
 */

#ifndef UI_LED_H
#define UI_LED_H

/* ==================== Includes / شامل‌ها ==================== */

#include <stdint.h>

/* ==================== Battery voltage mapping constants / ثابت‌های نگاشت ولتاژ باتری ==================== */

/**
 * @brief  [EN] Battery voltage mapped to zero percent, in millivolts.
 *         [FA] ولتاژ باتری متناظر با صفر درصد، بر حسب میلی‌ولت.
 */
#define UI_BAT_V_MIN_MV                 21000u

/**
 * @brief  [EN] Battery voltage mapped to one hundred percent, in millivolts.
 *         [FA] ولتاژ باتری متناظر با صد درصد، بر حسب میلی‌ولت.
 */
#define UI_BAT_V_MAX_MV                 28000u

/* ==================== Input voltage thresholds and hysteresis / آستانه‌ها و هیسترزیس ولتاژ ورودی ==================== */

/**
 * @brief  [EN] Input voltage at or above which the input is considered connected, in millivolts.
 *         [FA] ولتاژ ورودی که از آن به بعد ورودی متصل در نظر گرفته می‌شود، بر حسب میلی‌ولت.
 */
#define UI_INPUT_CONNECTED_THRESHOLD_MV 21000u

/**
 * @brief  [EN] Hysteresis band for input connected/disconnected detection, in millivolts.
 *         [FA] پهنای هیسترزیس تشخیص وصل/قطع ورودی، بر حسب میلی‌ولت.
 */
#define UI_INPUT_HYSTERESIS_MV          1000u

/**
 * @brief  [EN] Input voltage at or below which the input is considered disconnected, in millivolts.
 *         Derived from the connected threshold minus the hysteresis band.
 *         [FA] ولتاژ ورودی که از آن به پایین ورودی قطع در نظر گرفته می‌شود، بر حسب میلی‌ولت.
 *         از کم‌کردن هیسترزیس از آستانه وصل به دست می‌آید.
 */
#define UI_INPUT_DISCONNECTED_THRESHOLD_MV \
    (UI_INPUT_CONNECTED_THRESHOLD_MV - UI_INPUT_HYSTERESIS_MV)

/* ==================== Input overvoltage error constants / ثابت‌های خطای اضافه‌ولتاژ ورودی ==================== */

/**
 * @brief  [EN] Input voltage above which the overvoltage error is activated, in millivolts.
 *         The comparison is strict: values greater than this threshold activate the error.
 *         [FA] ولتاژ ورودی که بیشتر از آن خطای اضافه‌ولتاژ فعال می‌شود، بر حسب میلی‌ولت.
 *         مقایسه strict است؛ مقدار بزرگ‌تر از این آستانه خطا را فعال می‌کند.
 */
#define UI_INPUT_OVERVOLTAGE_THRESHOLD_MV 28000u

/**
 * @brief  [EN] Hysteresis band used to clear the input overvoltage error, in millivolts.
 *         [FA] پهنای هیسترزیس پاک‌کردن خطای اضافه‌ولتاژ ورودی، بر حسب میلی‌ولت.
 */
#define UI_INPUT_OVERVOLTAGE_HYSTERESIS_MV 1000u

/**
 * @brief  [EN] Input voltage at or below which the overvoltage error is cleared, in millivolts.
 *         Derived from the overvoltage threshold minus its hysteresis band.
 *         [FA] ولتاژ ورودی که از آن به پایین خطای اضافه‌ولتاژ پاک می‌شود، بر حسب میلی‌ولت.
 *         از کم‌کردن هیسترزیس خطا از آستانه اضافه‌ولتاژ به دست می‌آید.
 */
#define UI_INPUT_OVERVOLTAGE_CLEAR_THRESHOLD_MV \
    (UI_INPUT_OVERVOLTAGE_THRESHOLD_MV - UI_INPUT_OVERVOLTAGE_HYSTERESIS_MV)

/* ==================== Scenario timing constants / ثابت‌های زمانی سناریوها ==================== */

/**
 * @brief  [EN] Red LED period while the input overvoltage error is displayed, in milliseconds.
 *         [FA] دوره چشمک LED قرمز هنگام نمایش خطای اضافه‌ولتاژ ورودی، بر حسب میلی‌ثانیه.
 */
#define UI_INPUT_OVERVOLTAGE_LED_PERIOD_MS 1000u

/**
 * @brief  [EN] Red LED duty while the input overvoltage error is displayed, in percent.
 *         [FA] دیوتی چشمک LED قرمز هنگام نمایش خطای اضافه‌ولتاژ ورودی، بر حسب درصد.
 */
#define UI_INPUT_OVERVOLTAGE_LED_DUTY_PERCENT 50u

/**
 * @brief  [EN] Buzzer pattern period for the input overvoltage error, in milliseconds.
 *         [FA] دوره الگوی بوق خطای اضافه‌ولتاژ ورودی، بر حسب میلی‌ثانیه.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_PERIOD_MS 10000u

/**
 * @brief  [EN] Buzzer ON duration for one input overvoltage warning, in milliseconds.
 *         [FA] مدت روشن‌بودن بوق در هر هشدار اضافه‌ولتاژ ورودی، بر حسب میلی‌ثانیه.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_DURATION_MS 1000u

/**
 * @brief  [EN] Buzzer duty derived from the overvoltage warning ON duration and period.
 *         [FA] دیوتی بوق که از مدت روشن‌بودن و دوره هشدار اضافه‌ولتاژ به دست می‌آید.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_DUTY_PERCENT \
    ((UI_INPUT_OVERVOLTAGE_BEEP_DURATION_MS * UI_PERCENT_SCALE) / UI_INPUT_OVERVOLTAGE_BEEP_PERIOD_MS)

/**
 * @brief  [EN] Number of buzzer pulses in one input overvoltage warning pattern.
 *         [FA] تعداد پالس بوق در الگوی هشدار اضافه‌ولتاژ ورودی.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_COUNT 1u

/**
 * @brief  [EN] Gap between adjacent overvoltage pulses; one pulse does not use a gap.
 *         [FA] گپ بین پالس‌های اضافه‌ولتاژ؛ یک پالس گپ ندارد.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_GAP_MS 0u

/**
 * @brief  [EN] Delay used while InputOk holds the green LED steady.
 *         [FA] تأخیر سناریوی InputOk هنگام ثابت نگه‌داشتن LED سبز.
 */
#define UI_INPUT_OK_POLL_MS             500u

/**
 * @brief  [EN] Duration of each LED step in the one-shot BoardTest.
 *         [FA] مدت هر مرحله LED در تست یک‌باره برد.
 */
#define UI_SELFTEST_LED_MS              500u

/**
 * @brief  [EN] Complete green blink period used by BatteryRun, in milliseconds.
 *         [FA] دوره کامل چشمک سبز در BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BLINK_PERIOD_MS              1000u

/**
 * @brief  [EN] Minimum green LED OFF time used to keep the full-battery blink visible.
 *         [FA] کمترین زمان خاموشی LED سبز برای قابل‌مشاهده ماندن چشمک باتری فول.
 */
#define UI_GREEN_MIN_OFF_MS             10u

/**
 * @brief  [EN] Complete yellow blink period used by Charging, in milliseconds.
 *         [FA] دوره کامل چشمک زرد در Charging، بر حسب میلی‌ثانیه.
 */
#define UI_CHARGING_BLINK_PERIOD_MS     1000u

/**
 * @brief  [EN] Minimum yellow LED OFF time near full charge, in milliseconds.
 *         [FA] کمترین زمان خاموشی LED زرد نزدیک شارژ کامل، بر حسب میلی‌ثانیه.
 */
#define UI_CHARGING_YELLOW_MIN_OFF_MS   10u

/* ==================== BatteryRun warning constants / ثابت‌های هشدار BatteryRun ==================== */

/**
 * @brief  [EN] Battery percentage below which the standard BatteryRun warning beep is enabled.
 *         [FA] درصد باتری که پایین‌تر از آن بوق هشدار معمول BatteryRun فعال می‌شود.
 */
#define UI_BATTERY_RUN_BEEP_START_PERCENT 40u

/**
 * @brief  [EN] Battery percentage below which the standard warning uses two beeps.
 *         [FA] درصد باتری که پایین‌تر از آن هشدار معمول با دو بوق اجرا می‌شود.
 */
#define UI_BATTERY_RUN_BEEP_DOUBLE_PERCENT 20u

/**
 * @brief  [EN] Battery percentage below which the warning uses three beeps every 20 seconds.
 *         [FA] درصد باتری که پایین‌تر از آن هشدار با سه بوق هر ۲۰ ثانیه اجرا می‌شود.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_PERCENT 10u

/**
 * @brief  [EN] Battery percentage below which the critical ten-second beep is used.
 *         [FA] درصد باتری که پایین‌تر از آن بوق بحرانی ده‌ثانیه‌ای اجرا می‌شود.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_PERCENT 1u

/**
 * @brief  [EN] Interval shared by the one-beep and two-beep BatteryRun warnings, in milliseconds.
 *         [FA] فاصله مشترک هشدار یک‌بوق و دو‌بوق BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS 60000u

/**
 * @brief  [EN] Interval of the three-beep BatteryRun warning, in milliseconds.
 *         [FA] فاصله هشدار سه‌بوق BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS 20000u

/**
 * @brief  [EN] Complete period used for the single critical ten-second beep, in milliseconds.
 *         [FA] دوره کامل بوق بحرانی ده‌ثانیه‌ای، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_PERIOD_MS 10000u

/**
 * @brief  [EN] Duty of the continuous critical BatteryRun beep, in percent.
 *         [FA] دیوتی بوق ممتد بحرانی BatteryRun، بر حسب درصد.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_DUTY_PERCENT 100u

/**
 * @brief  [EN] Pulse count of the critical BatteryRun beep.
 *         [FA] تعداد پالس بوق بحرانی BatteryRun.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_COUNT 1u

/**
 * @brief  [EN] Duration of each one-beep or two-beep BatteryRun pulse, in milliseconds.
 *         [FA] مدت هر بوق در هشدار یک‌بوق یا دو‌بوق BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS 1000u

/**
 * @brief  [EN] Approximate duty for one standard beep, derived from duration and interval.
 *         [FA] دیوتی تقریبی یک بوق معمول که از مدت و فاصله محاسبه می‌شود.
 */
#define UI_BATTERY_RUN_BEEP_STANDARD_DUTY_PERCENT \
    ((UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS * UI_PERCENT_SCALE + UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS - 1u) / UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS)

/**
 * @brief  [EN] Approximate duty for two standard-duration beeps and their gap.
 *         [FA] دیوتی تقریبی دو بوق معمول به‌همراه گپ بین آن‌ها.
 */
#define UI_BATTERY_RUN_BEEP_DOUBLE_DUTY_PERCENT \
    ((((UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS * UI_BATTERY_RUN_BEEP_DOUBLE_COUNT) + \
       (UI_BATTERY_RUN_BEEP_GAP_MS * (UI_BATTERY_RUN_BEEP_DOUBLE_COUNT - 1u))) * \
      UI_PERCENT_SCALE + UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS - 1u) / \
     UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS)

/**
 * @brief  [EN] Approximate duty for three triple-duration beeps and their gaps.
 *         [FA] دیوتی تقریبی سه بوق مدت‌دار به‌همراه گپ‌های بین آن‌ها.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_DUTY_PERCENT \
    ((((UI_BATTERY_RUN_BEEP_TRIPLE_DURATION_MS * UI_BATTERY_RUN_BEEP_TRIPLE_COUNT) + \
       (UI_BATTERY_RUN_BEEP_GAP_MS * (UI_BATTERY_RUN_BEEP_TRIPLE_COUNT - 1u))) * \
      UI_PERCENT_SCALE + UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS - 1u) / \
     UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS)

/**
 * @brief  [EN] Duration of each pulse in the three-beep BatteryRun warning, in milliseconds.
 *         [FA] مدت هر بوق در هشدار سه‌بوق BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_DURATION_MS 2000u

/**
 * @brief  [EN] Duration of the single critical BatteryRun beep, in milliseconds.
 *         [FA] مدت بوق بحرانی تک‌باره BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_DURATION_MS 10000u

/**
 * @brief  [EN] Pulse count for the standard BatteryRun warning.
 *         [FA] تعداد پالس در هشدار معمول BatteryRun.
 */
#define UI_BATTERY_RUN_BEEP_STANDARD_COUNT 1u

/**
 * @brief  [EN] Pulse count for the low-battery BatteryRun warning.
 *         [FA] تعداد پالس در هشدار باتری پایین BatteryRun.
 */
#define UI_BATTERY_RUN_BEEP_DOUBLE_COUNT 2u

/**
 * @brief  [EN] Pulse count for the critical BatteryRun warning above the empty threshold.
 *         [FA] تعداد پالس در هشدار بحرانی BatteryRun بالاتر از آستانه خالی.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_COUNT 3u

/**
 * @brief  [EN] Low gap between adjacent BatteryRun warning pulses, in milliseconds.
 *         [FA] فاصله خاموش بین بوق‌های متوالی BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_GAP_MS 100u

/* ==================== Percentage constants / ثابت‌های درصد ==================== */

/**
 * @brief  [EN] Full battery percentage and upper bound of percentage calculations.
 *         [FA] درصد شارژ کامل و حد بالای محاسبات درصد.
 */
#define UI_PERCENT_FULL                 100u

/**
 * @brief  [EN] Scale used to convert a ratio into a percentage.
 *         [FA] مقیاس تبدیل نسبت به درصد.
 */
#define UI_PERCENT_SCALE                100u

/* ==================== RTOS tick / تیک RTOS ==================== */

/**
 * @brief  [EN] Base UI task delay in milliseconds.
 *         [FA] تأخیر پایه تسک UI بر حسب میلی‌ثانیه.
 */
#define UI_TICK_MS                      10u

/* ==================== Battery Voltage To Percent / تبدیل ولتاژ باتری به درصد ==================== */

/**
 * @brief  [EN] Convert battery voltage to percent 0..100. Non-linear broken into steps: range, offset, scaled, percent.
 *         [FA] تبدیل ولتاژ باتری به درصد - غیرخطی ۴ گام.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, range 0..40000mV, clamped / ولتاژ باتری میلی‌ولت
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func__Ui_BatteryVoltageToPercent(uint32_t uint32_t__batteryMv);

/* ==================== Ui Init / مقداردهی اولیه UI ==================== */

/**
 * @brief  [EN] Drive all UI outputs low (safe state).
 *         [FA] همه خروجی‌های UI خاموش (حالت امن).
 */
void func__Ui_Init(void);

/* ==================== Board Test Start / شروع تست برد ==================== */

/**
 * @brief  [EN] One-shot wiring check: red, yellow, green and the previous short buzzer check via the independent buzzer service.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز و بوق کوتاه قبلی با سرویس مستقل بوق.
 */
void func__Ui_BoardTest_Start(void);

/* ==================== Scenario InputOk / سناریوی ورودی عادی ==================== */

/**
 * @brief  [EN] InputOk: green steady, red/yellow/buzzer off. RTOS simple with vTaskDelay, MCU not locked.
 *         [FA] ورودی عادی: سبز ثابت، قرمز/زرد/بوق خاموش، ساده RTOS.
 */
void func__Ui_ScenarioInputOk(void);

/* ==================== Scenario Charging Tick / تیک سناریوی شارژ ==================== */

/**
 * @brief  [EN] Charging: green steady, yellow remaining to full non-linear (remainingPercent, periodPerPercent, yellowOnMs/offMs).
 *         RTOS simple with vTaskDelay.
 *         [FA] شارژ: سبز ثابت، زرد مانده تا فول غیرخطی، ساده RTOS.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV / ولتاژ باتری
 */
void func__Ui_ScenarioCharging_Tick(uint32_t uint32_t__batteryMv);

/* ==================== Scenario BatteryRun Tick / تیک سناریوی دشارژ ==================== */

/**
 * @brief  [EN] BatteryRun: green blink from the linear 21V..28V percentage and four requested buzzer bands.
 *         Below 1%, all LEDs turn off after one ten-second critical beep.
 *         [FA] دشارژ: سبز بر اساس درصد خطی ۲۱ تا ۲۸ ولت و چهار بازه بوق درخواستی چشمک می‌زند.
 *         زیر ۱٪، بعد از یک بوق بحرانی ده‌ثانیه‌ای همه LEDها خاموش می‌شوند.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, 21000=0% 28000=100% / ولتاژ باتری
 */
void func__Ui_ScenarioBatteryRun_Tick(uint32_t uint32_t__batteryMv);

/* ==================== Ui Tick / تیک اصلی UI ==================== */

/**
 * @brief  [EN] Ui main tick - decides which scenario based on input and battery, RTOS simple readable.
 *         Call every UI_TICK_MS from task.
 *         [FA] تیکه اصلی UI - تصمیم سناریو بر اساس ورودی و باتری، ساده خوانا.
 * @param  uint32_t__inputVoltageMv [EN] Input voltage mV / ولتاژ ورودی
 * @param  uint32_t__batteryVoltageMv [EN] Battery voltage mV / ولتاژ باتری
 */
void func__Ui_Tick(uint32_t uint32_t__inputVoltageMv, uint32_t uint32_t__batteryVoltageMv);

#endif /* UI_LED_H */
