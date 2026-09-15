/**
 * @file    bsp_pwm.h
 * @brief   [EN] PWM wrapper for charger channels (placeholder). Full type naming, func_ prefix.
 *          [FA] پوشش PWM کانال شارژر (اسکلت). نام تایپ کامل.
 */

#ifndef BSP_PWM_H
#define BSP_PWM_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/**
 * @brief  [EN] Store timer handles. No PWM output until later stage.
 *         [FA] هندل تایمر را نگه می‌دارد. خروجی PWM هنوز نیست.
 * @param  TIM_HandleTypeDef_htimCh1 [EN] Timer for channel 1 / تایمر کانال ۱
 * @param  TIM_HandleTypeDef_htimCh2 [EN] Timer for channel 2 / تایمر کانال ۲
 */
void func_BspPwm_Init(TIM_HandleTypeDef *TIM_HandleTypeDef_htimCh1, TIM_HandleTypeDef *TIM_HandleTypeDef_htimCh2);

/**
 * @brief  [EN] Set duty in permille (0..1000). No-op until implemented.
 *         [FA] وظیفه را به پرمیل می‌گذارد. تا پیاده‌سازی کاری نمی‌کند.
 * @param  uint8_t_channel [EN] 1 or 2 / کانال ۱ یا ۲
 * @param  uint16_t_permille [EN] 0=off, 1000=100% / پرمیل
 */
void func_BspPwm_SetDutyPermille(uint8_t uint8_t_channel, uint16_t uint16_t_permille);

/**
 * @brief  [EN] Force both PWM channels off.
 *         [FA] هر دو کانال PWM را خاموش می‌کند.
 */
void func_BspPwm_StopAll(void);

#endif /* BSP_PWM_H */

