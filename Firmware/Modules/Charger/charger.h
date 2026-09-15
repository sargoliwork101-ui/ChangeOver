/**
 * @file    charger.h
 * @brief   [EN] Charger PWM policy (placeholder).
 *          [FA] سیاست PWM شارژر (اسکلت).
 */

#ifndef CHARGER_H
#define CHARGER_H

#include "app_types.h"

/**
 * @brief  [EN] Init charger policy. Must leave PWM at 0 %.
 *         [FA] سیاست شارژر را Init می‌کند. PWM باید ۰٪ بماند.
 */
void func_Charger_Init(void);

/**
 * @brief  [EN] Compute PWM from snapshot and system state. No-op for now.
 *         [FA] PWM را از نمونه و حالت حساب می‌کند. فعلاً کاری نمی‌کند.
 * @param  measurement_snapshot_t_snap [EN] Snapshot / نمونه
 * @param  app_state_t_state [EN] System state / حالت
 */
void func_Charger_Evaluate(const measurement_snapshot_t *measurement_snapshot_t_snap, app_state_t app_state_t_state);

#endif /* CHARGER_H */
