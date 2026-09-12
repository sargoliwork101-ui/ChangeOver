/**
 * @file    measurement.h
 * @brief   [EN] Convert ADC counts to millivolt / milliamp (placeholder).
 *          [FA] تبدیل شمارش ADC به میلی‌ولت / میلی‌آمپر (اسکلت).
 *
 * @stage   Placeholder
 */

#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include "app_types.h"

/**
 * @brief  [EN] Zero the last snapshot.
 *         [FA] آخرین نمونه را صفر می‌کند.
 */
void Measurement_Init(void);

/**
 * @brief  [EN] Pull one ADC frame and convert. No-op until ADC is enabled.
 *         [FA] یک فریم ADC می‌گیرد و تبدیل می‌کند. تا ADC روشن نشود کاری نمی‌کند.
 */
void Measurement_Run(void);

/**
 * @brief  [EN] Copy last snapshot. Returns false if not valid.
 *         [FA] آخرین نمونه را کپی می‌کند. اگر معتبر نباشد false.
 */
bool Measurement_GetSnapshot(measurement_snapshot_t *out);

#endif /* MEASUREMENT_H */
