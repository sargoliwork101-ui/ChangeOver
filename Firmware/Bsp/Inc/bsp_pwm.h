/**
 * @file    bsp_pwm.h
 * @brief   [EN] Logical PWM interface for both schematic charger outputs.
 * @note    [EN] Timer instances, channels and HAL handles are private to the
 *              board implementation. The interface remains available while
 *              the Charger module is disabled.
 */
#ifndef BSP_PWM_H
#define BSP_PWM_H
#include <stdint.h>
#include <stdbool.h>
typedef enum
{
    BSP_PWM_CHARGER_1 = 0,
    BSP_PWM_CHARGER_2,
    BSP_PWM_CHANNEL_COUNT
} bsp_pwm_channel_t;
/* ==================== BspPwm_Init ==================== */
/**
 * @brief  [EN] Start both timer backend channels once, gates initially
 *              low. The counters then run forever; "off" is compare=0.
 *              Phase is a compile switch (BSP_PWM_TIM3_PHASE_OFFSET_IN_PHASE):
 *              1u = both gates in phase (bench experiment since 2026-09-24,
 *              user order), 0u = the frozen half-period (10 us at 50 kHz)
 *              interleave of the production design.
 */
void func__BspPwm_Init(void);
/* ==================== BspPwm_SetDutyPermille ==================== */
/**
 * @brief  [EN] Set one charger duty in the inclusive range 0..1000 permille.
 *              Values above 1000 are clamped; zero stops that output.
 * @param  bsp_pwm_channel_t__channel [EN] Logical charger channel /
 * @param  uint16_t__permille [EN] Duty, 0=off and 1000=100 percent /
 */
void func__BspPwm_SetDutyPermille(bsp_pwm_channel_t bsp_pwm_channel_t__channel,
                                   uint16_t uint16_t__permille);
/* ==================== BspPwm_StopAll ==================== */
/**
 * @brief  [EN] Force both gates low (compare 0); the counters keep running
 *              so the frozen interleave offset is preserved. The internal
 *              CH2 sampling triggers are parked too (no mid-ON edges).
 */
void func__BspPwm_StopAll(void);
/* ==================== BspPwm_IsGatePulsing ==================== */
/**
 * @brief  [EN] True while the logical charger gate is pulsing (compare > 0).
 *              Used by the board ADC port to know whether a synchronized
 *              mid-ON current trigger edge will arrive; a parked gate means
 *              the primary current is zero.
 * @param  bsp_pwm_channel_t__channel [EN] Logical charger channel /
* @return bool true while that gate pulses / true
 */
bool func__BspPwm_IsGatePulsing(bsp_pwm_channel_t bsp_pwm_channel_t__channel);
/* ==================== BspPwm_GetCompareCounts ==================== */
/**
 * @brief  [EN] Read the live CH1 compare (gate ON width) in timer ticks.
 *              The board ADC port uses it to reject mid-ON triggers on
 *              runt pulses (compare 1..7): the ADC aperture (625 ns) is
 *              wider than such a window, so the synchronized sample would
 *              be garbage and the asynchronous scan fallback is the honest
 *              value.
* @param bsp_pwm_channel_t__channel Logical charger channel /
* @return uint32_t Compare counts, 0 for an invalid channel /
 */
uint32_t func__BspPwm_GetCompareCounts(bsp_pwm_channel_t bsp_pwm_channel_t__channel);
#endif /* BSP_PWM_H */
