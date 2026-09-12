/**
 * @file    ui.c
 * @brief   [EN] LED/buzzer pin writes and blink scenarios.
 *          [FA] نوشتن پایه‌های LED/بازر و سناریوهای چشمک.
 */

#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>

/**
 * @brief  [EN] Green LED PB10, active-high via Q6.
 *         [FA] LED سبز PB10، active-high از طریق Q6.
 */
static void green(bool on)
{
    BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, on);
}

/**
 * @brief  [EN] Red LED PB0, active-high via Q4.
 *         [FA] LED قرمز PB0، active-high از طریق Q4.
 */
static void red(bool on)
{
    BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, on);
}

/**
 * @brief  [EN] Yellow LED PB1, active-high via Q5.
 *         [FA] LED زرد PB1، active-high از طریق Q5.
 */
static void yellow(bool on)
{
    BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, on);
}

/**
 * @brief  [EN] Buzzer PA4, active-high via Q7.
 *         [FA] بازر PA4، active-high از طریق Q7.
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

/**
 * @brief  [EN] All UI outputs off.
 *         [FA] همه خروجی‌های UI خاموش.
 */
void Ui_Init(void)
{
    all_off();
}

/**
 * @brief  [EN] One-shot R/Y/G then beep. Times come from APP_CONFIG.
 *         [FA] تست یک‌باره قرمز/زرد/سبز و بوق. زمان از APP_CONFIG است.
 */
void Ui_BoardTest(void)
{
    all_off();

    red(true);
    vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_selftest_led_ms));
    red(false);

    yellow(true);
    vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_selftest_led_ms));
    yellow(false);

    green(true);
    vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_selftest_led_ms));
    green(false);

    buzzer(true);
    vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_boot_beep_ms));
    buzzer(false);
}

/**
 * @brief  [EN] Green on/off forever. Red stays off.
 *         [FA] سبز روشن/خاموش برای همیشه. قرمز خاموش می‌ماند.
 */
void Ui_Scenario1(void)
{
    for (;;)
    {
        green(true);
        red(false);
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_scen1_on_ms));

        green(false);
        red(false);
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_scen1_off_ms));
    }
}

/**
 * @brief  [EN] Green on then long off; red toggles every red interval.
 *         [FA] سبز روشن بعد خاموشی بلند؛ قرمز هر بازه چشمک.
 *
 * @note   [EN] Green off time is two red intervals (500+500 = 1000 ms).
 *         [FA] خاموشی سبز برابر دو بازه قرمز است (۵۰۰+۵۰۰ = ۱۰۰۰ ms).
 */
void Ui_Scenario2(void)
{
    const uint32_t red_ms = APP_CONFIG.ui_scen2_red_ms;
    const uint32_t green_on_ms = APP_CONFIG.ui_scen2_green_on_ms;

    for (;;)
    {
        green(true);
        red(true);
        vTaskDelay(pdMS_TO_TICKS(green_on_ms));

        green(false);
        red(false);
        vTaskDelay(pdMS_TO_TICKS(red_ms));

        green(false);
        red(true);
        vTaskDelay(pdMS_TO_TICKS(red_ms));
    }
}
