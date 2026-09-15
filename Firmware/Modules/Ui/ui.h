/**
 * @file    ui.h
 * @brief   [EN] LED/buzzer scenarios: InputOk, BatteryRun (smart beep), Charging (yellow). All our functions have func_ prefix per AI rule.
 *          [FA] سناریوهای LED/بازر: ورودی عادی، دشارژ با بوق هوشمند، شارژ با زرد. همه توابع خودمان با پیشوند func_ طبق قانون AI.
 *
 * @note    [EN] Battery 0% = 21V (21000mV) = 0%, 100% = 28V (28000mV). Input present if V_in >=20V. Thresholds in ui_config.h single file.
 *          [FA] باتری صفر درصد ۲۱V، فول ۲۸V. ورودی وصل اگر >=۲۰V. آستانه‌ها در ui_config.h.
 */

#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  [EN] Drive all UI outputs low (safe state).
 *         [FA] همه خروجی‌های UI خاموش (حالت امن).
 */
void func_Ui_Init(void);

/**
 * @brief  [EN] One-shot wiring check: red, yellow, green, short beep.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز، بوق.
 */
void func_Ui_BoardTest(void);

/**
 * @brief  [EN] Separate buzzer beep - callable from any scenario.
 *         [FA] تابع جدا بازر - از هر سناریو قابل صدا زدن.
 * @param  uint32_t_durationMs [EN] Beep duration in ms, 0=use base 250ms. Range 0..5000ms / طول بوق میلی‌ثانیه
 */
void func_Ui_BuzzerBeep(uint32_t uint32_t_durationMs);

/**
 * @brief  [EN] Convert battery voltage to percent 0..100. 0%=21V 100%=28V.
 *         [FA] تبدیل ولتاژ باتری به درصد.
 * @param  uint32_t_batteryMv [EN] Battery voltage in mV, range 0..40000mV, clamped via Vmin/Vmax / ولتاژ باتری میلی‌ولت
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func_Ui_BatteryVoltageToPercent(uint32_t uint32_t_batteryMv);

/**
 * @brief  [EN] Scenario InputOk: green steady, others off. One cycle = 500ms.
 *         [FA] سناریوی ورودی عادی: سبز ثابت.
 */
void func_Ui_ScenarioInputOk(void);

/**
 * @brief  [EN] Scenario BatteryRun: green blink, yellow OFF, smart beep.
 *         [FA] سناریوی دشارژ: سبز چشمک، زرد خاموش، بوق هوشمند.
 * @param  uint32_t_batteryMv [EN] Battery voltage mV, 21000=0% 28000=100%, range 21000..28000 / ولتاژ باتری
 */
void func_Ui_ScenarioBatteryRun(uint32_t uint32_t_batteryMv);

/**
 * @brief  [EN] Scenario Charging: green steady, yellow remaining to full. 0%=ON 100%=OFF.
 *         [FA] سناریوی شارژ: سبز ثابت، زرد مانده تا فول.
 * @param  uint32_t_batteryMv [EN] Battery voltage mV, 21000=0% ON, 28000=100% OFF, range 21000..28000 / ولتاژ باتری
 */
void func_Ui_ScenarioCharging(uint32_t uint32_t_batteryMv);

#endif /* UI_H */
