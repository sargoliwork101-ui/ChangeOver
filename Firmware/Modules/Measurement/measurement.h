/**
 * @file    measurement.h
 * @brief   [EN] Convert ADC counts to millivolt / milliamp (placeholder). Full type naming, func_ prefix.
 *          [FA] تبدیل شمارش ADC به میلی‌ولت / میلی‌آمپر (اسکلت). نام تایپ کامل.
 */

#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include "app_types.h"

/**
 * @brief  [EN] Zero the last snapshot.
 *         [FA] آخرین نمونه را صفر می‌کند.
 */
void func_Measurement_Init(void);

/**
 * @brief  [EN] Pull one ADC frame and convert. No-op until ADC is enabled.
 *         [FA] یک فریم ADC می‌گیرد و تبدیل می‌کند. تا ADC روشن نشود کاری نمی‌کند.
 */
void func_Measurement_Run(void);

/**
 * @brief  [EN] Copy last snapshot. Returns false if not valid.
 *         [FA] آخرین نمونه را کپی می‌کند. اگر معتبر نباشد false.
 * @param  measurement_snapshot_t_out [EN] Output pointer / اشاره‌گر خروجی
 * @return bool [EN] true if valid / اگر معتبر true
 */
bool func_Measurement_GetSnapshot(measurement_snapshot_t *measurement_snapshot_t_out);

#endif /* MEASUREMENT_H */
