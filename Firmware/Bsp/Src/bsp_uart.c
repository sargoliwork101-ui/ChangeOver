/**
 * @file    bsp_uart.c
 * @brief   [EN] USART1 wrapper (placeholder). Full type naming, func__ prefix.
 *          [FA] پوشش USART1 (اسکلت). نام تایپ کامل.
 */

#include "bsp_uart.h"
#include <stddef.h>

static UART_HandleTypeDef *UART_HANDLETYPEDEF__G__Huart = NULL;

/**
 * @brief  [EN] Store UART handle.
 *         [FA] هندل UART را نگه می‌دارد.
 * @param  UART_HandleTypeDef__huart [EN] HAL handle / هندل
 */
void func__BspUart_Init(UART_HandleTypeDef *UART_HandleTypeDef__huart)
{
    UART_HANDLETYPEDEF__G__Huart = UART_HandleTypeDef__huart;
}

/**
 * @brief  [EN] Transmit bytes. Returns false until implemented.
 *         [FA] ارسال بایت. تا پیاده‌سازی false.
 * @param  uint8_t__data [EN] Data pointer / اشاره‌گر داده
 * @param  uint16_t__length [EN] Length / طول
 * @return bool [EN] false until implemented / تا پیاده‌سازی false
 */
bool func__BspUart_Write(const uint8_t *uint8_t__data, uint16_t uint16_t__length)
{
    (void)uint8_t__data;
    (void)uint16_t__length;
    (void)UART_HANDLETYPEDEF__G__Huart;
    return false;
}

/**
 * @brief  [EN] Read one byte if available.
 *         [FA] اگر بایتی باشد می‌خواند.
 * @param  uint8_t__byte [EN] Output byte pointer / اشاره‌گر خروجی
 * @return bool [EN] false until implemented / تا پیاده‌سازی false
 */
bool func__BspUart_ReadByte(uint8_t *uint8_t__byte)
{
    (void)uint8_t__byte;
    return false;
}

