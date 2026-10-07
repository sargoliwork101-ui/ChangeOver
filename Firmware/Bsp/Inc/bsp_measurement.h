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

/* ==================== BspMeasurement Counts To Mv ==================== */

/**
 * @brief  [EN] Convert normalized ADC counts to voltage at the ADC pin.
 *         [FA] شمارش استاندارد ADC را به ولتاژ روی پایهٔ ADC تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] ADC pin voltage in mV / ولتاژ پایه بر حسب mV
 */
uint32_t func__BspMeasurement_CountsToMv(uint16_t uint16_t__counts);

/* ==================== BspMeasurement V24 Counts To Mv ==================== */

/**
 * @brief  [EN] Convert the normalized 24 V channel to source voltage.
 *         [FA] کانال استاندارد ۲۴ ولت را به ولتاژ منبع تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Source voltage in mV / ولتاژ منبع بر حسب mV
 */
uint32_t func__BspMeasurement_V24CountsToMv(uint16_t uint16_t__counts);

/* ==================== BspMeasurement Battery24 Counts To Mv ==================== */

/**
 * @brief  [EN] Convert the battery-PACK 24 V channel (user divider factor,
 *              2026-09-25: attenuation 6.8k/69.2k to the pin).
 *         [FA] کانال باتری‌پک ۲۴ ولت را تبدیل می‌کند (ضریب مقسم کاربر،
 *              ۲۰۲۶-۰۹-۲۵: تضعیف ‎6.8k/69.2k‎ تا پایه).
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Pack voltage in mV / ولتاژ پک mV
 */
uint32_t func__BspMeasurement_Battery24CountsToMv(uint16_t uint16_t__counts);

/* ==================== BspMeasurement V12 Counts To Mv ==================== */

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
 *              at runtime, clamped to 0..255; flash-persisted (v1.14 NVM),
 *              ESP panel (user order 2026-09-22).
 *         [FA] آفست جریان صفر یک کانال در زمان اجرا، گیرهٔ ۰..۲۵۵؛ روی
 *              فلش می‌ماند (NVM نسخهٔ ۱.۱۴)، پنل ESP (دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = channel 1, 1 = channel 2‎ / ۰ یا ۱
 * @param  uint32_t__offsetCounts [EN] Requested offset / آفست درخواستی
 * @return uint32_t [EN] Applied offset / آفست اعمال‌شده
 */
uint32_t func__BspMeasurement_SetCurrentOffsetCounts(uint8_t uint8_t__channelIndex,
                                                     uint32_t uint32_t__offsetCounts);

/**
 * @brief  [EN] Set the bench gain trim (permille) of one current channel at
 *              runtime, clamped to 100..3000; flash-persisted (v1.14 NVM),
 *              ESP panel (user order 2026-09-22).
 *         [FA] ضریب گین بنچ یک کانال در زمان اجرا، گیرهٔ ۱۰۰..۳۰۰۰؛ روی
 *              فلش می‌ماند (NVM نسخهٔ ۱.۱۴)، پنل ESP (دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = channel 1, 1 = channel 2‎ / ۰ یا ۱
 * @param  uint32_t__gainPermille [EN] Requested gain permille / گین درخواستی
 * @return uint32_t [EN] Applied gain permille / گین اعمال‌شده
 */
uint32_t func__BspMeasurement_SetCurrentGainPermille(uint8_t uint8_t__channelIndex,
                                                     uint32_t uint32_t__gainPermille);

/**
 * @brief  [EN] Read the live zero-current offset (counts) of one channel.
 *         [FA] آفست جریان صفر زندهٔ یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = channel 1, 1 = channel 2‎ / ۰ یا ۱
 * @return uint32_t [EN] Offset in counts / آفست بر حسب شمارش
 */
uint32_t func__BspMeasurement_GetCurrentOffsetCounts(uint8_t uint8_t__channelIndex);

/**
 * @brief  [EN] Read the live bench gain trim (permille) of one channel.
 *         [FA] ضریب گین بنچ زندهٔ یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = channel 1, 1 = channel 2‎ / ۰ یا ۱
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

/* ==================== BspMeasurement Current Counts To Ma (legacy) ==================== */

/**
 * @brief  [EN] Legacy generic converter, identical to the channel-2
 *              function above because it uses the channel-2 calibration
 *              pair. Kept only so older callers keep linking; new code must
 *              pick the per-channel function (user order 2026-09-20:
 *              charger 1 no longer rides on charger 2's calibration).
 *         [FA] مبدل عمومی قدیمی که دقیقاً همان تابع کانال ۲ است، چون از جفت
 *              کالیبراسیون کانال ۲ استفاده می‌کند. فقط برای این نگه داشته
 *              شده که فراخوان‌های قدیمی لینک شوند؛ کد جدید باید تابع
 *              پر-کانال را بردارد (دستور کاربر ۲۰۲۶-۰۹-۲۰: شارژر ۱ دیگر روی
 *              کالیبراسیون شارژر ۲ سوار نیست).
 * @param  uint16_t__counts [EN] Normalized ADC count of a current channel /
 *                              شمارش استاندارد ADC یک کانال جریان
 * @return uint32_t [EN] Current in mA, channel-2 calibration /
 *                      جریان بر حسب mA با کالیبراسیون کانال ۲
 */
uint32_t func__BspMeasurement_CurrentCountsToMa(uint16_t uint16_t__counts);

/* ==================== BspMeasurement Current Counts To Shunt Uv ==================== */

/**
 * @brief  [EN] Pure hardware chain (reference + R41/R42 divider + amplifier
 *              gain, no offset, no trim) from raw current counts to the
 *              sense-shunt voltage in uV - live diagnostic for the
 *              current-chain review, user order 2026-09-22.
 *         [FA] زنجیرهٔ فقط-سخت‌افزاری (مرجع + مقسم ‎R41/R42‎ + گین تقویت‌کننده،
 *              بدون آفست و اصلاح) از شمارش خام جریان به ولتاژ شانت بر حسب
 *              uV - دیاگ زندهٔ بررسی زنجیرهٔ جریان، دستور کاربر ۲۰۲۶-۰۹-۲۲.
 * @param  uint16_t__counts [EN] Raw ADC count of a current channel /
 *                              شمارش خام ADC یک کانال جریان
 * @return uint32_t [EN] Shunt voltage in uV / ولتاژ شانت بر حسب uV
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
 *         [FA] مرجع واقعی ADC یعنی VDDA را از کانال داخلی ۱٫۲۰ ولتی VREFINT
 *              اندازه می‌گیرد: چون ولتاژ VREFINT معلوم است، شمارشی که تولید
 *              می‌کند می‌گوید مقیاس کامل چقدر می‌ارزد. مقدار مرجع از بیرون
 *              داده می‌شود تا این لایهٔ برد به هدرهای کالیبراسیون بنچ وابسته
 *              نشود. اگر نتیجه بیرون از چیزی باشد که یک ریل ۳٫۳ ولت فیزیکاً
 *              می‌تواند باشد، صفر برمی‌گردد تا کانال گیرکرده یا فعال‌نشده
 *              هرگز بی‌صدا همهٔ خوانش‌های محصول را بازمقیاس نکند.
 * @param  uint16_t__vrefintCounts [EN] Raw counts of the VREFINT channel,
 *                                     range 0..BSP_ADC_FULL_SCALE; 0 means
 *                                     "not sampled" and returns 0 /
 *                                     شمارش خام کانال VREFINT، بازهٔ ۰ تا
 *                                     مقیاس کامل؛ صفر یعنی نمونه‌برداری نشده
 * @param  uint32_t__vrefintMv     [EN] Reference voltage of that channel in
 *                                     mV, typically CAL_VREFINT_MV around
 *                                     1200 (datasheet spread 1160..1240) /
 *                                     ولتاژ مرجع همان کانال بر حسب mV،
 *                                     معمولاً حدود ۱۲۰۰ (۱۱۶۰ تا ۱۲۴۰)
 * @return uint32_t [EN] Measured VDDA in mV inside
 *                      BSP_MEASUREMENT_VDDA_MIN_MV..MAX_MV, or 0 when the
 *                      reading is implausible /
 *                      VDDA اندازه‌گیری‌شده بر حسب mV در بازهٔ مجاز، یا صفر
 *                      اگر خوانش نامعقول باشد
 */
uint32_t func__BspMeasurement_VddaMv(uint16_t uint16_t__vrefintCounts,
                                    uint32_t uint32_t__vrefintMv);

#endif /* BSP_MEASUREMENT_H */
