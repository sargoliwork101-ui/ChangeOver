/**
 * @file    ui.c
 * @brief   [EN] LED/buzzer scenarios - fully RTOS, no delay, non-blocking state machines.
 *          All constants in ui.h (ui_config.h deleted). Naming with __ after type, func__ prefix.
 *          [FA] سناریوهای LED/بازر - کاملاً RTOS بدون delay، استیت ماشین غیربلوکه.
 *          همه ثابت‌ها در ui.h. نام‌گذاری با __ بعد تایپ و func__.
 *
 * @note    [EN] No HAL_Delay, no vTaskDelay inside module. Only xTaskGetTickCount() and state machines.
 *          Task calls Tick every UI_TICK_MS (10ms) via vTaskDelayUntil.
 *          [FA] بدون هیچ delay داخل ماژول، فقط تیکه ۱۰ms از تسک.
 */

#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* ===== Low-level helpers - func__ prefix, __ after type ===== */

/**
 * @brief  [EN] Green PB10 on/off.
 *         [FA] سبز PB10.
 * @param  bool__greenOn [EN] true=green on / سبز روشن
 */
static void func__green(bool bool__greenOn)
{
    func__BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, bool__greenOn);
}

/**
 * @brief  [EN] Red PB0 on/off.
 *         [FA] قرمز PB0.
 * @param  bool__redOn [EN] true=red on / قرمز روشن
 */
static void func__red(bool bool__redOn)
{
    func__BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, bool__redOn);
}

/**
 * @brief  [EN] Yellow PB1 on/off.
 *         [FA] زرد PB1.
 * @param  bool__yellowOn [EN] true=yellow on / زرد روشن
 */
static void func__yellow(bool bool__yellowOn)
{
    func__BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, bool__yellowOn);
}

/**
 * @brief  [EN] Buzzer PA4 on/off.
 *         [FA] بازر PA4.
 * @param  bool__buzzerOn [EN] true=buzzer sound / صدای بازر
 */
static void func__buzzer(bool bool__buzzerOn)
{
    func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, bool__buzzerOn);
}

/**
 * @brief  [EN] All UI outputs off safe.
 *         [FA] همه خاموش امن.
 */
static void func__all_off(void)
{
    func__green(false);
    func__red(false);
    func__yellow(false);
    func__buzzer(false);
}

/**
 * @brief  [EN] Calculate beep ON per pulse inside total ON time.
 *         [FA] محاسبه زمان روشن هر پالس.
 * @param  uint32_t__buzzerTotalOnMs [EN] Total including gaps / کل شامل گپ
 * @param  uint8_t__buzzerRepeatCount [EN] Repeat 1..10 / تکرار
 * @param  uint32_t__buzzerGapMs [EN] Gap ms / گپ
 * @return uint32_t [EN] Beep ON per pulse ms / زمان هر پالس
 */
static uint32_t func__calc_beep_on(uint32_t uint32_t__buzzerTotalOnMs, uint8_t uint8_t__buzzerRepeatCount, uint32_t uint32_t__buzzerGapMs)
{
    uint32_t uint32_t__buzzerTotalGapMs;
    uint32_t uint32_t__buzzerPulseOnMs;

    if (uint8_t__buzzerRepeatCount <= 1u)
    {
        return uint32_t__buzzerTotalOnMs;
    }

    uint32_t__buzzerTotalGapMs = (uint32_t)uint32_t__buzzerGapMs * (uint32_t)(uint8_t__buzzerRepeatCount - 1u);

    if (uint32_t__buzzerTotalGapMs >= uint32_t__buzzerTotalOnMs)
    {
        uint32_t__buzzerPulseOnMs = uint32_t__buzzerTotalOnMs / (uint32_t)((uint8_t__buzzerRepeatCount * 2u) - 1u);
        if (uint32_t__buzzerPulseOnMs < 10u)
        {
            uint32_t__buzzerPulseOnMs = 10u;
        }
        return uint32_t__buzzerPulseOnMs;
    }

    uint32_t__buzzerPulseOnMs = (uint32_t__buzzerTotalOnMs - uint32_t__buzzerTotalGapMs) / (uint32_t)uint8_t__buzzerRepeatCount;
    if (uint32_t__buzzerPulseOnMs < 10u)
    {
        uint32_t__buzzerPulseOnMs = 10u;
    }
    return uint32_t__buzzerPulseOnMs;
}

/* ===== Buzzer pattern state machine - non-blocking ===== */

typedef enum
{
    BUZZER_STATE_IDLE = 0,
    BUZZER_STATE_PULSE_ON,
    BUZZER_STATE_GAP_OFF,
    BUZZER_STATE_PERIOD_OFF
} buzzer_state_t;

static buzzer_state_t BUZZER_STATE__G__State = BUZZER_STATE_IDLE;
static uint32_t UINT32_T__G__BuzzerTotalOnMs = 0u;
static uint32_t UINT32_T__G__BuzzerGapMs = 0u;
static uint32_t UINT32_T__G__BuzzerPulseOnMs = 0u;
static uint32_t UINT32_T__G__BuzzerPeriodMs = 0u;
static uint8_t UINT8_T__G__BuzzerRepeatCount = 0u;
static uint8_t UINT8_T__G__BuzzerPulseIndex = 0u;
static TickType_t TICKTYPE_T__G__BuzzerLastTick = 0;
static bool BOOL__G__BuzzerRunning = false;

/**
 * @brief  [EN] Internal buzzer pattern start - non-blocking setup.
 *         [FA] شروع داخلی الگوی بازر - ست‌آپ غیربلوکه.
 * @param  uint32_t__periodMs [EN] Period ms / دوره تناوب
 * @param  uint32_t__totalOnMs [EN] Total ON ms / کل زمان روشن
 * @param  uint8_t__repeatCount [EN] Repeat / تکرار
 * @param  uint32_t__gapMs [EN] Gap ms / گپ
 */
static void func__buzzer_pattern_start_internal(uint32_t uint32_t__periodMs, uint32_t uint32_t__totalOnMs, uint8_t uint8_t__repeatCount, uint32_t uint32_t__gapMs)
{
    uint32_t uint32_t__buzzerTotalOnClamped;
    uint32_t uint32_t__buzzerGapClamped;
    uint8_t uint8_t__buzzerRepeatClamped;

    if (uint32_t__totalOnMs < 10u)
    {
        uint32_t__buzzerTotalOnClamped = 10u;
    }
    else if (uint32_t__totalOnMs > 10000u)
    {
        uint32_t__buzzerTotalOnClamped = 10000u;
    }
    else
    {
        uint32_t__buzzerTotalOnClamped = uint32_t__totalOnMs;
    }

    if (uint32_t__gapMs > 5000u)
    {
        uint32_t__buzzerGapClamped = 5000u;
    }
    else
    {
        uint32_t__buzzerGapClamped = uint32_t__gapMs;
    }

    if (uint8_t__repeatCount == 0u)
    {
        uint8_t__buzzerRepeatClamped = 1u;
    }
    else if (uint8_t__repeatCount > 10u)
    {
        uint8_t__buzzerRepeatClamped = 10u;
    }
    else
    {
        uint8_t__buzzerRepeatClamped = uint8_t__repeatCount;
    }

    if (uint8_t__buzzerRepeatClamped > 1u)
    {
        if ((uint32_t__buzzerGapClamped * (uint32_t)(uint8_t__buzzerRepeatClamped - 1u)) >= uint32_t__buzzerTotalOnClamped)
        {
            uint32_t__buzzerGapClamped = (uint32_t__buzzerTotalOnClamped * UI_BUZZER_DEFAULT_GAP_PERCENT / 100u);
            if (uint8_t__buzzerRepeatClamped > 1u)
            {
                uint32_t__buzzerGapClamped = uint32_t__buzzerGapClamped / (uint32_t)(uint8_t__buzzerRepeatClamped - 1u);
            }
        }
    }

    UINT32_T__G__BuzzerTotalOnMs = uint32_t__buzzerTotalOnClamped;
    UINT32_T__G__BuzzerGapMs = uint32_t__buzzerGapClamped;
    UINT8_T__G__BuzzerRepeatCount = uint8_t__buzzerRepeatClamped;
    UINT32_T__G__BuzzerPeriodMs = uint32_t__periodMs;
    UINT8_T__G__BuzzerPulseIndex = 0u;
    UINT32_T__G__BuzzerPulseOnMs = func__calc_beep_on(uint32_t__buzzerTotalOnClamped, uint8_t__buzzerRepeatClamped, uint32_t__buzzerGapClamped);
    TICKTYPE_T__G__BuzzerLastTick = xTaskGetTickCount();
    BUZZER_STATE__G__State = BUZZER_STATE_PULSE_ON;
    BOOL__G__BuzzerRunning = true;

    func__buzzer(true);
}

/* ===== Board test state machine ===== */

typedef enum
{
    BOARDTEST_STATE_IDLE = 0,
    BOARDTEST_STATE_RED_ON,
    BOARDTEST_STATE_YELLOW_ON,
    BOARDTEST_STATE_GREEN_ON,
    BOARDTEST_STATE_BEEP_ON,
    BOARDTEST_STATE_DONE
} boardtest_state_t;

static boardtest_state_t BOARDTEST_STATE__G__State = BOARDTEST_STATE_IDLE;
static TickType_t TICKTYPE_T__G__BoardTestLastTick = 0;
static bool BOOL__G__BoardTestRunning = false;

/* ===== BatteryRun state machine ===== */

static uint32_t UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;
static TickType_t TICKTYPE_T__G__GreenLastToggleTick = 0;
static bool BOOL__G__GreenOn = false;
static uint32_t UINT32_T__G__GreenOnMs = 0u;
static uint32_t UINT32_T__G__GreenOffMs = 0u;
static TickType_t TICKTYPE_T__G__BeepLastTick = 0;

/* ===== Charging state machine ===== */

static TickType_t TICKTYPE_T__G__YellowLastToggleTick = 0;
static bool BOOL__G__YellowOn = false;
static uint32_t UINT32_T__G__YellowOnMs = 0u;
static uint32_t UINT32_T__G__YellowOffMs = 0u;

/* ===== Public - func__ prefix, __ after type ===== */

/**
 * @brief  [EN] Init safe: all off.
 *         [FA] Init امن: همه خاموش.
 */
void func__Ui_Init(void)
{
    func__all_off();
    BUZZER_STATE__G__State = BUZZER_STATE_IDLE;
    BOOL__G__BuzzerRunning = false;
    BOARDTEST_STATE__G__State = BOARDTEST_STATE_IDLE;
    BOOL__G__BoardTestRunning = false;
    UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;
    TICKTYPE_T__G__GreenLastToggleTick = xTaskGetTickCount();
    TICKTYPE_T__G__YellowLastToggleTick = xTaskGetTickCount();
    TICKTYPE_T__G__BeepLastTick = xTaskGetTickCount();
}

/**
 * @brief  [EN] Board test start - non-blocking.
 *         [FA] شروع تست برد - غیربلوکه.
 */
void func__Ui_BoardTest_Start(void)
{
    func__all_off();
    BOARDTEST_STATE__G__State = BOARDTEST_STATE_RED_ON;
    TICKTYPE_T__G__BoardTestLastTick = xTaskGetTickCount();
    BOOL__G__BoardTestRunning = true;
    func__red(true);
}

/**
 * @brief  [EN] Board test tick - call every 10ms, non-blocking.
 *         [FA] تیکه تست برد - هر ۱۰ms، بدون delay.
 * @return bool [EN] true=running / در حال اجرا
 */
bool func__Ui_BoardTest_Tick(void)
{
    TickType_t ticktype__nowTick;
    uint32_t uint32_t__elapsedMs;

    if (BOOL__G__BoardTestRunning == false)
    {
        return false;
    }

    ticktype__nowTick = xTaskGetTickCount();
    uint32_t__elapsedMs = (uint32_t)((ticktype__nowTick - TICKTYPE_T__G__BoardTestLastTick) * portTICK_PERIOD_MS);

    switch (BOARDTEST_STATE__G__State)
    {
        case BOARDTEST_STATE_RED_ON:
            if (uint32_t__elapsedMs >= UI_SELFTEST_LED_MS)
            {
                func__red(false);
                func__yellow(true);
                BOARDTEST_STATE__G__State = BOARDTEST_STATE_YELLOW_ON;
                TICKTYPE_T__G__BoardTestLastTick = ticktype__nowTick;
            }
            break;

        case BOARDTEST_STATE_YELLOW_ON:
            if (uint32_t__elapsedMs >= UI_SELFTEST_LED_MS)
            {
                func__yellow(false);
                func__green(true);
                BOARDTEST_STATE__G__State = BOARDTEST_STATE_GREEN_ON;
                TICKTYPE_T__G__BoardTestLastTick = ticktype__nowTick;
            }
            break;

        case BOARDTEST_STATE_GREEN_ON:
            if (uint32_t__elapsedMs >= UI_SELFTEST_LED_MS)
            {
                func__green(false);
                BOARDTEST_STATE__G__State = BOARDTEST_STATE_BEEP_ON;
                TICKTYPE_T__G__BoardTestLastTick = ticktype__nowTick;
                func__buzzer_pattern_start_internal(0u, UI_BOOT_BEEP_MS, 1u, 0u);
            }
            break;

        case BOARDTEST_STATE_BEEP_ON:
            if (func__Ui_BuzzerPatternMs_Tick() == false)
            {
                BOARDTEST_STATE__G__State = BOARDTEST_STATE_DONE;
                TICKTYPE_T__G__BoardTestLastTick = ticktype__nowTick;
            }
            break;

        case BOARDTEST_STATE_DONE:
            func__all_off();
            BOOL__G__BoardTestRunning = false;
            BOARDTEST_STATE__G__State = BOARDTEST_STATE_IDLE;
            return false;

        default:
            BOOL__G__BoardTestRunning = false;
            return false;
    }

    return true;
}

/* ===== Buzzer pattern public ===== */

/**
 * @brief  [EN] Buzzer pattern Ms start - non-blocking.
 *         [FA] شروع الگوی بازر Ms - غیربلوکه.
 * @param  uint32_t__periodMs [EN] Period ms / دوره تناوب
 * @param  uint32_t__onTimeMs [EN] Total ON ms / زمان روشن
 * @param  uint8_t__repeatCount [EN] Repeat / تکرار
 * @param  uint32_t__gapMs [EN] Gap ms / گپ
 */
void func__Ui_BuzzerPatternMs_Start(uint32_t uint32_t__periodMs, uint32_t uint32_t__onTimeMs, uint8_t uint8_t__repeatCount, uint32_t uint32_t__gapMs)
{
    func__buzzer_pattern_start_internal(uint32_t__periodMs, uint32_t__onTimeMs, uint8_t__repeatCount, uint32_t__gapMs);
}

/**
 * @brief  [EN] Buzzer pattern Ms tick - call every 10ms, non-blocking.
 *         [FA] تیکه الگوی بازر Ms - هر ۱۰ms.
 * @return bool [EN] true=running / در حال اجرا
 */
bool func__Ui_BuzzerPatternMs_Tick(void)
{
    TickType_t ticktype__nowTick;
    uint32_t uint32_t__elapsedMs;

    if (BOOL__G__BuzzerRunning == false)
    {
        return false;
    }

    ticktype__nowTick = xTaskGetTickCount();
    uint32_t__elapsedMs = (uint32_t)((ticktype__nowTick - TICKTYPE_T__G__BuzzerLastTick) * portTICK_PERIOD_MS);

    switch (BUZZER_STATE__G__State)
    {
        case BUZZER_STATE_PULSE_ON:
            if (uint32_t__elapsedMs >= UINT32_T__G__BuzzerPulseOnMs)
            {
                func__buzzer(false);
                if (UINT8_T__G__BuzzerPulseIndex < (UINT8_T__G__BuzzerRepeatCount - 1u))
                {
                    BUZZER_STATE__G__State = BUZZER_STATE_GAP_OFF;
                    TICKTYPE_T__G__BuzzerLastTick = ticktype__nowTick;
                }
                else
                {
                    /* [EN] Pattern finished, check period
                       [FA] الگو تمام، چک دوره */
                    if ((UINT32_T__G__BuzzerPeriodMs == 0u) || (UINT32_T__G__BuzzerPeriodMs <= UINT32_T__G__BuzzerTotalOnMs))
                    {
                        BUZZER_STATE__G__State = BUZZER_STATE_IDLE;
                        BOOL__G__BuzzerRunning = false;
                        return false;
                    }
                    else
                    {
                        BUZZER_STATE__G__State = BUZZER_STATE_PERIOD_OFF;
                        TICKTYPE_T__G__BuzzerLastTick = ticktype__nowTick;
                    }
                }
            }
            break;

        case BUZZER_STATE_GAP_OFF:
            if (uint32_t__elapsedMs >= UINT32_T__G__BuzzerGapMs)
            {
                UINT8_T__G__BuzzerPulseIndex++;
                BUZZER_STATE__G__State = BUZZER_STATE_PULSE_ON;
                TICKTYPE_T__G__BuzzerLastTick = ticktype__nowTick;
                func__buzzer(true);
            }
            break;

        case BUZZER_STATE_PERIOD_OFF:
            if (uint32_t__elapsedMs >= (UINT32_T__G__BuzzerPeriodMs - UINT32_T__G__BuzzerTotalOnMs))
            {
                BUZZER_STATE__G__State = BUZZER_STATE_IDLE;
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

/**
 * @brief  [EN] Buzzer pattern percent start - non-blocking.
 *         [FA] شروع الگوی بازر درصدی - غیربلوکه.
 * @param  uint32_t__periodMs [EN] Period ms / دوره تناوب
 * @param  uint32_t__onTimeMs [EN] ON time ms / زمان روشن
 * @param  uint8_t__repeatCount [EN] Repeat / تکرار
 * @param  uint8_t__gapPercent [EN] Gap percent / گپ درصدی
 */
void func__Ui_BuzzerPatternPercent_Start(uint32_t uint32_t__periodMs, uint32_t uint32_t__onTimeMs, uint8_t uint8_t__repeatCount, uint8_t uint8_t__gapPercent)
{
    uint32_t uint32_t__buzzerGapMs;
    uint8_t uint8_t__buzzerGapPercentClamped;

    if (uint8_t__gapPercent > 90u)
    {
        uint8_t__buzzerGapPercentClamped = 90u;
    }
    else
    {
        uint8_t__buzzerGapPercentClamped = uint8_t__gapPercent;
    }

    if (uint8_t__repeatCount <= 1u)
    {
        func__buzzer_pattern_start_internal(uint32_t__periodMs, uint32_t__onTimeMs, uint8_t__repeatCount, 0u);
        return;
    }

    uint32_t__buzzerGapMs = (uint32_t__onTimeMs * (uint32_t)uint8_t__buzzerGapPercentClamped) / 100u;

    func__buzzer_pattern_start_internal(uint32_t__periodMs, uint32_t__onTimeMs, uint8_t__repeatCount, uint32_t__buzzerGapMs);
}

/**
 * @brief  [EN] Buzzer pattern percent tick.
 *         [FA] تیکه الگوی بازر درصدی.
 * @return bool [EN] true=running / در حال اجرا
 */
bool func__Ui_BuzzerPatternPercent_Tick(void)
{
    return func__Ui_BuzzerPatternMs_Tick();
}

/**
 * @brief  [EN] Stop buzzer pattern immediately.
 *         [FA] توقف فوری الگوی بازر.
 */
void func__Ui_BuzzerPattern_Stop(void)
{
    func__buzzer(false);
    BUZZER_STATE__G__State = BUZZER_STATE_IDLE;
    BOOL__G__BuzzerRunning = false;
}

/**
 * @brief  [EN] Battery voltage to percent 0..100.
 *         [FA] ولتاژ باتری به درصد.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV / ولتاژ باتری
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func__Ui_BatteryVoltageToPercent(uint32_t uint32_t__batteryMv)
{
    uint32_t uint32_t__voltageRangeMv;
    uint32_t uint32_t__voltageOffsetMv;
    uint8_t uint8_t__batteryPercent;

    if (uint32_t__batteryMv <= UI_BAT_V_MIN_MV)
    {
        return 0u;
    }

    if (uint32_t__batteryMv >= UI_BAT_V_MAX_MV)
    {
        return UI_PERCENT_FULL;
    }

    uint32_t__voltageRangeMv = UI_BAT_V_MAX_MV - UI_BAT_V_MIN_MV;
    uint32_t__voltageOffsetMv = uint32_t__batteryMv - UI_BAT_V_MIN_MV;

    if (uint32_t__voltageRangeMv == 0u)
    {
        return 0u;
    }

    uint8_t__batteryPercent = (uint8_t)((uint32_t__voltageOffsetMv * 100u) / uint32_t__voltageRangeMv);

    if (uint8_t__batteryPercent > UI_PERCENT_FULL)
    {
        uint8_t__batteryPercent = UI_PERCENT_FULL;
    }

    return uint8_t__batteryPercent;
}

/**
 * @brief  [EN] InputOk: green steady, others off. Non-blocking.
 *         [FA] ورودی عادی: سبز ثابت، بدون delay.
 */
void func__Ui_ScenarioInputOk(void)
{
    func__green(true);
    func__red(false);
    func__yellow(false);
    func__buzzer(false);
}

/**
 * @brief  [EN] BatteryRun tick - non-blocking, call every 10ms.
 *         [FA] تیکه دشارژ - هر ۱۰ms، بدون delay.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV / ولتاژ باتری
 */
void func__Ui_ScenarioBatteryRun_Tick(uint32_t uint32_t__batteryMv)
{
    uint8_t uint8_t__batteryPercent;
    TickType_t ticktype__nowTick;
    uint32_t uint32_t__elapsedGreenMs;
    uint32_t uint32_t__elapsedBeepMs;
    uint32_t uint32_t__beepIntervalMs;
    uint32_t uint32_t__beepDurationMs;

    uint8_t__batteryPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);

    /* [EN] Green blink timing - non-blocking
       [FA] چشمک سبز - غیربلوکه */
    if (UINT32_T__G__GreenOnMs == 0u)
    {
        uint32_t uint32_t__greenOffMs;

        uint32_t__greenOffMs = (uint32_t)(UI_PERCENT_FULL - uint8_t__batteryPercent) * (UI_BLINK_PERIOD_MS / 100u);
        if (uint32_t__greenOffMs < UI_GREEN_MIN_OFF_MS)
        {
            uint32_t__greenOffMs = UI_GREEN_MIN_OFF_MS;
        }
        UINT32_T__G__GreenOnMs = UI_BLINK_PERIOD_MS - uint32_t__greenOffMs;
        UINT32_T__G__GreenOffMs = uint32_t__greenOffMs;
        TICKTYPE_T__G__GreenLastToggleTick = xTaskGetTickCount();
        BOOL__G__GreenOn = true;
        func__green(true);
        func__red(false);
        func__yellow(false);
    }

    ticktype__nowTick = xTaskGetTickCount();
    uint32_t__elapsedGreenMs = (uint32_t)((ticktype__nowTick - TICKTYPE_T__G__GreenLastToggleTick) * portTICK_PERIOD_MS);

    if (BOOL__G__GreenOn == true)
    {
        if (uint32_t__elapsedGreenMs >= UINT32_T__G__GreenOnMs)
        {
            func__green(false);
            BOOL__G__GreenOn = false;
            TICKTYPE_T__G__GreenLastToggleTick = ticktype__nowTick;
        }
    }
    else
    {
        if (uint32_t__elapsedGreenMs >= UINT32_T__G__GreenOffMs)
        {
            /* [EN] Recalc on/off based on current percent
               [FA] بازمحاسبه بر اساس درصد فعلی */
            uint32_t uint32_t__greenOffMs;

            uint32_t__greenOffMs = (uint32_t)(UI_PERCENT_FULL - uint8_t__batteryPercent) * (UI_BLINK_PERIOD_MS / 100u);
            if (uint32_t__greenOffMs < UI_GREEN_MIN_OFF_MS)
            {
                uint32_t__greenOffMs = UI_GREEN_MIN_OFF_MS;
            }
            UINT32_T__G__GreenOnMs = UI_BLINK_PERIOD_MS - uint32_t__greenOffMs;
            UINT32_T__G__GreenOffMs = uint32_t__greenOffMs;

            func__green(true);
            BOOL__G__GreenOn = true;
            TICKTYPE_T__G__GreenLastToggleTick = ticktype__nowTick;
        }
    }

    /* [EN] Smart beep - non-blocking, every batteryPercent seconds
       [FA] بوق هوشمند - هر درصد ثانیه، بدون delay */
    if (uint8_t__batteryPercent >= UI_BEEP_START_PCT)
    {
        UINT32_T__G__UiBatteryRunBeepCycleCnt = 0u;
        TICKTYPE_T__G__BeepLastTick = ticktype__nowTick;
        return;
    }

    uint32_t__elapsedBeepMs = (uint32_t)((ticktype__nowTick - TICKTYPE_T__G__BeepLastTick) * portTICK_PERIOD_MS);
    uint32_t__beepIntervalMs = (uint32_t)uint8_t__batteryPercent * 1000u;
    if (uint32_t__beepIntervalMs < 1000u)
    {
        uint32_t__beepIntervalMs = 1000u;
    }

    if (uint32_t__elapsedBeepMs >= uint32_t__beepIntervalMs)
    {
        uint32_t__beepDurationMs = UI_BEEP_BASE_MS;
        if (uint8_t__batteryPercent < UI_BEEP_DOUBLE_THRESH_PCT)
        {
            uint32_t__beepDurationMs *= 2u;
        }

        /* [EN] Start buzzer pattern non-blocking: period 0, onTime beepDuration, repeat 1
           [FA] شروع الگوی بازر غیربلوکه داخل سناریو LED */
        func__Ui_BuzzerPatternMs_Start(0u, uint32_t__beepDurationMs, 1u, 0u);
        TICKTYPE_T__G__BeepLastTick = ticktype__nowTick;
        UINT32_T__G__UiBatteryRunBeepCycleCnt++;
    }

    /* [EN] Tick buzzer pattern if running
       [FA] تیکه بازر اگر در حال اجرا */
    if (BOOL__G__BuzzerRunning == true)
    {
        (void)func__Ui_BuzzerPatternMs_Tick();
    }
}

/**
 * @brief  [EN] Charging tick - non-blocking, call every 10ms.
 *         [FA] تیکه شارژ - هر ۱۰ms، بدون delay.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV / ولتاژ باتری
 */
void func__Ui_ScenarioCharging_Tick(uint32_t uint32_t__batteryMv)
{
    uint8_t uint8_t__batteryPercent;
    TickType_t ticktype__nowTick;
    uint32_t uint32_t__elapsedYellowMs;

    uint8_t__batteryPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);

    if (uint8_t__batteryPercent >= UI_PERCENT_FULL)
    {
        func__yellow(false);
        func__green(true);
        func__red(false);
        func__buzzer(false);
        return;
    }

    if (uint8_t__batteryPercent == 0u)
    {
        func__yellow(true);
        func__green(true);
        func__red(false);
        func__buzzer(false);
        return;
    }

    if (UINT32_T__G__YellowOnMs == 0u)
    {
        uint32_t uint32_t__yellowOnMs;

        uint32_t__yellowOnMs = (uint32_t)(UI_PERCENT_FULL - uint8_t__batteryPercent) * (UI_CHARGING_BLINK_PERIOD_MS / 100u);
        if (uint32_t__yellowOnMs < UI_CHARGING_YELLOW_MIN_OFF_MS)
        {
            uint32_t__yellowOnMs = UI_CHARGING_YELLOW_MIN_OFF_MS;
        }
        if (uint32_t__yellowOnMs > UI_CHARGING_BLINK_PERIOD_MS)
        {
            uint32_t__yellowOnMs = UI_CHARGING_BLINK_PERIOD_MS;
        }
        UINT32_T__G__YellowOnMs = uint32_t__yellowOnMs;
        UINT32_T__G__YellowOffMs = UI_CHARGING_BLINK_PERIOD_MS - uint32_t__yellowOnMs;
        TICKTYPE_T__G__YellowLastToggleTick = xTaskGetTickCount();
        BOOL__G__YellowOn = true;
        func__yellow(true);
        func__green(true);
        func__red(false);
        func__buzzer(false);
    }

    ticktype__nowTick = xTaskGetTickCount();
    uint32_t__elapsedYellowMs = (uint32_t)((ticktype__nowTick - TICKTYPE_T__G__YellowLastToggleTick) * portTICK_PERIOD_MS);

    if (BOOL__G__YellowOn == true)
    {
        if (uint32_t__elapsedYellowMs >= UINT32_T__G__YellowOnMs)
        {
            func__yellow(false);
            BOOL__G__YellowOn = false;
            TICKTYPE_T__G__YellowLastToggleTick = ticktype__nowTick;
        }
    }
    else
    {
        if (uint32_t__elapsedYellowMs >= UINT32_T__G__YellowOffMs)
        {
            uint32_t uint32_t__yellowOnMs;

            uint32_t__yellowOnMs = (uint32_t)(UI_PERCENT_FULL - uint8_t__batteryPercent) * (UI_CHARGING_BLINK_PERIOD_MS / 100u);
            if (uint32_t__yellowOnMs < UI_CHARGING_YELLOW_MIN_OFF_MS)
            {
                uint32_t__yellowOnMs = UI_CHARGING_YELLOW_MIN_OFF_MS;
            }
            if (uint32_t__yellowOnMs > UI_CHARGING_BLINK_PERIOD_MS)
            {
                uint32_t__yellowOnMs = UI_CHARGING_BLINK_PERIOD_MS;
            }
            UINT32_T__G__YellowOnMs = uint32_t__yellowOnMs;
            UINT32_T__G__YellowOffMs = UI_CHARGING_BLINK_PERIOD_MS - uint32_t__yellowOnMs;

            func__yellow(true);
            BOOL__G__YellowOn = true;
            TICKTYPE_T__G__YellowLastToggleTick = ticktype__nowTick;
        }
    }

    func__green(true);
    func__red(false);
    func__buzzer(false);
}

/**
 * @brief  [EN] Ui main tick - decides scenario, non-blocking, call every 10ms.
 *         [FA] تیکه اصلی UI - تصمیم سناریو، بدون delay، هر ۱۰ms.
 * @param  uint32_t__inputVoltageMv [EN] Input voltage mV / ولتاژ ورودی
 * @param  uint32_t__batteryVoltageMv [EN] Battery voltage mV / ولتاژ باتری
 */
void func__Ui_Tick(uint32_t uint32_t__inputVoltageMv, uint32_t uint32_t__batteryVoltageMv)
{
    uint8_t uint8_t__batteryPercent;
    bool bool__inputPresent;

    if (uint32_t__batteryVoltageMv > UI_BAT_V_MAX_MV)
    {
        uint32_t__batteryVoltageMv = UI_BAT_V_MAX_MV;
    }

    uint8_t__batteryPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryVoltageMv);
    bool__inputPresent = (uint32_t__inputVoltageMv >= UI_INPUT_THRESHOLD_MV);

    if (bool__inputPresent == true)
    {
        if (uint8_t__batteryPercent < UI_PERCENT_FULL)
        {
            func__Ui_ScenarioCharging_Tick(uint32_t__batteryVoltageMv);
        }
        else
        {
            func__Ui_ScenarioInputOk();
        }
    }
    else
    {
        func__Ui_ScenarioBatteryRun_Tick(uint32_t__batteryVoltageMv);
    }
}
