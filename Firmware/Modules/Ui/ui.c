#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"

#include <stdint.h>
#include <stdbool.h>

static uint32_t s_mode = 1u;
static uint32_t s_step = 0u;

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
    s_mode = 1u;
    s_step = 0u;
    green(false);
    red(false);
}

void Ui_SetProfile(ui_profile_t profile)
{
    s_mode = (uint32_t)profile;
    s_step = 0u;
}

void Ui_Run(void)
{
    if (s_mode == 1u)
    {
        if (s_step == 0u)
        {
            green(true);
            red(false);
            s_step = 1u;
        }
        else
        {
            green(false);
            red(false);
            s_step = 0u;
        }
    }
    else
    {
        if (s_step == 0u)
        {
            green(true);
            red(true);
            s_step = 1u;
        }
        else if (s_step == 1u)
        {
            green(false);
            red(false);
            s_step = 2u;
        }
        else
        {
            green(false);
            red(true);
            s_step = 0u;
        }
    }
}
