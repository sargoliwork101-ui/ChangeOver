/**
 * @file    bsp_adc.c
 * @brief   [EN] ADC+DMA wrapper (placeholder). Full type naming, func_ prefix.
 *          [FA] پوشش ADC+DMA (اسکلت). نام تایپ کامل.
 */

#include "bsp_adc.h"
#include <stddef.h>

static ADC_HandleTypeDef *ADC_HANDLETYPEDEF_G_Hadc = NULL;
static volatile uint16_t UINT16_T_G_Raw[BSP_ADC_CHANNEL_COUNT];
static volatile bool BOOL_G_Ready = false;

/**
 * @brief  [EN] Store ADC handle. DMA start later.
 *         [FA] هندل ADC را نگه می‌دارد. شروع DMA مرحله بعد.
 * @param  ADC_HandleTypeDef_hadc [EN] HAL handle / هندل
 */
void func_BspAdc_Init(ADC_HandleTypeDef *ADC_HandleTypeDef_hadc)
{
    ADC_HANDLETYPEDEF_G_Hadc = ADC_HandleTypeDef_hadc;
    BOOL_G_Ready = false;
}

/**
 * @brief  [EN] Start DMA. Returns false until implemented.
 *         [FA] شروع DMA. تا پیاده‌سازی false.
 * @return bool [EN] false until implemented / تا پیاده‌سازی false
 */
bool func_BspAdc_Start(void)
{
    if (ADC_HANDLETYPEDEF_G_Hadc == NULL)
    {
        return false;
    }
    return false;
}

/**
 * @brief  [EN] Copy last raw frame. Returns false if not ready.
 *         [FA] آخرین فریم خام را کپی می‌کند.
 * @param  uint16_t_out [EN] Output array / آرایه خروجی
 * @return bool [EN] true if copied / اگر کپی شد true
 */
bool func_BspAdc_GetRaw(uint16_t uint16_t_out[BSP_ADC_CHANNEL_COUNT])
{
    uint32_t uint32_t_i;

    if (BOOL_G_Ready == false)
    {
        return false;
    }

    for (uint32_t_i = 0u; uint32_t_i < BSP_ADC_CHANNEL_COUNT; uint32_t_i++)
    {
        uint16_t_out[uint32_t_i] = UINT16_T_G_Raw[uint32_t_i];
    }
    return true;
}

/**
 * @brief  [EN] True when DMA frame available.
 *         [FA] وقتی فریم DMA آماده باشد true.
 * @return bool [EN] true if ready / اگر آماده true
 */
bool func_BspAdc_IsFrameReady(void)
{
    return BOOL_G_Ready;
}

/**
 * @brief  [EN] Call from HAL_ADC_ConvCpltCallback.
 *         [FA] از داخل HAL_ADC_ConvCpltCallback صدا زده شود.
 */
void func_BspAdc_OnDmaComplete(void)
{
    (void)UINT16_T_G_Raw;
}

