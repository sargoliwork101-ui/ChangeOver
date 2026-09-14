/**
 * @file    bsp_gpio.h
 * @brief   [EN] GPIO wrapper around STM32 HAL.
 *          [FA] پوشش GPIO روی HAL استم.
 *
 * @note    [EN] Product modules must not call HAL_GPIO_* directly.
 *          [FA] ماژول محصول نباید مستقیم HAL_GPIO صدا بزند.
 */

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdbool.h>
#include <stdint.h>
#include "board_pins.h"

/**
 * @brief  [EN] Write a pin high or low.
 *         [FA] پایه را High یا Low می‌کند.
 * @param  port  [EN] GPIOA/GPIOB/... ; ignored if NULL
 *               [FA] پورت؛ اگر NULL باشد کاری نمی‌کند
 * @param  pin   [EN] Pin mask e.g. GPIO_PIN_0
 *               [FA] ماسک پایه
 * @param  high  [EN] true = 3.3 V, false = 0 V
 *               [FA] true یعنی ۳٫۳ ولت
 */
void BspGpio_Write(GPIO_TypeDef *port, uint16_t pin, bool high);

/**
 * @brief  [EN] Read pin logic level.
 *         [FA] سطح منطقی پایه را می‌خواند.
 * @return [EN] true if high; false if low or port is NULL
 *         [FA] اگر High باشد true
 */
bool BspGpio_Read(GPIO_TypeDef *port, uint16_t pin);

#endif /* BSP_GPIO_H */
