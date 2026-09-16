/**
 * @file    bsp_exti.c
 * @brief   [EN] External interrupt flags (placeholder). Full type naming, func__ prefix.
 *          [FA] پرچم وقفه خارجی (اسکلت). نام تایپ کامل.
 */

#include "bsp_exti.h"

static volatile uint8_t UINT8_T__G__Flags[BSP_EXTI_SOURCE_COUNT];

/**
 * @brief  [EN] Clear software event flags.
 *         [FA] پرچم‌های نرم‌افزاری را صفر می‌کند.
 */
/* ==================== BspExti_Init ==================== */

void func__BspExti_Init(void)
{
    uint32_t uint32_t__index;

    for (uint32_t__index = 0u;
         uint32_t__index < (uint32_t)BSP_EXTI_SOURCE_COUNT;
         uint32_t__index++)
    {
        UINT8_T__G__Flags[uint32_t__index] = 0u;
    }
}

/**
 * @brief  [EN] Set flag from HAL GPIO EXTI callback.
 *         [FA] پرچم را از کال‌بک EXTI می‌گذارد.
 * @param  bsp_exti_src_t__src [EN] Source / منبع
 */
/* ==================== BspExti_OnIrq ==================== */

void func__BspExti_OnIrq(bsp_exti_src_t bsp_exti_src_t__src)
{
    if ((uint32_t)bsp_exti_src_t__src < (uint32_t)BSP_EXTI_SOURCE_COUNT)
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
/* ==================== BspExti_TakeEvent ==================== */

bool func__BspExti_TakeEvent(bsp_exti_src_t bsp_exti_src_t__src)
{
    bool bool__taken = false;

    if ((uint32_t)bsp_exti_src_t__src < (uint32_t)BSP_EXTI_SOURCE_COUNT)
    {
        bool__taken = (UINT8_T__G__Flags[bsp_exti_src_t__src] != 0u);
        UINT8_T__G__Flags[bsp_exti_src_t__src] = 0u;
    }
    return bool__taken;
}
