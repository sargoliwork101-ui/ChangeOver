/**
 * @file    protection.h
 * @brief   [EN] Over-current and low-battery checks (placeholder).
 *          [FA] بررسی اضافه جریان و باتری ضعیف (اسکلت).
 */

#ifndef PROTECTION_H
#define PROTECTION_H

#include "app_types.h"

/**
 * @brief  [EN] Init protection state.
 *         [FA] حالت حفاظت را Init می‌کند.
 */
void Protection_Init(void);

/**
 * @brief  [EN] Compare snapshot against limits; latch faults.
 *         [FA] نمونه را با حد مقایسه می‌کند و خطا را قفل می‌کند.
 */
void Protection_Run(const measurement_snapshot_t *snap);

#endif /* PROTECTION_H */
