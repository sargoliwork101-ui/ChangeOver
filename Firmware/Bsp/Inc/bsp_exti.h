/**
 * @file    bsp_exti.h
 * @brief   [EN] Logical external-event flags for schematic jitter and input.
 * @note    [EN] The board port configures the open-collector active-low
 *              LM393 JITTER1/JITTER2 as falling-edge EXTI inputs; PB4
 *              input presence stays rising/falling. Product code consumes
 *              logical events only.
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
 */
void func__BspExti_Init(void);
/* ==================== BspExti_OnIrq ==================== */
/**
 * @brief  [EN] Latch one logical event from the board interrupt adapter.
* @param bsp_exti_src_t__src Logical source /
 */
void func__BspExti_OnIrq(bsp_exti_src_t bsp_exti_src_t__src);
/* ==================== BspExti_TakeEvent ==================== */
/**
 * @brief  [EN] Read and clear one logical EXTI event flag.
* @param bsp_exti_src_t__src Logical source /
* @return bool true when an event was pending / pending true
 */
bool func__BspExti_TakeEvent(bsp_exti_src_t bsp_exti_src_t__src);
#endif /* BSP_EXTI_H */
