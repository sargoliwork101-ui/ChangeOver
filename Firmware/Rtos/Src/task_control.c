/**
 * @file    task_control.c
 * @brief   [EN] FreeRTOS task for changeover/charger policy (placeholder). Full type naming, func_ prefix.
 *          [FA] تسک سیاست Changeover و شارژر (اسکلت). نام تایپ کامل.
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "FreeRTOS.h"
#include "task.h"

#if MODULE_MEASUREMENT
#include "measurement.h"
#endif
#if MODULE_FAULT
#include "fault.h"
#endif
#if MODULE_CHANGEOVER
#include "changeover.h"
#endif
#if MODULE_CHARGER
#include "charger.h"
#endif
#if MODULE_JITTER
#include "jitter.h"
#endif

/**
 * @brief  [EN] Control task entry. Idle loop until a control module is enabled.
 *         [FA] ورود تسک کنترل. تا ماژول کنترل روشن نشود کار نمی‌کند.
 * @param  void_ptr_argument [EN] Required by FreeRTOS, unused / آرگومان FreeRTOS
 */
void func_TaskControl(void *void_ptr_argument)
{
    (void)void_ptr_argument;

    for (;;)
    {
#if (MODULE_CHANGEOVER || MODULE_CHARGER || MODULE_JITTER)
        {
            measurement_snapshot_t measurement_snapshot_t_snap;
            fault_mask_t fault_mask_t_faults = FAULT_NONE;
            app_state_t app_state_t_state = APP_STATE_IDLE;

            measurement_snapshot_t_snap.valid = false;
#if MODULE_MEASUREMENT
            (void)func_Measurement_GetSnapshot(&measurement_snapshot_t_snap);
#endif
#if MODULE_FAULT
            fault_mask_t_faults = func_Fault_Get();
#endif
#if MODULE_JITTER
            func_Jitter_Run();
#endif
#if MODULE_CHANGEOVER
            app_state_t_state = func_Changeover_Evaluate(&measurement_snapshot_t_snap, fault_mask_t_faults);
#endif
#if MODULE_CHARGER
            func_Charger_Evaluate(&measurement_snapshot_t_snap, app_state_t_state);
#endif
            (void)app_state_t_state;
        }
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.control_period_ms));
#else
        vTaskDelay(pdMS_TO_TICKS(1000u));
#endif
    }
}

