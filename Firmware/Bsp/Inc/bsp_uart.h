/**
 * @file    bsp_uart.h
 * @brief   [EN] Logical byte-stream interface for the schematic ESP-Link UART.
 *          [FA] رابط منطقی جریان بایت برای UART ارتباط ESP-Link شماتیک.
 *
 * @note    [EN] USART instance, pins, baud configuration and HAL handle are
 *              private to the board port. The interface remains available
 *              while MODULE_ESP is disabled.
 *          [FA] نمونه USART، پایه‌ها، تنظیم baud و هندل HAL در پورت برد
 *              خصوصی هستند. رابط حتی با خاموش‌بودن MODULE_ESP باقی می‌ماند.
 */

#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== BspUart_Init ==================== */
/**
 * @brief  [EN] Select the initialized board UART backend for ESP-Link.
 *         [FA] backend UART مقداردهی‌شدهٔ برد را برای ESP-Link انتخاب می‌کند.
 */
void func__BspUart_Init(void);

/* ==================== BspUart_Write ==================== */
/**
 * @brief  [EN] Transmit a byte buffer through the logical ESP-Link UART.
 *         [FA] یک بافر بایت را از UART منطقی ESP-Link ارسال می‌کند.
 * @param  uint8_t__data [EN] Data buffer, not null when length is non-zero /
 *                            بافر داده، در طول غیرصفر نباید NULL باشد
 * @param  uint16_t__length [EN] Number of bytes, 0..65535 /
 *                               تعداد بایت‌ها
 * @return bool [EN] true when HAL accepts the complete buffer /
 *                   اگر HAL کل بافر را پذیرفت true
 */
bool func__BspUart_Write(const uint8_t *uint8_t__data, uint16_t uint16_t__length);

/* ==================== BspUart_ReadByte ==================== */
/**
 * @brief  [EN] Poll one received ESP-Link byte without blocking.
 *         [FA] یک بایت دریافتی ESP-Link را بدون بلوکه‌کردن poll می‌کند.
 * @param  uint8_t__byte [EN] Output byte pointer / اشاره‌گر بایت خروجی
 * @return bool [EN] true when one byte was available / اگر بایت موجود بود true
 */
bool func__BspUart_ReadByte(uint8_t *uint8_t__byte);

#endif /* BSP_UART_H */
