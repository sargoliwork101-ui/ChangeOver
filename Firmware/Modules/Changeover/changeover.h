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
void Changeover_Init(void);

/**
 * @brief  [EN] Evaluate next system state from snapshot and faults.
 *         [FA] حالت بعدی سیستم را از نمونه و خطا حساب می‌کند.
 */
app_state_t Changeover_Evaluate(const measurement_snapshot_t *snap, fault_mask_t faults);

#endif /* CHANGEOVER_H */
