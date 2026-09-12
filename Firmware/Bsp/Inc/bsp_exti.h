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

void BspExti_Init(void);
void BspExti_OnIrq(bsp_exti_src_t src); /* call from HAL GPIO EXTI callback */
bool BspExti_TakeEvent(bsp_exti_src_t src);

#endif /* BSP_EXTI_H */
