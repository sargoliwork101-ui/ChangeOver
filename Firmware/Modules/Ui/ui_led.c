/**
 * @file    ui_led.c
 * @brief   [EN] UI LED scenarios - green/red/yellow, battery percent, InputOk/Charging/BatteryRun.
 *          Split from UI into LED and BUZZER per user request. Constants for LED in ui_led.h.
 *          RTOS simple readable, non-linear formulas, markers above each function and variable in h and c.
 *          [FA] سناریوهای LED ماژول UI - ثابت‌های LED در هدر خودش، هر تابع و متغیر با جدا کننده و کامنت.
 *
 * @note    [EN] ui_led.h provides defaults; runtime-tunable values are read from const APP_CONFIG. Naming __ after type, func__ prefix.
 *          RTOS: vTaskDelay allowed, HAL_Delay forbidden. Formulas non-linear broken into steps.
 *          [FA] ui_led.h پیش‌فرض‌ها را می‌دهد؛ مقدارهای قابل تنظیم زمان اجرا از APP_CONFIG ثابت خوانده می‌شوند. نام‌گذاری با __، پیشوند func__، فرمول غیرخطی.
 */

#include "ui_led.h"
#include "ui_buzzer.h"
#include "app_config.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* ==================== Battery Voltage To Percent ==================== */

/**
 * @brief  [EN] Battery voltage to percent 0..100. Non-linear formula broken into 4 steps: range, offset, scaled, percent.
 *         [FA] ولتاژ باتری به درصد - فرمول غیرخطی ۴ گام: بازه، فاصله، مقیاس، درصد.
 * @param  uint32_t__batteryMv [EN] Battery voltage in mV, 0..40000mV, 21000=0% 28000=100% / ولتاژ باتری میلی‌ولت
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func__Ui_BatteryVoltageToPercent(uint32_t uint32_t__batteryMv)
{
    uint32_t uint32_t__voltageRangeMv;
    uint32_t uint32_t__voltageOffsetMv;
    uint32_t uint32_t__scaledOffset;
    uint8_t uint8_t__batteryPercent;

    if (uint32_t__batteryMv <= APP_CONFIG.ui_bat_v_min_mv)
    {
        return 0u;
    }

    if (uint32_t__batteryMv >= APP_CONFIG.ui_bat_v_max_mv)
    {
        return UI_PERCENT_FULL;
    }

    /* [EN] Step 1: range = Vmax - Vmin
       [FA] گام ۱: بازه ولتاژ */
    uint32_t__voltageRangeMv = APP_CONFIG.ui_bat_v_max_mv - APP_CONFIG.ui_bat_v_min_mv;

    /* [EN] Step 2: offset = Vbat - Vmin
       [FA] گام ۲: فاصله از کف */
    uint32_t__voltageOffsetMv = uint32_t__batteryMv - APP_CONFIG.ui_bat_v_min_mv;

    if (uint32_t__voltageRangeMv == 0u)
    {
        return 0u;
    }

    /* [EN] Step 3: scaled = offset * 100
       [FA] گام ۳: مقیاس به درصد */
    uint32_t__scaledOffset = uint32_t__voltageOffsetMv * UI_PERCENT_SCALE;

    /* [EN] Step 4: percent = scaled / range
       [FA] گام ۴: تقسیم برای درصد */
    uint8_t__batteryPercent = (uint8_t)(uint32_t__scaledOffset / uint32_t__voltageRangeMv);

    if (uint8_t__batteryPercent > UI_PERCENT_FULL)
    {
        uint8_t__batteryPercent = UI_PERCENT_FULL;
    }

    return uint8_t__batteryPercent;
}

/* ==================== Green LED ==================== */

/**
 * @brief  [EN] Drive green LED on/off. Low-level wrapper around BSP GPIO.
 *         [FA] ال‌ای‌دی سبز را روشن/خاموش می‌کند - سطح پایین.
 * @param  bool__greenOn [EN] true=on, false=off / روشن یا خاموش
 */
static void func__green(bool bool__greenOn)
{
    func__BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, bool__greenOn);
}

/* ==================== Red LED ==================== */

/**
 * @brief  [EN] Drive red LED on/off. Low-level.
 *         [FA] ال‌ای‌دی قرمز را روشن/خاموش می‌کند.
 * @param  bool__redOn [EN] true=on, false=off / روشن یا خاموش
 */
static void func__red(bool bool__redOn)
{
    func__BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, bool__redOn);
}

/* ==================== Yellow LED ==================== */

/**
 * @brief  [EN] Drive yellow LED on/off. Low-level.
 *         [FA] ال‌ای‌دی زرد را روشن/خاموش می‌کند.
 * @param  bool__yellowOn [EN] true=on, false=off / روشن یا خاموش
 */
static void func__yellow(bool bool__yellowOn)
{
    func__BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, bool__yellowOn);
}

/* ==================== All Off Safe ==================== */

/**
 * @brief  [EN] Drive all LEDs and buzzer off - safe state after Init.
 *         [FA] همه ال‌ای‌دی‌ها و بازر خاموش - حالت امن.
 */
static void func__all_off(void)
{
    func__green(false);
    func__red(false);
    func__yellow(false);
    func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, false);
}

/* ==================== BatteryRun Beep Cycle Count ==================== */

/**
 * @brief  [EN] Cycle counter for smart beep in BatteryRun, counts completed 1s blink cycles; the first beep occurs on the requested cycle, then the counter resets.
 *         [FA] شمارنده سیکل برای بوق هوشمند در دشارژ، هر سیکل ۱ ثانیه.
 */
static uint32_t UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;

/* ==================== Scenario InputOk ==================== */

/**
 * @brief  [EN] InputOk scenario: green steady, red/yellow/buzzer off. RTOS simple with vTaskDelay 500ms, MCU not locked.
 *         [FA] سناریو ورودی وصل: سبز ثابت، بقیه خاموش، تاخیر RTOS ساده.
 */
void func__Ui_ScenarioInputOk(void)
{
    UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;

    func__green(true);
    func__red(false);
    func__yellow(false);
    func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, false);

    /* [EN] RTOS delay in task, not HAL_Delay - other tasks still run, MCU not locked, simple & readable
       [FA] تاخیر RTOS در تسک - میکرو قفل نمی‌شود، ساده و خوانا */
    vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_input_ok_poll_ms));
}

/* ==================== Scenario Charging Tick ==================== */

/**
 * @brief  [EN] Charging scenario tick: green steady, yellow shows remaining to full non-linear.
 *         Formula: remainingPercent = 100-pct, periodPerPercent = period/100, yellowOnMs = remaining*periodPer, yellowOffMs = period-yellowOn.
 *         [FA] سناریو شارژ: سبز ثابت، زرد مانده تا فول غیرخطی.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, 21000=0% 28000=100% / ولتاژ باتری
 */
void func__Ui_ScenarioCharging_Tick(uint32_t uint32_t__batteryMv)
{
    UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;

    uint8_t uint8_t__batteryPercent;
    uint32_t uint32_t__remainingPercent;
    uint32_t uint32_t__periodPerPercent;
    uint32_t uint32_t__yellowOnMs;
    uint32_t uint32_t__yellowOffMs;

    uint8_t__batteryPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);

    if (uint8_t__batteryPercent >= UI_PERCENT_FULL)
    {
        func__yellow(false);
        func__green(true);
        func__red(false);
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_charging_blink_period_ms));
        return;
    }

    if (uint8_t__batteryPercent == 0u)
    {
        func__yellow(true);
        func__green(true);
        func__red(false);
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_charging_blink_period_ms));
        return;
    }

    /* [EN] Non-linear formula: break into steps for readability
       [FA] فرمول غیرخطی: گام به گام برای خوانایی */
    uint32_t__remainingPercent = UI_PERCENT_FULL - uint8_t__batteryPercent;
    uint32_t__periodPerPercent = APP_CONFIG.ui_charging_blink_period_ms / UI_PERCENT_SCALE;
    uint32_t__yellowOnMs = uint32_t__remainingPercent * uint32_t__periodPerPercent;

    if (uint32_t__yellowOnMs < APP_CONFIG.ui_charging_yellow_min_off_ms)
    {
        uint32_t__yellowOnMs = APP_CONFIG.ui_charging_yellow_min_off_ms;
    }
    if (uint32_t__yellowOnMs > APP_CONFIG.ui_charging_blink_period_ms)
    {
        uint32_t__yellowOnMs = APP_CONFIG.ui_charging_blink_period_ms;
    }

    uint32_t__yellowOffMs = APP_CONFIG.ui_charging_blink_period_ms - uint32_t__yellowOnMs;

    func__green(true);
    func__red(false);

    func__yellow(true);
    vTaskDelay(pdMS_TO_TICKS(uint32_t__yellowOnMs));
    func__yellow(false);
    vTaskDelay(pdMS_TO_TICKS(uint32_t__yellowOffMs));
}

/* ==================== Scenario BatteryRun Tick ==================== */

/**
 * @brief  [EN] BatteryRun scenario tick: green blink non-linear (remainingPercent, periodPerPercent, greenOnMs/offMs), yellow OFF, smart beep.
 *         Beep every pct seconds, duration x2 if pct<20; first beep is on cycle pct. RTOS simple with vTaskDelay.
 *         [FA] سناریو دشارژ: سبز چشمک غیرخطی، زرد خاموش، بوق هوشمند با شمارش دقیق سیکل، ساده RTOS.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, 21000=0% 28000=100% / ولتاژ باتری
 */
void func__Ui_ScenarioBatteryRun_Tick(uint32_t uint32_t__batteryMv)
{
    uint8_t uint8_t__batteryPercent;
    uint32_t uint32_t__remainingPercent;
    uint32_t uint32_t__periodPerPercent;
    uint32_t uint32_t__greenOffMs;
    uint32_t uint32_t__greenOnMs;
    uint32_t uint32_t__beepIntervalCycles;
    uint32_t uint32_t__beepDurationMs;
    bool bool__shouldBeepNow;

    uint8_t__batteryPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);

    /* [EN] Non-linear: green blink OFF = remaining * period/100, with min
       [FA] فرمول غیرخطی سبز چشمک */
    uint32_t__remainingPercent = UI_PERCENT_FULL - uint8_t__batteryPercent;
    uint32_t__periodPerPercent = APP_CONFIG.ui_blink_period_ms / UI_PERCENT_SCALE;
    uint32_t__greenOffMs = uint32_t__remainingPercent * uint32_t__periodPerPercent;

    if (uint32_t__greenOffMs < APP_CONFIG.ui_green_min_off_ms)
    {
        uint32_t__greenOffMs = APP_CONFIG.ui_green_min_off_ms;
    }

    uint32_t__greenOnMs = APP_CONFIG.ui_blink_period_ms - uint32_t__greenOffMs;

    func__red(false);
    func__yellow(false);

    func__green(true);
    vTaskDelay(pdMS_TO_TICKS(uint32_t__greenOnMs));
    func__green(false);
    vTaskDelay(pdMS_TO_TICKS(uint32_t__greenOffMs));

    if (uint8_t__batteryPercent >= APP_CONFIG.ui_beep_start_pct)
    {
        UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;
        return;
    }

    uint32_t__beepIntervalCycles = (uint32_t)uint8_t__batteryPercent;
    if (uint32_t__beepIntervalCycles == 0u)
    {
        uint32_t__beepIntervalCycles = UI_BEEP_MIN_INTERVAL_CYCLES;
    }

    /* [EN] Count the completed one-second blink cycle before comparing.
       This makes the first beep occur exactly after the requested number of cycles.
       [FA] ابتدا سیکل چشمک یک‌ثانیه‌ای کامل‌شده را بشمار تا اولین بوق دقیقاً بعد از تعداد سیکل درخواستی باشد. */
    bool__shouldBeepNow = false;
    if (UINT32_T__G__UiBatteryRunBeepCycleCnt < uint32_t__beepIntervalCycles)
    {
        UINT32_T__G__UiBatteryRunBeepCycleCnt++;
    }

    if (UINT32_T__G__UiBatteryRunBeepCycleCnt >= uint32_t__beepIntervalCycles)
    {
        bool__shouldBeepNow = true;
        UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;
    }

    if (bool__shouldBeepNow == false)
    {
        return;
    }

    uint32_t__beepDurationMs = APP_CONFIG.ui_beep_base_ms;
    if (uint8_t__batteryPercent < APP_CONFIG.ui_beep_double_thresh_pct)
    {
        uint32_t__beepDurationMs = uint32_t__beepDurationMs * 2u;
    }

    /* [EN] Use buzzer pattern from buzzer module - simple RTOS delay, MCU not locked
       [FA] استفاده از تابع بازر جدا - ساده RTOS */
    func__Ui_BuzzerPatternMs_Start(0u, uint32_t__beepDurationMs, 1u, 0u);
    while (func__Ui_BuzzerPatternMs_Tick() == true)
    {
        vTaskDelay(pdMS_TO_TICKS(UI_TICK_MS));
    }
}

/* ==================== Ui Tick ==================== */

/**
 * @brief  [EN] Ui main tick - decides which scenario based on input and battery, RTOS simple readable.
 *         Call every UI_TICK_MS from task.
 *         [FA] تیکه اصلی UI - تصمیم سناریو بر اساس ورودی و باتری، ساده خوانا.
 * @param  uint32_t__inputVoltageMv [EN] Input voltage mV, 0..40000mV / ولتاژ ورودی
 * @param  uint32_t__batteryVoltageMv [EN] Battery voltage mV, 0..40000mV / ولتاژ باتری
 */
void func__Ui_Tick(uint32_t uint32_t__inputVoltageMv, uint32_t uint32_t__batteryVoltageMv)
{
    uint32_t uint32_t__batteryClampedMv;
    uint8_t uint8_t__batteryPercent;
    bool bool__inputPresent;

    uint32_t__batteryClampedMv = uint32_t__batteryVoltageMv;
    if (uint32_t__batteryClampedMv > APP_CONFIG.ui_bat_v_max_mv)
    {
        uint32_t__batteryClampedMv = APP_CONFIG.ui_bat_v_max_mv;
    }

    uint8_t__batteryPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryClampedMv);
    bool__inputPresent = (uint32_t__inputVoltageMv >= APP_CONFIG.ui_input_threshold_mv);

    if (bool__inputPresent == true)
    {
        if (uint8_t__batteryPercent < UI_PERCENT_FULL)
        {
            func__Ui_ScenarioCharging_Tick(uint32_t__batteryClampedMv);
        }
        else
        {
            func__Ui_ScenarioInputOk();
        }
    }
    else
    {
        func__Ui_ScenarioBatteryRun_Tick(uint32_t__batteryClampedMv);
    }
}

/* ==================== Ui Init ==================== */

/**
 * @brief  [EN] Drive all UI outputs low (safe state) and reset beep counter.
 *         [FA] همه خروجی‌های UI خاموش و ریست شمارنده بوق.
 */
void func__Ui_Init(void)
{
    func__all_off();
    UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;
}

/* ==================== Board Test Start ==================== */

/**
 * @brief  [EN] One-shot wiring check: red, yellow, green each 500ms, short beep 150ms, RTOS simple with vTaskDelay.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز هر کدام ۵۰۰ms، بوق ۱۵۰ms، ساده RTOS.
 */
void func__Ui_BoardTest_Start(void)
{
    func__all_off();

    func__red(true);
    vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_selftest_led_ms));
    func__red(false);

    func__yellow(true);
    vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_selftest_led_ms));
    func__yellow(false);

    func__green(true);
    vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_selftest_led_ms));
    func__green(false);

    func__Ui_BuzzerPatternMs_Start(0u, APP_CONFIG.ui_boot_beep_ms, 1u, 0u);
    while (func__Ui_BuzzerPatternMs_Tick() == true)
    {
        vTaskDelay(pdMS_TO_TICKS(UI_TICK_MS));
    }
}

/* ==================== Board Test Tick ==================== */

/**
 * @brief  [EN] Board test tick - for compatibility, returns false (test done in Start).
 *         [FA] تیکه تست برد - برای سازگاری false برمی‌گرداند.
 * @return bool [EN] true=still running, false=finished / در حال اجرا یا تمام
 */
bool func__Ui_BoardTest_Tick(void)
{
    /* [EN] For compatibility with non-blocking API, board test now done in Start with RTOS delays
       [FA] برای سازگاری، تست برد در Start با تاخیر RTOS انجام می‌شود */
    return false;
}
