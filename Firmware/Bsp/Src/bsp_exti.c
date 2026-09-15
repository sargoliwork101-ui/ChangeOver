/**
 * @file    bsp_exti.c
 * @brief   [EN] External interrupt flags (placeholder). Full type naming, func_ prefix.
 *          [FA] پرچم وقفه خارجی (اسکلت). نام تایپ کامل.
 */

#include "bsp_exti.h"

static volatile uint8_t UINT8_T_G_Flags[3];

/**
 * @brief  [EN] Clear software event flags.
 *         [FA] پرچم‌های نرم‌افزاری را صفر می‌کند.
 */
void func_BspExti_Init(void)
{
    UINT8_T_G_Flags[0] = 0u;
    UINT8_T_G_Flags[1] = 0u;
    UINT8_T_G_Flags[2] = 0u;
}

/**
 * @brief  [EN] Set flag from HAL GPIO EXTI callback.
 *         [FA] پرچم را از کال‌بک EXTI می‌گذارد.
 * @param  bsp_exti_src_t_src [EN] Source / منبع
 */
void func_BspExti_OnIrq(bsp_exti_src_t bsp_exti_src_t_src)
{
    if ((uint32_t)bsp_exti_src_t_src < 3u)
    {
        UINT8_T_G_Flags[bsp_exti_src_t_src] = 1u;
    }
}

/**
 * @brief  [EN] Read-and-clear one event flag.
 *         [FA] پرچم را می‌خواند و پاک می‌کند.
 * @param  bsp_exti_src_t_src [EN] Source / منبع
 * @return bool [EN] true if taken / اگر گرفته شد true
 */
bool func_BspExti_TakeEvent(bsp_exti_src_t bsp_exti_src_t_src)
{
    bool bool_taken = false;

    if ((uint32_t)bsp_exti_src_t_src < 3u)
    {
        bool_taken = (UINT8_T_G_Flags[bsp_exti_src_t_src] != 0u);
        UINT8_T_G_Flags[bsp_exti_src_t_src] = 0u;
    }
    return bool_taken;
}

