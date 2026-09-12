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

void Ui_Init(void)
{
    green(false);
    red(false);
}

/* ========== کد ۱ ========== */
void Ui_Scenario1(void)
{
    green(true);
    red(false);
    vTaskDelay(pdMS_TO_TICKS(500u));

    green(false);
    red(false);
    vTaskDelay(pdMS_TO_TICKS(500u));
}

/* ========== کد ۲ ========== */
void Ui_Scenario2(void)
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
