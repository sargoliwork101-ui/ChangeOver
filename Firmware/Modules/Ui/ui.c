/**
 * @file    ui.c
 * @brief   [EN] Linear, easy-to-edit LED/buzzer scenarios. Each function runs
 *          one blink cycle with plain on/delay/off steps and returns.
 *          [FA] سناریوهای خطی و قابل‌ویرایش LED/بازر. هر تابع یک سیکل چشمک با
 *          گام‌های ساده روشن/تاخیر/خاموش اجرا می‌کند و برمی‌گردد.
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

/**
 * @brief  [EN] Green LED PB10, active-high via Q6.
 *         [FA] LED سبز PB10، active-high از طریق Q6.
 */
static void green(bool on)
{
    BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, on);
}

/**
 * @brief  [EN] Red LED PB0, is used by a later critical scenario.
 *         [FA] LED قرمز PB0، برای سناریوی بحرانی بعدی.
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
 * @brief  [EN] Safe state before the scheduler runs: all outputs off.
 *         [FA] حالت امن قبل از شروع زمان‌بند: همه خروجی‌ها خاموش.
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
 * @brief  [EN] Scenario Input Normal — one cycle: steady green, others off.
 *         [FA] سناریوی ورودی عادی — یک سیکل: سبز ثابت، بقیه خاموش.
 */
void Ui_ScenarioInputOk(void)
{
    green(true);
    red(false);
    yellow(false);
    buzzer(false);

    /* Hold this picture for one poll cycle, then the task may switch mode.
       این حالت را یک سیکل نگه دار، بعد تسک ممکن است سناریو را عوض کند. */
    vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_input_ok_poll_ms));
}

/**
 * @brief  [EN] Scenario Battery Run — one 1 s green blink cycle.
 *         Off share grows as the battery drains: full battery is 99 % on,
 *         nearly empty is ~1 % on inside ui_blink_period_ms.
 *         [FA] سناریوی دشارژ باتری — یک سیکل چشمک سبز ۱ ثانیه‌ای.
 *         سهم خاموشی با تخلیه باتری زیاد می‌شود: فول ۹۹٪ روشن، تقریباً خالی
 *         حدود ۱٪ روشن در دوره ui_blink_period_ms.
 * @param  battery_percent [EN] 0..100 charge / درصد شارژ
 */
void Ui_ScenarioBatteryRun(uint8_t battery_percent)
{
    uint32_t off_ms;
    uint32_t on_ms;
    uint8_t pct = battery_percent;

    if (pct > UI_PERCENT_FULL)
    {
        pct = UI_PERCENT_FULL;  /* clamp bad readings / مقدار اشتباه محدود شود */
    }

    off_ms = (uint32_t)(UI_PERCENT_FULL - pct) *
             (APP_CONFIG.ui_blink_period_ms / UI_PERCENT_SCALE);
    if (off_ms < APP_CONFIG.ui_green_min_off_ms)
    {
        off_ms = APP_CONFIG.ui_green_min_off_ms;  /* full battery still blinks briefly / فول هم لحظه‌ای خاموش شود */
    }
    on_ms = APP_CONFIG.ui_blink_period_ms - off_ms;

    /* This scenario only drives green; make sure the warning outputs are off.
       این سناریو فقط سبز را می‌زند؛ خروجی‌های هشدار حتماً خاموش باشند. */
    red(false);
    yellow(false);
    buzzer(false);

    green(true);
    vTaskDelay(pdMS_TO_TICKS(on_ms));
    green(false);
    vTaskDelay(pdMS_TO_TICKS(off_ms));
}

/**
 * @brief  [EN] Scenario Battery Low — one 1 s cycle.
 *         Yellow on for ui_warn_yellow_on_ms then off. On the first cycle of
 *         every ui_warn_beep_period_ms window a short beep overlaps the start.
 *         [FA] سناریوی باتری ضعیف — یک سیکل ۱ ثانیه‌ای.
 *         زرد به اندازه ui_warn_yellow_on_ms روشن و بعد خاموش. در سیکل اول هر
 *         پنجره ui_warn_beep_period_ms یک بوق کوتاه با ابتدای روشن زرد هم‌پوشانی
 *         می‌شود.
 */
void Ui_ScenarioBatteryLow(void)
{
    static uint32_t s_cycle = 0u;  /* [EN] cycles since this scenario started / سیکل‌های ورود به این حالت */
    uint32_t beep_every_cycles;
    uint32_t on_ms = APP_CONFIG.ui_warn_yellow_on_ms;
    uint32_t off_ms;

    beep_every_cycles = APP_CONFIG.ui_warn_beep_period_ms / APP_CONFIG.ui_warn_period_ms;
    off_ms = APP_CONFIG.ui_warn_period_ms - on_ms;

    /* Green/red stay off in this warning scenario.
       در حالت هشدار، سبز و قرمز خاموش بمانند. */
    green(false);
    red(false);
    yellow(true);

    if ((s_cycle % beep_every_cycles) == 0u)
    {
        /* Beep for the first part of the yellow on-time.
           بوق در بخش ابتدایی زمان روشن‌بودن زرد. */
        buzzer(true);
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_warn_beep_ms));
        buzzer(false);
        on_ms -= APP_CONFIG.ui_warn_beep_ms;  /* complete the yellow on-time / بقیه زمان روشن زرد */
    }

    vTaskDelay(pdMS_TO_TICKS(on_ms));
    yellow(false);
    vTaskDelay(pdMS_TO_TICKS(off_ms));

    s_cycle++;
}
