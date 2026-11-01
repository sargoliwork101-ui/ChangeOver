/**
 * @file    bsp_adc.h
 * @brief   [EN] Logical board ADC+DMA interface. The port supplies
 *              BSP_ADC_CHANNEL_COUNT normalized analog values (six today)
 *              in a two-frame buffer; the reader copies only a completed
 *              frame.
 * @note    [EN] The public channel order is a normalized data contract. Each
 *              board-specific BSP maps its own ADC hardware to this order.
 */
#ifndef BSP_ADC_H
#define BSP_ADC_H
/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>
/* ==================== ADC channel map ==================== */
/*
 * [EN] Normalized frame positions; physical channels/pins are chosen
 *      only by the board BSP. Since 2026-09-22 (user order) CURRENT1/
 *      CURRENT2 carry PWM-synchronized samples taken at the exact middle
 *      of each gate's ON window (hardware timer trigger, raw counts); the
 *      asynchronous scan values remain only as fallback when a
 *      synchronized capture is impossible.
 */
#define BSP_ADC_CHANNEL_COUNT        6u
#define BSP_ADC_CHANNEL_CURRENT1     0u
#define BSP_ADC_CHANNEL_24V_IN       1u
#define BSP_ADC_CHANNEL_24V_BAT      2u
#define BSP_ADC_CHANNEL_12V_BAT      3u
#define BSP_ADC_CHANNEL_CURRENT2     4u
/*
 * [EN] Internal 1.20 V reference (ADC1_IN17). It measures nothing external -
 *      it exists so the firmware can work out the REAL VDDA instead of
 *      assuming 3.300 V, because VDDA is the one term every voltage and every
 *      current on this board is multiplied by.
 */
#define BSP_ADC_CHANNEL_VREFINT      5u
/* ==================== DMA buffer ==================== */
/*
 * [EN] The DMA buffer holds two full frames. When DMA writes one half, the
 *      other half is stable and can be copied. BSP_ADC_DMA_RETRY_COUNT
 *      protects the short copy from crossing a half-buffer boundary.
 */
#define BSP_ADC_DMA_FRAME_COUNT      2u
#define BSP_ADC_DMA_SAMPLE_COUNT     (BSP_ADC_CHANNEL_COUNT * BSP_ADC_DMA_FRAME_COUNT)
#define BSP_ADC_DMA_RETRY_COUNT      3u
/* ==================== BspAdc_Init ==================== */
/**
 * @brief  [EN] Select the board ADC backend and clear its DMA buffer.
 */
void func__BspAdc_Init(void);
/* ==================== BspAdc_Start ==================== */
/**
 * @brief  [EN] Calibrate the board ADC backend, then start its scan and DMA.
 *              DMA interrupt sources remain private to the board port; the
 *              public reader only returns completed normalized frames.
 * @return bool [EN] true when the board ADC backend starts successfully /
 */
bool func__BspAdc_Start(void);
/* ==================== BspAdc_IsFrameReady ==================== */
/**
 * @brief  [EN] Report whether ADC+DMA was started successfully. GetRaw still
 *              verifies that a complete half-frame is available before copy.
* @return bool true after successful start / true
 */
bool func__BspAdc_IsFrameReady(void);
/* ==================== BspAdc_GetRaw ==================== */
/**
 * @brief  [EN] Copy the newest completed frame (BSP_ADC_CHANNEL_COUNT
 *              samples, six today) into out[]. The
 *              DMA counter selects the half DMA is not writing; a
 *              before/after counter check rejects a boundary-crossing copy.
 *              Current positions carry PWM mid-ON synchronized samples
 *              (see the channel map note); voltages come from the
 *              continuous scan.
 * @param  uint16_t__out [EN] Output array, index order = BSP_ADC_CHANNEL_* /
* @return bool true when a stable frame was copied /
 */
bool func__BspAdc_GetRaw(uint16_t uint16_t__out[BSP_ADC_CHANNEL_COUNT]);
/* ==================== BspAdc_Recover ==================== */
/**
 * @brief  [EN] Silently reset, reconfigure, recalibrate and restart ADC1+DMA
 *              in place without touching GPIO pins or blocking RTOS tasks.
* @return bool true when ADC1+DMA resumed producing frames / true
 */
bool func__BspAdc_Recover(void);
/* ==================== BspAdc_WasRecoveryReset ==================== */
/**
 * @brief  [EN] Report whether the current boot followed the single software
 *              MCU reset triggered by ADC recovery supervision.
* @return bool true if this boot is the post-reset recovery window / true
 */
bool func__BspAdc_WasRecoveryReset(void);
/* ==================== BspAdc_TriggerRecoveryReset ==================== */
/**
 * @brief  [EN] Arm the single-use recovery marker, stop all PWM outputs, and
 *              issue a software MCU reset for one fresh post-reset ADC window.
 */
void func__BspAdc_TriggerRecoveryReset(void);
#endif /* BSP_ADC_H */
