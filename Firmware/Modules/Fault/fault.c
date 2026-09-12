/**
 * @file    fault.c
 * @brief   [EN] Latched fault bits (placeholder).
 *          [FA] بیت‌های خطای قفل‌شده (اسکلت).
 *
 * @stage   Placeholder
 */

#include "fault.h"

static fault_mask_t s_mask = FAULT_NONE;

void Fault_Init(void)
{
    s_mask = FAULT_NONE;
}

void Fault_Set(fault_mask_t bits)
{
    s_mask |= bits;
}

void Fault_Clear(fault_mask_t bits)
{
    s_mask &= (fault_mask_t)~bits;
}

fault_mask_t Fault_Get(void)
{
    return s_mask;
}

bool Fault_Any(void)
{
    return (s_mask != FAULT_NONE);
}
