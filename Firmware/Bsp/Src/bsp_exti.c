/**
 * @file    bsp_exti.c
 * @brief   [EN] External interrupt flags (placeholder). Full type naming, func__ prefix.
 *          [FA] پرچم وقفه خارجی (اسکلت). نام تایپ کامل.
 */

#include "bsp_exti.h"

static volatile uint8_t UINT8_T__G__Flags[3];

/**
 * @brief  [EN] Clear software event flags.
 *         [FA] پرچم‌های نرم‌افزاری را صفر می‌کند.
 */
void func__BspExti_Init(void)
{
    UINT8_T__G__Flags[0] = 0u;
    UINT8_T__G__Flags[1] = 0u;
    UINT8_T__G__Flags[2] = 0u;
}

/**
 * @brief  [EN] Set flag from HAL GPIO EXTI callback.
 *         [FA] پرچم را از کال‌بک EXTI می‌گذارد.
 * @param  bsp_exti_src_t__src [EN] Source / منبع
 */
void func__BspExti_OnIrq(bsp_exti_src_t bsp_exti_src_t__src)
{
    if ((uint32_t)bsp_exti_src_t__src < 3u)
    {
        UINT8_T__G__Flags[bsp_exti_src_t__src] = 1u;
    }
}

/**
 * @brief  [EN] Read-and-clear one event flag.
 *         [FA] پرچم را می‌خواند و پاک می‌کند.
 * @param  bsp_exti_src_t__src [EN] Source / منبع
 * @return bool [EN] true if taken / اگر گرفته شد true
 */
bool func__BspExti_TakeEvent(bsp_exti_src_t bsp_exti_src_t__src)
{
    bool bool__taken = false;

    if ((uint32_t)bsp_exti_src_t__src < 3u)
    {
        bool__taken = (UINT8_T__G__Flags[bsp_exti_src_t__src] != 0u);
        UINT8_T__G__Flags[bsp_exti_src_t__src] = 0u;
    }
    return bool__taken;
}

