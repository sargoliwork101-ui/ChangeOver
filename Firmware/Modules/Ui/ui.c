#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>

static void green(bool on)
{
    BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, on);
}

static void red(bool on)
{
    BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, on);
}

static void yellow(bool on)
{
    BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, on);
}

static void buzzer(bool on)
{
    BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, on);
}

static void all_off(void)
{
    green(false);
    red(false);
    yellow(false);
    buzzer(false);
}

void Ui_Init(void)
{
    all_off();
}

/* یک‌بار اجرا می‌شود و برمی‌گردد. حلقه ندارد. */
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
