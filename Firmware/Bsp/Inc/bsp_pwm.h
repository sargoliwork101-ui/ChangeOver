#ifndef BSP_PWM_H
#define BSP_PWM_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

void BspPwm_Init(TIM_HandleTypeDef *htim_ch1, TIM_HandleTypeDef *htim_ch2);
void BspPwm_SetDutyPermille(uint8_t channel /*1 or 2*/, uint16_t permille);
void BspPwm_StopAll(void);

#endif /* BSP_PWM_H */
