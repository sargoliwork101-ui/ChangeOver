/**
 * @file    task_control.c
 * @brief   [EN] CMSIS-RTOS2 control thread - simple RTOS with osDelay, readable.
 *          [FA] تسک کنترل ساده RTOS.
 *
 * @note    [EN] When MODULE_CHANGEOVER is enabled this task obtains the real
 *              Measurement snapshot via func__Measurement_GetSnapshot() and
 *              passes it with the fault bits to func__Changeover_Evaluate().
 *              The UI-owned flag BOOL__G__UiBatteryAlarmIssued is global:
 *              UI (task_ui / ui_led) owns and updates it, Changeover reads
 *              it directly - no duplicate flag or API is created here.
 *          [FA] فلگ UI به‌صورت سراسری در اختیار Changeover است؛ API تکراری ساخته نمی‌شود.
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "cmsis_os2.h"
#include "rtos_time.h"

#if MODULE_MEASUREMENT
#include "measurement.h"
#endif
#if MODULE_FAULT
#include "fault.h"
#endif
#if MODULE_CHANGEOVER
#include "changeover.h"
#include "ui_led.h"
#endif
#if MODULE_CHARGER
#include "charger.h"
#endif
#if MODULE_JITTER
#include "jitter.h"
#endif

/* ==================== Task Control ==================== */

void func__TaskControl(void *void_ptr__argument)
{
    (void)void_ptr__argument;

    for (;;)
    {
#if (MODULE_CHANGEOVER || MODULE_CHARGER || MODULE_JITTER)
        {
            measurement_snapshot_t measurement_snapshot_t__snap;
            fault_mask_t fault_mask_t__faults = FAULT_NONE;
            app_state_t app_state_t__state = APP_STATE_IDLE;

            measurement_snapshot_t__snap.valid = false;
#if MODULE_MEASUREMENT
            (void)func__Measurement_GetSnapshot(&measurement_snapshot_t__snap);
#endif
#if MODULE_FAULT
            fault_mask_t__faults = func__Fault_Get();
#endif
#if MODULE_JITTER
            func__Jitter_Run();
#endif
#if MODULE_CHANGEOVER
            /* [EN] UI owns BOOL__G__UiBatteryAlarmIssued and updates it in func__Ui_Tick()
               from snapshot.v_bat24_mv. Changeover reads the same global flag directly;
               no extra wiring is needed in this task beyond the snapshot + faults.
               [FA] UI مالک فلگ است و Changeover همان فلگ سراسری را می‌خواند. */
            app_state_t__state = func__Changeover_Evaluate(&measurement_snapshot_t__snap, fault_mask_t__faults);
#endif
#if MODULE_CHARGER
            func__Charger_Evaluate(&measurement_snapshot_t__snap, app_state_t__state);
#endif
            (void)app_state_t__state;
        }
        func__Rtos_DelayMilliseconds(APP_CONFIG.control_period_ms);
#else
        func__Rtos_DelayMilliseconds(1000u);
#endif
    }
}
