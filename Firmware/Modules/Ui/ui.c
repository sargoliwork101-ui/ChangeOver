/**
 * @file    ui.c
 * @brief   [EN] Non-blocking LED/buzzer indication driven by input flag and battery %.
 *          [FA] نمایش غیرمسدودکننده LED/بازر با فلگ ورودی و درصد باتری.
 */

#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>

#define UI_PERCENT_FULL   100u  /* [EN] 100 % charge / باتری فول */
#define UI_PERCENT_SCALE  100u  /* [EN] Divisor for percent math / مقسوم‌علیه درصد */

static ui_state_t s_state = UI_INPUT_OK;  /* [EN] Current shown mode / حالت نمایش فعلی */
static uint32_t   s_phase_ms;             /* [EN] Time inside the blink period / زمان درون دوره چشمک */
static uint32_t   s_warn_ms;              /* [EN] Time inside the warning-beep period / زمان درون دوره بوق هشدار */

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
    s_state = UI_INPUT_OK;
    s_phase_ms = 0u;
    s_warn_ms = 0u;
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
 * @brief  [EN] One non-blocking indication step.
 *         Input present          -> green steady (UI_INPUT_OK).
 *         Input lost, > low limit-> green blink; on-time follows charge %, so a
 *                                   full battery is 99 % on and a nearly empty
 *                                   battery is ~1 % on (UI_BATTERY_RUN).
 *         At/below low limit     -> yellow 500/500 blink and a short beep every
 *                                   warning period (UI_BATTERY_LOW).
 *         [FA] یک گام نمایش غیرمسدودکننده.
 *         ورودی وصل              -> سبز ثابت (UI_INPUT_OK).
 *         ورودی قطع، بالاتر از حد-> سبز چشمک؛ زمان روشن‌بودن برابر درصد شارژ است،
 *                                   باتری فول ۹۹٪ روشن و باتری تقریباً خالی ۱٪ روشن.
 *         در/زیر حد ضعیف         -> زرد ۵۰۰/۵۰۰ و یک بوق کوتاه در هر دوره هشدار.
 * @param  input_present   [EN] true while mains feeds the system / برق ورودی وصل
 * @param  battery_percent [EN] 0..100 battery charge / درصد باتری
 */
void Ui_Indicate(bool input_present, uint8_t battery_percent)
{
    ui_state_t state;
    uint8_t pct = battery_percent;
    uint32_t phase_period_ms;
    bool green_on = false;
    bool yellow_on = false;
    bool red_on = false;
    bool buzzer_on = false;

    if (pct > UI_PERCENT_FULL)
    {
        pct = UI_PERCENT_FULL;  /* clamp bad readings / مقدار اشتباه محدود شود */
    }

    if (input_present == true)
    {
        state = UI_INPUT_OK;
    }
    else if (pct <= APP_CONFIG.ui_low_battery_percent)
    {
        state = UI_BATTERY_LOW;
    }
    else
    {
        state = UI_BATTERY_RUN;
    }

    if (state != s_state)
    {
        /* Mode changed: restart both software timers so every pattern begins at
           its first edge. / حالت عوض شد: تایمرهای نرم از صفر شروع شوند. */
        s_state = state;
        s_phase_ms = 0u;
        s_warn_ms = 0u;
    }

    switch (s_state)
    {
        case UI_INPUT_OK:
            /* Steady green, everything else off. / سبز ثابت، بقیه خاموش. */
            green_on = true;
            phase_period_ms = APP_CONFIG.ui_blink_period_ms;
            break;

        case UI_BATTERY_RUN:
        {
            uint32_t off_ms;
            uint32_t on_ms;

            /* Off share grows linearly as the battery drains.
               At 100 % off is clamped to ui_green_min_off_ms (~1 %);
               at 1 % the LED is on only ~1 % of the period.
               سهم خاموشی با تخلیه باتری خطی زیاد می‌شود. */
            off_ms = (uint32_t)(UI_PERCENT_FULL - pct) *
                     (APP_CONFIG.ui_blink_period_ms / UI_PERCENT_SCALE);
            if (off_ms < APP_CONFIG.ui_green_min_off_ms)
            {
                off_ms = APP_CONFIG.ui_green_min_off_ms;
            }
            on_ms = APP_CONFIG.ui_blink_period_ms - off_ms;

            green_on = (s_phase_ms < on_ms);
            phase_period_ms = APP_CONFIG.ui_blink_period_ms;
            break;
        }

        case UI_BATTERY_LOW:
            /* Yellow 500/500 blink; short beep at the start of each 30 s window.
               زرد ۵۰۰/۵۰۰؛ یک بوق کوتاه در ابتدای هر پنجره ۳۰ ثانیه‌ای. */
            yellow_on = (s_phase_ms < APP_CONFIG.ui_warn_yellow_on_ms);
            buzzer_on = (s_warn_ms < APP_CONFIG.ui_warn_beep_ms);
            phase_period_ms = APP_CONFIG.ui_warn_period_ms;
            break;

        default:
            /* Defensive: unknown state keeps every output safely off.
               حالت ناشناخته: همه خروجی‌ها امن خاموش بمانند. */
            phase_period_ms = APP_CONFIG.ui_warn_period_ms;
            break;
    }

    green(green_on);
    red(red_on);
    yellow(yellow_on);
    buzzer(buzzer_on);

    /* Advance the blink-phase clock and wrap at the active period.
       ساعت فاز چشمک جلو برود و در پایان دوره صفر شود. */
    s_phase_ms += APP_CONFIG.ui_task_period_ms;
    if (s_phase_ms >= phase_period_ms)
    {
        s_phase_ms = 0u;
    }

    /* The beep timer only runs in the low-battery mode.
       تایمر بوق فقط در حالت باتری ضعیف کار می‌کند. */
    if (s_state == UI_BATTERY_LOW)
    {
        s_warn_ms += APP_CONFIG.ui_task_period_ms;
        if (s_warn_ms >= APP_CONFIG.ui_warn_beep_period_ms)
        {
            s_warn_ms = 0u;
        }
    }
}
