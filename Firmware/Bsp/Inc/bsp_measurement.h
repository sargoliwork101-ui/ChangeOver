/**
 * @file    bsp_measurement.h
 * @brief   [EN] Board calibration interface for converting normalized ADC data.
 * @note    [EN] Divider, reference, amplifier and shunt details belong to the
 *              board port. The Measurement module only consumes these results.
 */
#ifndef BSP_MEASUREMENT_H
#define BSP_MEASUREMENT_H
#include <stdint.h>
/* ==================== BspMeasurement Counts To Mv ==================== */
/**
 * @brief  [EN] Convert normalized ADC counts to voltage at the ADC pin.
* @param uint16_t__counts Normalized ADC count / ADC
* @return uint32_t ADC pin voltage in mV / mV
 */
uint32_t func__BspMeasurement_CountsToMv(uint16_t uint16_t__counts);
/* ==================== BspMeasurement V24 Counts To Mv ==================== */
/**
 * @brief  [EN] Convert the normalized 24 V channel to source voltage.
* @param uint16_t__counts Normalized ADC count / ADC
* @return uint32_t Source voltage in mV / mV
 */
uint32_t func__BspMeasurement_V24CountsToMv(uint16_t uint16_t__counts);
/* ==================== BspMeasurement Battery24 Counts To Mv ==================== */
/**
 * @brief  [EN] Convert the battery-PACK 24 V channel (user divider factor,
 *              2026-09-25: attenuation 6.8k/69.2k to the pin).
* @param uint16_t__counts Normalized ADC count / ADC
* @return uint32_t Pack voltage in mV / mV
 */
uint32_t func__BspMeasurement_Battery24CountsToMv(uint16_t uint16_t__counts);
/* ==================== BspMeasurement V12 Counts To Mv ==================== */
/**
 * @brief  [EN] Convert the normalized 12 V channel to source voltage.
* @param uint16_t__counts Normalized ADC count / ADC
* @return uint32_t Source voltage in mV / mV
 */
uint32_t func__BspMeasurement_V12CountsToMv(uint16_t uint16_t__counts);
/* ==================== BspMeasurement Current1 Counts To Ma ==================== */
/**
 * @brief  [EN] Convert the channel-1 normalized current counts to mA with
 *              the channel-1 (Trans1/Shunt1) calibration pair.
* @param uint16_t__counts Normalized ADC count / ADC
* @return uint32_t Current in mA / mA
 */
uint32_t func__BspMeasurement_Current1CountsToMa(uint16_t uint16_t__counts);
/* ==================== BspMeasurement Current Calibration Runtime ==================== */
/**
 * @brief  [EN] Set the zero-current offset (counts) of one current channel
 *              at runtime, clamped to 0..255; flash-persisted (v1.14 NVM),
 *              ESP panel (user order 2026-09-22).
* @param uint8_t__channelIndex 0 = channel 1, 1 = channel 2 /
* @param uint32_t__offsetCounts Requested offset /
* @return uint32_t Applied offset /
 */
uint32_t func__BspMeasurement_SetCurrentOffsetCounts(uint8_t uint8_t__channelIndex,
                                                     uint32_t uint32_t__offsetCounts);
/**
 * @brief  [EN] Set the bench gain trim (permille) of one current channel at
 *              runtime, clamped to 100..3000; flash-persisted (v1.14 NVM),
 *              ESP panel (user order 2026-09-22).
* @param uint8_t__channelIndex 0 = channel 1, 1 = channel 2 /
* @param uint32_t__gainPermille Requested gain permille /
* @return uint32_t Applied gain permille /
 */
uint32_t func__BspMeasurement_SetCurrentGainPermille(uint8_t uint8_t__channelIndex,
                                                     uint32_t uint32_t__gainPermille);
/**
 * @brief  [EN] Read the live zero-current offset (counts) of one channel.
* @param uint8_t__channelIndex 0 = channel 1, 1 = channel 2 /
* @return uint32_t Offset in counts /
 */
uint32_t func__BspMeasurement_GetCurrentOffsetCounts(uint8_t uint8_t__channelIndex);
/**
 * @brief  [EN] Read the live bench gain trim (permille) of one channel.
* @param uint8_t__channelIndex 0 = channel 1, 1 = channel 2 /
* @return uint32_t Gain permille /
 */
uint32_t func__BspMeasurement_GetCurrentGainPermille(uint8_t uint8_t__channelIndex);
/* ==================== BspMeasurement Current2 Counts To Ma ==================== */
/**
 * @brief  [EN] Convert the channel-2 normalized current counts to mA with
 *              the channel-2 (Trans2/Shunt2) bench-verified calibration pair.
* @param uint16_t__counts Normalized ADC count / ADC
* @return uint32_t Current in mA / mA
 */
uint32_t func__BspMeasurement_Current2CountsToMa(uint16_t uint16_t__counts);
/* ==================== BspMeasurement Chain Extension (tenths of mA) ==================== */
/*
 * [EN] Exact chain difference BSPfull(high) - BSPfull(low) in tenths of mA,
 *      rounded up once; used for the extrapolation above the last table anchor.
 */
uint32_t func__BspMeasurement_Current1DeltaTenthMa(uint16_t uint16_t__countsHigh,
                                                   uint16_t uint16_t__countsLow);
uint32_t func__BspMeasurement_Current2DeltaTenthMa(uint16_t uint16_t__countsHigh,
                                                   uint16_t uint16_t__countsLow);
/* ==================== BspMeasurement Current Counts To Shunt Uv ==================== */
/**
 * @brief  [EN] Pure hardware chain (reference + R41/R42 divider + amplifier
 *              gain, no offset, no trim) from raw current counts to the
 *              sense-shunt voltage in uV - live diagnostic for the
 *              current-chain review, user order 2026-09-22.
 * @param  uint16_t__counts [EN] Raw ADC count of a current channel /
* @return uint32_t Shunt voltage in uV / uV
 */
uint32_t func__BspMeasurement_CurrentCountsToShuntUv(uint16_t uint16_t__counts);
/* ==================== BspMeasurement Vdda Mv ==================== */
/**
 * @brief  [EN] Measure the real ADC reference (VDDA) from the internal
 *              1.20 V VREFINT channel: VREFINT sits at a known voltage, so
 *              the count it produces says what full scale is worth. The
 *              reference value is passed IN, which keeps this board layer
 *              free of bench-calibration headers. The result is rejected
 *              (0 returned) when it falls outside what a 3.3 V rail can
 *              physically be, so a stuck or un-enabled VREFINT channel can
 *              never silently rescale every reading on the product.
 * @param  uint16_t__vrefintCounts [EN] Raw counts of the VREFINT channel,
 *                                     range 0..BSP_ADC_FULL_SCALE; 0 means
 *                                     "not sampled" and returns 0 /
 * @param  uint32_t__vrefintMv     [EN] Reference voltage of that channel in
 *                                     mV, typically CAL_VREFINT_MV around
 *                                     1200 (datasheet spread 1160..1240) /
 * @return uint32_t [EN] Measured VDDA in mV inside
 *                      BSP_MEASUREMENT_VDDA_MIN_MV..MAX_MV, or 0 when the
 *                      reading is implausible /
 */
uint32_t func__BspMeasurement_VddaMv(uint16_t uint16_t__vrefintCounts,
                                    uint32_t uint32_t__vrefintMv);
#endif /* BSP_MEASUREMENT_H */
