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

/* ==================== Includes ==================== */

#include <stdint.h>

/* ==================== Battery voltage mapping constants ==================== */

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

/* ==================== Input voltage threshold ==================== */

/**
 * @brief  [EN] Minimum input voltage considered present, in millivolts.
 *         Values below this threshold select the BatteryRun scenario.
 *         [FA] کمترین ولتاژ ورودی که متصل در نظر گرفته می‌شود، بر حسب میلی‌ولت.
 *         مقدار کمتر از این آستانه سناریوی BatteryRun را انتخاب می‌کند.
 */
#define UI_INPUT_THRESHOLD_MV           21000u

/* ==================== Scenario timing constants ==================== */

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

/* ==================== Percentage constants ==================== */

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

/* ==================== RTOS tick ==================== */

/**
 * @brief  [EN] Base UI task delay in milliseconds.
 *         [FA] تأخیر پایه تسک UI بر حسب میلی‌ثانیه.
 */
#define UI_TICK_MS                      10u

/* ==================== Battery Voltage To Percent ==================== */

/**
 * @brief  [EN] Convert battery voltage to percent 0..100. Non-linear broken into steps: range, offset, scaled, percent.
 *         [FA] تبدیل ولتاژ باتری به درصد - غیرخطی ۴ گام.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, range 0..40000mV, clamped / ولتاژ باتری میلی‌ولت
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func__Ui_BatteryVoltageToPercent(uint32_t uint32_t__batteryMv);

/* ==================== Ui Init ==================== */

/**
 * @brief  [EN] Drive all UI outputs low (safe state).
 *         [FA] همه خروجی‌های UI خاموش (حالت امن).
 */
void func__Ui_Init(void);

/* ==================== Board Test Start ==================== */

/**
 * @brief  [EN] One-shot wiring check: red, yellow, green and the previous short buzzer check via the independent buzzer service.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز و بوق کوتاه قبلی با سرویس مستقل بوق.
 */
void func__Ui_BoardTest_Start(void);

/* ==================== Scenario InputOk ==================== */

/**
 * @brief  [EN] InputOk: green steady, red/yellow/buzzer off. RTOS simple with vTaskDelay, MCU not locked.
 *         [FA] ورودی عادی: سبز ثابت، قرمز/زرد/بوق خاموش، ساده RTOS.
 */
void func__Ui_ScenarioInputOk(void);

/* ==================== Scenario Charging Tick ==================== */

/**
 * @brief  [EN] Charging: green steady, yellow remaining to full non-linear (remainingPercent, periodPerPercent, yellowOnMs/offMs).
 *         RTOS simple with vTaskDelay.
 *         [FA] شارژ: سبز ثابت، زرد مانده تا فول غیرخطی، ساده RTOS.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV / ولتاژ باتری
 */
void func__Ui_ScenarioCharging_Tick(uint32_t uint32_t__batteryMv);

/* ==================== Scenario BatteryRun Tick ==================== */

/**
 * @brief  [EN] BatteryRun: green blink non-linear, yellow OFF, and the previous smart-beep schedule through ui_buzzer.c.
 *         RTOS simple with vTaskDelay.
 *         [FA] دشارژ: سبز چشمک غیرخطی، زرد خاموش و زمان‌بندی بوق هوشمند قبلی از ui_buzzer.c.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, 21000=0% 28000=100% / ولتاژ باتری
 */
void func__Ui_ScenarioBatteryRun_Tick(uint32_t uint32_t__batteryMv);

/* ==================== Ui Tick ==================== */

/**
 * @brief  [EN] Ui main tick - decides which scenario based on input and battery, RTOS simple readable.
 *         Call every UI_TICK_MS from task.
 *         [FA] تیکه اصلی UI - تصمیم سناریو بر اساس ورودی و باتری، ساده خوانا.
 * @param  uint32_t__inputVoltageMv [EN] Input voltage mV / ولتاژ ورودی
 * @param  uint32_t__batteryVoltageMv [EN] Battery voltage mV / ولتاژ باتری
 */
void func__Ui_Tick(uint32_t uint32_t__inputVoltageMv, uint32_t uint32_t__batteryVoltageMv);

#endif /* UI_LED_H */
