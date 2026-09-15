/**
 * @file    bsp_exti.h
 * @brief   [EN] External interrupt flags for jitter and input detect (placeholder). Full type naming, func__ prefix.
 *          [FA] پرچم وقفه خارجی برای جیتر و تشخیص ورودی (اسکلت). نام تایپ کامل.
 */

#ifndef BSP_EXTI_H
#define BSP_EXTI_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    BSP_EXTI_JITTER1 = 0,
    BSP_EXTI_JITTER2,
    BSP_EXTI_INPUT_DETECT
} bsp_exti_src_t;

/**
 * @brief  [EN] Clear software event flags.
 *         [FA] پرچم‌های نرم‌افزاری را صفر می‌کند.
 */
void func__BspExti_Init(void);

/**
 * @brief  [EN] Set flag from HAL GPIO EXTI callback.
 *         [FA] پرچم را از کال‌بک EXTI می‌گذارد.
 * @param  bsp_exti_src_t__src [EN] Source / منبع
 */
void func__BspExti_OnIrq(bsp_exti_src_t bsp_exti_src_t__src);

/**
 * @brief  [EN] Read-and-clear one event flag.
 *         [FA] پرچم را می‌خواند و پاک می‌کند.
 * @param  bsp_exti_src_t__src [EN] Source / منبع
 * @return bool [EN] true if event was taken / اگر رویداد گرفته شد true
 */
bool func__BspExti_TakeEvent(bsp_exti_src_t bsp_exti_src_t__src);

#endif /* BSP_EXTI_H */

