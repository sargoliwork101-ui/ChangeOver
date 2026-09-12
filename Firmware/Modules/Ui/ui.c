/**
 * فایل ۱ از ۲ — فقط پایه‌ها را روشن/خاموش می‌کند.
 * مثل digitalWrite در آردوینو.
 * این‌جا حلقه و delay نداریم.
 */

#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"

void green(bool on)
{
    BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, on);
}

void red(bool on)
{
    BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, on);
}

void yellow(bool on)
{
    BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, on);
}

void buzzer(bool on)
{
    BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, on);
}

void Ui_Init(void)
{
    green(false);
    red(false);
    yellow(false);
    buzzer(false);
}
