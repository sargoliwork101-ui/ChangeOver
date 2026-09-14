/**
 * @file    bsp_pwm.c
 * @brief   [EN] PWM wrapper for charger channels (placeholder).
 *          [FA] پوشش PWM کانال شارژر (اسکلت).
 */

#include "bsp_pwm.h"

#include <stddef.h>

static TIM_HandleTypeDef *s_tim1 = NULL;
static TIM_HandleTypeDef *s_tim2 = NULL;

/**
 * @brief  [EN] Store timer handles. No PWM output until a later stage.
 *         [FA] هندل تایمر را نگه می‌دارد. خروجی PWM هنوز نیست.
 */
void BspPwm_Init(TIM_HandleTypeDef *htim_ch1, TIM_HandleTypeDef *htim_ch2)
{
    s_tim1 = htim_ch1;
    s_tim2 = htim_ch2;
    BspPwm_StopAll();
}

/**
 * @brief  [EN] Set duty in permille (0..1000). No-op until implemented.
 *         [FA] وظیفه را به پرمیل می‌گذارد. تا پیاده‌سازی کاری نمی‌کند.
 */
void BspPwm_SetDutyPermille(uint8_t channel, uint16_t permille)
{
    (void)channel;
    (void)permille;
    (void)s_tim1;
    (void)s_tim2;
}

/**
 * @brief  [EN] Force both PWM channels off.
 *         [FA] هر دو کانال PWM را خاموش می‌کند.
 */
void BspPwm_StopAll(void)
{
}
