/**
 * @file    jitter.c
 * @brief   [EN] LM393 jitter trip flags (placeholder).
 *          [FA] پرچم تریپ جیتر LM393 (اسکلت).
 *
 * @stage   Placeholder
 */

#include "jitter.h"
#include "bsp_exti.h"

static bool s_trip[2];

void Jitter_Init(void)
{
    s_trip[0] = false;
    s_trip[1] = false;
    BspExti_Init();
}

void Jitter_Run(void)
{
    if (BspExti_TakeEvent(BSP_EXTI_JITTER1))
    {
        s_trip[0] = true;
    }
    if (BspExti_TakeEvent(BSP_EXTI_JITTER2))
    {
        s_trip[1] = true;
    }
}

bool Jitter_ChannelTripped(uint8_t channel)
{
    if (channel == 1u)
    {
        return s_trip[0];
    }
    if (channel == 2u)
    {
        return s_trip[1];
    }
    return false;
}
