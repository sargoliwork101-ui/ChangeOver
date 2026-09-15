/**
 * @file    fault.h
 * @brief   [EN] Bit-mask of latched faults (placeholder). Full type naming, func_ prefix.
 *          [FA] بیت‌ماسک خطاهای قفل‌شده (اسکلت). نام تایپ کامل.
 */

#ifndef FAULT_H
#define FAULT_H

#include "app_types.h"

/**
 * @brief  [EN] Clear all fault bits.
 *         [FA] همه بیت‌های خطا را پاک می‌کند.
 */
void func_Fault_Init(void);

/**
 * @brief  [EN] Latch bits (OR).
 *         [FA] بیت‌ها را قفل می‌کند (OR).
 * @param  fault_mask_t_bits [EN] Bits to set / بیت‌هایی که باید قفل شود
 */
void func_Fault_Set(fault_mask_t fault_mask_t_bits);

/**
 * @brief  [EN] Clear bits (AND NOT).
 *         [FA] بیت‌ها را پاک می‌کند.
 * @param  fault_mask_t_bits [EN] Bits to clear / بیت‌هایی که باید پاک شود
 */
void func_Fault_Clear(fault_mask_t fault_mask_t_bits);

/**
 * @brief  [EN] Return current mask.
 *         [FA] ماسک فعلی را برمی‌گرداند.
 * @return fault_mask_t [EN] Current mask / ماسک فعلی
 */
fault_mask_t func_Fault_Get(void);

/**
 * @brief  [EN] True if any bit is set.
 *         [FA] اگر هر بیتی روشن باشد true.
 * @return bool [EN] true if any fault / اگر خطایی باشد true
 */
bool func_Fault_Any(void);

#endif /* FAULT_H */
