/**
 * @file    ui.c
 * @brief   [EN] LED/buzzer scenarios - fully RTOS non-blocking but simple & readable, fewer lines.
 *          All constants in ui.h (ui_config.h deleted). Naming __ after type, func__ prefix.
 *          Functions separated with /* ==================== */ markers, buzzer at end.
 *          [FA] سناریوهای LED/بازر - کاملاً RTOS غیربلوکه ولی ساده و خوانا، خط کمتر.
 *
 * @note    [EN] No delay inside module, only xTaskGetTickCount() + small state machines, chunked.
 *          Task calls Tick every 10ms via vTaskDelayUntil, so MCU never locks.
 *          Readability over complexity: generic elapsed helper, one blink struct for all LEDs.
 *          [FA] بدون delay، خوانا و کم‌خط ولی غیربلوکه.
 */

#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* ==================== Battery Helper ==================== */

uint8_t func__Ui_BatteryVoltageToPercent(uint32_t uint32_t__batteryMv)
{
    if (uint32_t__batteryMv <= UI_BAT_V_MIN_MV) return 0u;
    if (uint32_t__batteryMv >= UI_BAT_V_MAX_MV) return UI_PERCENT_FULL;

    uint32_t uint32_t__rangeMv = UI_BAT_V_MAX_MV - UI_BAT_V_MIN_MV;
    uint32_t uint32_t__offsetMv = uint32_t__batteryMv - UI_BAT_V_MIN_MV;
    if (uint32_t__rangeMv == 0u) return 0u;

    uint8_t uint8_t__pct = (uint8_t)((uint32_t__offsetMv * 100u) / uint32_t__rangeMv);
    return (uint8_t__pct > UI_PERCENT_FULL) ? UI_PERCENT_FULL : uint8_t__pct;
}

/* ==================== LED Low-Level ==================== */

static void func__green(bool bool__on)  { func__BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, bool__on); }
static void func__red(bool bool__on)    { func__BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, bool__on); }
static void func__yellow(bool bool__on) { func__BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, bool__on); }

/* ==================== Generic Time Helper ==================== */

static uint32_t func__elapsed_ms(TickType_t ticktype__lastTick)
{
    return (uint32_t)((xTaskGetTickCount() - ticktype__lastTick) * portTICK_PERIOD_MS);
}

static bool func__has_elapsed(TickType_t *ticktype__lastTick, uint32_t uint32_t__periodMs)
{
    if (func__elapsed_ms(*ticktype__lastTick) >= uint32_t__periodMs)
    {
        *ticktype__lastTick = xTaskGetTickCount();
        return true;
    }
    return false;
}

/* ==================== Generic LED Blink ==================== */

typedef struct
{
    TickType_t lastToggle;
    bool isOn;
    uint32_t onMs;
    uint32_t offMs;
} led_blink_t;

static void func__blink_init(led_blink_t *led_blink__blink, uint32_t uint32_t__onMs, uint32_t uint32_t__offMs)
{
    led_blink__blink->lastToggle = xTaskGetTickCount();
    led_blink__blink->isOn = true;
    led_blink__blink->onMs = uint32_t__onMs;
    led_blink__blink->offMs = uint32_t__offMs;
}

static bool func__blink_tick(led_blink_t *led_blink__blink)
{
    uint32_t uint32_t__elapsedMs = func__elapsed_ms(led_blink__blink->lastToggle);
    if (led_blink__blink->isOn)
    {
        if (uint32_t__elapsedMs >= led_blink__blink->onMs)
        {
            led_blink__blink->isOn = false;
            led_blink__blink->lastToggle = xTaskGetTickCount();
            return true;
        }
    }
    else
    {
        if (uint32_t__elapsedMs >= led_blink__blink->offMs)
        {
            led_blink__blink->isOn = true;
            led_blink__blink->lastToggle = xTaskGetTickCount();
            return true;
        }
    }
    return false;
}

/* ==================== LED State ==================== */

static led_blink_t LED_BLINK__G__Green = {0};
static led_blink_t LED_BLINK__G__Yellow = {0};
static TickType_t TICKTYPE_T__G__BeepLastTick = 0;
static uint32_t UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;

/* ==================== All Off Safe ==================== */

static void func__all_off(void)
{
    func__green(false);
    func__red(false);
    func__yellow(false);
    func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, false);
}

/* ==================== LED Scenario - InputOk ==================== */

void func__Ui_ScenarioInputOk(void)
{
    func__green(true);
    func__red(false);
    func__yellow(false);
    func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, false);
}

/* ==================== LED Scenario - Charging Tick ==================== */

void func__Ui_ScenarioCharging_Tick(uint32_t uint32_t__batteryMv)
{
    uint8_t uint8_t__pct = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);

    if (uint8_t__pct >= UI_PERCENT_FULL)
    {
        func__yellow(false);
        func__green(true);
        func__red(false);
        return;
    }
    if (uint8_t__pct == 0u)
    {
        func__yellow(true);
        func__green(true);
        func__red(false);
        return;
    }

    uint32_t uint32_t__yellowOnMs = (uint32_t)(UI_PERCENT_FULL - uint8_t__pct) * (UI_CHARGING_BLINK_PERIOD_MS / 100u);
    if (uint32_t__yellowOnMs < UI_CHARGING_YELLOW_MIN_OFF_MS) uint32_t__yellowOnMs = UI_CHARGING_YELLOW_MIN_OFF_MS;
    if (uint32_t__yellowOnMs > UI_CHARGING_BLINK_PERIOD_MS)   uint32_t__yellowOnMs = UI_CHARGING_BLINK_PERIOD_MS;
    uint32_t uint32_t__yellowOffMs = UI_CHARGING_BLINK_PERIOD_MS - uint32_t__yellowOnMs;

    if (LED_BLINK__G__Yellow.onMs != uint32_t__yellowOnMs)
    {
        func__blink_init(&LED_BLINK__G__Yellow, uint32_t__yellowOnMs, uint32_t__yellowOffMs);
        func__yellow(true);
    }
    else if (func__blink_tick(&LED_BLINK__G__Yellow))
    {
        func__yellow(LED_BLINK__G__Yellow.isOn);
    }

    func__green(true);
    func__red(false);
}

/* ==================== LED Scenario - BatteryRun Led Tick ==================== */

void func__Ui_ScenarioBatteryRun_LedTick(uint32_t uint32_t__batteryMv)
{
    uint8_t uint8_t__pct = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);
    uint32_t uint32_t__greenOffMs = (uint32_t)(UI_PERCENT_FULL - uint8_t__pct) * (UI_BLINK_PERIOD_MS / 100u);
    if (uint32_t__greenOffMs < UI_GREEN_MIN_OFF_MS) uint32_t__greenOffMs = UI_GREEN_MIN_OFF_MS;
    uint32_t uint32_t__greenOnMs = UI_BLINK_PERIOD_MS - uint32_t__greenOffMs;

    if (LED_BLINK__G__Green.onMs != uint32_t__greenOnMs)
    {
        func__blink_init(&LED_BLINK__G__Green, uint32_t__greenOnMs, uint32_t__greenOffMs);
        func__green(true);
        func__red(false);
        func__yellow(false);
    }
    else if (func__blink_tick(&LED_BLINK__G__Green))
    {
        func__green(LED_BLINK__G__Green.isOn);
    }
}

/* ==================== LED Scenario - BatteryRun Tick (with buzzer) ==================== */

void func__Ui_ScenarioBatteryRun_Tick(uint32_t uint32_t__batteryMv)
{
    uint8_t uint8_t__pct = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);
    func__Ui_ScenarioBatteryRun_LedTick(uint32_t__batteryMv);

    if (uint8_t__pct >= UI_BEEP_START_PCT)
    {
        TICKTYPE_T__G__BeepLastTick = xTaskGetTickCount();
        return;
    }

    uint32_t uint32_t__intervalMs = (uint32_t)uint8_t__pct * 1000u;
    if (uint32_t__intervalMs < 1000u) uint32_t__intervalMs = 1000u;

    if (func__elapsed_ms(TICKTYPE_T__G__BeepLastTick) >= uint32_t__intervalMs)
    {
        uint32_t uint32_t__durMs = UI_BEEP_BASE_MS;
        if (uint8_t__pct < UI_BEEP_DOUBLE_THRESH_PCT) uint32_t__durMs *= 2u;

        func__Ui_BuzzerPatternMs_Start(0u, uint32_t__durMs, 1u, 0u);
        TICKTYPE_T__G__BeepLastTick = xTaskGetTickCount();
        UINT32_T__G__UiBatteryRunBeepCycleCnt++;
    }

    (void)func__Ui_BuzzerPatternMs_Tick();
}

/* ==================== Ui Main Tick ==================== */

void func__Ui_Tick(uint32_t uint32_t__inputVoltageMv, uint32_t uint32_t__batteryVoltageMv)
{
    if (uint32_t__batteryVoltageMv > UI_BAT_V_MAX_MV) uint32_t__batteryVoltageMv = UI_BAT_V_MAX_MV;
    uint8_t uint8_t__pct = func__Ui_BatteryVoltageToPercent(uint32_t__batteryVoltageMv);
    bool bool__inputPresent = (uint32_t__inputVoltageMv >= UI_INPUT_THRESHOLD_MV);

    if (bool__inputPresent)
    {
        if (uint8_t__pct < UI_PERCENT_FULL) func__Ui_ScenarioCharging_Tick(uint32_t__batteryVoltageMv);
        else                                func__Ui_ScenarioInputOk();
    }
    else
    {
        func__Ui_ScenarioBatteryRun_Tick(uint32_t__batteryVoltageMv);
    }
}

/* ==================== Ui Init ==================== */

void func__Ui_Init(void)
{
    func__all_off();
    LED_BLINK__G__Green.onMs = 0u;
    LED_BLINK__G__Yellow.onMs = 0u;
    TICKTYPE_T__G__BeepLastTick = xTaskGetTickCount();
    UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;
}

/* ==================== Board Test ==================== */

typedef enum { BOARDTEST_RED, BOARDTEST_YELLOW, BOARDTEST_GREEN, BOARDTEST_BEEP, BOARDTEST_DONE } boardtest_step_t;
static boardtest_step_t BOARDTEST_STEP__G__Step = BOARDTEST_DONE;
static TickType_t TICKTYPE_T__G__BoardTestLastTick = 0;
static bool BOOL__G__BoardTestRunning = false;

void func__Ui_BoardTest_Start(void)
{
    func__all_off();
    BOARDTEST_STEP__G__Step = BOARDTEST_RED;
    TICKTYPE_T__G__BoardTestLastTick = xTaskGetTickCount();
    BOOL__G__BoardTestRunning = true;
    func__red(true);
}

bool func__Ui_BoardTest_Tick(void)
{
    if (!BOOL__G__BoardTestRunning) return false;

    if (!func__has_elapsed(&TICKTYPE_T__G__BoardTestLastTick, UI_SELFTEST_LED_MS))
    {
        if (BOARDTEST_STEP__G__Step == BOARDTEST_BEEP) return func__Ui_BuzzerPatternMs_Tick();
        return true;
    }

    switch (BOARDTEST_STEP__G__Step)
    {
        case BOARDTEST_RED:
            func__red(false);
            func__yellow(true);
            BOARDTEST_STEP__G__Step = BOARDTEST_YELLOW;
            break;
        case BOARDTEST_YELLOW:
            func__yellow(false);
            func__green(true);
            BOARDTEST_STEP__G__Step = BOARDTEST_GREEN;
            break;
        case BOARDTEST_GREEN:
            func__green(false);
            BOARDTEST_STEP__G__Step = BOARDTEST_BEEP;
            func__Ui_BuzzerPatternMs_Start(0u, UI_BOOT_BEEP_MS, 1u, 0u);
            break;
        case BOARDTEST_BEEP:
            if (!func__Ui_BuzzerPatternMs_Tick())
            {
                BOARDTEST_STEP__G__Step = BOARDTEST_DONE;
                func__all_off();
                BOOL__G__BoardTestRunning = false;
                return false;
            }
            return true;
        default:
            BOOL__G__BoardTestRunning = false;
            return false;
    }
    return true;
}

/* ==================== Buzzer / Beep ==================== */
/* [EN] All buzzer code at end, separated from LED per AI rule.
   [FA] تمام کد بازر انتهای فایل، جدا از LED. */

static void func__buzzer(bool bool__on) { func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, bool__on); }

static uint32_t func__calc_beep_on(uint32_t uint32_t__totalOnMs, uint8_t uint8_t__repeatCount, uint32_t uint32_t__gapMs)
{
    if (uint8_t__repeatCount <= 1u) return uint32_t__totalOnMs;
    uint32_t uint32_t__totalGapMs = (uint32_t)uint32_t__gapMs * (uint32_t)(uint8_t__repeatCount - 1u);
    if (uint32_t__totalGapMs >= uint32_t__totalOnMs) return uint32_t__totalOnMs / (uint32_t)((uint8_t__repeatCount * 2u) - 1u);
    return (uint32_t__totalOnMs - uint32_t__totalGapMs) / (uint32_t)uint8_t__repeatCount;
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
    if (uint32_t__totalOnMs < 10u) uint32_t__totalOnMs = 10u;
    if (uint32_t__totalOnMs > 10000u) uint32_t__totalOnMs = 10000u;
    if (uint32_t__gapMs > 5000u) uint32_t__gapMs = 5000u;
    if (uint8_t__repeatCount == 0u) uint8_t__repeatCount = 1u;
    if (uint8_t__repeatCount > 10u) uint8_t__repeatCount = 10u;

    if (uint8_t__repeatCount > 1u && uint32_t__gapMs * (uint32_t)(uint8_t__repeatCount - 1u) >= uint32_t__totalOnMs)
    {
        uint32_t__gapMs = (uint32_t__totalOnMs * UI_BUZZER_DEFAULT_GAP_PERCENT / 100u) / (uint32_t)(uint8_t__repeatCount - 1u);
    }

    UINT32_T__G__BuzzerTotalOnMs = uint32_t__totalOnMs;
    UINT32_T__G__BuzzerGapMs = uint32_t__gapMs;
    UINT8_T__G__BuzzerRepeatCount = uint8_t__repeatCount;
    UINT32_T__G__BuzzerPeriodMs = uint32_t__periodMs;
    UINT8_T__G__BuzzerPulseIndex = 0u;
    UINT32_T__G__BuzzerPulseOnMs = func__calc_beep_on(uint32_t__totalOnMs, uint8_t__repeatCount, uint32_t__gapMs);
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
    if (!BOOL__G__BuzzerRunning) return false;

    uint32_t uint32_t__elapsedMs = func__elapsed_ms(TICKTYPE_T__G__BuzzerLastTick);

    switch (BUZZER_STATE__G__State)
    {
        case BUZZER_PULSE_ON:
            if (uint32_t__elapsedMs >= UINT32_T__G__BuzzerPulseOnMs)
            {
                func__buzzer(false);
                if (UINT8_T__G__BuzzerPulseIndex < UINT8_T__G__BuzzerRepeatCount - 1u)
                {
                    BUZZER_STATE__G__State = BUZZER_GAP_OFF;
                    TICKTYPE_T__G__BuzzerLastTick = xTaskGetTickCount();
                }
                else
                {
                    if (UINT32_T__G__BuzzerPeriodMs == 0u || UINT32_T__G__BuzzerPeriodMs <= UINT32_T__G__BuzzerTotalOnMs)
                    {
                        BOOL__G__BuzzerRunning = false;
                        return false;
                    }
                    BUZZER_STATE__G__State = BUZZER_PERIOD_OFF;
                    TICKTYPE_T__G__BuzzerLastTick = xTaskGetTickCount();
                }
            }
            break;

        case BUZZER_GAP_OFF:
            if (uint32_t__elapsedMs >= UINT32_T__G__BuzzerGapMs)
            {
                UINT8_T__G__BuzzerPulseIndex++;
                BUZZER_STATE__G__State = BUZZER_PULSE_ON;
                TICKTYPE_T__G__BuzzerLastTick = xTaskGetTickCount();
                func__buzzer(true);
            }
            break;

        case BUZZER_PERIOD_OFF:
            if (uint32_t__elapsedMs >= UINT32_T__G__BuzzerPeriodMs - UINT32_T__G__BuzzerTotalOnMs)
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
    if (uint8_t__gapPercent > 90u) uint8_t__gapPercent = 90u;
    if (uint8_t__repeatCount <= 1u)
    {
        func__buzzer_start_internal(uint32_t__periodMs, uint32_t__onTimeMs, uint8_t__repeatCount, 0u);
        return;
    }
    uint32_t uint32_t__gapMs = (uint32_t__onTimeMs * (uint32_t)uint8_t__gapPercent) / 100u;
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
