/**
 * @file    ui.c
 * @brief   [EN] Simple, linear LED/buzzer scenarios. Easy to edit later.
 *          [FA] سناریوهای ساده و خطی LED/بازر. قابل اصلاح آسان بعداً.
 *
 * @note    [EN] Simplicity rule (from AI_CONTEXT.md):
 *          - One function = one job, linear on/delay/off steps, no nested logic.
 *          - All thresholds in ui_config.h single file (no duplication).
 *          - Buzzer separate: Ui_BuzzerBeep() callable from any scenario.
 *          - Naming: U32_G_ global uppercase, u32_ local lowercase.
 *          [FA] قانون سادگی:
 *          - هر تابع یک کار، گام‌های خطی روشن/تاخیر/خاموش، بدون if تودرتو.
 *          - همه آستانه‌ها در ui_config.h یک فایل واحد.
 *          - بازر جدا.
 */

#include "ui.h"
#include "ui_config.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* ===== Simple low-level LED helpers - one line each, easy to edit ===== */

/**
 * @brief  [EN] Green PB10 on/off.
 *         [FA] سبز PB10 روشن/خاموش.
 */
static void green(bool b_on)
{
    BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, b_on);
}

/**
 * @brief  [EN] Red PB0 on/off.
 *         [FA] قرمز PB0.
 */
static void red(bool b_on)
{
    BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, b_on);
}

/**
 * @brief  [EN] Yellow PB1 on/off.
 *         [FA] زرد PB1.
 */
static void yellow(bool b_on)
{
    BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, b_on);
}

/**
 * @brief  [EN] Buzzer PA4 on/off.
 *         [FA] بازر PA4.
 */
static void buzzer(bool b_on)
{
    BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, b_on);
}

/**
 * @brief  [EN] All off.
 *         [FA] همه خاموش.
 */
static void all_off(void)
{
    green(false);
    red(false);
    yellow(false);
    buzzer(false);
}

/* ===== Beep counter - file scope global (uppercase type) ===== */
static uint32_t U32_G_BeepCnt = 0u;

/* ===== Public ===== */

/**
 * @brief  [EN] Safe init: all off.
 *         [FA] Init امن: همه خاموش.
 */
void Ui_Init(void)
{
    all_off();
}

/**
 * @brief  [EN] Board test: R, Y, G, beep - linear steps.
 *         [FA] تست برد: قرمز، زرد، سبز، بوق - خطی.
 */
void Ui_BoardTest(void)
{
    all_off();

    red(true);
    vTaskDelay(pdMS_TO_TICKS(UI_SELFTEST_LED_MS));
    red(false);

    yellow(true);
    vTaskDelay(pdMS_TO_TICKS(UI_SELFTEST_LED_MS));
    yellow(false);

    green(true);
    vTaskDelay(pdMS_TO_TICKS(UI_SELFTEST_LED_MS));
    green(false);

    buzzer(true);
    vTaskDelay(pdMS_TO_TICKS(UI_BOOT_BEEP_MS));
    buzzer(false);
}

/**
 * @brief  [EN] Simple buzzer beep - separate function.
 *         [FA] بوق ساده - تابع جدا.
 */
void Ui_BuzzerBeep(uint32_t u32_ms)
{
    uint32_t u32_d = u32_ms;

    if (u32_d == 0u)
    {
        u32_d = UI_BEEP_BASE_MS;
    }

    buzzer(true);
    vTaskDelay(pdMS_TO_TICKS(u32_d));
    buzzer(false);
}

/**
 * @brief  [EN] Battery V to percent: 21V=0% 28V=100%.
 *         [FA] ولتاژ باتری به درصد.
 */
uint8_t Ui_BatteryVoltageToPercent(uint32_t u32_mv)
{
    uint32_t u32_range;
    uint32_t u32_off;
    uint8_t u8_pct;

    if (u32_mv <= UI_BAT_V_MIN_MV)
    {
        return 0u;
    }

    if (u32_mv >= UI_BAT_V_MAX_MV)
    {
        return UI_PERCENT_FULL;
    }

    u32_range = UI_BAT_V_MAX_MV - UI_BAT_V_MIN_MV;
    u32_off = u32_mv - UI_BAT_V_MIN_MV;

    if (u32_range == 0u)
    {
        return 0u;
    }

    u8_pct = (uint8_t)((u32_off * 100u) / u32_range);

    if (u8_pct > UI_PERCENT_FULL)
    {
        u8_pct = UI_PERCENT_FULL;
    }

    return u8_pct;
}

/**
 * @brief  [EN] InputOk: green steady, others off. One cycle.
 *         [FA] ورودی عادی: سبز ثابت.
 */
void Ui_ScenarioInputOk(void)
{
    green(true);
    red(false);
    yellow(false);
    buzzer(false);

    vTaskDelay(pdMS_TO_TICKS(UI_INPUT_OK_POLL_MS));
}

/**
 * @brief  [EN] BatteryRun: green blink, yellow OFF, smart beep via separate function.
 *         [FA] دشارژ: سبز چشمک، زرد خاموش، بوق هوشمند جدا.
 */
void Ui_ScenarioBatteryRun(uint32_t u32_batMv)
{
    uint8_t u8_pct;
    uint32_t u32_on;
    uint32_t u32_off;
    uint32_t u32_beepInt;
    uint32_t u32_beepDur;
    bool b_beepNow = false;

    /* Step 1: voltage to percent */
    u8_pct = Ui_BatteryVoltageToPercent(u32_batMv);

    /* Step 2: calc on/off */
    u32_off = (uint32_t)(UI_PERCENT_FULL - u8_pct) * (UI_BLINK_PERIOD_MS / 100u);

    if (u32_off < UI_GREEN_MIN_OFF_MS)
    {
        u32_off = UI_GREEN_MIN_OFF_MS;
    }

    u32_on = UI_BLINK_PERIOD_MS - u32_off;

    /* Step 3: yellow OFF per new req */
    red(false);
    yellow(false);

    /* Step 4: green blink - linear */
    green(true);
    vTaskDelay(pdMS_TO_TICKS(u32_on));
    green(false);
    vTaskDelay(pdMS_TO_TICKS(u32_off));

    /* Step 5: smart beep - simple linear */
    if (u8_pct >= UI_BEEP_START_PCT)
    {
        U32_G_BeepCnt = 0u;
        return;
    }

    u32_beepInt = (uint32_t)u8_pct;

    if (u32_beepInt == 0u)
    {
        u32_beepInt = 1u;
    }

    if (U32_G_BeepCnt >= u32_beepInt)
    {
        b_beepNow = true;
        U32_G_BeepCnt = 0u;
    }
    else
    {
        U32_G_BeepCnt++;
    }

    if (b_beepNow == false)
    {
        return;
    }

    u32_beepDur = UI_BEEP_BASE_MS;

    if (u8_pct < UI_BEEP_DOUBLE_THRESH_PCT)
    {
        u32_beepDur = u32_beepDur * 2u;
    }

    Ui_BuzzerBeep(u32_beepDur);
}

/**
 * @brief  [EN] Charging: green steady, yellow shows remaining to full.
 *         [FA] شارژ: سبز ثابت، زرد مانده تا فول.
 */
void Ui_ScenarioCharging(uint32_t u32_batMv)
{
    uint8_t u8_pct;
    uint32_t u32_on;
    uint32_t u32_off;

    u8_pct = Ui_BatteryVoltageToPercent(u32_batMv);

    /* 100% => yellow OFF */
    if (u8_pct >= UI_PERCENT_FULL)
    {
        yellow(false);
        green(true);
        red(false);
        buzzer(false);
        vTaskDelay(pdMS_TO_TICKS(UI_CHARGING_BLINK_PERIOD_MS));
        return;
    }

    /* 0% => yellow steady ON */
    if (u8_pct == 0u)
    {
        yellow(true);
        green(true);
        red(false);
        buzzer(false);
        vTaskDelay(pdMS_TO_TICKS(UI_CHARGING_BLINK_PERIOD_MS));
        return;
    }

    /* Intermediate: ON = (100-pct)*period */
    u32_on = (uint32_t)(UI_PERCENT_FULL - u8_pct) * (UI_CHARGING_BLINK_PERIOD_MS / 100u);

    if (u32_on < UI_CHARGING_YELLOW_MIN_OFF_MS)
    {
        u32_on = UI_CHARGING_YELLOW_MIN_OFF_MS;
    }

    if (u32_on > UI_CHARGING_BLINK_PERIOD_MS)
    {
        u32_on = UI_CHARGING_BLINK_PERIOD_MS;
    }

    u32_off = UI_CHARGING_BLINK_PERIOD_MS - u32_on;

    green(true);
    red(false);
    buzzer(false);

    yellow(true);
    vTaskDelay(pdMS_TO_TICKS(u32_on));
    yellow(false);
    vTaskDelay(pdMS_TO_TICKS(u32_off));
}
