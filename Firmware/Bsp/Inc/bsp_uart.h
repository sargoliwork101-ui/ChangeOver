/**
 * @file    bsp_uart.h
 * @brief   [EN] USART1 wrapper for ESP link (placeholder).
 *          [FA] پوشش USART1 برای ارتباط ESP (اسکلت).
 */

#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/*
 * UART HAL is not enabled in CubeMX this stage, so stm32f1xx_hal_uart.h (and
 * the full UART_HandleTypeDef) is not generated. Declare the same struct tag
 * as an incomplete type so the placeholder API still compiles. When CubeMX
 * enables USART, hal_uart.h supplies the complete definition and this block is
 * skipped.
 *
 * در این مرحله HAL مربوط به UART در CubeMX فعال نیست، پس فایل hal_uart.h و تایپ
 * کامل UART_HandleTypeDef تولید نمی‌شود. همان برچسب struct را به‌صورت ناقص اعلام
 * می‌کنیم تا اسکلت کامپایل شود؛ با فعال‌شدن USART در مکعب، تعریف کامل می‌آید و
 * این بلوک نادیده گرفته می‌شود.
 */
#ifndef HAL_UART_MODULE_ENABLED
typedef struct __UART_HandleTypeDef UART_HandleTypeDef;
#endif

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
