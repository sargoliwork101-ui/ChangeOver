/**
 * @file    bsp_uart.h
 * @brief   [EN] Logical byte-stream interface for the schematic ESP-Link UART.
 *          [FA] رابط منطقی جریان بایت برای UART ارتباط ESP-Link شماتیک.
 *
 * @note    [EN] USART instance, pins and HAL handle are private to the board
 *              port; the port re-tunes the Cube default to 921600 baud and
 *              drives both directions with DMA (RX: circular 256-byte ring,
 *              zero CPU per byte; TX: 256-byte software ring drained by DMA,
 *              one completion interrupt per frame). The interface remains
 *              available while MODULE_ESP is disabled.
 *          [FA] نمونه USART، پایه‌ها و هندل HAL در پورت برد خصوصی هستند؛
 *              پورت baud پیش‌فرض Cube را به 921600 بازتنظیم می‌کند و هر دو
 *              جهت را با DMA می‌راند (RX: بافر حلقوی ۲۵۶ بایتی، صفر CPU به
 *              ازای هر بایت؛ TX: حلقه نرم‌افزاری ۲۵۶ بایتی که DMA تخلیه‌اش
 *              می‌کند، یک وقفهٔ کامل‌شدن به ازای هر فریم). رابط حتی با
 *              خاموش‌بودن MODULE_ESP باقی می‌ماند.
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
 * @brief  [EN] Queue a byte buffer on the logical ESP-Link UART (non-blocking).
 *         [FA] یک بافر بایت را در UART منطقی ESP-Link صف می‌کند (غیربلوکه).
 * @param  uint8_t__data [EN] Data buffer, not null when length is non-zero /
 *                            بافر داده، در طول غیرصفر نباید NULL باشد
 * @param  uint16_t__length [EN] Number of bytes, 0..65535 /
 *                               تعداد بایت‌ها
 * @return bool [EN] true when the buffer fully fits into the TX DMA ring and
 *                   is queued; false when the ring is short on space and the
 *                   WHOLE frame is dropped - never a partial frame on the
 *                   wire (never blocks; the next write drains the ring)
 *                   / اگر کل بافر در حلقه TX جا شد و صف شد true؛ اگر حلقه
 *                   جا نداشت کل فریم رها می‌شود - هرگز فریم ناقص روی سیم
 *                   نمی‌رود (هرگز بلوکه نمی‌کند؛ نوشتن بعدی حلقه را تخلیه
 *                   می‌کند)
 */
bool func__BspUart_Write(const uint8_t *uint8_t__data, uint16_t uint16_t__length);

/* ==================== BspUart_ReadByte ==================== */
/**
 * @brief  [EN] Poll one received ESP-Link byte without blocking; the byte
 *              comes from the DMA-filled circular RX ring (hardware wrote
 *              it, the CPU only copies it out).
 *         [FA] یک بایت دریافتی ESP-Link را بدون بلوکه‌کردن poll می‌کند؛
 *              بایت از بافر حلقوی RX که DMA پر کرده می‌آید (سخت‌افزار
 *              نوشته، CPU فقط آن را بیرون کپی می‌کند).
 * @param  uint8_t__byte [EN] Output byte pointer / اشاره‌گر بایت خروجی
 * @return bool [EN] true when one byte was available / اگر بایت موجود بود true
 */
bool func__BspUart_ReadByte(uint8_t *uint8_t__byte);

#endif /* BSP_UART_H */
