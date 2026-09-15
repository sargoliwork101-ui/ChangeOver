/**
 * @file    ui_buzzer.c
 * @brief   [EN] UI buzzer patterns - separate from LED, constants for buzzer in its own header.
 *          Non-linear formulas broken into steps, RTOS simple readable, markers above each func and variable.
 *          [FA] الگوهای بازر ماژول UI - ثابت‌های بازر در هدر خودش، هر تابع و متغیر با جدا کننده.
 *
 * @note    [EN] Buzzer constants in ui_buzzer.h per user request. Naming __ after type, func__ prefix.
 *          RTOS: vTaskDelay allowed, HAL_Delay forbidden. Formulas non-linear broken into steps.
 *          [FA] ثابت‌های بازر در همین هدر. نام‌گذاری با __، پیشوند func__، فرمول غیرخطی.
 */

#include "ui_buzzer.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* ==================== Buzzer / Beep ==================== */

/* ==================== Buzzer Low Level ==================== */

static void func__buzzer(bool bool__buzzerOn)
{
    func__BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, bool__buzzerOn);
}

/* ==================== Calc Beep On ==================== */

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

/* ==================== Buzzer State Type ==================== */

typedef enum { BUZZER_IDLE, BUZZER_PULSE_ON, BUZZER_GAP_OFF, BUZZER_PERIOD_OFF } buzzer_state_t;

/* ==================== Buzzer State ==================== */

static buzzer_state_t BUZZER_STATE__G__State = BUZZER_IDLE;

/* ==================== Buzzer Total On Ms ==================== */

static uint32_t UINT32_T__G__BuzzerTotalOnMs = 0u;

/* ==================== Buzzer Gap Ms ==================== */

static uint32_t UINT32_T__G__BuzzerGapMs = 0u;

/* ==================== Buzzer Pulse On Ms ==================== */

static uint32_t UINT32_T__G__BuzzerPulseOnMs = 0u;

/* ==================== Buzzer Period Ms ==================== */

static uint32_t UINT32_T__G__BuzzerPeriodMs = 0u;

/* ==================== Buzzer Repeat Count ==================== */

static uint8_t UINT8_T__G__BuzzerRepeatCount = 0u;

/* ==================== Buzzer Pulse Index ==================== */

static uint8_t UINT8_T__G__BuzzerPulseIndex = 0u;

/* ==================== Buzzer Last Tick ==================== */

static TickType_t TICKTYPE_T__G__BuzzerLastTick = 0;

/* ==================== Buzzer Running ==================== */

static bool BOOL__G__BuzzerRunning = false;

/* ==================== Buzzer Start Internal ==================== */

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

/* ==================== Buzzer Pattern Ms Start ==================== */

void func__Ui_BuzzerPatternMs_Start(uint32_t uint32_t__periodMs, uint32_t uint32_t__onTimeMs, uint8_t uint8_t__repeatCount, uint32_t uint32_t__gapMs)
{
    func__buzzer_start_internal(uint32_t__periodMs, uint32_t__onTimeMs, uint8_t__repeatCount, uint32_t__gapMs);
}

/* ==================== Buzzer Pattern Ms Tick ==================== */

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

/* ==================== Buzzer Pattern Percent Start ==================== */

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

/* ==================== Buzzer Pattern Percent Tick ==================== */

bool func__Ui_BuzzerPatternPercent_Tick(void)
{
    return func__Ui_BuzzerPatternMs_Tick();
}

/* ==================== Buzzer Pattern Stop ==================== */

void func__Ui_BuzzerPattern_Stop(void)
{
    func__buzzer(false);
    BUZZER_STATE__G__State = BUZZER_IDLE;
    BOOL__G__BuzzerRunning = false;
}
