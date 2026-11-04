/**
 * @file    ui_buzzer.c
 * @brief   [EN] One non-blocking periodic buzzer service. It calculates pulse timing from period, duty, count, and gap.
 * @note    [EN] Only explicit scenarios call this service; all buzzer GPIO control remains here. The caller supplies the four inputs to func__Ui_Buzzer_Tick().
 */
#include "ui_buzzer.h"
#include "bsp_gpio.h"
#include "cmsis_os2.h"
#include "rtos_time.h"
#include <stdbool.h>
#include <stdint.h>
/* ==================== Buzzer / Beep ==================== */
/* ==================== Buzzer persistent state ==================== */
/**
 * @brief  [EN] Validity flag of the stored pattern; false forces the next
 *         valid request to start a new cycle.
 */
static bool BOOL__G__BuzzerPatternValid = false;
/* ==================== Buzzer period ==================== */
/**
 * @brief  [EN] Last accepted period [ms]; a change restarts the cycle.
 */
static uint32_t UINT32_T__G__BuzzerPeriodMs = 0u;
/* ==================== Buzzer duty and count ==================== */
/**
 * @brief  [EN] Last accepted duty window [percent]; a change restarts.
 */
static uint8_t UINT8_T__G__BuzzerDutyPercent = 0u;
/**
 * @brief  [EN] Last accepted pulse count; a change restarts the pattern.
 */
static uint8_t UINT8_T__G__BuzzerCount = 0u;
/* ==================== Buzzer gap ==================== */
/**
 * @brief  [EN] Last accepted gap between pulses [ms]; a change restarts.
 */
static uint32_t UINT32_T__G__BuzzerGapMs = 0u;
/* ==================== Buzzer cycle start ==================== */
/**
 * @brief  [EN] Tick of the current cycle start; the non-blocking service
 *         derives elapsed time (on / gap / period tail) from it.
 */
static uint32_t UINT32_T__G__BuzzerCycleStartTick = 0;
/* ==================== Buzzer exact window ==================== */
/**
 * @brief  [EN] Exact millisecond duty window queued by func__Ui_BeepDutyPercent,
 *         plus the last accepted window for change detection.
 */
static uint32_t UINT32_T__G__BuzzerPendingWindowMs = 0u;
static uint32_t UINT32_T__G__BuzzerWindowMs = 0u;
/* ==================== Ui_Buzzer_SetDutyWindowMs ==================== */
/**
 * @brief  [EN] Queue an exact millisecond duty window for the next buzzer tick.
* @param uint32_t__windowMs Exact duty window in ms (0 = use percent) / ms
 */
void func__Ui_Buzzer_SetDutyWindowMs(uint32_t uint32_t__windowMs)
{
    UINT32_T__G__BuzzerPendingWindowMs = uint32_t__windowMs;
}
/* ==================== Buzzer service ==================== */
/**
 * @brief  [EN] Service one periodic buzzer pattern without blocking the task.
 *         The duty window is period*duty/100. Pulses and gaps fill that window;
 *         the remaining time is low until the next period starts.
* @param uint32_t__periodMs Pattern period in ms / ms
* @param uint8_t__dutyPercent Duty window 0..100 percent /
* @param uint8_t__beepCount Number of pulses; zero disables /
* @param uint32_t__gapMs Low gap between adjacent pulses in ms / ms
 * @return int32_t [EN] Recommended next-call delay, zero for valid off, or -1 for invalid input.
 */
int32_t func__Ui_Buzzer_Tick(uint32_t uint32_t__periodMs, uint8_t uint8_t__dutyPercent, uint8_t uint8_t__beepCount, uint32_t uint32_t__gapMs)
{
    uint32_t uint32_t__nowTick;
    uint32_t uint32_t__dutyWindowMs;
    uint32_t uint32_t__gapCount;
    uint32_t uint32_t__effectiveGapMs;
    uint32_t uint32_t__totalGapMs;
    uint32_t uint32_t__availableOnMs;
    uint32_t uint32_t__beepOnMs;
    uint32_t uint32_t__lastBeepOnMs;
    uint32_t uint32_t__onRemainderMs;
    uint32_t uint32_t__periodTailMs;
    uint32_t uint32_t__smallestTimingMs;
    uint32_t uint32_t__nextCheckMs;
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__cursorMs;
    uint32_t uint32_t__currentBeepOnMs;
    uint8_t uint8_t__beepIndex;
    uint32_t uint32_t__gapProduct;
    uint32_t uint32_t__pendingWindowMs;
    bool bool__configurationChanged;
    bool bool__buzzerOn;
    uint32_t__pendingWindowMs = UINT32_T__G__BuzzerPendingWindowMs;
    UINT32_T__G__BuzzerPendingWindowMs = 0u;
    /*
 * [EN] Zero is an intentional safe-off command, not an invalid configuration.
 */
    if ((uint32_t__periodMs == 0u) ||
        (uint8_t__dutyPercent == 0u) ||
        (uint8_t__beepCount == 0u))
    {
        func__BspGpio_Write(BSP_GPIO_BUZZER, false);
        BOOL__G__BuzzerPatternValid = false;
        return UI_BUZZER_OFF_RESULT;
    }
    /*
 * [EN] Reject unsafe non-zero configurations and keep the buzzer LOW.
 */
    if ((uint32_t__periodMs < UI_BUZZER_MIN_PERIOD_MS) ||
        (uint8_t__dutyPercent > UI_BUZZER_DUTY_MAX_PERCENT) ||
        ((uint8_t__beepCount > 1u) && (uint32_t__gapMs < UI_BUZZER_MIN_GAP_MS)))
    {
        func__BspGpio_Write(BSP_GPIO_BUZZER, false);
        BOOL__G__BuzzerPatternValid = false;
        return UI_BUZZER_INVALID_RESULT;
    }
    /*
 * [EN] A gap has meaning only between adjacent pulses.
 */
    uint32_t__effectiveGapMs = 0u;
    if (uint8_t__beepCount > 1u)
    {
        uint32_t__effectiveGapMs = uint32_t__gapMs;
    }
    /*
 * [EN] Duty window without u64 (flash diet): (p/100)*d + ((p%100)*d)/100
 * is the identical quotient for the full u32 range; (p/100)*d cannot
 * overflow because duty <= 100.
 */
    uint32_t__dutyWindowMs =
        ((uint32_t__periodMs / UI_BUZZER_PERCENT_SCALE) * (uint32_t)uint8_t__dutyPercent) +
        (((uint32_t__periodMs % UI_BUZZER_PERCENT_SCALE) * (uint32_t)uint8_t__dutyPercent) /
         UI_BUZZER_PERCENT_SCALE);
    if ((uint32_t__pendingWindowMs > 0u) &&
        (uint32_t__pendingWindowMs <= uint32_t__periodMs))
    {
        uint32_t__dutyWindowMs = uint32_t__pendingWindowMs;
    }
    uint32_t__gapCount = (uint32_t)uint8_t__beepCount - 1u;
    /*
 * [EN] Gap total in u32 (flash diet 2026-09-27): an overflowing product
 * can only exceed the u32 duty window, i.e. exactly the reject the u64
 * path took - clamp it to UINT32_MAX and the comparisons below keep
 * identical outcomes; otherwise the product is identical.
 */
    uint32_t__gapProduct = ((uint32_t__gapCount != 0u) &&
                            (uint32_t__effectiveGapMs > (UINT32_MAX / uint32_t__gapCount)))
                               ? UINT32_MAX
                               : (uint32_t__effectiveGapMs * uint32_t__gapCount);
    /*
 * [EN] Reject a pattern when gaps leave no positive time for every pulse.
 */
    if ((uint32_t__dutyWindowMs == 0u) ||
        (uint32_t__gapProduct >= uint32_t__dutyWindowMs) ||
        ((uint32_t__dutyWindowMs - uint32_t__gapProduct) < (uint32_t)uint8_t__beepCount))
    {
        func__BspGpio_Write(BSP_GPIO_BUZZER, false);
        BOOL__G__BuzzerPatternValid = false;
        return UI_BUZZER_INVALID_RESULT;
    }
    uint32_t__totalGapMs = uint32_t__gapProduct;
    uint32_t__availableOnMs = uint32_t__dutyWindowMs - uint32_t__totalGapMs;
    uint32_t__beepOnMs = uint32_t__availableOnMs / (uint32_t)uint8_t__beepCount;
    uint32_t__onRemainderMs = uint32_t__availableOnMs % (uint32_t)uint8_t__beepCount;
    uint32_t__lastBeepOnMs = uint32_t__beepOnMs + uint32_t__onRemainderMs;
    uint32_t__periodTailMs = uint32_t__periodMs - uint32_t__dutyWindowMs;
    /*
 * [EN] The smallest positive segment controls the next RTOS check.
 */
    uint32_t__smallestTimingMs = uint32_t__beepOnMs;
    if (uint32_t__lastBeepOnMs < uint32_t__smallestTimingMs)
    {
        uint32_t__smallestTimingMs = uint32_t__lastBeepOnMs;
    }
    if ((uint32_t__effectiveGapMs > 0u) &&
        (uint32_t__effectiveGapMs < uint32_t__smallestTimingMs))
    {
        uint32_t__smallestTimingMs = uint32_t__effectiveGapMs;
    }
    if ((uint32_t__periodTailMs > 0u) &&
        (uint32_t__periodTailMs < uint32_t__smallestTimingMs))
    {
        uint32_t__smallestTimingMs = uint32_t__periodTailMs;
    }
    /*
 * [EN] Next-check percent WITHOUT u64 (flash diet 2026-09-27): same
 * exact split as the duty window (CHECK_PERCENT = 10 <= 100).
 */
    uint32_t__nextCheckMs =
        ((uint32_t__smallestTimingMs / UI_BUZZER_PERCENT_SCALE) * UI_BUZZER_CHECK_PERCENT) +
        (((uint32_t__smallestTimingMs % UI_BUZZER_PERCENT_SCALE) * UI_BUZZER_CHECK_PERCENT) /
         UI_BUZZER_PERCENT_SCALE);
    if (uint32_t__nextCheckMs < UI_BUZZER_MIN_CHECK_MS)
    {
        uint32_t__nextCheckMs = UI_BUZZER_MIN_CHECK_MS;
    }
    bool__configurationChanged = (BOOL__G__BuzzerPatternValid == false);
    if (UINT32_T__G__BuzzerPeriodMs != uint32_t__periodMs)
    {
        bool__configurationChanged = true;
    }
    if (UINT8_T__G__BuzzerDutyPercent != uint8_t__dutyPercent)
    {
        bool__configurationChanged = true;
    }
    if (UINT8_T__G__BuzzerCount != uint8_t__beepCount)
    {
        bool__configurationChanged = true;
    }
    if (UINT32_T__G__BuzzerGapMs != uint32_t__effectiveGapMs)
    {
        bool__configurationChanged = true;
    }
    if (UINT32_T__G__BuzzerWindowMs != uint32_t__dutyWindowMs)
    {
        bool__configurationChanged = true;
    }
    uint32_t__nowTick = osKernelGetTickCount();
    if (bool__configurationChanged == true)
    {
        UINT32_T__G__BuzzerPeriodMs = uint32_t__periodMs;
        UINT8_T__G__BuzzerDutyPercent = uint8_t__dutyPercent;
        UINT8_T__G__BuzzerCount = uint8_t__beepCount;
        UINT32_T__G__BuzzerGapMs = uint32_t__effectiveGapMs;
        UINT32_T__G__BuzzerWindowMs = uint32_t__dutyWindowMs;
        UINT32_T__G__BuzzerCycleStartTick = uint32_t__nowTick;
        BOOL__G__BuzzerPatternValid = true;
    }
    uint32_t__elapsedMs = func__Rtos_TicksToMilliseconds(uint32_t__nowTick - UINT32_T__G__BuzzerCycleStartTick);
    if (uint32_t__elapsedMs >= uint32_t__periodMs)
    {
        UINT32_T__G__BuzzerCycleStartTick = uint32_t__nowTick;
        uint32_t__elapsedMs = 0u;
    }
    bool__buzzerOn = false;
    if (uint32_t__elapsedMs < uint32_t__dutyWindowMs)
    {
        uint32_t__cursorMs = 0u;
        for (uint8_t__beepIndex = 0u; uint8_t__beepIndex < uint8_t__beepCount; uint8_t__beepIndex++)
        {
            uint32_t__currentBeepOnMs = uint32_t__beepOnMs;
            if (uint8_t__beepIndex == (uint8_t__beepCount - 1u))
            {
                uint32_t__currentBeepOnMs = uint32_t__lastBeepOnMs;
            }
            if (uint32_t__elapsedMs < (uint32_t__cursorMs + uint32_t__currentBeepOnMs))
            {
                bool__buzzerOn = true;
                break;
            }
            uint32_t__cursorMs += uint32_t__currentBeepOnMs;
            if (uint8_t__beepIndex < (uint8_t__beepCount - 1u))
            {
                if (uint32_t__elapsedMs < (uint32_t__cursorMs + uint32_t__effectiveGapMs))
                {
                    break;
                }
                uint32_t__cursorMs += uint32_t__effectiveGapMs;
            }
        }
    }
    func__BspGpio_Write(BSP_GPIO_BUZZER, bool__buzzerOn);
    return (int32_t)uint32_t__nextCheckMs;
}
