/**
 * @file    ui.c
 * @brief   [EN] LED/buzzer scenarios - readable, full type names, func_ prefix for our functions.
 *          [FA] سناریوهای LED/بازر - خوانا، نام تایپ کامل، پیشوند func_ برای توابع خودمان.
 *
 * @note    [EN] Naming per AI_CONTEXT.md:
 *          - Variables: full type first, then name. Global UPPERCASE with G_: UINT32_T_G_..., UINT8_T_G_..., BOOL_G_...
 *            Local lowercase: uint32_t_..., uint8_t_..., bool_...
 *          - Functions we write: func_ prefix, system functions (HAL, FreeRTOS) untouched.
 *          - Thresholds in ui_config.h single file.
 *          - Memory: no malloc/free, only static/stack, small locals, const config in Flash.
 *          [FA] نام‌گذاری طبق AI:
 *          - متغیر: اول تایپ کامل بعد نام. گلوبال حروف بزرگ با G_: UINT32_T_G_...
 *          - تابع خودمان: پیشوند func_، سیستمی دست نزن.
 *          - حافظه: بدون malloc، فقط استاتیک/استک.
 */

#include "ui.h"
#include "ui_config.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* ===== Simple low-level helpers - func_ prefix, full type param ===== */

/**
 * @brief  [EN] Green PB10 on/off.
 *         [FA] سبز PB10.
 * @param  bool_on [EN] true=on via Q6, false=off / روشن/خاموش
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
 * @param  bool_on [EN] true=on via Q5 / روشن
 */
static void func_yellow(bool bool_on)
{
    func_BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, bool_on);
}

/**
 * @brief  [EN] Buzzer PA4 on/off low-level.
 *         [FA] بازر PA4 سطح پایین.
 * @param  bool_on [EN] true=sound via Q7 / صدا
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
 * @brief  [EN] Clamp uint32 to range.
 *         [FA] محدود کردن به بازه.
 * @param  uint32_t_value [EN] Value / مقدار
 * @param  uint32_t_min [EN] Min / کمینه
 * @param  uint32_t_max [EN] Max / بیشینه
 * @return uint32_t [EN] Clamped value / مقدار محدود شده
 */
static uint32_t func_clamp_u32(uint32_t uint32_t_value, uint32_t uint32_t_min, uint32_t uint32_t_max)
{
    if (uint32_t_value < uint32_t_min)
    {
        return uint32_t_min;
    }
    if (uint32_t_value > uint32_t_max)
    {
        return uint32_t_max;
    }
    return uint32_t_value;
}

/**
 * @brief  [EN] Calculate beep ON duration inside onTime with repeat and gap.
 *         If repeat=1, returns onTime (gap ignored).
 *         Else beepOn = (onTime - totalGap)/repeat, totalGap=gap*(repeat-1).
 *         If totalGap>=onTime, beepOn is clamped to min.
 *         [FA] محاسبه زمان روشن هر بوق داخل زمان کل روشن با تکرار و گپ.
 *         اگر تکرار ۱ بود کل زمان روشن برمی‌گردد.
 * @param  uint32_t_onTimeMs [EN] Total time including gaps / کل زمان شامل گپ
 * @param  uint8_t_repeatCount [EN] Repeat count 1..10 / تعداد تکرار
 * @param  uint32_t_gapMs [EN] Gap ms / گپ میلی‌ثانیه
 * @return uint32_t [EN] Beep ON ms per pulse / زمان روشن هر پالس
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
        /* [EN] Gap too large, clamp: distribute onTime into (repeat*2-1) slots, beep gets half
           [FA] گپ خیلی بزرگ، تقسیم می‌کنیم */
        uint32_t_beepOn = uint32_t_onTimeMs / (uint32_t)((uint8_t_repeatCount * 2u) - 1u);
        if (uint32_t_beepOn < UI_BUZZER_MIN_ON_MS)
        {
            uint32_t_beepOn = UI_BUZZER_MIN_ON_MS;
        }
        return uint32_t_beepOn;
    }

    uint32_t_beepOn = (uint32_t_onTimeMs - uint32_t_totalGap) / (uint32_t)uint8_t_repeatCount;

    if (uint32_t_beepOn < UI_BUZZER_MIN_ON_MS)
    {
        uint32_t_beepOn = UI_BUZZER_MIN_ON_MS;
    }

    return uint32_t_beepOn;
}

/* ===== File-scope globals - full type uppercase ===== */
static uint32_t UINT32_T_G_BeepCnt = 0u;

/* ===== Public - func_ prefix ===== */

/**
 * @brief  [EN] Init safe: all off. Called once before scheduler.
 *         [FA] Init امن: همه خاموش. یک‌بار قبل زمان‌بند.
 */
void func_Ui_Init(void)
{
    func_all_off();
}

/**
 * @brief  [EN] Board test: R, Y, G, beep linear readable.
 *         [FA] تست برد: قرمز، زرد، سبز، بوق.
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

    func_buzzer(true);
    vTaskDelay(pdMS_TO_TICKS(UI_BOOT_BEEP_MS));
    func_buzzer(false);
}

/**
 * @brief  [EN] Separate buzzer beep.
 *         [FA] بوق جدا.
 * @param  uint32_t_durationMs [EN] Duration ms, 0=base 250ms, range 0..5000 / طول بوق
 */
void func_Ui_BuzzerBeep(uint32_t uint32_t_durationMs)
{
    uint32_t uint32_t_d;

    uint32_t_d = uint32_t_durationMs;

    if (uint32_t_d == 0u)
    {
        uint32_t_d = UI_BEEP_BASE_MS;
    }

    func_buzzer(true);
    vTaskDelay(pdMS_TO_TICKS(uint32_t_d));
    func_buzzer(false);
}

/* ===== Buzzer pattern - separate scenario ===== */

/**
 * @brief  [EN] Buzzer pattern once with gap in ms.
 *         If repeat=1, gap ignored, buzzer ON for onTime.
 *         Example: onTime=1000, repeat=2, gap=200 => ON 400 OFF 200 ON 400.
 *         [FA] الگوی بازر یک‌باره با گپ میلی‌ثانیه.
 *         اگر تکرار ۱ بود گپ حساب نمی‌شود.
 * @param  uint32_t_onTimeMs [EN] Total ON time including gaps, 10..10000ms / کل زمان روشن شامل گپ‌ها
 * @param  uint8_t_repeatCount [EN] Repeat inside ON, 1..10 / تکرار داخل روشن
 * @param  uint32_t_gapMs [EN] Gap ms, 0..5000, ignored if repeat=1 / گپ میلی‌ثانیه
 */
void func_Ui_BuzzerPatternOnceMs(uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint32_t uint32_t_gapMs)
{
    uint32_t uint32_t_onTimeClamped;
    uint32_t uint32_t_gapClamped;
    uint8_t uint8_t_repeatClamped;
    uint32_t uint32_t_beepOnMs;
    uint32_t uint32_t_totalGap;
    uint8_t uint8_t_i;

    uint32_t_onTimeClamped = func_clamp_u32(uint32_t_onTimeMs, UI_BUZZER_MIN_ON_MS, UI_BUZZER_MAX_ON_MS);
    uint32_t_gapClamped = func_clamp_u32(uint32_t_gapMs, 0u, UI_BUZZER_MAX_GAP_MS);

    if (uint8_t_repeatCount == 0u)
    {
        uint8_t_repeatClamped = 1u;
    }
    else if (uint8_t_repeatCount > UI_BUZZER_MAX_REPEAT)
    {
        uint8_t_repeatClamped = UI_BUZZER_MAX_REPEAT;
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
        return;
    }

    uint32_t_totalGap = uint32_t_gapClamped * (uint32_t)(uint8_t_repeatClamped - 1u);

    if (uint32_t_totalGap >= uint32_t_onTimeClamped)
    {
        /* [EN] Recalc gap to fit: gap = onTime * 20% / (repeat-1) as fallback
           [FA] گپ بزرگ، از پیش‌فرض ۲۰٪ استفاده می‌کنیم */
        uint32_t_gapClamped = (uint32_t_onTimeClamped * UI_BUZZER_DEFAULT_GAP_PERCENT / 100u);
        if (uint8_t_repeatClamped > 1u)
        {
            uint32_t_gapClamped = uint32_t_gapClamped / (uint32_t)(uint8_t_repeatClamped - 1u);
        }
        if (uint32_t_gapClamped > UI_BUZZER_MAX_GAP_MS)
        {
            uint32_t_gapClamped = UI_BUZZER_MAX_GAP_MS;
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

/**
 * @brief  [EN] Buzzer pattern once with gap percent.
 *         gapMs = onTime * gapPercent /100, ignored if repeat=1.
 *         [FA] الگوی بازر یک‌باره با گپ درصدی.
 * @param  uint32_t_onTimeMs [EN] Total ON time including gaps, 10..10000ms / کل زمان روشن
 * @param  uint8_t_repeatCount [EN] Repeat inside ON, 1..10 / تکرار داخل روشن
 * @param  uint8_t_gapPercent [EN] Gap percent 0..90%, ignored if repeat=1 / گپ درصدی
 */
void func_Ui_BuzzerPatternOncePercent(uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint8_t uint8_t_gapPercent)
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
        func_Ui_BuzzerPatternOnceMs(uint32_t_onTimeMs, uint8_t_repeatCount, 0u);
        return;
    }

    uint32_t_gapMs = (uint32_t_onTimeMs * (uint32_t)uint8_t_gapPctClamped) / 100u;

    func_Ui_BuzzerPatternOnceMs(uint32_t_onTimeMs, uint8_t_repeatCount, uint32_t_gapMs);
}

/**
 * @brief  [EN] Buzzer pattern periodic: pattern + off = period-onTime.
 *         If period=0 or period<=onTime, only pattern once.
 *         [FA] الگوی بازر دوره‌ای: الگو + خاموشی تا دوره کامل.
 * @param  uint32_t_periodMs [EN] Period ms, 0=once, 0..60000 / دوره تناوب
 * @param  uint32_t_onTimeMs [EN] ON time ms, 10..10000 / زمان روشن
 * @param  uint8_t_repeatCount [EN] Repeat inside ON, 1..10 / تکرار داخل روشن
 * @param  uint32_t_gapMs [EN] Gap ms, 0..5000, ignored if repeat=1 / گپ
 */
void func_Ui_BuzzerPatternPeriodicMs(uint32_t uint32_t_periodMs, uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint32_t uint32_t_gapMs)
{
    uint32_t uint32_t_periodClamped;
    uint32_t uint32_t_onTimeClamped;

    uint32_t_periodClamped = func_clamp_u32(uint32_t_periodMs, 0u, UI_BUZZER_MAX_PERIOD_MS);
    uint32_t_onTimeClamped = func_clamp_u32(uint32_t_onTimeMs, UI_BUZZER_MIN_ON_MS, UI_BUZZER_MAX_ON_MS);

    func_Ui_BuzzerPatternOnceMs(uint32_t_onTimeClamped, uint8_t_repeatCount, uint32_t_gapMs);

    if ((uint32_t_periodClamped == 0u) || (uint32_t_periodClamped <= uint32_t_onTimeClamped))
    {
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(uint32_t_periodClamped - uint32_t_onTimeClamped));
}

/**
 * @brief  [EN] Buzzer pattern periodic with gap percent.
 *         [FA] الگوی بازر دوره‌ای با گپ درصدی.
 * @param  uint32_t_periodMs [EN] Period ms, 0=once / دوره تناوب
 * @param  uint32_t_onTimeMs [EN] ON time ms / زمان روشن
 * @param  uint8_t_repeatCount [EN] Repeat inside ON / تکرار داخل روشن
 * @param  uint8_t_gapPercent [EN] Gap percent / گپ درصدی
 */
void func_Ui_BuzzerPatternPeriodicPercent(uint32_t uint32_t_periodMs, uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint8_t uint8_t_gapPercent)
{
    uint32_t uint32_t_gapMs;
    uint8_t uint8_t_gapPctClamped;
    uint32_t uint32_t_periodClamped;
    uint32_t uint32_t_onTimeClamped;

    uint32_t_periodClamped = func_clamp_u32(uint32_t_periodMs, 0u, UI_BUZZER_MAX_PERIOD_MS);
    uint32_t_onTimeClamped = func_clamp_u32(uint32_t_onTimeMs, UI_BUZZER_MIN_ON_MS, UI_BUZZER_MAX_ON_MS);

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
        func_Ui_BuzzerPatternPeriodicMs(uint32_t_periodClamped, uint32_t_onTimeClamped, uint8_t_repeatCount, 0u);
        return;
    }

    uint32_t_gapMs = (uint32_t_onTimeClamped * (uint32_t)uint8_t_gapPctClamped) / 100u;

    func_Ui_BuzzerPatternPeriodicMs(uint32_t_periodClamped, uint32_t_onTimeClamped, uint8_t_repeatCount, uint32_t_gapMs);
}

/**
 * @brief  [EN] Repeat pattern N times with period.
 *         [FA] تکرار الگو N بار با دوره.
 * @param  uint32_t_periodMs [EN] Period ms / دوره
 * @param  uint32_t_onTimeMs [EN] ON time ms / زمان روشن
 * @param  uint8_t_repeatCount [EN] Repeat inside ON / تکرار داخل روشن
 * @param  uint32_t_gapMs [EN] Gap ms / گپ
 * @param  uint32_t_repeatTimes [EN] How many periods, 0=1 / تعداد تکرار دوره
 */
void func_Ui_BuzzerPatternRepeatMs(uint32_t uint32_t_periodMs, uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint32_t uint32_t_gapMs, uint32_t uint32_t_repeatTimes)
{
    uint32_t uint32_t_times;
    uint32_t uint32_t_i;

    if (uint32_t_repeatTimes == 0u)
    {
        uint32_t_times = 1u;
    }
    else
    {
        uint32_t_times = uint32_t_repeatTimes;
    }

    if (uint32_t_times > 1000u)
    {
        uint32_t_times = 1000u;
    }

    for (uint32_t_i = 0u; uint32_t_i < uint32_t_times; uint32_t_i++)
    {
        func_Ui_BuzzerPatternPeriodicMs(uint32_t_periodMs, uint32_t_onTimeMs, uint8_t_repeatCount, uint32_t_gapMs);
    }
}

/**
 * @brief  [EN] Repeat with gap percent.
 *         [FA] تکرار با گپ درصدی.
 * @param  uint32_t_periodMs [EN] Period ms / دوره
 * @param  uint32_t_onTimeMs [EN] ON time ms / زمان روشن
 * @param  uint8_t_repeatCount [EN] Repeat inside ON / تکرار داخل روشن
 * @param  uint8_t_gapPercent [EN] Gap percent / گپ درصدی
 * @param  uint32_t_repeatTimes [EN] Repeat times / تعداد تکرار
 */
void func_Ui_BuzzerPatternRepeatPercent(uint32_t uint32_t_periodMs, uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint8_t uint8_t_gapPercent, uint32_t uint32_t_repeatTimes)
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
        func_Ui_BuzzerPatternRepeatMs(uint32_t_periodMs, uint32_t_onTimeMs, uint8_t_repeatCount, 0u, uint32_t_repeatTimes);
        return;
    }

    uint32_t_gapMs = (uint32_t_onTimeMs * (uint32_t)uint8_t_gapPctClamped) / 100u;

    func_Ui_BuzzerPatternRepeatMs(uint32_t_periodMs, uint32_t_onTimeMs, uint8_t_repeatCount, uint32_t_gapMs, uint32_t_repeatTimes);
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
 * @brief  [EN] BatteryRun: V_in<20V, green blink ON=pct*10ms, yellow OFF, smart beep.
 *         [FA] دشارژ: ورودی قطع، سبز چشمک، زرد خاموش، بوق هوشمند.
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

    func_Ui_BuzzerBeep(uint32_beepDur);
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
