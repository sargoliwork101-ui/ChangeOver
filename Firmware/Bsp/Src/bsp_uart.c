/**
 * @file    bsp_uart.c
 * @brief   [EN] USART1 wrapper (placeholder).
 *          [FA] پوشش USART1 (اسکلت).
 *
 * @stage   Placeholder
 */

#include "bsp_uart.h"

static UART_HandleTypeDef *s_huart = 0;

void BspUart_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
}

bool BspUart_Write(const uint8_t *data, uint16_t length)
{
    (void)data;
    (void)length;
    (void)s_huart;
    return false;
}

bool BspUart_ReadByte(uint8_t *byte)
{
    (void)byte;
    return false;
}
