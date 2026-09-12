/**
 * فایل LED — الگوها این‌جا هستند، نه در FreeRTOS.
 *
 * هر بار که Ui_Run صدا می‌شود، فقط ONE خط انجام می‌شود
 * و عدد صبر را برمی‌گرداند. خودِ خوابیدن کار Task است.
 *
 * معادل:
 *   green(true);
 *   return 500;   ← به‌جای vTaskDelay(500) این‌جا
 */

#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"

static ui_profile_t s_mode = UI_PROFILE_EVENT1;
static uint32_t s_step = 0u;

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

void Ui_Init(void)
{
    s_mode = UI_PROFILE_EVENT1;
    s_step = 0u;
    green(false);
    red(false);
    yellow(false);
    buzzer(false);
}

void Ui_SetProfile(ui_profile_t profile)
{
    s_mode = profile;
    s_step = 0u;
}

uint32_t Ui_Run(void)
{
    if (s_mode == UI_PROFILE_EVENT1)
    {
        if (s_step == 0u)
        {
            green(true);
            red(false);
            s_step = 1u;
            return 500u;
        }

        green(false);
        red(false);
        s_step = 0u;
        return 500u;
    }

    /* رویداد 2 */
    if (s_step == 0u)
    {
        green(true);
        red(true);
        s_step = 1u;
        return 500u;
    }

    if (s_step == 1u)
    {
        green(false);
        red(false);
        s_step = 2u;
        return 500u;
    }

    green(false);
    red(true);
    s_step = 0u;
    return 500u;
}
