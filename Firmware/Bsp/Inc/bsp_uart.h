/**
 * @file    bsp_uart.h
 * @brief   [EN] Logical byte-stream interface for the schematic ESP-Link UART.
 * @note    [EN] USART instance, pins and HAL handle are private to the board
 *              port; the port re-tunes the Cube default to 921600 baud and
 *              drives both directions with DMA (RX: circular 256-byte ring,
 *              zero CPU per byte, no reception interrupt at all; TX:
 *              1024-byte software ring drained by DMA - per frame only a
 *              couple of lightweight completion interrupts: DMA-complete
 *              plus the UART transmit-complete this HAL arms afterwards).
 *              The interface remains available while MODULE_ESP is disabled.
 */
#ifndef BSP_UART_H
#define BSP_UART_H
#include <stdint.h>
#include <stdbool.h>
/* ==================== BspUart_Init ==================== */
/**
 * @brief  [EN] Select the initialized board UART backend for ESP-Link.
 */
void func__BspUart_Init(void);
/* ==================== BspUart_Write ==================== */
/**
 * @brief  [EN] Queue a byte buffer on the logical ESP-Link UART (non-blocking).
 * @param  uint8_t__data [EN] Data buffer, not null when length is non-zero /
 * @param  uint16_t__length [EN] Number of bytes, 0..65535 /
 * @return bool [EN] true when the buffer fully fits into the TX DMA ring and
 *                   is queued; false when the ring is short on space and the
 *                   WHOLE frame is dropped - never a partial frame on the
 *                   wire (never blocks; the next write drains the ring)
 */
bool func__BspUart_Write(const uint8_t *uint8_t__data, uint16_t uint16_t__length);
/* ==================== BspUart_ReadByte ==================== */
/**
 * @brief  [EN] Poll one received ESP-Link byte without blocking; the byte
 *              comes from the DMA-filled circular RX ring (hardware wrote
 *              it, the CPU only copies it out).
* @param uint8_t__byte Output byte pointer /
* @return bool true when one byte was available / true
 */
bool func__BspUart_ReadByte(uint8_t *uint8_t__byte);
#endif /* BSP_UART_H */
