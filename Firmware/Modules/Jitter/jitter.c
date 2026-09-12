/**
 * @file    jitter.c
 * @brief   [EN] LM393 jitter trip flags (placeholder).
 *          [FA] پرچم تریپ جیتر LM393 (اسکلت).
 */

#include "jitter.h"
#include "bsp_exti.h"

static bool s_trip[2];

/**
 * @brief  [EN] Clear trip flags and EXTI software flags.
 *         [FA] پرچم تریپ و EXTI را پاک می‌کند.
 */
void Jitter_Init(void)
{
    s_trip[0] = false;
    s_trip[1] = false;
    BspExti_Init();
}

/**
 * @brief  [EN] Latch trip if an EXTI event was taken.
 *         [FA] اگر رویداد EXTI آمده باشد تریپ را قفل می‌کند.
 */
void Jitter_Run(void)
{
    if (BspExti_TakeEvent(BSP_EXTI_JITTER1) == true)
    {
        s_trip[0] = true;
    }
    if (BspExti_TakeEvent(BSP_EXTI_JITTER2) == true)
    {
        s_trip[1] = true;
    }
}

/**
 * @brief  [EN] True if channel 1 or 2 has latched a trip.
 *         [FA] اگر کانال ۱ یا ۲ تریپ قفل کرده باشد true.
 */
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
