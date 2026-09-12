/**
 * @file    bsp_adc.c
 * @brief   [EN] ADC+DMA wrapper (placeholder).
 *          [FA] پوشش ADC+DMA (اسکلت).
 */

#include "bsp_adc.h"

#include <stddef.h>

static ADC_HandleTypeDef *s_hadc = NULL;
static volatile uint16_t s_raw[BSP_ADC_CHANNEL_COUNT];
static volatile bool s_ready = false;

/**
 * @brief  [EN] Store ADC handle. DMA start is a later stage.
 *         [FA] هندل ADC را نگه می‌دارد. شروع DMA مرحله بعد است.
 */
void BspAdc_Init(ADC_HandleTypeDef *hadc)
{
    s_hadc = hadc;
    s_ready = false;
}

/**
 * @brief  [EN] Start DMA conversions. Returns false until implemented.
 *         [FA] شروع تبدیل DMA. تا پیاده‌سازی false برمی‌گرداند.
 */
bool BspAdc_Start(void)
{
    if (s_hadc == NULL)
    {
        return false;
    }

    return false;
}

/**
 * @brief  [EN] Copy last raw frame. Returns false if not ready.
 *         [FA] آخرین فریم خام را کپی می‌کند. اگر آماده نباشد false.
 */
bool BspAdc_GetRaw(uint16_t out[BSP_ADC_CHANNEL_COUNT])
{
    uint32_t i;

    if (s_ready == false)
    {
        return false;
    }

    for (i = 0u; i < BSP_ADC_CHANNEL_COUNT; i++)
    {
        out[i] = s_raw[i];
    }
    return true;
}

/**
 * @brief  [EN] True when a DMA frame is available.
 *         [FA] وقتی یک فریم DMA آماده باشد true است.
 */
bool BspAdc_IsFrameReady(void)
{
    return s_ready;
}

/**
 * @brief  [EN] Call from HAL_ADC_ConvCpltCallback.
 *         [FA] از داخل HAL_ADC_ConvCpltCallback صدا زده شود.
 */
void BspAdc_OnDmaComplete(void)
{
    (void)s_raw;
}
