/**
 * @file    bsp_uart.c
 * @brief   [EN] USART1 wrapper (placeholder).
 *          [FA] پوشش USART1 (اسکلت).
 */

#include "bsp_uart.h"

#include <stddef.h>

static UART_HandleTypeDef *s_huart = NULL;

/**
 * @brief  [EN] Store UART handle.
 *         [FA] هندل UART را نگه می‌دارد.
 */
void BspUart_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
}

/**
 * @brief  [EN] Transmit bytes. Returns false until implemented.
 *         [FA] ارسال بایت. تا پیاده‌سازی false برمی‌گرداند.
 */
bool BspUart_Write(const uint8_t *data, uint16_t length)
{
    (void)data;
    (void)length;
    (void)s_huart;
    return false;
}

/**
 * @brief  [EN] Read one byte if available.
 *         [FA] اگر بایتی باشد می‌خواند.
 */
bool BspUart_ReadByte(uint8_t *byte)
{
    (void)byte;
    return false;
}
