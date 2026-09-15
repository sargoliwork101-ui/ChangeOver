/**
 * @file    bsp_adc.h
 * @brief   [EN] ADC+DMA wrapper (placeholder). Full type naming, func_ prefix.
 *          [FA] پوشش ADC+DMA (اسکلت). نام تایپ کامل.
 */

#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

#define BSP_ADC_CHANNEL_COUNT 5u

/**
 * @brief  [EN] Store ADC handle. DMA start is later stage.
 *         [FA] هندل ADC را نگه می‌دارد. شروع DMA مرحله بعد.
 * @param  ADC_HandleTypeDef_hadc [EN] HAL ADC handle / هندل ADC
 */
void func_BspAdc_Init(ADC_HandleTypeDef *ADC_HandleTypeDef_hadc);

/**
 * @brief  [EN] Start DMA conversions. Returns false until implemented.
 *         [FA] شروع تبدیل DMA. تا پیاده‌سازی false برمی‌گرداند.
 * @return bool [EN] false until implemented / تا پیاده‌سازی false
 */
bool func_BspAdc_Start(void);

/**
 * @brief  [EN] Copy last raw frame. Returns false if not ready.
 *         [FA] آخرین فریم خام را کپی می‌کند. اگر آماده نباشد false.
 * @param  uint16_t_out [EN] Output array size BSP_ADC_CHANNEL_COUNT / آرایه خروجی
 * @return bool [EN] true if copied / اگر کپی شد true
 */
bool func_BspAdc_GetRaw(uint16_t uint16_t_out[BSP_ADC_CHANNEL_COUNT]);

/**
 * @brief  [EN] True when DMA frame available.
 *         [FA] وقتی فریم DMA آماده باشد true.
 * @return bool [EN] true if ready / اگر آماده true
 */
bool func_BspAdc_IsFrameReady(void);

/**
 * @brief  [EN] Call from HAL_ADC_ConvCpltCallback.
 *         [FA] از داخل HAL_ADC_ConvCpltCallback صدا زده شود.
 */
void func_BspAdc_OnDmaComplete(void);

#endif /* BSP_ADC_H */

