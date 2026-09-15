/**
 * @file    ui.c
 * @brief   [EN] LED/buzzer scenarios - readable, full type names, func_ prefix.
 *          Buzzer pattern separate scenario with period, onTime, repeat, gap.
 *          [FA] سناریوهای LED/بازر - خوانا، نام تایپ کامل، پیشوند func_.
 *          الگوی بازر سناریو جدا با دوره تناوب، زمان روشن، تکرار داخل روشن، گپ.
 *
 * @note    [EN] Naming per AI_CONTEXT.md:
 *          - Variables: full type first, global UPPERCASE with G_, local lowercase.
 *          - Names must be related to their work/purpose, not generic tmp/val.
 *          - Constants: module prefix (UI_) shows file/module, dot not valid in C, UI_ already means ui_config.h single source.
 *          - Memory: no malloc/free, only static/stack, small locals, const config in Flash.
 *          [FA] نام‌گذاری طبق AI: اول تایپ کامل، نام مرتبط با کار، ثابت با پیشوند ماژول.
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
 * @param  bool_greenOn [EN] true=green on / سبز روشن
 */
static void func_green(bool bool_greenOn)
{
    func_BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, bool_greenOn);
}

/**
 * @brief  [EN] Red PB0 on/off.
 *         [FA] قرمز PB0.
 * @param  bool_redOn [EN] true=red on / قرمز روشن
 */
static void func_red(bool bool_redOn)
{
    func_BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, bool_redOn);
}

/**
 * @brief  [EN] Yellow PB1 on/off.
 *         [FA] زرد PB1.
 * @param  bool_yellowOn [EN] true=yellow on / زرد روشن
 */
static void func_yellow(bool bool_yellowOn)
{
    func_BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, bool_yellowOn);
}

/**
 * @brief  [EN] Buzzer PA4 on/off.
 *         [FA] بازر PA4.
 * @param  bool_buzzerOn [EN] true=buzzer sound / صدای بازر
 */
static void func_buzzer(bool bool_buzzerOn)
{
    func_BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, bool_buzzerOn);
}

/**
 * @brief  [EN] All UI outputs off safe.
 *         [FA] همه خروجی‌های UI خاموش امن.
 */
static void func_all_off(void)
{
    func_green(false);
    func_red(false);
    func_yellow(false);
    func_buzzer(false);
}

/**
 * @brief  [EN] Calculate beep ON per pulse inside total ON time.
 *         If repeat=1 returns total ON (gap ignored). Else beepOn=(totalON-totalGap)/repeat.
 *         [FA] محاسبه زمان روشن هر پالس بوق داخل زمان کل روشن.
 * @param  uint32_t_buzzerTotalOnMs [EN] Total including gaps, battery buzzer pattern / کل زمان روشن بازر شامل گپ
 * @param  uint8_t_buzzerRepeatCount [EN] Repeat 1..10, how many beeps inside total / تعداد بوق داخل کل
 * @param  uint32_t_buzzerGapMs [EN] Gap ms between beeps / گپ بین بوق‌ها
 * @return uint32_t [EN] Beep ON per pulse ms / زمان روشن هر پالس
 */
static uint32_t func_calc_beep_on(uint32_t uint32_t_buzzerTotalOnMs, uint8_t uint8_t_buzzerRepeatCount, uint32_t uint32_t_buzzerGapMs)
{
    uint32_t uint32_t_buzzerTotalGapMs;
    uint32_t uint32_t_buzzerPulseOnMs;

    if (uint8_t_buzzerRepeatCount <= 1u)
    {
        return uint32_t_buzzerTotalOnMs;
    }

    uint32_t_buzzerTotalGapMs = (uint32_t)uint32_t_buzzerGapMs * (uint32_t)(uint8_t_buzzerRepeatCount - 1u);

    if (uint32_t_buzzerTotalGapMs >= uint32_t_buzzerTotalOnMs)
    {
        uint32_t_buzzerPulseOnMs = uint32_t_buzzerTotalOnMs / (uint32_t)((uint8_t_buzzerRepeatCount * 2u) - 1u);
        if (uint32_t_buzzerPulseOnMs < 10u)
        {
            uint32_t_buzzerPulseOnMs = 10u;
        }
        return uint32_t_buzzerPulseOnMs;
    }

    uint32_t_buzzerPulseOnMs = (uint32_t_buzzerTotalOnMs - uint32_t_buzzerTotalGapMs) / (uint32_t)uint8_t_buzzerRepeatCount;
    if (uint32_t_buzzerPulseOnMs < 10u)
    {
        uint32_t_buzzerPulseOnMs = 10u;
    }
    return uint32_t_buzzerPulseOnMs;
}

/* ===== Globals - meaningful name related to work ===== */
static uint32_t UINT32_T_G_UiBatteryRunBeepCycleCnt = 0u;

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

/* ===== Buzzer pattern - 2 funcs only, meaningful names ===== */

/**
 * @brief  [EN] Buzzer pattern with gap ms.
 *         Inputs: period (repeat time), onTime, repeat inside onTime, gap ms.
 *         If repeat=1 gap ignored. Example: onTime=1000 repeat=2 gap=200 => ON400 OFF200 ON400.
 *         If period=0 or period<=onTime, only pattern once.
 *         [FA] الگوی بازر با گپ میلی‌ثانیه.
 * @param  uint32_t_periodMs [EN] Period 0=once, 0..60000ms / دوره تناوب تکرار بوق
 * @param  uint32_t_onTimeMs [EN] Total ON including gaps, 10..10000ms / زمان روشن بودن بوق
 * @param  uint8_t_repeatCount [EN] Repeat inside ON 1..10 / تکرار زمان روشن بودن
 * @param  uint32_t_gapMs [EN] Gap ms 0..5000, ignored if repeat=1 / گپ روشن بودن میلی‌ثانیه، اگر تکرار ۱ بود نادیده
 */
void func_Ui_BuzzerPatternMs(uint32_t uint32_t_periodMs, uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint32_t uint32_t_gapMs)
{
    uint32_t uint32_t_buzzerTotalOnMs;
    uint32_t uint32_t_buzzerGapMs;
    uint8_t uint8_t_buzzerRepeatCount;
    uint32_t uint32_t_buzzerPulseOnMs;
    uint8_t uint8_t_buzzerPulseIndex;

    if (uint32_t_onTimeMs < 10u)
    {
        uint32_t_buzzerTotalOnMs = 10u;
    }
    else if (uint32_t_onTimeMs > 10000u)
    {
        uint32_t_buzzerTotalOnMs = 10000u;
    }
    else
    {
        uint32_t_buzzerTotalOnMs = uint32_t_onTimeMs;
    }

    if (uint32_t_gapMs > 5000u)
    {
        uint32_t_buzzerGapMs = 5000u;
    }
    else
    {
        uint32_t_buzzerGapMs = uint32_t_gapMs;
    }

    if (uint8_t_repeatCount == 0u)
    {
        uint8_t_buzzerRepeatCount = 1u;
    }
    else if (uint8_t_repeatCount > 10u)
    {
        uint8_t_buzzerRepeatCount = 10u;
    }
    else
    {
        uint8_t_buzzerRepeatCount = uint8_t_repeatCount;
    }

    if (uint8_t_buzzerRepeatCount <= 1u)
    {
        func_buzzer(true);
        vTaskDelay(pdMS_TO_TICKS(uint32_t_buzzerTotalOnMs));
        func_buzzer(false);
    }
    else
    {
        if ((uint32_t_buzzerGapMs * (uint32_t)(uint8_t_buzzerRepeatCount - 1u)) >= uint32_t_buzzerTotalOnMs)
        {
            uint32_t_buzzerGapMs = (uint32_t_buzzerTotalOnMs * UI_BUZZER_DEFAULT_GAP_PERCENT / 100u);
            if (uint8_t_buzzerRepeatCount > 1u)
            {
                uint32_t_buzzerGapMs = uint32_t_buzzerGapMs / (uint32_t)(uint8_t_buzzerRepeatCount - 1u);
            }
        }

        uint32_t_buzzerPulseOnMs = func_calc_beep_on(uint32_t_buzzerTotalOnMs, uint8_t_buzzerRepeatCount, uint32_t_buzzerGapMs);

        for (uint8_t_buzzerPulseIndex = 0u; uint8_t_buzzerPulseIndex < uint8_t_buzzerRepeatCount; uint8_t_buzzerPulseIndex++)
        {
            func_buzzer(true);
            vTaskDelay(pdMS_TO_TICKS(uint32_t_buzzerPulseOnMs));
            func_buzzer(false);

            if (uint8_t_buzzerPulseIndex < (uint8_t_buzzerRepeatCount - 1u))
            {
                vTaskDelay(pdMS_TO_TICKS(uint32_t_buzzerGapMs));
            }
        }
    }

    if ((uint32_t_periodMs == 0u) || (uint32_t_periodMs <= uint32_t_buzzerTotalOnMs))
    {
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(uint32_t_periodMs - uint32_t_buzzerTotalOnMs));
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
    uint32_t uint32_t_buzzerGapMs;
    uint8_t uint8_t_buzzerGapPercentClamped;

    if (uint8_t_gapPercent > 90u)
    {
        uint8_t_buzzerGapPercentClamped = 90u;
    }
    else
    {
        uint8_t_buzzerGapPercentClamped = uint8_t_gapPercent;
    }

    if (uint8_t_repeatCount <= 1u)
    {
        func_Ui_BuzzerPatternMs(uint32_t_periodMs, uint32_t_onTimeMs, uint8_t_repeatCount, 0u);
        return;
    }

    uint32_t_buzzerGapMs = (uint32_t_onTimeMs * (uint32_t)uint8_t_buzzerGapPercentClamped) / 100u;

    func_Ui_BuzzerPatternMs(uint32_t_periodMs, uint32_t_onTimeMs, uint8_t_repeatCount, uint32_t_buzzerGapMs);
}

/**
 * @brief  [EN] Battery voltage to percent 0..100. 0%=21V 100%=28V.
 *         [FA] ولتاژ باتری به درصد.
 * @param  uint32_t_batteryMv [EN] Battery voltage mV, range 0..40000, clamped / ولتاژ باتری
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func_Ui_BatteryVoltageToPercent(uint32_t uint32_t_batteryMv)
{
    uint32_t uint32_t_voltageRangeMv;
    uint32_t uint32_t_voltageOffsetMv;
    uint8_t uint8_t_batteryPercent;

    if (uint32_t_batteryMv <= UI_BAT_V_MIN_MV)
    {
        return 0u;
    }

    if (uint32_t_batteryMv >= UI_BAT_V_MAX_MV)
    {
        return UI_PERCENT_FULL;
    }

    uint32_t_voltageRangeMv = UI_BAT_V_MAX_MV - UI_BAT_V_MIN_MV;
    uint32_t_voltageOffsetMv = uint32_t_batteryMv - UI_BAT_V_MIN_MV;

    if (uint32_voltageRangeMv == 0u)
    {
        return 0u;
    }

    uint8_t_batteryPercent = (uint8_t)((uint32_t_voltageOffsetMv * 100u) / uint32_voltageRangeMv);

    if (uint8_t_batteryPercent > UI_PERCENT_FULL)
    {
        uint8_t_batteryPercent = UI_PERCENT_FULL;
    }

    return uint8_t_batteryPercent;
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
    uint8_t uint8_t_batteryPercent;
    uint32_t uint32_t_greenBlinkOnMs;
    uint32_t uint32_t_greenBlinkOffMs;
    uint32_t uint32_t_beepIntervalCycles;
    uint32_t uint32_t_beepDurationMs;
    bool bool_shouldBeepNow;

    uint8_t_batteryPercent = func_Ui_BatteryVoltageToPercent(uint32_t_batteryMv);

    uint32_t_greenBlinkOffMs = (uint32_t)(UI_PERCENT_FULL - uint8_batteryPercent) * (UI_BLINK_PERIOD_MS / 100u);

    if (uint32_greenBlinkOffMs < UI_GREEN_MIN_OFF_MS)
    {
        uint32_greenBlinkOffMs = UI_GREEN_MIN_OFF_MS;
    }

    uint32_t_greenBlinkOnMs = UI_BLINK_PERIOD_MS - uint32_greenBlinkOffMs;

    func_red(false);
    func_yellow(false);

    func_green(true);
    vTaskDelay(pdMS_TO_TICKS(uint32_t_greenBlinkOnMs));
    func_green(false);
    vTaskDelay(pdMS_TO_TICKS(uint32_t_greenBlinkOffMs));

    if (uint8_batteryPercent >= UI_BEEP_START_PCT)
    {
        UINT32_T_G_UiBatteryRunBeepCycleCnt = 0u;
        return;
    }

    uint32_t_beepIntervalCycles = (uint32_t)uint8_batteryPercent;

    if (uint32_beepIntervalCycles == 0u)
    {
        uint32_beepIntervalCycles = 1u;
    }

    bool_shouldBeepNow = false;

    if (UINT32_T_G_UiBatteryRunBeepCycleCnt >= uint32_beepIntervalCycles)
    {
        bool_shouldBeepNow = true;
        UINT32_T_G_UiBatteryRunBeepCycleCnt = 0u;
    }
    else
    {
        UINT32_T_G_UiBatteryRunBeepCycleCnt++;
    }

    if (bool_shouldBeepNow == false)
    {
        return;
    }

    uint32_t_beepDurationMs = UI_BEEP_BASE_MS;

    if (uint8_batteryPercent < UI_BEEP_DOUBLE_THRESH_PCT)
    {
        uint32_t_beepDurationMs *= 2u;
    }

    /* [EN] Use new buzzer pattern inside LED scenario: period 0 (once), onTime beepDuration, repeat 1, gap ignored
       [FA] استفاده از تابع جدید بازر داخل سناریو LED: دوره ۰ یعنی یک‌بار، زمان روشن beepDur، تکرار ۱، گپ نادیده */
    func_Ui_BuzzerPatternMs(0u, uint32_t_beepDurationMs, 1u, 0u);
}

/**
 * @brief  [EN] Charging: V_in>=20V and bat<100%. Green steady, yellow remaining to full.
 *         0%=yellow ON, 100%=OFF, ON=(100-pct)*period.
 *         [FA] شارژ: ورودی وصل و باتری زیر فول.
 * @param  uint32_t_batteryMv [EN] Battery voltage mV, 21000=0% ON, 28000=100% OFF, range 21000..28000 / ولتاژ باتری
 */
void func_Ui_ScenarioCharging(uint32_t uint32_t_batteryMv)
{
    uint8_t uint8_t_batteryPercent;
    uint32_t uint32_t_yellowOnMs;
    uint32_t uint32_t_yellowOffMs;

    uint8_t_batteryPercent = func_Ui_BatteryVoltageToPercent(uint32_t_batteryMv);

    if (uint8_batteryPercent >= UI_PERCENT_FULL)
    {
        func_yellow(false);
        func_green(true);
        func_red(false);
        func_buzzer(false);
        vTaskDelay(pdMS_TO_TICKS(UI_CHARGING_BLINK_PERIOD_MS));
        return;
    }

    if (uint8_batteryPercent == 0u)
    {
        func_yellow(true);
        func_green(true);
        func_red(false);
        func_buzzer(false);
        vTaskDelay(pdMS_TO_TICKS(UI_CHARGING_BLINK_PERIOD_MS));
        return;
    }

    uint32_t_yellowOnMs = (uint32_t)(UI_PERCENT_FULL - uint8_batteryPercent) * (UI_CHARGING_BLINK_PERIOD_MS / 100u);

    if (uint32_t_yellowOnMs < UI_CHARGING_YELLOW_MIN_OFF_MS)
    {
        uint32_t_yellowOnMs = UI_CHARGING_YELLOW_MIN_OFF_MS;
    }

    if (uint32_t_yellowOnMs > UI_CHARGING_BLINK_PERIOD_MS)
    {
        uint32_t_yellowOnMs = UI_CHARGING_BLINK_PERIOD_MS;
    }

    uint32_t_yellowOffMs = UI_CHARGING_BLINK_PERIOD_MS - uint32_t_yellowOnMs;

    func_green(true);
    func_red(false);
    func_buzzer(false);

    func_yellow(true);
    vTaskDelay(pdMS_TO_TICKS(uint32_t_yellowOnMs));
    func_yellow(false);
    vTaskDelay(pdMS_TO_TICKS(uint32_t_yellowOffMs));
}
