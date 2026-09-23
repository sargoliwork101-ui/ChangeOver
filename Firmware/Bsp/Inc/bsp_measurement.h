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

/* ==================== BspMeasurement Current1 Counts To Ma ==================== */

/**
 * @brief  [EN] Convert the channel-1 normalized current counts to mA with
 *              the channel-1 (Trans1/Shunt1) calibration pair.
 *         [FA] کانال جریان ۱ را با کالیبراسیون مستقل به mA تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Current in mA / جریان بر حسب mA
 */
uint32_t func__BspMeasurement_Current1CountsToMa(uint16_t uint16_t__counts);

/* ==================== BspMeasurement Current Calibration Runtime ==================== */

/**
 * @brief  [EN] Set the zero-current offset (counts) of one current channel
 *              at runtime, clamped to 0..255; RAM only, ESP panel (user
 *              order 2026-09-22).
 *         [FA] آفست جریان صفر یک کانال در زمان اجرا، گیرهٔ ۰..۲۵۵؛ فقط
 *              RAM، پنل ESP (دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = channel 1, 1 = channel 2 / ۰ یا ۱
 * @param  uint32_t__offsetCounts [EN] Requested offset / آفست درخواستی
 * @return uint32_t [EN] Applied offset / آفست اعمال‌شده
 */
uint32_t func__BspMeasurement_SetCurrentOffsetCounts(uint8_t uint8_t__channelIndex,
                                                     uint32_t uint32_t__offsetCounts);

/**
 * @brief  [EN] Set the bench gain trim (permille) of one current channel at
 *              runtime, clamped to 100..3000; RAM only, ESP panel (user
 *              order 2026-09-22).
 *         [FA] ضریب گین بنچ یک کانال در زمان اجرا، گیرهٔ ۱۰۰..۳۰۰۰؛ فقط
 *              RAM، پنل ESP (دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = channel 1, 1 = channel 2 / ۰ یا ۱
 * @param  uint32_t__gainPermille [EN] Requested gain permille / گین درخواستی
 * @return uint32_t [EN] Applied gain permille / گین اعمال‌شده
 */
uint32_t func__BspMeasurement_SetCurrentGainPermille(uint8_t uint8_t__channelIndex,
                                                     uint32_t uint32_t__gainPermille);

/**
 * @brief  [EN] Read the live zero-current offset (counts) of one channel.
 *         [FA] آفست جریان صفر زندهٔ یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = channel 1, 1 = channel 2 / ۰ یا ۱
 * @return uint32_t [EN] Offset in counts / آفست بر حسب شمارش
 */
uint32_t func__BspMeasurement_GetCurrentOffsetCounts(uint8_t uint8_t__channelIndex);

/**
 * @brief  [EN] Read the live bench gain trim (permille) of one channel.
 *         [FA] ضریب گین بنچ زندهٔ یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = channel 1, 1 = channel 2 / ۰ یا ۱
 * @return uint32_t [EN] Gain permille / گین پرمیل
 */
uint32_t func__BspMeasurement_GetCurrentGainPermille(uint8_t uint8_t__channelIndex);

/* ==================== BspMeasurement Current2 Counts To Ma ==================== */

/**
 * @brief  [EN] Convert the channel-2 normalized current counts to mA with
 *              the channel-2 (Trans2/Shunt2) bench-verified calibration pair.
 *         [FA] کانال جریان ۲ را با کالیبراسیون بنچ تأییدشده به mA تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Current in mA / جریان بر حسب mA
 */
uint32_t func__BspMeasurement_Current2CountsToMa(uint16_t uint16_t__counts);
/* [EN] Legacy generic converter = channel-2 calibration; new code must pick
 *      the per-channel function above (user order 2026-09-20: charger 1 no
 *      longer rides on charger 2's calibration).
 * [FA] مبدل عمومی قدیمی = کالیبراسیون کاnal ۲؛ کد جدید از تابع پر-کانال. */
uint32_t func__BspMeasurement_CurrentCountsToMa(uint16_t uint16_t__counts);

/* ==================== BspMeasurement Current Counts To Shunt Uv ==================== */

/**
 * @brief  [EN] Pure hardware chain (reference + R41/R42 divider + amplifier
 *              gain, no offset, no trim) from raw current counts to the
 *              sense-shunt voltage in uV - live diagnostic for the
 *              current-chain review, user order 2026-09-22.
 *         [FA] زنجیرهٔ فقط-سخت‌افزاری (مرجع + مقسم R41/R42 + گین تقویت‌کننده،
 *              بدون آفست و اصلاح) از شمارش خام جریان به ولتاژ شانت بر حسب
 *              uV - دیاگ زندهٔ بررسی زنجیرهٔ جریان، دستور کاربر ۲۰۲۶-۰۹-۲۲.
 * @param  uint16_t__counts [EN] Raw ADC count of a current channel /
 *                              شمارش خام ADC یک کانال جریان
 * @return uint32_t [EN] Shunt voltage in uV / ولتاژ شانت بر حسب uV
 */
uint32_t func__BspMeasurement_CurrentCountsToShuntUv(uint16_t uint16_t__counts);

#endif /* BSP_MEASUREMENT_H */
