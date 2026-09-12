/**
 * @file    bsp_uart.h
 * @brief   [EN] USART1 wrapper for ESP link (placeholder).
 *          [FA] پوشش USART1 برای ارتباط ESP (اسکلت).
 *
 * @stage   Placeholder
 */

#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/**
 * @brief  [EN] Store UART handle.
 *         [FA] هندل UART را نگه می‌دارد.
 */
void BspUart_Init(UART_HandleTypeDef *huart);

/**
 * @brief  [EN] Transmit bytes. Returns false until implemented.
 *         [FA] ارسال بایت. تا پیاده‌سازی false برمی‌گرداند.
 */
bool BspUart_Write(const uint8_t *data, uint16_t length);

/**
 * @brief  [EN] Read one byte if available.
 *         [FA] اگر بایتی باشد می‌خواند.
 */
bool BspUart_ReadByte(uint8_t *byte);

#endif /* BSP_UART_H */
