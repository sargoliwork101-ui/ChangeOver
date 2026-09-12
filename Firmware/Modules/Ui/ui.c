/**
 * @file ui.c
 *
 * همان مثال TaskLed، ولی delay این‌جا نیست.
 *
 * معادل آردوینو/مثال قبلی:
 *   green(1);  vTaskDelay(500);
 * این‌جا یعنی:
 *   green(1);  و 500 ms صبر کن (صبر را Task با vTaskDelay(100)
 *   چند بار پشت‌سرهم انجام می‌دهد تا جمعش 500 شود)
 *
 * چرا delay را این فایل نمی‌نویسیم؟
 *   اگر این‌جا vTaskDelay(1000) بگذاری، تا 1 ثانیه نمی‌توانی
 *   از رویداد 1 به 2 بروی. Task هر 100 ms بیدار می‌شود و
 *   ما فقط می‌شماریم 100، 200، ... 500.
 */

#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "app_config.h"

#include <stdint.h>
#include <stdbool.h>

static ui_profile_t s_mode;
static uint32_t s_step;     /* کدام خط از الگوی فعلی */
static uint32_t s_waited_ms; /* چند ms از این خط گذشته */

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

static bool wait_ms(uint32_t duration_ms)
{
    uint32_t tick_ms;

    tick_ms = APP_CONFIG.ui_period_ms;
    if (tick_ms == 0u)
    {
        tick_ms = 100u;
    }

    s_waited_ms = s_waited_ms + tick_ms;

    if (s_waited_ms >= duration_ms)
    {
        s_waited_ms = 0u;
        return true; /* مدت این خط تمام شد */
    }

    return false; /* هنوز صبر کن */
}

void Ui_Init(void)
{
    s_mode = UI_PROFILE_OFF;
    s_step = 0u;
    s_waited_ms = 0u;
    green(false);
    red(false);
    yellow(false);
    buzzer(false);
}

void Ui_SetProfile(ui_profile_t profile)
{
    s_mode = profile;
    s_step = 0u;
    s_waited_ms = 0u;
    green(false);
    red(false);
    yellow(false);
    buzzer(false);
}

void Ui_Run(void)
{
    /*
     * این همان if (mode == 1) / else مثال تو است.
     * هر بار که Task صدا می‌زند، فقط یک تکه کوچک جلو می‌رویم.
     */
    if (s_mode == UI_PROFILE_EVENT1)
    {
        /* رویداد 1:
         *   green(1); red(0);  vTaskDelay(500);
         *   green(0); red(0);  vTaskDelay(500);
         */
        if (s_step == 0u)
        {
            green(true);
            red(false);
            if (wait_ms(500u) == true)
            {
                s_step = 1u;
            }
        }
        else
        {
            green(false);
            red(false);
            if (wait_ms(500u) == true)
            {
                s_step = 0u; /* برگرد اول الگو */
            }
        }
    }
    else if (s_mode == UI_PROFILE_EVENT2)
    {
        /* رویداد 2:
         *   green(1); red(1);  vTaskDelay(500);
         *   green(0); red(0);  vTaskDelay(500);
         *   green(0); red(1);  vTaskDelay(500);
         */
        if (s_step == 0u)
        {
            green(true);
            red(true);
            if (wait_ms(500u) == true)
            {
                s_step = 1u;
            }
        }
        else if (s_step == 1u)
        {
            green(false);
            red(false);
            if (wait_ms(500u) == true)
            {
                s_step = 2u;
            }
        }
        else
        {
            green(false);
            red(true);
            if (wait_ms(500u) == true)
            {
                s_step = 0u;
            }
        }
    }
    else
    {
        green(false);
        red(false);
        yellow(false);
        buzzer(false);
    }
}
