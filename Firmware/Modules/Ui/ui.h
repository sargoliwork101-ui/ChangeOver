/**
 * @file    ui.h
 * @brief   [EN] LED and buzzer API: board test and blink scenarios.
 *          [FA] API ال‌ای‌دی و بازر: تست برد و سناریوهای چشمک.
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
 * @brief  [EN] Scenario 1 — green blink, red off. Does not return.
 *         [FA] سناریو ۱ — چشمک سبز، قرمز خاموش. برنمی‌گردد.
 */
void Ui_Scenario1(void);

/**
 * @brief  [EN] Scenario 2 — green long off, red blink. Does not return.
 *         [FA] سناریو ۲ — سبز با خاموشی بلند، قرمز چشمک. برنمی‌گردد.
 */
void Ui_Scenario2(void);

#endif /* UI_H */
