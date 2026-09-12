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
    /* Step 7: write CCR, never exceed APP_CONFIG.pwm_max_duty_permille */
}

void BspPwm_StopAll(void)
{
    /* Step 7: CCR = 0 and stop output compare if needed */
}
