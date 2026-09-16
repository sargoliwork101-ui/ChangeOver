/**
 * @file    bsp_uart.c
 * @brief   [EN] Board UART port placeholder.
 *          [FA] اسکلت پورت UART برد.
 *
 * @note    [EN] A future board implementation may include its HAL UART and
 *              DMA headers here. Product modules will continue to use only
 *              bsp_uart.h.
 *          [FA] در پورت برد آینده می‌توان headerهای HAL UART و DMA را فقط
 *              اینجا آورد. ماژول‌های محصول همچنان فقط bsp_uart.h را می‌بینند.
 */

#include "bsp_uart.h"

/**
 * @brief  [EN] Initialize the board UART backend.
 *         [FA] Backend UART برد را مقداردهی می‌کند.
 */
/* ==================== BspUart_Init ==================== */
void func__BspUart_Init(void)
{
}

/**
 * @brief  [EN] Transmit bytes. This stage is unavailable until USART is enabled.
 *         [FA] بایت‌ها را ارسال می‌کند؛ تا فعال‌شدن USART این مرحله در دسترس نیست.
 * @param  uint8_t__data [EN] Data pointer / اشاره‌گر داده
 * @param  uint16_t__length [EN] Length / طول
 * @return bool [EN] false while unavailable / تا زمان در دسترس نبودن false
 */
/* ==================== BspUart_Write ==================== */
bool func__BspUart_Write(const uint8_t *uint8_t__data, uint16_t uint16_t__length)
{
    (void)uint8_t__data;
    (void)uint16_t__length;
    return false;
}

/**
 * @brief  [EN] Read one byte if available. This stage is unavailable.
 *         [FA] اگر بایتی باشد می‌خواند؛ این مرحله هنوز در دسترس نیست.
 * @param  uint8_t__byte [EN] Output byte pointer / اشاره‌گر خروجی
 * @return bool [EN] false while unavailable / تا زمان در دسترس نبودن false
 */
/* ==================== BspUart_ReadByte ==================== */
bool func__BspUart_ReadByte(uint8_t *uint8_t__byte)
{
    (void)uint8_t__byte;
    return false;
}
