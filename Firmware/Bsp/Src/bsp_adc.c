/**
 * @file    bsp_adc.c
 * @brief   [EN] ADC+DMA wrapper (placeholder). Full type naming, func__ prefix.
 *          [FA] پوشش ADC+DMA (اسکلت). نام تایپ کامل.
 */

#include "bsp_adc.h"
#include <stddef.h>

static ADC_HandleTypeDef *ADC_HANDLETYPEDEF__G__Hadc = NULL;
static volatile uint16_t UINT16_T__G__Raw[BSP_ADC_CHANNEL_COUNT];
static volatile bool BOOL__G__Ready = false;

/**
 * @brief  [EN] Store ADC handle. DMA start later.
 *         [FA] هندل ADC را نگه می‌دارد. شروع DMA مرحله بعد.
 * @param  ADC_HandleTypeDef__hadc [EN] HAL handle / هندل
 */
/* ==================== BspAdc_Init ==================== */

void func__BspAdc_Init(ADC_HandleTypeDef *ADC_HandleTypeDef__hadc)
{
    ADC_HANDLETYPEDEF__G__Hadc = ADC_HandleTypeDef__hadc;
    BOOL__G__Ready = false;
}

/**
 * @brief  [EN] Start DMA. Returns false until implemented.
 *         [FA] شروع DMA. تا پیاده‌سازی false.
 * @return bool [EN] false until implemented / تا پیاده‌سازی false
 */
/* ==================== BspAdc_Start ==================== */

bool func__BspAdc_Start(void)
{
    if (ADC_HANDLETYPEDEF__G__Hadc == NULL)
    {
        return false;
    }
    return false;
}

/**
 * @brief  [EN] Copy last raw frame. Returns false if not ready.
 *         [FA] آخرین فریم خام را کپی می‌کند.
 * @param  uint16_t__out [EN] Output array / آرایه خروجی
 * @return bool [EN] true if copied / اگر کپی شد true
 */
/* ==================== BspAdc_GetRaw ==================== */

bool func__BspAdc_GetRaw(uint16_t uint16_t__out[BSP_ADC_CHANNEL_COUNT])
{
    uint32_t uint32_t__i;

    if (BOOL__G__Ready == false)
    {
        return false;
    }

    for (uint32_t__i = 0u; uint32_t__i < BSP_ADC_CHANNEL_COUNT; uint32_t__i++)
    {
        uint16_t__out[uint32_t__i] = UINT16_T__G__Raw[uint32_t__i];
    }
    return true;
}

/**
 * @brief  [EN] True when DMA frame available.
 *         [FA] وقتی فریم DMA آماده باشد true.
 * @return bool [EN] true if ready / اگر آماده true
 */
/* ==================== BspAdc_IsFrameReady ==================== */

bool func__BspAdc_IsFrameReady(void)
{
    return BOOL__G__Ready;
}

/**
 * @brief  [EN] Call from HAL_ADC_ConvCpltCallback.
 *         [FA] از داخل HAL_ADC_ConvCpltCallback صدا زده شود.
 */
/* ==================== BspAdc_OnDmaComplete ==================== */

void func__BspAdc_OnDmaComplete(void)
{
    (void)UINT16_T__G__Raw;
}
