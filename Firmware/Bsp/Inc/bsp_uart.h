/**
 * @file    bsp_uart.h
 * @brief   [EN] USART1 wrapper for ESP link (placeholder). Full type naming, func_ prefix.
 *          [FA] پوشش USART1 برای ارتباط ESP (اسکلت). نام تایپ کامل.
 */

#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

#ifndef HAL_UART_MODULE_ENABLED
typedef struct __UART_HandleTypeDef UART_HandleTypeDef;
#endif

/**
 * @brief  [EN] Store UART handle.
 *         [FA] هندل UART را نگه می‌دارد.
 * @param  UART_HandleTypeDef_huart [EN] HAL UART handle / هندل UART
 */
void func_BspUart_Init(UART_HandleTypeDef *UART_HandleTypeDef_huart);

/**
 * @brief  [EN] Transmit bytes. Returns false until implemented.
 *         [FA] ارسال بایت. تا پیاده‌سازی false برمی‌گرداند.
 * @param  uint8_t_data [EN] Data pointer / اشاره‌گر داده
 * @param  uint16_t_length [EN] Length / طول
 * @return bool [EN] false until implemented / تا پیاده‌سازی false
 */
bool func_BspUart_Write(const uint8_t *uint8_t_data, uint16_t uint16_t_length);

/**
 * @brief  [EN] Read one byte if available.
 *         [FA] اگر بایتی باشد می‌خواند.
 * @param  uint8_t_byte [EN] Output byte pointer / اشاره‌گر بایت خروجی
 * @return bool [EN] true if byte read / اگر بایتی خوانده شد true
 */
bool func_BspUart_ReadByte(uint8_t *uint8_t_byte);

#endif /* BSP_UART_H */

