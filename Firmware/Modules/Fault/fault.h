/**
 * @file    fault.h
 * @brief   [EN] Bit-mask of latched faults (placeholder).
 *          [FA] بیت‌ماسک خطاهای قفل‌شده (اسکلت).
 *
 * @stage   Placeholder
 */

#ifndef FAULT_H
#define FAULT_H

#include "app_types.h"

/**
 * @brief  [EN] Clear all fault bits.
 *         [FA] همه بیت‌های خطا را پاک می‌کند.
 */
void Fault_Init(void);

/**
 * @brief  [EN] Latch bits (OR).
 *         [FA] بیت‌ها را قفل می‌کند (OR).
 */
void Fault_Set(fault_mask_t bits);

/**
 * @brief  [EN] Clear bits (AND NOT).
 *         [FA] بیت‌ها را پاک می‌کند.
 */
void Fault_Clear(fault_mask_t bits);

/**
 * @brief  [EN] Return current mask.
 *         [FA] ماسک فعلی را برمی‌گرداند.
 */
fault_mask_t Fault_Get(void);

/**
 * @brief  [EN] True if any bit is set.
 *         [FA] اگر هر بیتی روشن باشد true.
 */
bool Fault_Any(void);

#endif /* FAULT_H */
