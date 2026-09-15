/**
 * @file    ui_led.h
 * @brief   [EN] UI LED scenarios - green/red/yellow, battery percent, InputOk/Charging/BatteryRun.
 *          Part of UI split into LED and BUZZER per user request. RTOS simple readable, non-linear formulas.
 *          [FA] سناریوهای LED ماژول UI - سبز/قرمز/زرد، درصد باتری، سناریوهای ورودی/شارژ/دشارژ.
 *
 * @note    [EN] All thresholds in ui.h (single source). Naming __ after type, func__ prefix.
 *          RTOS: vTaskDelay allowed, HAL_Delay forbidden. Formulas broken into steps.
 *          [FA] همه آستانه‌ها در ui.h. نام‌گذاری با __، پیشوند func__.
 */

#ifndef UI_LED_H
#define UI_LED_H

/* ==================== Includes ==================== */

#include <stdint.h>
#include <stdbool.h>

/* ==================== Battery Voltage To Percent ==================== */

/**
 * @brief  [EN] Convert battery voltage to percent 0..100. Non-linear broken into steps: range, offset, scaled, percent.
 *         [FA] تبدیل ولتاژ باتری به درصد - غیرخطی ۴ گام.
 * @param  uint32_t__batteryMv [EN] Battery voltage in mV, range 0..40000mV, clamped / ولتاژ باتری میلی‌ولت
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
