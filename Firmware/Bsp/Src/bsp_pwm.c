/**
 * @file    bsp_pwm.c
 * @brief   [EN] PWM wrapper for charger channels (placeholder).
 *          [FA] پوشش PWM کانال شارژر (اسکلت).
 *
 * @stage   Placeholder
 */

#include "bsp_pwm.h"

static TIM_HandleTypeDef *s_tim1 = 0;
static TIM_HandleTypeDef *s_tim2 = 0;

void BspPwm_Init(TIM_HandleTypeDef *htim_ch1, TIM_HandleTypeDef *htim_ch2)
{
    s_tim1 = htim_ch1;
    s_tim2 = htim_ch2;
    BspPwm_StopAll();
}

void BspPwm_SetDutyPermille(uint8_t channel, uint16_t permille)
{
    (void)channel;
    (void)permille;
    (void)s_tim1;
    (void)s_tim2;
}

void BspPwm_StopAll(void)
{
}
