/**
 * @file    task_control.c
 * @brief   [EN] CMSIS-RTOS2 control thread - simple RTOS with osDelay, readable.
 *          [FA] تسک کنترل ساده RTOS.
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
#endif
#if MODULE_UI
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
            app_state_t__state = func__Changeover_Evaluate(
                &measurement_snapshot_t__snap,
                fault_mask_t__faults,
#if MODULE_UI
                BOOL__G__UiBatteryAlarmIssued);
#else
                false);
#endif
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
