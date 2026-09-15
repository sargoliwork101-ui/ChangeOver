/**
 * @file    fault.c
 * @brief   [EN] Latched fault bits (placeholder). Full type naming, func__ prefix.
 *          [FA] بیت‌های خطای قفل‌شده (اسکلت). نام تایپ کامل.
 */

#include "fault.h"

static fault_mask_t FAULT_MASK_T__G__Mask = FAULT_NONE;

/**
 * @brief  [EN] Clear all fault bits.
 *         [FA] همه بیت‌های خطا را پاک می‌کند.
 */
void func__Fault_Init(void)
{
    FAULT_MASK_T__G__Mask = FAULT_NONE;
}

/**
 * @brief  [EN] Latch bits (OR).
 *         [FA] بیت‌ها را قفل می‌کند (OR).
 * @param  fault_mask_t__bits [EN] Bits to set / بیت‌هایی که باید قفل شود
 */
void func__Fault_Set(fault_mask_t fault_mask_t__bits)
{
    FAULT_MASK_T__G__Mask |= fault_mask_t__bits;
}

/**
 * @brief  [EN] Clear bits (AND NOT).
 *         [FA] بیت‌ها را پاک می‌کند.
 * @param  fault_mask_t__bits [EN] Bits to clear / بیت‌هایی که باید پاک شود
 */
void func__Fault_Clear(fault_mask_t fault_mask_t__bits)
{
    FAULT_MASK_T__G__Mask &= (fault_mask_t)~fault_mask_t__bits;
}

/**
 * @brief  [EN] Return current mask.
 *         [FA] ماسک فعلی را برمی‌گرداند.
 * @return fault_mask_t [EN] Current fault mask / ماسک فعلی
 */
fault_mask_t func__Fault_Get(void)
{
    return FAULT_MASK_T__G__Mask;
}

/**
 * @brief  [EN] True if any bit is set.
 *         [FA] اگر هر بیتی روشن باشد true.
 * @return bool [EN] true if any fault latched / اگر خطایی قفل شده true
 */
bool func__Fault_Any(void)
{
    return (FAULT_MASK_T__G__Mask != FAULT_NONE);
}
