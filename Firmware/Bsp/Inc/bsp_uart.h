#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

void BspUart_Init(UART_HandleTypeDef *huart);
bool BspUart_Write(const uint8_t *data, uint16_t length);
bool BspUart_ReadByte(uint8_t *byte);

#endif /* BSP_UART_H */
