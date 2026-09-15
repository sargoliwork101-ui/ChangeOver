/**
 * @file    ui_led.c
 * @brief   [EN] UI LED scenarios - green/red/yellow, battery percent, InputOk/Charging/BatteryRun.
 *          Split from ui.c into LED and BUZZER per user request. RTOS simple readable, non-linear formulas.
 *          [FA] سناریوهای LED ماژول UI - جدا شده از ui.c به دو بخش LED و BUZZER.
 *
 * @note    [EN] All thresholds in ui.h (single source). Naming __ after type, func__ prefix.
 *          RTOS: vTaskDelay allowed (does not lock MCU), HAL_Delay forbidden. Formulas non-linear broken into steps.
 *          [FA] همه آستانه‌ها در ui.h. نام‌گذاری با __، پیشوند func__، فرمول غیرخطی.
 */

#include "ui.h"
#include "ui_led.h"
#include "ui_buzzer.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* ==================== Battery Voltage To Percent ==================== */

/**
 * @brief  [EN] Battery voltage to percent 0..100. Non-linear formula broken into steps.
 *         [FA] ولتاژ باتری به درصد - فرمول غیرخطی.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV / ولتاژ باتری
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func__Ui_BatteryVoltageToPercent(uint32_t uint32_t__batteryMv)
{
    uint32_t uint32_t__voltageRangeMv;
    uint32_t uint32_t__voltageOffsetMv;
    uint32_t uint32_t__scaledOffset;
    uint8_t uint8_t__batteryPercent;

    if (uint32_t__batteryMv <= UI_BAT_V_MIN_MV)
    {
        return 0u;
    }

    if (uint32_t__batteryMv >= UI_BAT_V_MAX_MV)
    {
        return UI_PERCENT_FULL;
    }

    /* [EN] Step 1: range = Vmax - Vmin
       [FA] گام ۱: بازه ولتاژ */
    uint32_t__voltageRangeMv = UI_BAT_V_MAX_MV - UI_BAT_V_MIN_MV;

    /* [EN] Step 2: offset = Vbat - Vmin
       [FA] گام ۲: فاصله از کف */
    uint32_t__voltageOffsetMv = uint32_t__batteryMv - UI_BAT_V_MIN_MV;

    if (uint32_t__voltageRangeMv == 0u)
    {
        return 0u;
    }

    /* [EN] Step 3: scaled = offset * 100
       [FA] گام ۳: مقیاس به درصد */
    uint32_t__scaledOffset = uint32_t__voltageOffsetMv * 100u;

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

static void func__green(bool bool__greenOn)
{
    func__BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, bool__greenOn);
}

/* ==================== Red LED ==================== */

static void func__red(bool bool__redOn)
{
    func__BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, bool__redOn);
}

/* ==================== Yellow LED ==================== */

static void func__yellow(bool bool__yellowOn)
{
    func__BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, bool__yellowOn);
}

/* ==================== All Off Safe ==================== */

static void func__all_off(void)
{
    func__green(false);
    func__red(false);
    func__yellow(false);
    func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, false);
}

/* ==================== LED State ==================== */

static uint32_t UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;

/* ==================== Scenario InputOk ==================== */

void func__Ui_ScenarioInputOk(void)
{
    func__green(true);
    func__red(false);
    func__yellow(false);
    func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, false);

    /* [EN] RTOS delay in task, not HAL_Delay - other tasks still run, MCU not locked, simple & readable
       [FA] تاخیر RTOS در تسک - میکرو قفل نمی‌شود، ساده و خوانا */
    vTaskDelay(pdMS_TO_TICKS(UI_INPUT_OK_POLL_MS));
}

/* ==================== Scenario Charging Tick ==================== */

void func__Ui_ScenarioCharging_Tick(uint32_t uint32_t__batteryMv)
{
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
        vTaskDelay(pdMS_TO_TICKS(UI_CHARGING_BLINK_PERIOD_MS));
        return;
    }

    if (uint8_t__batteryPercent == 0u)
    {
        func__yellow(true);
        func__green(true);
        func__red(false);
        vTaskDelay(pdMS_TO_TICKS(UI_CHARGING_BLINK_PERIOD_MS));
        return;
    }

    /* [EN] Non-linear formula: break into steps for readability
       [FA] فرمول غیرخطی: گام به گام برای خوانایی */
    uint32_t__remainingPercent = UI_PERCENT_FULL - uint8_t__batteryPercent;
    uint32_t__periodPerPercent = UI_CHARGING_BLINK_PERIOD_MS / 100u;
    uint32_t__yellowOnMs = uint32_t__remainingPercent * uint32_t__periodPerPercent;

    if (uint32_t__yellowOnMs < UI_CHARGING_YELLOW_MIN_OFF_MS)
    {
        uint32_t__yellowOnMs = UI_CHARGING_YELLOW_MIN_OFF_MS;
    }
    if (uint32_t__yellowOnMs > UI_CHARGING_BLINK_PERIOD_MS)
    {
        uint32_t__yellowOnMs = UI_CHARGING_BLINK_PERIOD_MS;
    }

    uint32_t__yellowOffMs = UI_CHARGING_BLINK_PERIOD_MS - uint32_t__yellowOnMs;

    func__green(true);
    func__red(false);

    func__yellow(true);
    vTaskDelay(pdMS_TO_TICKS(uint32_t__yellowOnMs));
    func__yellow(false);
    vTaskDelay(pdMS_TO_TICKS(uint32_t__yellowOffMs));
}

/* ==================== Scenario BatteryRun Tick ==================== */

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
    uint32_t__periodPerPercent = UI_BLINK_PERIOD_MS / 100u;
    uint32_t__greenOffMs = uint32_t__remainingPercent * uint32_t__periodPerPercent;

    if (uint32_t__greenOffMs < UI_GREEN_MIN_OFF_MS)
    {
        uint32_t__greenOffMs = UI_GREEN_MIN_OFF_MS;
    }

    uint32_t__greenOnMs = UI_BLINK_PERIOD_MS - uint32_t__greenOffMs;

    func__red(false);
    func__yellow(false);

    func__green(true);
    vTaskDelay(pdMS_TO_TICKS(uint32_t__greenOnMs));
    func__green(false);
    vTaskDelay(pdMS_TO_TICKS(uint32_t__greenOffMs));

    if (uint8_t__batteryPercent >= UI_BEEP_START_PCT)
    {
        UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;
        return;
    }

    uint32_t__beepIntervalCycles = (uint32_t)uint8_t__batteryPercent;
    if (uint32_t__beepIntervalCycles == 0u)
    {
        uint32_t__beepIntervalCycles = 1u;
    }

    bool__shouldBeepNow = false;
    if (UINT32_T__G__UiBatteryRunBeepCycleCnt >= uint32_t__beepIntervalCycles)
    {
        bool__shouldBeepNow = true;
        UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;
    }
    else
    {
        UINT32_T__G__UiBatteryRunBeepCycleCnt++;
    }

    if (bool__shouldBeepNow == false)
    {
        return;
    }

    uint32_t__beepDurationMs = UI_BEEP_BASE_MS;
    if (uint8_t__batteryPercent < UI_BEEP_DOUBLE_THRESH_PCT)
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

void func__Ui_Tick(uint32_t uint32_t__inputVoltageMv, uint32_t uint32_t__batteryVoltageMv)
{
    uint32_t uint32_t__batteryClampedMv;
    uint8_t uint8_t__batteryPercent;
    bool bool__inputPresent;

    uint32_t__batteryClampedMv = uint32_t__batteryVoltageMv;
    if (uint32_t__batteryClampedMv > UI_BAT_V_MAX_MV)
    {
        uint32_t__batteryClampedMv = UI_BAT_V_MAX_MV;
    }

    uint8_t__batteryPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryClampedMv);
    bool__inputPresent = (uint32_t__inputVoltageMv >= UI_INPUT_THRESHOLD_MV);

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

void func__Ui_Init(void)
{
    func__all_off();
    UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;
}

/* ==================== Board Test Start ==================== */

void func__Ui_BoardTest_Start(void)
{
    func__all_off();

    func__red(true);
    vTaskDelay(pdMS_TO_TICKS(UI_SELFTEST_LED_MS));
    func__red(false);

    func__yellow(true);
    vTaskDelay(pdMS_TO_TICKS(UI_SELFTEST_LED_MS));
    func__yellow(false);

    func__green(true);
    vTaskDelay(pdMS_TO_TICKS(UI_SELFTEST_LED_MS));
    func__green(false);

    func__Ui_BuzzerPatternMs_Start(0u, UI_BOOT_BEEP_MS, 1u, 0u);
    while (func__Ui_BuzzerPatternMs_Tick() == true)
    {
        vTaskDelay(pdMS_TO_TICKS(UI_TICK_MS));
    }
}

/* ==================== Board Test Tick ==================== */

bool func__Ui_BoardTest_Tick(void)
{
    /* [EN] For compatibility with non-blocking API, board test now done in Start with RTOS delays
       [FA] برای سازگاری، تست برد در Start با تاخیر RTOS انجام می‌شود */
    return false;
}
