/**
 * @file    bsp_pwm.h
 * @brief   [EN] PWM wrapper for charger channels (placeholder). Full type naming, func__ prefix.
 *          [FA] پوشش PWM کانال شارژر (اسکلت). نام تایپ کامل.
 */

#ifndef BSP_PWM_H
#define BSP_PWM_H

/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/**
 * @brief  [EN] Store timer handles. No PWM output until later stage.
 *         [FA] هندل تایمر را نگه می‌دارد. خروجی PWM هنوز نیست.
 * @param  TIM_HandleTypeDef__htimCh1 [EN] Timer for channel 1 / تایمر کانال ۱
 * @param  TIM_HandleTypeDef__htimCh2 [EN] Timer for channel 2 / تایمر کانال ۲
 */
/* ==================== Functions ==================== */
void func__BspPwm_Init(TIM_HandleTypeDef *TIM_HandleTypeDef__htimCh1, TIM_HandleTypeDef *TIM_HandleTypeDef__htimCh2);

/**
 * @brief  [EN] Set duty in permille (0..1000). No-op until implemented.
 *         [FA] وظیفه را به پرمیل می‌گذارد. تا پیاده‌سازی کاری نمی‌کند.
 * @param  uint8_t__channel [EN] 1 or 2 / کانال ۱ یا ۲
 * @param  uint16_t__permille [EN] 0=off, 1000=100% / پرمیل
 */
void func__BspPwm_SetDutyPermille(uint8_t uint8_t__channel, uint16_t uint16_t__permille);

/**
 * @brief  [EN] Force both PWM channels off.
 *         [FA] هر دو کانال PWM را خاموش می‌کند.
 */
void func__BspPwm_StopAll(void);

#endif /* BSP_PWM_H */

