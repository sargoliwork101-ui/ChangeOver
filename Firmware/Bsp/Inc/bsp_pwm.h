/**
 * @file    bsp_pwm.h
 * @brief   [EN] Logical PWM interface for charger outputs.
 *          [FA] رابط منطقی PWM برای خروجی‌های شارژر.
 *
 * @note    [EN] Timer instances, channels and HAL handles are private to the
 *              board implementation. The current board port is a placeholder
 *              because PWM is not enabled in the active CubeMX stage.
 *          [FA] نمونه‌های تایمر، کانال‌ها و هندل‌های HAL در پیاده‌سازی برد
 *              خصوصی هستند. پورت فعلی اسکلت است چون PWM در مرحلهٔ فعال
 *              CubeMX روشن نیست.
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

/**
 * @brief  [EN] Initialize the board PWM backend and force outputs off.
 *         [FA] Backend PWM برد را مقداردهی و خروجی‌ها را خاموش می‌کند.
 */
/* ==================== Functions ==================== */
void func__BspPwm_Init(void);

/**
 * @brief  [EN] Set logical charger duty in permille (0..1000).
 *         [FA] وظیفهٔ منطقی شارژر را بر حسب پرمیل (۰..۱۰۰۰) تنظیم می‌کند.
 * @param  bsp_pwm_channel_t__channel [EN] Logical charger channel / کانال منطقی
 * @param  uint16_t__permille [EN] 0 = off, 1000 = 100 percent / پرمیل
 */
void func__BspPwm_SetDutyPermille(bsp_pwm_channel_t bsp_pwm_channel_t__channel,
                                   uint16_t uint16_t__permille);

/**
 * @brief  [EN] Force every board PWM output off.
 *         [FA] همهٔ خروجی‌های PWM برد را خاموش می‌کند.
 */
void func__BspPwm_StopAll(void);

#endif /* BSP_PWM_H */
