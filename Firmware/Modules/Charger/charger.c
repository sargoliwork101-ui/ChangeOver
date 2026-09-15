/**
 * @file    charger.c
 * @brief   [EN] Charger PWM policy (placeholder). Full type naming, func_ prefix.
 *          [FA] سیاست PWM شارژر (اسکلت). نام تایپ کامل و پیشوند func_.
 */

#include "charger.h"
#include "app_config.h"

/**
 * @brief  [EN] Init charger policy. Must leave PWM at 0%.
 *         [FA] سیاست شارژر را Init می‌کند. PWM باید ۰٪ بماند.
 */
void func_Charger_Init(void)
{
}

/**
 * @brief  [EN] Compute PWM from snapshot and system state. No-op for now.
 *         [FA] PWM را از نمونه و حالت حساب می‌کند. فعلاً کاری نمی‌کند.
 * @param  measurement_snapshot_t_snap [EN] Snapshot / نمونه
 * @param  app_state_t_state [EN] System state / حالت سیستم
 */
void func_Charger_Evaluate(const measurement_snapshot_t *measurement_snapshot_t_snap, app_state_t app_state_t_state)
{
    (void)measurement_snapshot_t_snap;
    (void)app_state_t_state;
    (void)APP_CONFIG;
}
