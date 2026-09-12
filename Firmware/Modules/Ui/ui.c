/**
 * @file    ui.c
 * @brief   [EN] LED/buzzer pin writes and blink scenarios.
 *          [FA] نوشتن پایه‌های LED/بازر و سناریوهای چشمک.
 *
 * @details
 *   [EN] Patterns live here. vTaskDelay is allowed because these functions
 *        run in TaskUi context.
 *   [FA] الگو این‌جاست. vTaskDelay مجاز است چون از داخل TaskUi صدا می‌شود.
 *
 * @stage   Active
 */

#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>

/**
 * @brief  [EN] Green LED on PB10 (active high via Q6).
 *         [FA] LED سبز PB10 (active-high از طریق Q6).
 */
static void green(bool on)
{
    BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, on);
}

/**
 * @brief  [EN] Red LED on PB0 (active high via Q4).
 *         [FA] LED قرمز PB0 (active-high از طریق Q4).
 */
static void red(bool on)
{
    BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, on);
}

/**
 * @brief  [EN] Yellow LED on PB1 (active high via Q5).
 *         [FA] LED زرد PB1 (active-high از طریق Q5).
 */
static void yellow(bool on)
{
    BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, on);
}

/**
 * @brief  [EN] Buzzer on PA4 (active high via Q7).
 *         [FA] بازر PA4 (active-high از طریق Q7).
 */
static void buzzer(bool on)
{
    BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, on);
}

/**
 * @brief  [EN] All UI outputs off.
 *         [FA] همه خروجی‌های UI خاموش.
 */
static void all_off(void)
{
    green(false);
    red(false);
    yellow(false);
    buzzer(false);
}

/** @brief  [EN] All UI outputs off. [FA] همه خروجی UI خاموش. */
void Ui_Init(void)
{
    all_off();
}

/** @brief  [EN] One-shot R/Y/G then beep. [FA] تست یک‌باره قرمز/زرد/سبز و بوق. */
void Ui_BoardTest(void)
{
    all_off();

    red(true);
    vTaskDelay(pdMS_TO_TICKS(500u));
    red(false);

    yellow(true);
    vTaskDelay(pdMS_TO_TICKS(500u));
    yellow(false);

    green(true);
    vTaskDelay(pdMS_TO_TICKS(500u));
    green(false);

    buzzer(true);
    vTaskDelay(pdMS_TO_TICKS(150u));
    buzzer(false);
}

/** @brief  [EN] Green 500/500 forever. [FA] سبز ۵۰۰/۵۰۰ برای همیشه. */
void Ui_Scenario1(void)
{
    for (;;)
    {
        green(true);
        red(false);
        vTaskDelay(pdMS_TO_TICKS(500u));

        green(false);
        red(false);
        vTaskDelay(pdMS_TO_TICKS(500u));
    }
}

void Ui_Scenario2(void)
{
    for (;;)
    {
        green(true);
        red(true);
        vTaskDelay(pdMS_TO_TICKS(500u));

        green(false);
        red(false);
        vTaskDelay(pdMS_TO_TICKS(500u));

        green(false);
        red(true);
        vTaskDelay(pdMS_TO_TICKS(500u));
    }
}
