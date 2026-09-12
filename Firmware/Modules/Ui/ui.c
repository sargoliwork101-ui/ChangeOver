/**
 * @file    ui.c
 * @brief   تست پایه‌های LED و بازر، بعد چشمک سبز به‌عنوان ضربان قلب.
 *
 * ترتیب بعد از Reset (زمان‌ها از app_config.c می‌آیند):
 *   1) LED قرمز روشن
 *   2) LED زرد روشن
 *   3) LED سبز روشن
 *   4) یک بوق کوتاه
 *   5) سبز چشمک، بقیه خاموش
 *
 * چرا این ترتیب؟
 *   برای اولین پروگرام باید بفهمی کدام سیم/پایه اشتباه است.
 *   اگر زرد روشن نشد ولی قرمز شد، مشکل از PB1 است نه از کل برد.
 *
 * این فایل HAL را مستقیم صدا نمی‌زند. فقط BspGpio_Write.
 * دلیل: فردا اگر HAL عوض شد، UI دست نمی‌خورد.
 */

#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "app_config.h"

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------------- */
/* فازهای تست. enum به‌جای عدد خام: MISRA، خوانایی، جلوگیری از magic number. */
/* ------------------------------------------------------------------------- */
typedef enum
{
    UI_PHASE_TEST_RED = 0,
    UI_PHASE_TEST_YELLOW,
    UI_PHASE_TEST_GREEN,
    UI_PHASE_TEST_BUZZER,
    UI_PHASE_HEARTBEAT
} ui_phase_t;

/* وضعیت داخلی ماژول: static یعنی فقط همین فایل می‌بیند (مثل private). */
static ui_phase_t s_phase;
static uint32_t s_ticks_in_phase;
static bool s_heartbeat_on;

/**
 * @brief ms را به تعداد تیک Task تبدیل می‌کند.
 *
 * مثال: 500 ms با دوره 100 ms می‌شود 5 تیک.
 *
 * MISRA: قبل از تقسیم، مخرج صفر نباشد.
 */
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
        /* تقسیم سقفی: 150 ms / 100 ms = 2 تیک، نه 1 */
        ticks = (time_ms + (period_ms - 1u)) / period_ms;
        if (ticks == 0u)
        {
            ticks = 1u;
        }
    }

    return ticks;
}

/**
 * @brief چهار خروجی UI را با هم می‌نویسد.
 *
 * یک تابع مرکزی تا در هر فاز چهار بار Copy/Paste digitalWrite نداشته باشیم.
 */
static void ui_apply(bool red_on, bool yellow_on, bool green_on, bool buzzer_on)
{
    BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, red_on);
    BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, yellow_on);
    BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, green_on);
    BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, buzzer_on);
}

void Ui_Init(void)
{
    s_phase = UI_PHASE_TEST_RED;
    s_ticks_in_phase = 0u;
    s_heartbeat_on = false;

    /* قبل از هر چیز همه چیز خاموش. Reset سخت‌افزار هم Low است، این تضمین نرم‌افزاری است. */
    ui_apply(false, false, false, false);
}

void Ui_Run(void)
{
    uint32_t led_ticks;
    uint32_t beep_ticks;
    uint32_t blink_ticks;

    led_ticks = ui_ms_to_ticks(APP_CONFIG.ui_selftest_led_ms);
    beep_ticks = ui_ms_to_ticks(APP_CONFIG.ui_boot_beep_ms);
    blink_ticks = ui_ms_to_ticks(APP_CONFIG.ui_heartbeat_half_ms);

    s_ticks_in_phase = s_ticks_in_phase + 1u;

    switch (s_phase)
    {
        case UI_PHASE_TEST_RED:
            ui_apply(true, false, false, false);
            if (s_ticks_in_phase >= led_ticks)
            {
                s_ticks_in_phase = 0u;
                s_phase = UI_PHASE_TEST_YELLOW;
            }
            break;

        case UI_PHASE_TEST_YELLOW:
            ui_apply(false, true, false, false);
            if (s_ticks_in_phase >= led_ticks)
            {
                s_ticks_in_phase = 0u;
                s_phase = UI_PHASE_TEST_GREEN;
            }
            break;

        case UI_PHASE_TEST_GREEN:
            ui_apply(false, false, true, false);
            if (s_ticks_in_phase >= led_ticks)
            {
                s_ticks_in_phase = 0u;
                s_phase = UI_PHASE_TEST_BUZZER;
            }
            break;

        case UI_PHASE_TEST_BUZZER:
            /* LEDها خاموش، فقط بوق. اگر اینجا سوت ممتد شنیدی، Ui_Run گیر کرده یا delay گذاشتی. */
            ui_apply(false, false, false, true);
            if (s_ticks_in_phase >= beep_ticks)
            {
                s_ticks_in_phase = 0u;
                s_heartbeat_on = true;
                s_phase = UI_PHASE_HEARTBEAT;
            }
            break;

        case UI_PHASE_HEARTBEAT:
            if (s_ticks_in_phase >= blink_ticks)
            {
                s_ticks_in_phase = 0u;
                /* toggle بدون عملگر ! روی int: MISRA خواناتر است با if صریح */
                if (s_heartbeat_on == true)
                {
                    s_heartbeat_on = false;
                }
                else
                {
                    s_heartbeat_on = true;
                }
            }
            ui_apply(false, false, s_heartbeat_on, false);
            break;

        default:
            /* اگر enum خراب شد، همه چیز را خاموش کن. حالت امن UI. */
            ui_apply(false, false, false, false);
            s_phase = UI_PHASE_HEARTBEAT;
            s_ticks_in_phase = 0u;
            break;
    }
}
