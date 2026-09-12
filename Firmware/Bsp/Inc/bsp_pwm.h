/**
 * @file    bsp_pwm.h
 * @brief   [EN] PWM wrapper for charger channels (placeholder).
 *          [FA] پوشش PWM کانال شارژر (اسکلت).
 *
 * @stage   Placeholder
 */

#ifndef BSP_PWM_H
#define BSP_PWM_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/**
 * @brief  [EN] Store timer handles. No PWM output until a later stage.
 *         [FA] هندل تایمر را نگه می‌دارد. خروجی PWM هنوز نیست.
 */
void BspPwm_Init(TIM_HandleTypeDef *htim_ch1, TIM_HandleTypeDef *htim_ch2);

/**
 * @brief  [EN] Set duty in permille (0..1000). No-op until implemented.
 *         [FA] وظیفه را به پرمیل می‌گذارد. تا پیاده‌سازی کاری نمی‌کند.
 * @param  channel   [EN] 1 or 2
 *                   [FA] کانال ۱ یا ۲
 * @param  permille  [EN] 0 = off, 1000 = 100 %
 *                   [FA] ۰ خاموش، ۱۰۰۰ یعنی ۱۰۰ درصد
 */
void BspPwm_SetDutyPermille(uint8_t channel, uint16_t permille);

/**
 * @brief  [EN] Force both PWM channels off.
 *         [FA] هر دو کانال PWM را خاموش می‌کند.
 */
void BspPwm_StopAll(void);

#endif /* BSP_PWM_H */
