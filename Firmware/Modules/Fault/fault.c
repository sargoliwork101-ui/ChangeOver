/**
 * @file    fault.c
 * @brief   [EN] Latched fault bits (placeholder).
 *          [FA] بیت‌های خطای قفل‌شده (اسکلت).
 */

#include "fault.h"

static fault_mask_t s_mask = FAULT_NONE;

/**
 * @brief  [EN] Clear all fault bits.
 *         [FA] همه بیت‌های خطا را پاک می‌کند.
 */
void Fault_Init(void)
{
    s_mask = FAULT_NONE;
}

/**
 * @brief  [EN] Latch bits (OR).
 *         [FA] بیت‌ها را قفل می‌کند (OR).
 */
void Fault_Set(fault_mask_t bits)
{
    s_mask |= bits;
}

/**
 * @brief  [EN] Clear bits (AND NOT).
 *         [FA] بیت‌ها را پاک می‌کند.
 */
void Fault_Clear(fault_mask_t bits)
{
    s_mask &= (fault_mask_t)~bits;
}

/**
 * @brief  [EN] Return current mask.
 *         [FA] ماسک فعلی را برمی‌گرداند.
 */
fault_mask_t Fault_Get(void)
{
    return s_mask;
}

/**
 * @brief  [EN] True if any bit is set.
 *         [FA] اگر هر بیتی روشن باشد true.
 */
bool Fault_Any(void)
{
    return (s_mask != FAULT_NONE);
}
