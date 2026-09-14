/**
 * @file    bsp_adc.h
 * @brief   [EN] ADC+DMA wrapper (placeholder).
 *          [FA] پوشش ADC+DMA (اسکلت، هنوز فعال نیست).
 */

#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/*
 * ADC HAL is not enabled in CubeMX this stage, so stm32f1xx_hal_adc.h (and the
 * full ADC_HandleTypeDef) is not generated. Declare the same struct tag as an
 * incomplete type so the placeholder API still compiles. When CubeMX enables
 * ADC, hal_adc.h supplies the complete definition and this block is skipped.
 *
 * در این مرحله HAL مربوط به ADC در CubeMX فعال نیست، پس فایل hal_adc.h و تایپ
 * کامل ADC_HandleTypeDef تولید نمی‌شود. همان برچسب struct را به‌صورت ناقص اعلام
 * می‌کنیم تا اسکلت کامپایل شود؛ با فعال‌شدن ADC در مکعب، تعریف کامل می‌آید و این
 * بلوک نادیده گرفته می‌شود.
 */
#ifndef HAL_ADC_MODULE_ENABLED
typedef struct __ADC_HandleTypeDef ADC_HandleTypeDef;
#endif

#define BSP_ADC_CHANNEL_COUNT  5u

/* Rank order must match CubeMX:
 * 0 PA1 Current1
 * 1 PA2 24V in
 * 2 PA3 24V bat
 * 3 PA5 12V bat
 * 4 PA7 Current2
 */

/**
 * @brief  [EN] Store ADC handle. DMA start is a later stage.
 *         [FA] هندل ADC را نگه می‌دارد. شروع DMA مرحله بعد است.
 */
void BspAdc_Init(ADC_HandleTypeDef *hadc);

/**
 * @brief  [EN] Start DMA conversions. Returns false until implemented.
 *         [FA] شروع تبدیل DMA. تا پیاده‌سازی false برمی‌گرداند.
 */
bool BspAdc_Start(void);

/**
 * @brief  [EN] Copy last raw frame. Returns false if not ready.
 *         [FA] آخرین فریم خام را کپی می‌کند. اگر آماده نباشد false.
 */
bool BspAdc_GetRaw(uint16_t out[BSP_ADC_CHANNEL_COUNT]);

/**
 * @brief  [EN] True when a DMA frame is available.
 *         [FA] وقتی یک فریم DMA آماده باشد true است.
 */
bool BspAdc_IsFrameReady(void);

/**
 * @brief  [EN] Call from HAL_ADC_ConvCpltCallback.
 *         [FA] از داخل HAL_ADC_ConvCpltCallback صدا زده شود.
 */
void BspAdc_OnDmaComplete(void);

#endif /* BSP_ADC_H */
