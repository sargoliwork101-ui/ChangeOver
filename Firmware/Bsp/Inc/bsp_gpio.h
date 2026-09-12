/**
 * @file    bsp_gpio.h
 * @brief   تنها لایه‌ای که حق دارد HAL_GPIO را ببیند.
 *
 * چرا BSP جدا از UI؟
 *   UI می‌گوید «سبز روشن». نمی‌داند STM32 چیست.
 *   اگر فردا میکرو عوض شد، فقط همین فایل عوض می‌شود.
 *
 * معادل آردوینو: digitalWrite / digitalRead.
 */

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdbool.h>
#include <stdint.h>
#include "board_pins.h"

/**
 * @brief یک پایه خروجی را High یا Low می‌کند.
 * @param port  پورت GPIOA/GPIOB/... ؛ اگر NULL باشد هیچ کاری نمی‌کند.
 * @param pin   ماسک پایه مثل GPIO_PIN_0
 * @param high  true = High (3.3 V) ، false = Low (0 V)
 */
void BspGpio_Write(GPIO_TypeDef *port, uint16_t pin, bool high);

/**
 * @brief خواندن سطح منطقی یک پایه.
 * @return true اگر پایه High باشد. اگر port خالی باشد false.
 */
bool BspGpio_Read(GPIO_TypeDef *port, uint16_t pin);

#endif /* BSP_GPIO_H */
