/**
 * @file    bsp_pwm.h
 * @brief   [EN] Logical PWM interface for both schematic charger outputs.
 *          [FA] رابط منطقی PWM برای هر دو خروجی شارژر شماتیک.
 *
 * @note    [EN] Timer instances, channels and HAL handles are private to the
 *              board implementation. The interface remains available while
 *              the Charger module is disabled.
 *          [FA] نمونهٔ تایمر، کانال و هندل HAL در پورت برد خصوصی هستند. این
 *              رابط حتی وقتی ماژول Charger خاموش است حفظ می‌شود.
 */

#ifndef BSP_PWM_H
#define BSP_PWM_H

#include <stdint.h>

typedef enum
{
    BSP_PWM_CHARGER_1 = 0,
    BSP_PWM_CHARGER_2,
    BSP_PWM_CHANNEL_COUNT
} bsp_pwm_channel_t;

/* ==================== BspPwm_Init ==================== */
/**
 * @brief  [EN] Initialize the logical PWM backend and force both channels off.
 *         [FA] backend منطقی PWM را مقداردهی و هر دو کانال را خاموش می‌کند.
 */
void func__BspPwm_Init(void);

/* ==================== BspPwm_SetDutyPermille ==================== */
/**
 * @brief  [EN] Set one charger duty in the inclusive range 0..1000 permille.
 *              Values above 1000 are clamped; zero stops that output.
 *         [FA] وظیفهٔ یک شارژر را در بازهٔ ۰ تا ۱۰۰۰ پرمیل تنظیم می‌کند.
 *              مقدار بالاتر از ۱۰۰۰ محدود و صفر باعث توقف خروجی می‌شود.
 * @param  bsp_pwm_channel_t__channel [EN] Logical charger channel /
 *                                     کانال منطقی شارژر
 * @param  uint16_t__permille [EN] Duty, 0=off and 1000=100 percent /
 *                                 وظیفه، صفر خاموش و ۱۰۰۰ برابر صددرصد
 */
void func__BspPwm_SetDutyPermille(bsp_pwm_channel_t bsp_pwm_channel_t__channel,
                                   uint16_t uint16_t__permille);

/* ==================== BspPwm_StopAll ==================== */
/**
 * @brief  [EN] Set compare values to zero and stop every board PWM channel.
 *         [FA] مقدار compare همهٔ کانال‌ها را صفر و PWM برد را متوقف می‌کند.
 */
void func__BspPwm_StopAll(void);

/* ==================== BspPwm_TripOffFromIsr ==================== */
/**
 * @brief  [EN] ISR-safe trip: zero compare, disable outputs, latch fault.
 *         Must not use HAL_TIM_PWM_Stop, osDelay, queue, mutex, malloc, logging.
 *         [FA] تریپ ISR-safe: صفر CCR، قطع خروجی، بدون HAL یا RTOS.
 */
void func__BspPwm_TripOffFromIsr(void);

/* ==================== BspPwm_GetFrequency ==================== */
/**
 * @brief  [EN] Return actual PWM frequency for test: TIMCLK/(PSC+1)/(ARR+1).
 *         [FA] فرکانس واقعی PWM برای تست.
 * @return uint32_t [EN] Hz
 */
uint32_t func__BspPwm_GetFrequencyHz(void);

#endif /* BSP_PWM_H */
