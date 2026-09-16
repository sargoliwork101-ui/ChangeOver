/**
 * @file    bsp_uart.c
 * @brief   [EN] STM32F103C8T6 USART1 port for the ESP-Link connector.
 *          [FA] پورت USART1 روی STM32F103C8T6 برای کانکتور ESP-Link.
 *
 * @note    [EN] CubeMX initializes USART1 at 115200 8-N-1 on PA9/PA10.
 *              Product modules use only the logical byte-stream API.
 *          [FA] CubeMX، USART1 را با 115200 و قالب 8-N-1 روی PA9/PA10
 *              مقداردهی می‌کند و ماژول‌ها فقط API منطقی بایت را می‌بینند.
 */

#include "bsp_uart.h"
#include "main.h"

#include <stddef.h>

#define BSP_UART_TX_TIMEOUT_MS 100u

static UART_HandleTypeDef *UART_HANDLETYPEDEF__G__EspLink = NULL;
static bool BOOL__G__Initialized = false;

/* ==================== BspUart_Init ==================== */
/**
 * @brief  [EN] Select the initialized USART1 board handle.
 *         [FA] هندل USART1 مقداردهی‌شدهٔ برد را انتخاب می‌کند.
 */
void func__BspUart_Init(void)
{
    UART_HANDLETYPEDEF__G__EspLink = &huart1;
    BOOL__G__Initialized = true;
}

/* ==================== BspUart_Write ==================== */
/**
 * @brief  [EN] Transmit a complete byte buffer through USART1.
 *         [FA] یک بافر کامل بایت را از USART1 ارسال می‌کند.
 * @param  uint8_t__data [EN] Data buffer / بافر داده
 * @param  uint16_t__length [EN] Number of bytes / تعداد بایت‌ها
 * @return bool [EN] true when transmission succeeds / اگر ارسال موفق باشد true
 */
bool func__BspUart_Write(const uint8_t *uint8_t__data, uint16_t uint16_t__length)
{
    if ((BOOL__G__Initialized == false) ||
        (UART_HANDLETYPEDEF__G__EspLink == NULL) ||
        ((uint8_t__data == NULL) && (uint16_t__length != 0u)))
    {
        return false;
    }

    if (uint16_t__length == 0u)
    {
        return true;
    }

    return (HAL_UART_Transmit(UART_HANDLETYPEDEF__G__EspLink,
                              (uint8_t *)uint8_t__data,
                              uint16_t__length,
                              BSP_UART_TX_TIMEOUT_MS) == HAL_OK);
}

/* ==================== BspUart_ReadByte ==================== */
/**
 * @brief  [EN] Poll one USART1 byte with a zero timeout.
 *         [FA] یک بایت USART1 را با timeout صفر poll می‌کند.
 * @param  uint8_t__byte [EN] Output byte pointer / اشاره‌گر بایت خروجی
 * @return bool [EN] true if one byte was read / اگر یک بایت خوانده شد true
 */
bool func__BspUart_ReadByte(uint8_t *uint8_t__byte)
{
    if ((BOOL__G__Initialized == false) ||
        (UART_HANDLETYPEDEF__G__EspLink == NULL) ||
        (uint8_t__byte == NULL))
    {
        return false;
    }

    return (HAL_UART_Receive(UART_HANDLETYPEDEF__G__EspLink,
                             uint8_t__byte,
                             1u,
                             0u) == HAL_OK);
}
