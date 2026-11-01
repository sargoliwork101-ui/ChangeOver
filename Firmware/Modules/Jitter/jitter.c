/**
 * @file    jitter.c
 * @brief   [EN] Implementation of the LM393 trip-flag latch: it polls the
 *               EXTI software flags of the board layer, latches one sticky
 *               boolean per channel and offers a per-channel clear. No HAL and no pin numbers appear here - which EXTI
 *               line belongs to which comparator is the board port's job.
 * @note    [EN] Full-program audit 2026-10-05: the stale "placeholder"
 *               label of the old header line was removed.
 */
#include "jitter.h"
#include "bsp_exti.h"
static bool BOOL__G__Trip[2];
/**
 * @brief  [EN] Clear trip flags and EXTI software flags.
 */
/* ==================== Jitter_Init ==================== */
void func__Jitter_Init(void)
{
    BOOL__G__Trip[0] = false;
    BOOL__G__Trip[1] = false;
    func__BspExti_Init();
}
/**
 * @brief  [EN] Latch trip if an EXTI event was taken.
 */
/* ==================== Jitter_Run ==================== */
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
* @param uint8_t__channel Channel number 1 or 2, other values return false /
* @return bool true if tripped / true
 */
/* ==================== Jitter_ChannelTripped ==================== */
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
/**
 * @brief  [EN] Clear one latched channel so the next retry can observe a new
 *              comparator edge rather than the old event.
* @param uint8_t__channel Channel number 1 or 2 /
 */
/* ==================== Jitter_ClearChannel ==================== */
void func__Jitter_ClearChannel(uint8_t uint8_t__channel)
{
    if (uint8_t__channel == 1u)
    {
        BOOL__G__Trip[0] = false;
    }
    else if (uint8_t__channel == 2u)
    {
        BOOL__G__Trip[1] = false;
    }
    else
    {
        /* [EN] Invalid channel is intentionally ignored. */
    }
}
