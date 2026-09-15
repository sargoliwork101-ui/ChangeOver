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
 *          [FA] نام‌گذاری طبق AI:
 *          - متغیر: اول تایپ کامل بعد نام. گلوبال حروف بزرگ با G_: UINT32_T_G_...
 *          - تابع خودمان: پیشوند func_، سیستمی دست نزن.
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
