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
 * @brief  [EN] Start both timer backend channels once with a frozen
 *              half-period (10 us at 50 kHz) interleave, gates initially
 *              low. The counters then run forever; "off" is compare=0.
 *         [FA] هر دو کانال تایمر را یک‌بار با درهم‌گذاری ثابتِ نیم‌دوره
 *              (۱۰µs در ۵۰kHz) شروع می‌کند و گیت‌ها پایین می‌مانند. شمارنده‌ها
 *              دیگر همیشه می‌چرخند و «خاموش» یعنی compare=0.
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
 * @brief  [EN] Force both gates low (compare 0); the counters keep running
 *              so the frozen interleave offset is preserved.
 *         [FA] هر دو گیت را پایین می‌آورد (compare صفر)؛ شمارنده‌ها به چرخش
 *              می‌مانند تا آفست درهم‌گذاری حفظ شود.
 */
void func__BspPwm_StopAll(void);

#endif /* BSP_PWM_H */
