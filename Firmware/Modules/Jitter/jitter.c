/**
 * @file    jitter.c
 * @brief   [EN] LM393 jitter trip flags (placeholder). Full type naming, func__ prefix.
 *          [FA] پرچم تریپ جیتر LM393 (اسکلت). نام تایپ کامل.
 */

#include "jitter.h"
#include "bsp_exti.h"

static bool BOOL__G__Trip[2];

/**
 * @brief  [EN] Clear trip flags and EXTI software flags.
 *         [FA] پرچم تریپ و EXTI را پاک می‌کند.
 */
void func__Jitter_Init(void)
{
    BOOL__G__Trip[0] = false;
    BOOL__G__Trip[1] = false;
    func__BspExti_Init();
}

/**
 * @brief  [EN] Latch trip if an EXTI event was taken.
 *         [FA] اگر رویداد EXTI آمده باشد تریپ را قفل می‌کند.
 */
void func__Jitter_Run(void)
{
    if (func__BspExti_TakeEvent(BSP_EXTI_JITTER1) == true)
    {
        BOOL__G__Trip[0] = true;
    }
    if (func__BspExti_TakeEvent(BSP_EXTI_JITTER2) == true)
    {
        BOOL__G__Trip[1] = true;
    }
}

/**
 * @brief  [EN] True if channel 1 or 2 has latched a trip.
 *         [FA] اگر کانال ۱ یا ۲ تریپ قفل کرده باشد true.
 * @param  uint8_t__channel [EN] Channel number 1 or 2, other values return false / شماره کانال ۱ یا ۲
 * @return bool [EN] true if tripped / اگر تریپ کرده true
 */
bool func__Jitter_ChannelTripped(uint8_t uint8_t__channel)
{
    if (uint8_t__channel == 1u)
    {
        return BOOL__G__Trip[0];
    }
    if (uint8_t__channel == 2u)
    {
        return BOOL__G__Trip[1];
    }
    return false;
}
