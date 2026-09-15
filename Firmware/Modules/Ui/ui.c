/**
 * @file    ui.c
 * @brief   [EN] LED/buzzer scenarios - fully RTOS simple & readable, fewer lines, non-linear formulas.
 *          All constants in ui.h (ui_config.h deleted). Naming __ after type, func__ prefix.
 *          Functions separated with /* ==================== */ markers, buzzer at end.
 *          [FA] سناریوهای LED/بازر - کاملاً RTOS ساده و خوانا، فرمول‌ها غیرخطی.
 *
 * @note    [EN] RTOS simple: vTaskDelay in tasks is allowed (does not lock MCU, other tasks run).
 *          HAL_Delay forbidden. Formulas broken into steps with meaningful vars, not linear.
 *          [FA] RTOS ساده: vTaskDelay در تسک مجاز، HAL_Delay ممنوع. فرمول‌ها غیرخطی.
 */

#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* ==================== Battery Helper ==================== */

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

/* ==================== LED Low-Level ==================== */

static void func__green(bool bool__greenOn)  { func__BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, bool__greenOn); }

/* ==================== Red LED ==================== */

static void func__red(bool bool__redOn)      { func__BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, bool__redOn); }

/* ==================== Yellow LED ==================== */

static void func__yellow(bool bool__yellowOn){ func__BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, bool__yellowOn); }

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

/* ==================== LED Scenario - InputOk ==================== */

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

/* ==================== LED Scenario - Charging ==================== */

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

/* ==================== LED Scenario - BatteryRun ==================== */

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

    /* [EN] Use new buzzer pattern inside LED scenario - simple RTOS delay, MCU not locked
       [FA] استفاده از تابع جدید بازر داخل سناریو LED - ساده RTOS */
    func__Ui_BuzzerPatternMs_Start(0u, uint32_t__beepDurationMs, 1u, 0u);
    /* [EN] Tick buzzer until done - broken into small chunks, readable
       [FA] تیکه بازر تا تمام - خورد شده */
    while (func__Ui_BuzzerPatternMs_Tick() == true)
    {
        vTaskDelay(pdMS_TO_TICKS(UI_TICK_MS));
    }
}

/* ==================== Ui Main Tick ==================== */

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

/* ==================== Board Test ==================== */

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

bool func__Ui_BoardTest_Tick(void)
{
    /* [EN] For compatibility with non-blocking API, board test now done in Start with RTOS delays
       [FA] برای سازگاری، تست برد در Start با تاخیر RTOS انجام می‌شود */
    return false;
}

/* ==================== Buzzer / Beep ==================== */
/* [EN] All buzzer code at end, separated from LED per AI rule. Simple RTOS, readable, non-linear formulas.
   [FA] تمام کد بازر انتهای فایل، جدا از LED، ساده و خوانا. */

static void func__buzzer(bool bool__buzzerOn)
{
    func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, bool__buzzerOn);
}

/* ==================== Buzzer / Beep - Calc ==================== */

static uint32_t func__calc_beep_on(uint32_t uint32_t__totalOnMs, uint8_t uint8_t__repeatCount, uint32_t uint32_t__gapMs)
{
    uint32_t uint32_t__totalGapMs;
    uint32_t uint32_t__pulseOnMs;
    uint32_t uint32_t__repeatMinusOne;
    uint32_t uint32_t__denominator;

    if (uint8_t__repeatCount <= 1u)
    {
        return uint32_t__totalOnMs;
    }

    uint32_t__repeatMinusOne = (uint32_t)uint8_t__repeatCount - 1u;
    uint32_t__totalGapMs = uint32_t__gapMs * uint32_t__repeatMinusOne;

    if (uint32_t__totalGapMs >= uint32_t__totalOnMs)
    {
        uint32_t__denominator = (uint32_t)uint8_t__repeatCount * 2u - 1u;
        uint32_t__pulseOnMs = uint32_t__totalOnMs / uint32_t__denominator;
        if (uint32_t__pulseOnMs < 10u)
        {
            uint32_t__pulseOnMs = 10u;
        }
        return uint32_t__pulseOnMs;
    }

    uint32_t__pulseOnMs = (uint32_t__totalOnMs - uint32_t__totalGapMs) / (uint32_t)uint8_t__repeatCount;
    if (uint32_t__pulseOnMs < 10u)
    {
        uint32_t__pulseOnMs = 10u;
    }
    return uint32_t__pulseOnMs;
}

/* ==================== Buzzer / Beep - State ==================== */

typedef enum { BUZZER_IDLE, BUZZER_PULSE_ON, BUZZER_GAP_OFF, BUZZER_PERIOD_OFF } buzzer_state_t;
static buzzer_state_t BUZZER_STATE__G__State = BUZZER_IDLE;
static uint32_t UINT32_T__G__BuzzerTotalOnMs = 0u;
static uint32_t UINT32_T__G__BuzzerGapMs = 0u;
static uint32_t UINT32_T__G__BuzzerPulseOnMs = 0u;
static uint32_t UINT32_T__G__BuzzerPeriodMs = 0u;
static uint8_t UINT8_T__G__BuzzerRepeatCount = 0u;
static uint8_t UINT8_T__G__BuzzerPulseIndex = 0u;
static TickType_t TICKTYPE_T__G__BuzzerLastTick = 0;
static bool BOOL__G__BuzzerRunning = false;

static void func__buzzer_start_internal(uint32_t uint32_t__periodMs, uint32_t uint32_t__totalOnMs, uint8_t uint8_t__repeatCount, uint32_t uint32_t__gapMs)
{
    uint32_t uint32_t__totalOnClamped;
    uint32_t uint32_t__gapClamped;
    uint8_t uint8_t__repeatClamped;
    uint32_t uint32_t__gapTimesRepeat;

    if (uint32_t__totalOnMs < 10u) uint32_t__totalOnClamped = 10u;
    else if (uint32_t__totalOnMs > 10000u) uint32_t__totalOnClamped = 10000u;
    else uint32_t__totalOnClamped = uint32_t__totalOnMs;

    if (uint32_t__gapMs > 5000u) uint32_t__gapClamped = 5000u;
    else uint32_t__gapClamped = uint32_t__gapMs;

    if (uint8_t__repeatCount == 0u) uint8_t__repeatClamped = 1u;
    else if (uint8_t__repeatCount > 10u) uint8_t__repeatClamped = 10u;
    else uint8_t__repeatClamped = uint8_t__repeatCount;

    if (uint8_t__repeatClamped > 1u)
    {
        uint32_t__gapTimesRepeat = uint32_t__gapClamped * (uint32_t)(uint8_t__repeatClamped - 1u);
        if (uint32_t__gapTimesRepeat >= uint32_t__totalOnClamped)
        {
            uint32_t uint32_t__defaultGapTotal;
            uint32_t__defaultGapTotal = (uint32_t__totalOnClamped * UI_BUZZER_DEFAULT_GAP_PERCENT) / 100u;
            uint32_t__gapClamped = uint32_t__defaultGapTotal / (uint32_t)(uint8_t__repeatClamped - 1u);
        }
    }

    UINT32_T__G__BuzzerTotalOnMs = uint32_t__totalOnClamped;
    UINT32_T__G__BuzzerGapMs = uint32_t__gapClamped;
    UINT8_T__G__BuzzerRepeatCount = uint8_t__repeatClamped;
    UINT32_T__G__BuzzerPeriodMs = uint32_t__periodMs;
    UINT8_T__G__BuzzerPulseIndex = 0u;
    UINT32_T__G__BuzzerPulseOnMs = func__calc_beep_on(uint32_t__totalOnClamped, uint8_t__repeatClamped, uint32_t__gapClamped);
    TICKTYPE_T__G__BuzzerLastTick = xTaskGetTickCount();
    BUZZER_STATE__G__State = BUZZER_PULSE_ON;
    BOOL__G__BuzzerRunning = true;
    func__buzzer(true);
}

/* ==================== Buzzer / Beep - Pattern Ms ==================== */

void func__Ui_BuzzerPatternMs_Start(uint32_t uint32_t__periodMs, uint32_t uint32_t__onTimeMs, uint8_t uint8_t__repeatCount, uint32_t uint32_t__gapMs)
{
    func__buzzer_start_internal(uint32_t__periodMs, uint32_t__onTimeMs, uint8_t__repeatCount, uint32_t__gapMs);
}

bool func__Ui_BuzzerPatternMs_Tick(void)
{
    TickType_t ticktype__nowTick;
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__periodOffMs;

    if (BOOL__G__BuzzerRunning == false) return false;

    ticktype__nowTick = xTaskGetTickCount();
    uint32_t__elapsedMs = (uint32_t)((ticktype__nowTick - TICKTYPE_T__G__BuzzerLastTick) * portTICK_PERIOD_MS);

    switch (BUZZER_STATE__G__State)
    {
        case BUZZER_PULSE_ON:
            if (uint32_t__elapsedMs >= UINT32_T__G__BuzzerPulseOnMs)
            {
                func__buzzer(false);
                if (UINT8_T__G__BuzzerPulseIndex < UINT8_T__G__BuzzerRepeatCount - 1u)
                {
                    BUZZER_STATE__G__State = BUZZER_GAP_OFF;
                    TICKTYPE_T__G__BuzzerLastTick = ticktype__nowTick;
                }
                else
                {
                    if (UINT32_T__G__BuzzerPeriodMs == 0u || UINT32_T__G__BuzzerPeriodMs <= UINT32_T__G__BuzzerTotalOnMs)
                    {
                        BOOL__G__BuzzerRunning = false;
                        return false;
                    }
                    BUZZER_STATE__G__State = BUZZER_PERIOD_OFF;
                    TICKTYPE_T__G__BuzzerLastTick = ticktype__nowTick;
                }
            }
            break;

        case BUZZER_GAP_OFF:
            if (uint32_t__elapsedMs >= UINT32_T__G__BuzzerGapMs)
            {
                UINT8_T__G__BuzzerPulseIndex++;
                BUZZER_STATE__G__State = BUZZER_PULSE_ON;
                TICKTYPE_T__G__BuzzerLastTick = ticktype__nowTick;
                func__buzzer(true);
            }
            break;

        case BUZZER_PERIOD_OFF:
            uint32_t__periodOffMs = UINT32_T__G__BuzzerPeriodMs - UINT32_T__G__BuzzerTotalOnMs;
            if (uint32_t__elapsedMs >= uint32_t__periodOffMs)
            {
                BOOL__G__BuzzerRunning = false;
                return false;
            }
            break;

        default:
            BOOL__G__BuzzerRunning = false;
            return false;
    }
    return true;
}

/* ==================== Buzzer / Beep - Pattern Percent ==================== */

void func__Ui_BuzzerPatternPercent_Start(uint32_t uint32_t__periodMs, uint32_t uint32_t__onTimeMs, uint8_t uint8_t__repeatCount, uint8_t uint8_t__gapPercent)
{
    uint8_t uint8_t__gapPctClamped;
    uint32_t uint32_t__gapMs;

    if (uint8_t__gapPercent > 90u) uint8_t__gapPctClamped = 90u;
    else uint8_t__gapPctClamped = uint8_t__gapPercent;

    if (uint8_t__repeatCount <= 1u)
    {
        func__buzzer_start_internal(uint32_t__periodMs, uint32_t__onTimeMs, uint8_t__repeatCount, 0u);
        return;
    }

    /* [EN] Non-linear: gap = onTime * percent / 100, broken into steps
       [FA] فرمول غیرخطی گپ درصدی */
    uint32_t uint32_t__onTimeTimesPercent = uint32_t__onTimeMs * (uint32_t)uint8_t__gapPctClamped;
    uint32_t__gapMs = uint32_t__onTimeTimesPercent / 100u;

    func__buzzer_start_internal(uint32_t__periodMs, uint32_t__onTimeMs, uint8_t__repeatCount, uint32_t__gapMs);
}

bool func__Ui_BuzzerPatternPercent_Tick(void) { return func__Ui_BuzzerPatternMs_Tick(); }

/* ==================== Buzzer / Beep - Stop ==================== */

void func__Ui_BuzzerPattern_Stop(void)
{
    func__buzzer(false);
    BUZZER_STATE__G__State = BUZZER_IDLE;
    BOOL__G__BuzzerRunning = false;
}
