#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

#define BSP_ADC_CHANNEL_COUNT  5u

/* Rank order must match CubeMX:
 * 0 PA1 Current1
 * 1 PA2 24V in
 * 2 PA3 24V bat
 * 3 PA5 12V bat
 * 4 PA7 Current2
 */
void BspAdc_Init(ADC_HandleTypeDef *hadc);
bool BspAdc_Start(void);
bool BspAdc_GetRaw(uint16_t out[BSP_ADC_CHANNEL_COUNT]);
bool BspAdc_IsFrameReady(void);
void BspAdc_OnDmaComplete(void); /* call from HAL_ADC_ConvCpltCallback */

#endif /* BSP_ADC_H */
