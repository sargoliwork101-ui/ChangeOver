/**
 * @file    protection.h
 * @brief   [EN] Over-current and low-battery checks (placeholder). Full type naming, func__ prefix.
 *          [FA] بررسی اضافه جریان و باتری ضعیف (اسکلت). نام تایپ کامل.
 */

#ifndef PROTECTION_H
#define PROTECTION_H

#include "app_types.h"

/**
 * @brief  [EN] Init protection state.
 *         [FA] حالت حفاظت را Init می‌کند.
 */
void func__Protection_Init(void);

/**
 * @brief  [EN] Compare snapshot against limits; latch faults.
 *         [FA] نمونه را با حد مقایسه می‌کند و خطا را قفل می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Snapshot pointer / اشاره‌گر نمونه
 */
void func__Protection_Run(const measurement_snapshot_t *measurement_snapshot_t__snap);

#endif /* PROTECTION_H */
