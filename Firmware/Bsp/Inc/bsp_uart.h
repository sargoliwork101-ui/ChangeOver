/**
 * @file    bsp_uart.h
 * @brief   [EN] Logical byte-stream interface for the ESP link.
 *          [FA] رابط منطقی جریان بایت برای ارتباط ESP.
 *
 * @note    [EN] UART instances, pins, DMA and HAL handles are private to the
 *              board implementation. The current board port is a placeholder
 *              because USART is not enabled in the active CubeMX stage.
 *          [FA] نمونهٔ UART، پایه‌ها، DMA و هندل‌های HAL در پیاده‌سازی برد
 *              خصوصی هستند. پورت فعلی اسکلت است چون USART در مرحلهٔ فعال
 *              CubeMX روشن نیست.
 */

#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  [EN] Initialize the board UART backend.
 *         [FA] Backend UART برد را مقداردهی می‌کند.
 */
/* ==================== Functions ==================== */
void func__BspUart_Init(void);

/**
 * @brief  [EN] Transmit bytes through the logical board UART.
 *         [FA] بایت‌ها را از طریق UART منطقی برد ارسال می‌کند.
 * @param  uint8_t__data [EN] Data pointer / اشاره‌گر داده
 * @param  uint16_t__length [EN] Length / طول
 * @return bool [EN] true if accepted, false if unavailable / پذیرش داده
 */
bool func__BspUart_Write(const uint8_t *uint8_t__data, uint16_t uint16_t__length);

/**
 * @brief  [EN] Read one byte from the logical board UART if available.
 *         [FA] اگر بایتی در UART منطقی برد موجود باشد آن را می‌خواند.
 * @param  uint8_t__byte [EN] Output byte pointer / اشاره‌گر بایت خروجی
 * @return bool [EN] true if a byte was read / اگر بایت خوانده شد true
 */
bool func__BspUart_ReadByte(uint8_t *uint8_t__byte);

#endif /* BSP_UART_H */
