/**
 * @file    ui.h
 * @brief   [EN] LED/buzzer status indication: input normal, battery run, low battery.
 *          [FA] نمایش وضعیت با LED/بازر: ورودی عادی، دشارژ باتری، باتری ضعیف.
 */

#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief [EN] Operating mode shown on the LEDs/buzzer.
 *        [FA] حالتی که روی LED/بازر نمایش داده می‌شود.
 */
typedef enum
{
    UI_INPUT_OK = 0,  /**< [EN] Mains input present, batteries not on the load / ورودی وصل */
    UI_BATTERY_RUN,   /**< [EN] Input lost, battery discharging, above low limit / دشارژ باتری */
    UI_BATTERY_LOW    /**< [EN] Battery at or below the low limit / باتری در حد ضعیف */
} ui_state_t;

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
 * @brief  [EN] Non-blocking indication step. Call periodically (ui_task_period_ms).
 *         [FA] گام نمایش غیرمسدودکننده. به صورت تناوبی (ui_task_period_ms) صدا زده شود.
 * @param  input_present   [EN] true = mains input connected / برق ورودی وصل
 * @param  battery_percent [EN] Battery charge 0..100 / درصد شارژ باتری (۰ تا ۱۰۰)
 */
void Ui_Indicate(bool input_present, uint8_t battery_percent);

#endif /* UI_H */
