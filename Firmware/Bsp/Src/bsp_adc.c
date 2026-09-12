#include "bsp_adc.h"

static ADC_HandleTypeDef *s_hadc = 0;
static volatile uint16_t s_raw[BSP_ADC_CHANNEL_COUNT];
static volatile bool s_ready = false;

void BspAdc_Init(ADC_HandleTypeDef *hadc)
{
    s_hadc = hadc;
    s_ready = false;
}

bool BspAdc_Start(void)
{
    if (s_hadc == 0)
    {
        return false;
    }

    /* Step 3: HAL_ADC_Start_DMA(...) */
    return false;
}

bool BspAdc_GetRaw(uint16_t out[BSP_ADC_CHANNEL_COUNT])
{
    uint32_t i;

    if (!s_ready)
    {
        return false;
    }

    for (i = 0u; i < BSP_ADC_CHANNEL_COUNT; i++)
    {
        out[i] = s_raw[i];
    }
    return true;
}

bool BspAdc_IsFrameReady(void)
{
    return s_ready;
}

void BspAdc_OnDmaComplete(void)
{
    /* Step 3: copy DMA buffer into s_raw, set s_ready */
    (void)s_raw;
}
