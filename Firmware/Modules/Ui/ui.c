/**
 * @file    ui.c
 * @brief   پیاده‌سازی پروفایل‌های LED/بازر.
 *
 * قانون: HAL این‌جا نیست. فقط BspGpio_Write.
 * قانون: delay این‌جا نیست. Ui_Run هر تیک یک قدم جلو می‌رود.
 */

#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "app_config.h"

#include <stdint.h>
#include <stdbool.h>

static ui_profile_t s_profile;
static uint32_t s_ticks;
static uint32_t s_step;
static bool s_blink_on;

static uint32_t ui_ms_to_ticks(uint32_t time_ms)
{
    uint32_t period_ms;
    uint32_t ticks;

    period_ms = APP_CONFIG.ui_period_ms;
    if (period_ms == 0u)
    {
        ticks = 1u;
    }
    else
    {
        ticks = (time_ms + (period_ms - 1u)) / period_ms;
        if (ticks == 0u)
        {
            ticks = 1u;
        }
    }

    return ticks;
}

static void ui_apply(bool red_on, bool yellow_on, bool green_on, bool buzzer_on)
{
    BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, red_on);
    BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, yellow_on);
    BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, green_on);
    BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, buzzer_on);
}

static void ui_toggle_blink(uint32_t half_ms)
{
    if (s_ticks >= ui_ms_to_ticks(half_ms))
    {
        s_ticks = 0u;
        if (s_blink_on == true)
        {
            s_blink_on = false;
        }
        else
        {
            s_blink_on = true;
        }
    }
}

/* --- پروفایل‌ها: همه این‌جا، نه در main و نه در Task --- */

static void ui_profile_off(void)
{
    ui_apply(false, false, false, false);
}

static void ui_profile_selftest(void)
{
    uint32_t led_ticks;
    uint32_t beep_ticks;

    led_ticks = ui_ms_to_ticks(APP_CONFIG.ui_selftest_led_ms);
    beep_ticks = ui_ms_to_ticks(APP_CONFIG.ui_boot_beep_ms);

    switch (s_step)
    {
        case 0u:
            ui_apply(true, false, false, false);  /* قرمز */
            if (s_ticks >= led_ticks)
            {
                s_ticks = 0u;
                s_step = 1u;
            }
            break;

        case 1u:
            ui_apply(false, true, false, false);  /* زرد */
            if (s_ticks >= led_ticks)
            {
                s_ticks = 0u;
                s_step = 2u;
            }
            break;

        case 2u:
            ui_apply(false, false, true, false);  /* سبز */
            if (s_ticks >= led_ticks)
            {
                s_ticks = 0u;
                s_step = 3u;
            }
            break;

        case 3u:
            ui_apply(false, false, false, true);  /* بوق */
            if (s_ticks >= beep_ticks)
            {
                /* تست تمام شد → ضربان قلب. بقیه کد لازم نیست این را بداند. */
                Ui_SetProfile(UI_PROFILE_HEARTBEAT);
            }
            break;

        default:
            Ui_SetProfile(UI_PROFILE_HEARTBEAT);
            break;
    }
}

static void ui_profile_heartbeat(void)
{
    ui_toggle_blink(APP_CONFIG.ui_heartbeat_half_ms);
    ui_apply(false, false, s_blink_on, false);
}

static void ui_profile_fault(void)
{
    ui_toggle_blink(APP_CONFIG.ui_heartbeat_half_ms);
    ui_apply(s_blink_on, false, false, false);
}

void Ui_Init(void)
{
    s_profile = UI_PROFILE_OFF;
    s_ticks = 0u;
    s_step = 0u;
    s_blink_on = false;
    ui_profile_off();
}

void Ui_SetProfile(ui_profile_t profile)
{
    s_profile = profile;
    s_ticks = 0u;
    s_step = 0u;
    s_blink_on = true;
    ui_profile_off();
}

void Ui_Run(void)
{
    s_ticks = s_ticks + 1u;

    switch (s_profile)
    {
        case UI_PROFILE_OFF:
            ui_profile_off();
            break;

        case UI_PROFILE_SELFTEST:
            ui_profile_selftest();
            break;

        case UI_PROFILE_HEARTBEAT:
            ui_profile_heartbeat();
            break;

        case UI_PROFILE_FAULT:
            ui_profile_fault();
            break;

        default:
            ui_profile_off();
            break;
    }
}
