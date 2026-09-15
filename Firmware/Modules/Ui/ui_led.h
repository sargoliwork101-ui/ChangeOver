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
#include <stdbool.h>

/* ==================== Battery voltage mapping ==================== */

#define UI_BAT_V_MIN_MV                 21000u  /* [EN] 0% = 21V / صفر درصد = ۲۱ ولت */
#define UI_BAT_V_MAX_MV                 28000u  /* [EN] 100% = 28V / فول = ۲۸ ولت */

/* ==================== Input voltage threshold ==================== */

#define UI_INPUT_THRESHOLD_MV           20000u  /* [EN] <20V = no input, >=20V = present / زیر ۲۰ ولت نداریم */

/* ==================== Blink / Poll timings ==================== */

#define UI_INPUT_OK_POLL_MS             500u    /* [EN] InputOk steady hold / سبز ثابت ورودی وصل */
#define UI_SELFTEST_LED_MS              500u    /* [EN] Board test LED step / گام تست برد */
#define UI_BLINK_PERIOD_MS              1000u   /* [EN] Green blink period BatteryRun / دوره چشمک سبز دشارژ */
#define UI_GREEN_MIN_OFF_MS             10u     /* [EN] Min off for green full / حداقل خاموشی سبز فول */
#define UI_CHARGING_BLINK_PERIOD_MS     1000u   /* [EN] Yellow blink period Charging / دوره چشمک زرد شارژ */
#define UI_CHARGING_YELLOW_MIN_OFF_MS   10u     /* [EN] Min off yellow almost full / حداقل خاموشی زرد */

/* ==================== Percent helpers ==================== */

#define UI_PERCENT_FULL                 100u
#define UI_PERCENT_SCALE                100u

/* ==================== RTOS tick ==================== */

#define UI_TICK_MS                      10u     /* [EN] Ui task tick 10ms, simple RTOS / تیکه ۱۰ میلی‌ثانیه */

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
 * @brief  [EN] One-shot wiring check: red, yellow, green, short beep using buzzer pattern, RTOS simple with vTaskDelay.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز، بوق با vTaskDelay ساده.
 */
void func__Ui_BoardTest_Start(void);

/* ==================== Board Test Tick ==================== */

/**
 * @brief  [EN] Board test tick - for compatibility, returns false (test done in Start).
 *         [FA] تیکه تست برد - برای سازگاری false برمی‌گرداند.
 * @return bool [EN] true=still running, false=finished / در حال اجرا یا تمام
 */
bool func__Ui_BoardTest_Tick(void);

/* ==================== Scenario InputOk ==================== */

/**
 * @brief  [EN] InputOk: green steady, others off. RTOS simple with vTaskDelay, MCU not locked.
 *         [FA] ورودی عادی: سبز ثابت، ساده RTOS.
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
 * @brief  [EN] BatteryRun: green blink non-linear (remainingPercent, periodPerPercent, greenOnMs/offMs), yellow OFF, smart beep.
 *         RTOS simple with vTaskDelay.
 *         [FA] دشارژ: سبز چشمک غیرخطی، زرد خاموش، بوق هوشمند، ساده RTOS.
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
