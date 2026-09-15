/**
 * @file    changeover.c
 * @brief   [EN] Input vs battery path state machine (placeholder). Func_ prefix, full type naming.
 *          [FA] ماشین حالت مسیر ورودی یا باتری (اسکلت). پیشوند func_ و نام تایپ کامل.
 */

#include "changeover.h"

static app_state_t APP_STATE_T_G_State = APP_STATE_BOOT;

/**
 * @brief  [EN] Start in BOOT.
 *         [FA] از حالت BOOT شروع می‌کند.
 */
void func_Changeover_Init(void)
{
    APP_STATE_T_G_State = APP_STATE_BOOT;
}

/**
 * @brief  [EN] Evaluate next system state from snapshot and faults.
 *         [FA] حالت بعدی سیستم را از نمونه و خطا حساب می‌کند.
 * @param  measurement_snapshot_t_snap [EN] Snapshot from measurement, may be NULL / نمونه اندازه‌گیری
 * @param  fault_mask_t_faults [EN] Fault bits from Fault module / بیت‌های خطا
 * @return app_state_t [EN] Next system state / حالت بعدی
 */
app_state_t func_Changeover_Evaluate(const measurement_snapshot_t *measurement_snapshot_t_snap, fault_mask_t fault_mask_t_faults)
{
    (void)measurement_snapshot_t_snap;

    if (fault_mask_t_faults != FAULT_NONE)
    {
        APP_STATE_T_G_State = APP_STATE_FAULT;
    }
    else
    {
        APP_STATE_T_G_State = APP_STATE_IDLE;
    }

    return APP_STATE_T_G_State;
}
