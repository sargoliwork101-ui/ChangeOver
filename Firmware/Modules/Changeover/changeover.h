/**
 * @file    changeover.h
 * @brief   [EN] Input vs battery path state machine (placeholder).
 *          [FA] ماشین حالت مسیر ورودی یا باتری (اسکلت).
 */

#ifndef CHANGEOVER_H
#define CHANGEOVER_H

/* ==================== Includes ==================== */
#include "app_types.h"

/**
 * @brief  [EN] Start in BOOT.
 *         [FA] از حالت BOOT شروع می‌کند.
 */
/* ==================== Functions ==================== */
void func__Changeover_Init(void);

/**
 * @brief  [EN] Evaluate next system state from snapshot and faults.
 *         [FA] حالت بعدی سیستم را از نمونه و خطا حساب می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Snapshot / نمونه
 * @param  fault_mask_t__faults [EN] Fault bits / بیت‌های خطا
 * @return app_state_t [EN] Next state / حالت بعدی
 */
app_state_t func__Changeover_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap, fault_mask_t fault_mask_t__faults);

#endif /* CHANGEOVER_H */
