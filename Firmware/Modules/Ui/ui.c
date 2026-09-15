/**
 * @file    ui.c
 * @brief   [EN] LED/buzzer scenarios - readable, not purely linear, easy to edit later.
 *          [FA] سناریوهای LED/بازر - خوانا، نه صرفاً خطی، قابل اصلاح آسان.
 *
 * @note    [EN] Readability rule (AI_CONTEXT.md):
 *          - Each function has one job, but written for readability, not just linear on/delay/off.
 *          - All thresholds in ui_config.h single file (no duplication).
 *          - Buzzer separate function callable from any scenario.
 *          - Naming: U32_G_ global uppercase, u32_ local lowercase.
 *          - Every function documents what it does and each param (unit, range).
 *          [FA] قانون خوانایی:
 *          - هر تابع یک کار، ولی خوانا نوشته شده، نه فقط خطی.
 *          - همه آستانه‌ها در ui_config.h.
 */

#include "ui.h"
#include "ui_config.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* ===== Low-level helpers - simple, readable, one purpose ===== */

/**
 * @brief  [EN] Control green LED PB10.
 *         [FA] کنترل LED سبز PB10.
 * @param  b_on [EN] true = LED on (3.3V via Q6), false = off / روشن یا خاموش
 */
static void green(bool b_on)
{
    BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, b_on);
}

/**
 * @brief  [EN] Control red LED PB0. Reserved for future critical scenario.
 *         [FA] کنترل LED قرمز PB0، برای سناریوی بحرانی بعدی رزرو.
 * @param  b_on [EN] true = on, false = off / روشن/خاموش
 */
static void red(bool b_on)
{
    BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, b_on);
}

/**
 * @brief  [EN] Control yellow LED PB1. Used in charging scenario.
 *         [FA] کنترل LED زرد PB1، در سناریوی شارژ.
 * @param  b_on [EN] true = on (HIGH = yellow on via Q5) / روشن
 */
static void yellow(bool b_on)
{
    BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, b_on);
}

/**
 * @brief  [EN] Control buzzer PA4 low-level.
 *         [FA] کنترل بازر PA4 سطح پایین.
 * @param  b_on [EN] true = sound on (HIGH via Q7), false = off / صدا روشن/خاموش
 */
static void buzzer(bool b_on)
{
    BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, b_on);
}

/**
 * @brief  [EN] Turn all UI outputs off (safe state).
 *         [FA] همه خروجی‌های UI خاموش (حالت امن).
 */
static void all_off(void)
{
    green(false);
    red(false);
    yellow(false);
    buzzer(false);
}

/* ===== File-scope beep tracking - global type uppercase ===== */
static uint32_t U32_G_BeepCnt = 0u;

/* ===== Public API ===== */

/**
 * @brief  [EN] Initialize UI to safe state (all off). Called once before scheduler.
 *         [FA] مقداردهی اولیه UI به حالت امن (همه خاموش). یک‌بار قبل از زمان‌بند.
 */
void Ui_Init(void)
{
    all_off();
}

/**
 * @brief  [EN] One-shot board wiring test: R -> Y -> G -> beep.
 *         Each step is on, delay, off - readable sequence.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز، بوق - ترتیب خوانا.
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
 * @brief  [EN] Separate buzzer function - beep for given duration. Callable from any scenario.
 *         [FA] تابع جدا بازر - به مدت داده شده بوق می‌زند. از هر سناریو قابل صدا زدن.
 * @param  u32_ms [EN] Beep duration in ms, 0 = use base (250ms). Range 0..5000ms / طول بوق میلی‌ثانیه
 */
void Ui_BuzzerBeep(uint32_t u32_ms)
{
    uint32_t u32_d;

    u32_d = u32_ms;

    if (u32_d == 0u)
    {
        u32_d = UI_BEEP_BASE_MS;
    }

    buzzer(true);
    vTaskDelay(pdMS_TO_TICKS(u32_d));
    buzzer(false);
}

/**
 * @brief  [EN] Convert battery voltage to percent 0..100.
 *         Mapping: 0% = UI_BAT_V_MIN_MV (21V), 100% = UI_BAT_V_MAX_MV (28V). Clamped.
 *         Formula: pct = (V - Vmin)*100 / (Vmax - Vmin)
 *         [FA] تبدیل ولتاژ باتری به درصد ۰..۱۰۰. نگاشت ۰٪=۲۱V و ۱۰۰٪=۲۸V.
 * @param  u32_mv [EN] Battery voltage in mV. Range 0..40000mV, but clamped to 0..100% via Vmin/Vmax / ولتاژ باتری میلی‌ولت
 * @return uint8_t [EN] Percent 0..100, 0=empty (21V), 100=full (28V) / درصد باتری
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
 * @brief  [EN] Scenario InputOk: input voltage present, battery full.
 *         Green steady ON, others OFF. Holds for UI_INPUT_OK_POLL_MS then returns.
 *         [FA] سناریوی ورودی عادی: ورودی وصل و باتری فول.
 * @note   [EN] No params - uses global voltage via task. Timing from ui_config.h.
 *         [FA] بدون پارامتر - ولتاژ از تسک می‌آید. تایم از ui_config.h
 */
void Ui_ScenarioInputOk(void)
{
    /* Green indicates input OK */
    green(true);

    /* Others must be off in this scenario */
    red(false);
    yellow(false);
    buzzer(false);

    vTaskDelay(pdMS_TO_TICKS(UI_INPUT_OK_POLL_MS));
}

/**
 * @brief  [EN] Scenario BatteryRun: discharging, input lost (V_in <20V).
 *         Green blinks where on-time = battery percent. Yellow OFF per new requirement.
 *         Smart beep: if pct<50, beep every pct seconds (40%->40s). If pct<20, duration x2.
 *         Uses separate Ui_BuzzerBeep() function.
 *         [FA] سناریوی دشارژ: ورودی قطع، سبز چشمک با روشن‌بودن برابر درصد، زرد خاموش، بوق هوشمند جدا.
 * @param  u32_batMv [EN] Battery voltage in mV. 21000mV=0% 28000mV=100%. Range 21000..28000mV / ولتاژ باتری
 */
void Ui_ScenarioBatteryRun(uint32_t u32_batMv)
{
    uint8_t u8_pct;
    uint32_t u32_on;
    uint32_t u32_off;
    uint32_t u32_beepInt;
    uint32_t u32_beepDur;
    bool b_beepNow;

    /* 1. Convert voltage to percent - readable step */
    u8_pct = Ui_BatteryVoltageToPercent(u32_batMv);

    /* 2. Calculate green blink timing */
    u32_off = (uint32_t)(UI_PERCENT_FULL - u8_pct) * (UI_BLINK_PERIOD_MS / 100u);

    if (u32_off < UI_GREEN_MIN_OFF_MS)
    {
        u32_off = UI_GREEN_MIN_OFF_MS;
    }

    u32_on = UI_BLINK_PERIOD_MS - u32_off;

    /* 3. Ensure unrelated outputs off */
    red(false);
    yellow(false);

    /* 4. Green blink */
    green(true);
    vTaskDelay(pdMS_TO_TICKS(u32_on));
    green(false);
    vTaskDelay(pdMS_TO_TICKS(u32_off));

    /* 5. Smart beep handling - readable, not nested deeply */
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

    b_beepNow = false;

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
        u32_beepDur *= 2u;
    }

    Ui_BuzzerBeep(u32_beepDur);
}

/**
 * @brief  [EN] Scenario Charging: input present (V_in >=20V) and battery <100%.
 *         Green steady ON (input present). Yellow shows remaining to full:
 *         0% (21V) = yellow steady ON, 100% (28V) = OFF, intermediate ON=(100-pct)*period.
 *         [FA] سناریوی شارژ: ورودی وصل و باتری زیر فول. سبز ثابت، زرد مانده تا فول.
 * @param  u32_batMv [EN] Battery voltage in mV. 21000=0% (yellow ON), 28000=100% (yellow OFF). Range 21000..28000 / ولتاژ باتری
 */
void Ui_ScenarioCharging(uint32_t u32_batMv)
{
    uint8_t u8_pct;
    uint32_t u32_on;
    uint32_t u32_off;

    u8_pct = Ui_BatteryVoltageToPercent(u32_batMv);

    /* Full => yellow OFF */
    if (u8_pct >= UI_PERCENT_FULL)
    {
        yellow(false);
        green(true);
        red(false);
        buzzer(false);
        vTaskDelay(pdMS_TO_TICKS(UI_CHARGING_BLINK_PERIOD_MS));
        return;
    }

    /* Empty => yellow steady ON */
    if (u8_pct == 0u)
    {
        yellow(true);
        green(true);
        red(false);
        buzzer(false);
        vTaskDelay(pdMS_TO_TICKS(UI_CHARGING_BLINK_PERIOD_MS));
        return;
    }

    /* Intermediate: remaining to full */
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
