/**
 * @file    bsp_measurement.h
 * @brief   [EN] Board calibration interface for converting normalized ADC data.
 *          [FA] رابط کالیبراسیون برد برای تبدیل دادهٔ استاندارد ADC.
 *
 * @note    [EN] Divider, reference, amplifier and shunt details belong to the
 *              board port. The Measurement module only consumes these results.
 *          [FA] جزئیات تقسیم مقاومتی، مرجع، تقویت‌کننده و شانت متعلق به پورت
 *              برد است؛ ماژول Measurement فقط نتیجه را مصرف می‌کند.
 */

#ifndef BSP_MEASUREMENT_H
#define BSP_MEASUREMENT_H

#include <stdint.h>

/**
 * @brief  [EN] Convert normalized ADC counts to voltage at the ADC pin.
 *         [FA] شمارش استاندارد ADC را به ولتاژ روی پایهٔ ADC تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] ADC pin voltage in mV / ولتاژ پایه بر حسب mV
 */
uint32_t func__BspMeasurement_CountsToMv(uint16_t uint16_t__counts);

/**
 * @brief  [EN] Convert the normalized 24 V channel to source voltage.
 *         [FA] کانال استاندارد ۲۴ ولت را به ولتاژ منبع تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Source voltage in mV / ولتاژ منبع بر حسب mV
 */
uint32_t func__BspMeasurement_V24CountsToMv(uint16_t uint16_t__counts);

/**
 * @brief  [EN] Convert the normalized 12 V channel to source voltage.
 *         [FA] کانال استاندارد ۱۲ ولت را به ولتاژ منبع تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Source voltage in mV / ولتاژ منبع بر حسب mV
 */
uint32_t func__BspMeasurement_V12CountsToMv(uint16_t uint16_t__counts);

/**
 * @brief  [EN] Convert the normalized 24 V INPUT channel with the board
 *              offset (reading was 0.2 V high on this board).
 *         [FA] کانال ورودی ۲۴ ولت را با آفست برد تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Input voltage in mV / ولتاژ ورودی بر حسب mV
 */
uint32_t func__BspMeasurement_VinCountsToMv(uint16_t uint16_t__counts);

/**
 * @brief  [EN] Convert a normalized current channel to milliamps.
 *         [FA] کانال استاندارد جریان را به میلی‌آمپر تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Current in mA / جریان بر حسب mA
 */
uint32_t func__BspMeasurement_CurrentCountsToMa(uint16_t uint16_t__counts);

#endif /* BSP_MEASUREMENT_H */
