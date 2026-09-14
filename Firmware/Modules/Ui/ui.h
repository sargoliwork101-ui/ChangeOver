/**
 * @file    ui.h
 * @brief   [EN] LED/buzzer scenarios: input normal, battery run, battery low.
 *          [FA] سناریوهای LED/بازر: ورودی عادی، دشارژ باتری، باتری ضعیف.
 *
 * @note    [EN] Each scenario function runs ONE cycle and returns, so the task
 *                can re-check the test inputs and switch between scenarios.
 *          [FA] هر تابع سناریو فقط یک سیکل اجرا می‌شود و برمی‌گردد تا تسک بتواند
 *                ورودی‌های تست را دوباره بخواند و بین سناریوها سوییچ کند.
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
 * @brief  [EN] Scenario "Input Normal": one cycle of steady green. Everything
 *               else is forced off. Cycle length: ui_input_ok_poll_ms.
 *         [FA] سناریوی «ورودی عادی»: یک سیکل سبز ثابت؛ بقیه خاموش.
 */
void Ui_ScenarioInputOk(void);

/**
 * @brief  [EN] Scenario "Battery Run": one 1 s blink cycle. On-time follows the
 *               battery percent (full = 99 % on, 1 % = ~1 % on).
 *         [FA] سناریوی «دشارژ باتری»: یک سیکل چشمک ۱ ثانیه‌ای؛ زمان روشن‌بودن
 *               برابر درصد باتری است.
 * @param  battery_percent [EN] 0..100 charge / درصد شارژ باتری
 */
void Ui_ScenarioBatteryRun(uint8_t battery_percent);

/**
 * @brief  [EN] Scenario "Battery Low": one 1 s cycle of yellow 500/500 blink;
 *               a 250 ms beep is added on the first cycle of every 30 s window.
 *               Keeps its own cycle counter, so call it every time while low.
 *         [FA] سناریوی «باتری ضعیف»: یک سیکل ۱ ثانیه‌ای زرد ۵۰۰/۵۰۰؛ در ابتدای
 *               هر پنجره ۳۰ ثانیه‌ای یک بوق ۲۵۰ms اضافه می‌شود.
 */
void Ui_ScenarioBatteryLow(void);

#endif /* UI_H */
