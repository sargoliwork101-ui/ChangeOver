/**
 * @file    ui_buzzer.h
 * @brief   [EN] One public UI buzzer service: periodic pattern from period, duty, count, and gap.
 * @note    [EN] The return value is the recommended next-call delay in milliseconds.
 */
#ifndef UI_BUZZER_H
#define UI_BUZZER_H
/* ==================== Includes ==================== */
#include <stdint.h>
/* ==================== Buzzer timing constants ==================== */
/**
 * @brief  [EN] Percentage scale used by duty and check calculations.
 *         100 means that a duty value is expressed as a percentage.
 */
#define UI_BUZZER_PERCENT_SCALE         100u
/**
 * @brief  [EN] Maximum accepted duty-window percentage.
 *         Values above this limit are invalid and turn the buzzer off.
 */
#define UI_BUZZER_DUTY_MAX_PERCENT      100u
/**
 * @brief  [EN] Minimum accepted non-zero complete pattern period in milliseconds.
 *         This prevents very fast periodic switching of the buzzer GPIO.
 */
#define UI_BUZZER_MIN_PERIOD_MS         1000u
/**
 * @brief  [EN] Minimum gap between adjacent pulses when beepCount is above one.
 *         A single pulse has no adjacent gap and does not use this limit.
 */
#define UI_BUZZER_MIN_GAP_MS            100u
/**
 * @brief  [EN] Percentage of the smallest positive pattern segment used to recommend the next RTOS check.
 */
#define UI_BUZZER_CHECK_PERCENT         10u
/**
 * @brief  [EN] Minimum positive delay returned to a caller between buzzer checks.
 */
#define UI_BUZZER_MIN_CHECK_MS          1u
/* ==================== Buzzer result constants ==================== */
/**
 * @brief  [EN] Return value for a valid command that intentionally disables the buzzer.
 */
#define UI_BUZZER_OFF_RESULT            0
/**
 * @brief  [EN] Return value for an unsafe or invalid buzzer configuration.
 *         The GPIO is forced LOW when this result is returned.
 */
#define UI_BUZZER_INVALID_RESULT        (-1)
/* ==================== BoardTest compatibility constant ==================== */
/**
 * @brief  [EN] Default one-shot beep duration used by the BoardTest scenario.
 *         APP_CONFIG copies this default and the scenario converts it to a valid duty window.
 */
#define UI_BOOT_BEEP_MS                 150u
/* ==================== Buzzer service ==================== */
/**
 * @brief  [EN] Service one periodic buzzer pattern without blocking the
 *         task. Pattern repeats every periodMs; duty window =
 *         periodMs*dutyPercent/100, beepCount pulses share it, gapMs sits
 *         between adjacent pulses. Zero period/duty/count = OFF result;
 * a too-short period, out-of-range duty, too-small gap for multiple
 *         pulses, or no positive time per pulse = INVALID result (buzzer
 *         off). Valid patterns return the recommended next-call delay
 * (UI_BUZZER_CHECK_PERCENT of the smallest positive segment).
* @param uint32_t__periodMs Pattern period in ms / ms
* @param uint8_t__dutyPercent Duty window 0..100 percent /
* @param uint8_t__beepCount Number of pulses; zero disables /
* @param uint32_t__gapMs Low gap between adjacent pulses in ms / ms
 * @return int32_t [EN] Next-call delay, zero for valid off, or -1 for invalid configuration.
 */
int32_t func__Ui_Buzzer_Tick(uint32_t uint32_t__periodMs, uint8_t uint8_t__dutyPercent, uint8_t uint8_t__beepCount, uint32_t uint32_t__gapMs);
/* ==================== Buzzer duty-window override ==================== */
/**
 * @brief  [EN] Supply an exact millisecond duty window for the next
 *         func__Ui_Buzzer_Tick() call so long periods (e.g. 1 h = 3600000 ms)
 *         keep millisecond pulse precision instead of 1%-of-period rounding.
* @param uint32_t__windowMs Exact duty window in ms (0 = use percent) / ms
 */
void func__Ui_Buzzer_SetDutyWindowMs(uint32_t uint32_t__windowMs);
#endif /* UI_BUZZER_H */
