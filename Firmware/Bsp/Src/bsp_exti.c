/**
 * @file    bsp_exti.c
 * @brief   [EN] External interrupt flags (placeholder).
 *          [FA] پرچم وقفه خارجی (اسکلت).
 *
 * @stage   Placeholder
 */

#include "bsp_exti.h"

static volatile uint8_t s_flags[3];

void BspExti_Init(void)
{
    s_flags[0] = 0u;
    s_flags[1] = 0u;
    s_flags[2] = 0u;
}

void BspExti_OnIrq(bsp_exti_src_t src)
{
    if ((uint32_t)src < 3u)
    {
        s_flags[src] = 1u;
    }
}

bool BspExti_TakeEvent(bsp_exti_src_t src)
{
    bool taken = false;

    if ((uint32_t)src < 3u)
    {
        taken = (s_flags[src] != 0u);
        s_flags[src] = 0u;
    }
    return taken;
}
