/**
 * @file    ui.c
 * @brief   [EN] LED/buzzer scenarios - readable, full type names, func_ prefix.
 *          Buzzer pattern separate scenario with period, onTime, repeat, gap.
 *          [FA] سناریوهای LED/بازر - خوانا، نام تایپ کامل، پیشوند func_.
 *          الگوی بازر سناریو جدا با دوره تناوب، زمان روشن، تکرار داخل روشن، گپ.
 *
 * @note    [EN] Naming per AI_CONTEXT.md: full type first, global UPPERCASE with G_, local lowercase.
 *          Memory: no malloc/free, only static/stack, small locals, const config in Flash.
 *          [FA] نام‌گذاری طبق AI: اول تایپ کامل. حافظه بدون malloc.
 */

#include "ui.h"
#include "ui_config.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* ===== Low-level helpers ===== */

/**
 * @brief  [EN] Green PB10 on/off.
 *         [FA] سبز PB10.
 * @param  bool_on [EN] true=on / روشن
 */
static void func_green(bool bool_on)
{
    func_BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, bool_on);
}

/**
 * @brief  [EN] Red PB0 on/off.
 *         [FA] قرمز PB0.
 * @param  bool_on [EN] true=on / روشن
 */
static void func_red(bool bool_on)
{
    func_BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, bool_on);
}

/**
 * @brief  [EN] Yellow PB1 on/off.
 *         [FA] زرد PB1.
 * @param  bool_on [EN] true=on / روشن
 */
static void func_yellow(bool bool_on)
{
    func_BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, bool_on);
}

/**
 * @brief  [EN] Buzzer PA4 on/off.
 *         [FA] بازر PA4.
 * @param  bool_on [EN] true=sound / صدا
 */
static void func_buzzer(bool bool_on)
{
    func_BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, bool_on);
}

/**
 * @brief  [EN] All off safe.
 *         [FA] همه خاموش امن.
 */
static void func_all_off(void)
{
    func_green(false);
    func_red(false);
    func_yellow(false);
    func_buzzer(false);
}

/**
 * @brief  [EN] Calculate beep ON per pulse inside onTime.
 *         If repeat=1 returns onTime (gap ignored). Else beepOn=(onTime-totalGap)/repeat.
 *         [FA] محاسبه زمان روشن هر بوق.
 * @param  uint32_t_onTimeMs [EN] Total including gaps / کل شامل گپ
 * @param  uint8_t_repeatCount [EN] Repeat 1..10 / تکرار
 * @param  uint32_t_gapMs [EN] Gap ms / گپ
 * @return uint32_t [EN] Beep ON per pulse / زمان هر پالس
 */
static uint32_t func_calc_beep_on(uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint32_t uint32_t_gapMs)
{
    uint32_t uint32_t_totalGap;
    uint32_t uint32_t_beepOn;

    if (uint8_t_repeatCount <= 1u)
    {
        return uint32_t_onTimeMs;
    }

    uint32_t_totalGap = (uint32_t)uint32_t_gapMs * (uint32_t)(uint8_t_repeatCount - 1u);

    if (uint32_t_totalGap >= uint32_t_onTimeMs)
    {
        uint32_t_beepOn = uint32_t_onTimeMs / (uint32_t)((uint8_t_repeatCount * 2u) - 1u);
        if (uint32_t_beepOn < 10u)
        {
            uint32_t_beepOn = 10u;
        }
        return uint32_t_beepOn;
    }

    uint32_t_beepOn = (uint32_t_onTimeMs - uint32_t_totalGap) / (uint32_t)uint8_t_repeatCount;
    if (uint32_t_beepOn < 10u)
    {
        uint32_t_beepOn = 10u;
    }
    return uint32_t_beepOn;
}

/* ===== Globals ===== */
static uint32_t UINT32_T_G_BeepCnt = 0u;

/* ===== Public ===== */

/**
 * @brief  [EN] Init safe: all off.
 *         [FA] Init امن: همه خاموش.
 */
void func_Ui_Init(void)
{
    func_all_off();
}

/**
 * @brief  [EN] Board test: R, Y, G, beep using new buzzer pattern.
 *         [FA] تست برد: قرمز، زرد، سبز، بوق با تابع جدید.
 */
void func_Ui_BoardTest(void)
{
    func_all_off();

    func_red(true);
    vTaskDelay(pdMS_TO_TICKS(UI_SELFTEST_LED_MS));
    func_red(false);

    func_yellow(true);
    vTaskDelay(pdMS_TO_TICKS(UI_SELFTEST_LED_MS));
    func_yellow(false);

    func_green(true);
    vTaskDelay(pdMS_TO_TICKS(UI_SELFTEST_LED_MS));
    func_green(false);

    /* [EN] Use new buzzer pattern: period 0 (once), onTime BOOT_BEEP_MS, repeat 1, gap 0
       [FA] استفاده از تابع جدید بازر: دوره ۰ یعنی یک‌بار، زمان روشن BOOT، تکرار ۱ */
    func_Ui_BuzzerPatternMs(0u, UI_BOOT_BEEP_MS, 1u, 0u);
}

/* ===== Buzzer pattern - 2 funcs only ===== */

/**
 * @brief  [EN] Buzzer pattern with gap ms.
 *         Inputs: period (repeat time), onTime, repeat inside onTime, gap ms.
 *         If repeat=1 gap ignored. Example: onTime=1000 repeat=2 gap=200 => ON400 OFF200 ON400.
 *         If period=0 or period<=onTime, only pattern once.
 *         [FA] الگوی بازر با گپ میلی‌ثانیه.
 * @param  uint32_t_periodMs [EN] Period 0=once, 0..60000ms / دوره تناوب
 * @param  uint32_t_onTimeMs [EN] Total ON including gaps, 10..10000ms / زمان روشن
 * @param  uint8_t_repeatCount [EN] Repeat inside ON 1..10 / تکرار داخل روشن
 * @param  uint32_t_gapMs [EN] Gap ms 0..5000, ignored if repeat=1 / گپ میلی‌ثانیه
 */
void func_Ui_BuzzerPatternMs(uint32_t uint32_t_periodMs, uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint32_t uint32_t_gapMs)
{
    uint32_t uint32_t_onTimeClamped;
    uint32_t uint32_t_gapClamped;
    uint8_t uint8_t_repeatClamped;
    uint32_t uint32_t_beepOnMs;
    uint8_t uint8_t_i;

    if (uint32_t_onTimeMs < 10u)
    {
        uint32_t_onTimeClamped = 10u;
    }
    else if (uint32_t_onTimeMs > 10000u)
    {
        uint32_t_onTimeClamped = 10000u;
    }
    else
    {
        uint32_t_onTimeClamped = uint32_t_onTimeMs;
    }

    if (uint32_t_gapMs > 5000u)
    {
        uint32_t_gapClamped = 5000u;
    }
    else
    {
        uint32_t_gapClamped = uint32_t_gapMs;
    }

    if (uint8_t_repeatCount == 0u)
    {
        uint8_t_repeatClamped = 1u;
    }
    else if (uint8_t_repeatCount > 10u)
    {
        uint8_t_repeatClamped = 10u;
    }
    else
    {
        uint8_t_repeatClamped = uint8_t_repeatCount;
    }

    if (uint8_t_repeatClamped <= 1u)
    {
        func_buzzer(true);
        vTaskDelay(pdMS_TO_TICKS(uint32_t_onTimeClamped));
        func_buzzer(false);
    }
    else
    {
        if ((uint32_t_gapClamped * (uint32_t)(uint8_t_repeatClamped - 1u)) >= uint32_t_onTimeClamped)
        {
            uint32_t_gapClamped = (uint32_t_onTimeClamped * UI_BUZZER_DEFAULT_GAP_PERCENT / 100u);
            if (uint8_t_repeatClamped > 1u)
            {
                uint32_t_gapClamped = uint32_t_gapClamped / (uint32_t)(uint8_t_repeatClamped - 1u);
            }
        }

        uint32_t_beepOnMs = func_calc_beep_on(uint32_t_onTimeClamped, uint8_t_repeatClamped, uint32_t_gapClamped);

        for (uint8_t_i = 0u; uint8_t_i < uint8_t_repeatClamped; uint8_t_i++)
        {
            func_buzzer(true);
            vTaskDelay(pdMS_TO_TICKS(uint32_t_beepOnMs));
            func_buzzer(false);

            if (uint8_t_i < (uint8_t_repeatClamped - 1u))
            {
                vTaskDelay(pdMS_TO_TICKS(uint32_t_gapClamped));
            }
        }
    }

    if ((uint32_t_periodMs == 0u) || (uint32_t_periodMs <= uint32_t_onTimeClamped))
    {
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(uint32_t_periodMs - uint32_t_onTimeClamped));
}

/**
 * @brief  [EN] Buzzer pattern with gap percent.
 *         gapMs = onTime * gapPercent /100, ignored if repeat=1.
 *         [FA] الگوی بازر با گپ درصدی.
 * @param  uint32_t_periodMs [EN] Period ms 0=once / دوره تناوب
 * @param  uint32_t_onTimeMs [EN] ON time ms / زمان روشن
 * @param  uint8_t_repeatCount [EN] Repeat inside ON / تکرار داخل روشن
 * @param  uint8_t_gapPercent [EN] Gap percent 0..90, ignored if repeat=1 / گپ درصدی
 */
void func_Ui_BuzzerPatternPercent(uint32_t uint32_t_periodMs, uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint8_t uint8_t_gapPercent)
{
    uint32_t uint32_t_gapMs;
    uint8_t uint8_t_gapPctClamped;

    if (uint8_t_gapPercent > 90u)
    {
        uint8_t_gapPctClamped = 90u;
    }
    else
    {
        uint8_t_gapPctClamped = uint8_t_gapPercent;
    }

    if (uint8_t_repeatCount <= 1u)
    {
        func_Ui_BuzzerPatternMs(uint32_t_periodMs, uint32_t_onTimeMs, uint8_t_repeatCount, 0u);
        return;
    }

    uint32_t_gapMs = (uint32_t_onTimeMs * (uint32_t)uint8_t_gapPctClamped) / 100u;

    func_Ui_BuzzerPatternMs(uint32_t_periodMs, uint32_t_onTimeMs, uint8_t_repeatCount, uint32_t_gapMs);
}

/**
 * @brief  [EN] Battery voltage to percent 0..100. 0%=21V 100%=28V.
 *         [FA] ولتاژ باتری به درصد.
 * @param  uint32_t_batteryMv [EN] Battery voltage mV, range 0..40000, clamped / ولتاژ باتری
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func_Ui_BatteryVoltageToPercent(uint32_t uint32_t_batteryMv)
{
    uint32_t uint32_t_range;
    uint32_t uint32_t_off;
    uint8_t uint8_t_pct;

    if (uint32_t_batteryMv <= UI_BAT_V_MIN_MV)
    {
        return 0u;
    }

    if (uint32_t_batteryMv >= UI_BAT_V_MAX_MV)
    {
        return UI_PERCENT_FULL;
    }

    uint32_t_range = UI_BAT_V_MAX_MV - UI_BAT_V_MIN_MV;
    uint32_t_off = uint32_t_batteryMv - UI_BAT_V_MIN_MV;

    if (uint32_range == 0u)
    {
        return 0u;
    }

    uint8_t_pct = (uint8_t)((uint32_t_off * 100u) / uint32_range);

    if (uint8_t_pct > UI_PERCENT_FULL)
    {
        uint8_t_pct = UI_PERCENT_FULL;
    }

    return uint8_t_pct;
}

/**
 * @brief  [EN] InputOk: green steady, others off. One cycle 500ms.
 *         [FA] ورودی عادی: سبز ثابت.
 */
void func_Ui_ScenarioInputOk(void)
{
    func_green(true);
    func_red(false);
    func_yellow(false);
    func_buzzer(false);

    vTaskDelay(pdMS_TO_TICKS(UI_INPUT_OK_POLL_MS));
}

/**
 * @brief  [EN] BatteryRun: V_in<20V, green blink ON=pct*10ms, yellow OFF, smart beep using new buzzer pattern.
 *         [FA] دشارژ: ورودی قطع، سبز چشمک، زرد خاموش، بوق هوشمند با تابع جدید.
 * @param  uint32_t_batteryMv [EN] Battery voltage mV, 21000=0% 28000=100%, range 21000..28000 / ولتاژ باتری
 */
void func_Ui_ScenarioBatteryRun(uint32_t uint32_t_batteryMv)
{
    uint8_t uint8_t_pct;
    uint32_t uint32_t_on;
    uint32_t uint32_t_off;
    uint32_t uint32_t_beepInt;
    uint32_t uint32_t_beepDur;
    bool bool_beepNow;

    uint8_t_pct = func_Ui_BatteryVoltageToPercent(uint32_t_batteryMv);

    uint32_t_off = (uint32_t)(UI_PERCENT_FULL - uint8_pct) * (UI_BLINK_PERIOD_MS / 100u);

    if (uint32_off < UI_GREEN_MIN_OFF_MS)
    {
        uint32_off = UI_GREEN_MIN_OFF_MS;
    }

    uint32_on = UI_BLINK_PERIOD_MS - uint32_off;

    func_red(false);
    func_yellow(false);

    func_green(true);
    vTaskDelay(pdMS_TO_TICKS(uint32_on));
    func_green(false);
    vTaskDelay(pdMS_TO_TICKS(uint32_off));

    if (uint8_pct >= UI_BEEP_START_PCT)
    {
        UINT32_T_G_BeepCnt = 0u;
        return;
    }

    uint32_beepInt = (uint32_t)uint8_pct;

    if (uint32_beepInt == 0u)
    {
        uint32_beepInt = 1u;
    }

    bool_beepNow = false;

    if (UINT32_T_G_BeepCnt >= uint32_beepInt)
    {
        bool_beepNow = true;
        UINT32_T_G_BeepCnt = 0u;
    }
    else
    {
        UINT32_T_G_BeepCnt++;
    }

    if (bool_beepNow == false)
    {
        return;
    }

    uint32_beepDur = UI_BEEP_BASE_MS;

    if (uint8_pct < UI_BEEP_DOUBLE_THRESH_PCT)
    {
        uint32_beepDur *= 2u;
    }

    /* [EN] Use new buzzer pattern: period 0 (once), onTime beepDur, repeat 1, gap ignored
       [FA] استفاده از تابع جدید بازر داخل سناریو LED: دوره ۰ یعنی یک‌بار، زمان روشن beepDur، تکرار ۱، گپ نادیده */
    func_Ui_BuzzerPatternMs(0u, uint32_beepDur, 1u, 0u);
}

/**
 * @brief  [EN] Charging: V_in>=20V and bat<100%. Green steady, yellow remaining to full.
 *         0%=yellow ON, 100%=OFF, ON=(100-pct)*period.
 *         [FA] شارژ: ورودی وصل و باتری زیر فول.
 * @param  uint32_t_batteryMv [EN] Battery voltage mV, 21000=0% ON, 28000=100% OFF, range 21000..28000 / ولتاژ باتری
 */
void func_Ui_ScenarioCharging(uint32_t uint32_t_batteryMv)
{
    uint8_t uint8_t_pct;
    uint32_t uint32_t_on;
    uint32_t uint32_t_off;

    uint8_t_pct = func_Ui_BatteryVoltageToPercent(uint32_t_batteryMv);

    if (uint8_pct >= UI_PERCENT_FULL)
    {
        func_yellow(false);
        func_green(true);
        func_red(false);
        func_buzzer(false);
        vTaskDelay(pdMS_TO_TICKS(UI_CHARGING_BLINK_PERIOD_MS));
        return;
    }

    if (uint8_pct == 0u)
    {
        func_yellow(true);
        func_green(true);
        func_red(false);
        func_buzzer(false);
        vTaskDelay(pdMS_TO_TICKS(UI_CHARGING_BLINK_PERIOD_MS));
        return;
    }

    uint32_on = (uint32_t)(UI_PERCENT_FULL - uint8_pct) * (UI_CHARGING_BLINK_PERIOD_MS / 100u);

    if (uint32_on < UI_CHARGING_YELLOW_MIN_OFF_MS)
    {
        uint32_on = UI_CHARGING_YELLOW_MIN_OFF_MS;
    }

    if (uint32_on > UI_CHARGING_BLINK_PERIOD_MS)
    {
        uint32_on = UI_CHARGING_BLINK_PERIOD_MS;
    }

    uint32_off = UI_CHARGING_BLINK_PERIOD_MS - uint32_on;

    func_green(true);
    func_red(false);
    func_buzzer(false);

    func_yellow(true);
    vTaskDelay(pdMS_TO_TICKS(uint32_on));
    func_yellow(false);
    vTaskDelay(pdMS_TO_TICKS(uint32_off));
}
