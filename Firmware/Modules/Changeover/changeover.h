/**
 * @file    changeover.h
 * @brief   [EN] Input vs battery path state machine (placeholder).
 *          [FA] ماشین حالت مسیر ورودی یا باتری (اسکلت).
 */

#ifndef CHANGEOVER_H
#define CHANGEOVER_H

#include "app_types.h"

/**
 * @brief  [EN] Start in BOOT.
 *         [FA] از حالت BOOT شروع می‌کند.
 */
void func_Changeover_Init(void);

/**
 * @brief  [EN] Evaluate next system state from snapshot and faults.
 *         [FA] حالت بعدی سیستم را از نمونه و خطا حساب می‌کند.
 * @param  measurement_snapshot_t_snap [EN] Snapshot / نمونه
 * @param  fault_mask_t_faults [EN] Fault bits / بیت‌های خطا
 * @return app_state_t [EN] Next state / حالت بعدی
 */
app_state_t func_Changeover_Evaluate(const measurement_snapshot_t *measurement_snapshot_t_snap, fault_mask_t fault_mask_t_faults);

#endif /* CHANGEOVER_H */
