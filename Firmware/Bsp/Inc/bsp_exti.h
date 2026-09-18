/**
 * @file    bsp_exti.h
 * @brief   [EN] Logical external-event flags for schematic jitter and input.
 *          [FA] پرچم رویدادهای خارجی منطقی برای جیتر و ورودی شماتیک.
 *
 * @note    [EN] The board port configures the real EXTI lines. Both edges are
 *              captured so comparator polarity and connected/disconnected
 *              transitions are not lost; product code consumes logical events.
 *          [FA] پورت برد خطوط واقعی EXTI را تنظیم می‌کند. هر دو لبه ثبت می‌شود
 *              تا قطبیت comparator و تغییرات وصل/قطع از دست نرود؛ کد محصول
 *              فقط رویداد منطقی را مصرف می‌کند.
 */

#ifndef BSP_EXTI_H
#define BSP_EXTI_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BSP_EXTI_JITTER1 = 0,
    BSP_EXTI_JITTER2,
    BSP_EXTI_INPUT_DETECT,
    BSP_EXTI_SOURCE_COUNT
} bsp_exti_src_t;

/* ==================== BspExti_Init ==================== */
/**
 * @brief  [EN] Clear all logical EXTI event flags.
 *         [FA] همهٔ پرچم‌های رویداد منطقی EXTI را پاک می‌کند.
 */
void func__BspExti_Init(void);

/* ==================== BspExti_OnIrq ==================== */
/**
 * @brief  [EN] Latch one logical event from the board interrupt adapter.
 *         [FA] یک رویداد منطقی را از adapter وقفهٔ برد قفل می‌کند.
 * @param  bsp_exti_src_t__src [EN] Logical source / منبع منطقی
 */
void func__BspExti_OnIrq(bsp_exti_src_t bsp_exti_src_t__src);

/* ==================== BspExti_TakeEvent ==================== */
/**
 * @brief  [EN] Read and clear one logical EXTI event flag.
 *         [FA] پرچم یک رویداد منطقی EXTI را می‌خواند و پاک می‌کند.
 * @param  bsp_exti_src_t__src [EN] Logical source / منبع منطقی
 * @return bool [EN] true when an event was pending / اگر رویداد pending باشد true
 */
bool func__BspExti_TakeEvent(bsp_exti_src_t bsp_exti_src_t__src);

#endif /* BSP_EXTI_H */
