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
 * @brief  [EN] Green 500 ms on / 500 ms off forever. Red stays off.
 *         [FA] سبز ۵۰۰ روشن / ۵۰۰ خاموش برای همیشه. قرمز خاموش می‌ماند.
 */
void Ui_Scenario1(void)
{
    for (;;)  /* repeat forever; this task never returns */
    {
        green(true);   /* PB10 HIGH: green LED on */
        red(false);    /* PB0 LOW: red LED off */
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_scen1_on_ms));  /* sleep 500 ms; other tasks can run */

        green(false);  /* PB10 LOW: green LED off */
        red(false);    /* keep red off */
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_scen1_off_ms)); /* sleep 500 ms, then loop */
    }
}

/**
 * @brief  [EN] Green 500 on / 1000 off, red blinks every 500 ms.
 *         [FA] سبز ۵۰۰ روشن / ۱۰۰۰ خاموش، قرمز هر ۵۰۰ چشمک.
 */
void Ui_Scenario2(void)
{
    for (;;)  /* repeat forever; this task never returns */
    {
        green(true);   /* green on */
        red(true);     /* red on */
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_scen2_green_on_ms)); /* 500 ms */

        green(false);  /* green off (stays off for the next two waits = 1000 ms) */
        red(false);    /* red off */
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_scen2_red_ms)); /* 500 ms */

        green(false);  /* green still off */
        red(true);     /* red on again = 500 ms blink */
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_scen2_red_ms)); /* 500 ms, then loop */
    }
}
