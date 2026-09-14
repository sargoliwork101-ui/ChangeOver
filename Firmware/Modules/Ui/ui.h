/**
 * @file    ui.h
 * @brief   [EN] LED/buzzer scenarios: InputOk, BatteryRun (with smart beep), Charging (yellow).
 *          [FA] سناریوهای LED/بازر: ورودی عادی، دشارژ باتری با بوق هوشمند، شارژ با زرد چشمک‌زن.
 *
 * @note    [EN] Each scenario runs ONE cycle and returns. Battery 0% = 21V, 100% = 28V (tunable on top of ui.c).
 *                Input present if V_in >= 20V. Yellow LED is OFF in BatteryRun stage, used only in Charging.
 *                Buzzer is a separate function Ui_BuzzerBeep() callable from any scenario.
 *          [FA] هر سناریو یک سیکل اجرا و برمی‌گردد. باتری صفر درصد = ۲۱ ولت، فول = ۲۸ ولت (بالای ui.c قابل تغییر).
 *                ورودی وصل اگر V_in >= ۲۰ ولت. LED زرد در مرحله دشارژ خاموش است، فقط در شارژ استفاده می‌شود.
 *                بازر تابع جدا Ui_BuzzerBeep دارد که در هر سناریو می‌توان صدا زد.
 */

#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  [EN] Drive all UI outputs low (safe state).
 *         [FA] همه خروجی‌های UI را خاموش می‌کند (حالت امن).
 */
void Ui_Init(void);

/**
 * @brief  [EN] One-shot wiring check: red, yellow, green, short beep. Returns.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز، بوق کوتاه. برمی‌گردد.
 */
void Ui_BoardTest(void);

/**
 * @brief  [EN] Separate buzzer function. Beeps for given duration.
 *         [FA] تابع جدا بازر. به مدت داده‌شده بوق می‌زند.
 * @param  u32_durationMs [EN] Beep length in ms / طول بوق به میلی‌ثانیه
 */
void Ui_BuzzerBeep(uint32_t u32_durationMs);

/**
 * @brief  [EN] Convert battery voltage (mV) to percent 0..100 using Vmin=21V (0%) and Vmax=28V (100%).
 *         Clamped. Tunable parameters on top of ui.c.
 *         [FA] تبدیل ولتاژ باتری به درصد ۰..۱۰۰ با Vmin=۲۱V صفر درصد و Vmax=۲۸V فول. محدود شده.
 * @param  u32_batteryMv [EN] Battery voltage in mV / ولتاژ باتری به میلی‌ولت
 * @return uint8_t 0..100
 */
uint8_t Ui_BatteryVoltageToPercent(uint32_t u32_batteryMv);

/**
 * @brief  [EN] Scenario Input Normal: steady green, others off. One cycle = ui_input_ok_poll_ms.
 *         [FA] سناریوی ورودی عادی: سبز ثابت، بقیه خاموش.
 */
void Ui_ScenarioInputOk(void);

/**
 * @brief  [EN] Scenario Battery Run (discharging): green blink, on-time = battery percent.
 *         Yellow OFF in this stage. Smart beep: if pct<50, beep every pct seconds (40%->40s, 30%->30s);
 *         if pct<20, beep duration x2. Uses separate buzzer function.
 *         [FA] سناریوی دشارژ باتری: چشمک سبز، روشن‌بودن برابر درصد باتری. زرد خاموش.
 *         بوق هوشمند: اگر درصد<۵۰ هر درصد ثانیه یک بوق (۴۰٪→هر ۴۰ ثانیه)؛ اگر <۲۰٪ طول بوق ۲ برابر.
 * @param  u32_batteryMv [EN] Battery voltage mV (21V=0%, 28V=100%) / ولتاژ باتری
 */
void Ui_ScenarioBatteryRun(uint32_t u32_batteryMv);

/**
 * @brief  [EN] Scenario Charging: yellow indicates remaining to full. 0% = yellow steady ON, 100% = OFF,
 *         intermediate = blink where ON = (100-pct)*period. Green steady ON (input present). One cycle.
 *         [FA] سناریوی شارژ: زرد نشانگر مانده تا فول. ۰٪ زرد ثابت روشن، ۱۰۰٪ خاموش، بینشان چشمک با ON=(۱۰۰-درصد)*دوره.
 *         سبز ثابت روشن (ورودی وصل).
 * @param  u32_batteryMv [EN] Battery voltage mV / ولتاژ باتری
 */
void Ui_ScenarioCharging(uint32_t u32_batteryMv);

#endif /* UI_H */
