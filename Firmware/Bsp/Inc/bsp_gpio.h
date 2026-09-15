/**
 * @file    bsp_gpio.h
 * @brief   [EN] GPIO wrapper around STM32 HAL. Full type naming, func__ prefix.
 *          [FA] پوشش GPIO روی HAL استم. نام تایپ کامل و پیشوند func__.
 *
 * @note    [EN] Product modules must not call HAL_GPIO_* directly.
 *          [FA] ماژول محصول نباید مستقیم HAL_GPIO صدا بزند.
 */

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

/* ==================== Includes ==================== */
#include <stdbool.h>
#include <stdint.h>
#include "board_pins.h"

/**
 * @brief  [EN] Write a pin high or low.
 *         [FA] پایه را High یا Low می‌کند.
 * @param  GPIO_TypeDef__port [EN] GPIOA/GPIOB/... ; ignored if NULL / پورت
 * @param  uint16_t__pin [EN] Pin mask e.g. GPIO_PIN_0 / ماسک پایه
 * @param  bool__high [EN] true=3.3V, false=0V / High یعنی ۳٫۳ ولت
 */
/* ==================== Functions ==================== */
void func__BspGpio_Write(GPIO_TypeDef *GPIO_TypeDef__port, uint16_t uint16_t__pin, bool bool__high);

/**
 * @brief  [EN] Read pin logic level.
 *         [FA] سطح منطقی پایه را می‌خواند.
 * @param  GPIO_TypeDef__port [EN] Port / پورت
 * @param  uint16_t__pin [EN] Pin mask / ماسک پایه
 * @return bool [EN] true if high; false if low or port NULL / اگر High باشد true
 */
bool func__BspGpio_Read(GPIO_TypeDef *GPIO_TypeDef__port, uint16_t uint16_t__pin);

#endif /* BSP_GPIO_H */

