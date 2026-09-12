/**
 * @file    ui.h
 * @brief   [EN] LED and buzzer API: board test and blink scenarios.
 *          [FA] API ال‌ای‌دی و بازر: تست برد و سناریوهای چشمک.
 *
 * @stage   Active
 */

#ifndef UI_H
#define UI_H

/**
 * @brief  [EN] Drive all UI outputs low.
 *         [FA] همه خروجی‌های UI را خاموش می‌کند.
 */
void Ui_Init(void);

/**
 * @brief  [EN] One-shot wiring check: red, yellow, green, short beep. Returns.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز، بوق کوتاه. برمی‌گردد.
 */
void Ui_BoardTest(void);

/**
 * @brief  [EN] Scenario 1 — green 500 ms on / 500 ms off. Does not return.
 *         [FA] سناریو ۱ — سبز ۵۰۰ روشن / ۵۰۰ خاموش. برنمی‌گردد.
 */
void Ui_Scenario1(void);

/**
 * @brief  [EN] Scenario 2 — green 500/1000, red 500 blink. Does not return.
 *         [FA] سناریو ۲ — سبز ۵۰۰/۱۰۰۰، قرمز هر ۵۰۰ چشمک. برنمی‌گردد.
 */
void Ui_Scenario2(void);

#endif /* UI_H */
