/**
 * @file    task_control.c
 * @brief   [EN] FreeRTOS task for changeover/charger policy (placeholder).
 *          [FA] تسک سیاست Changeover و شارژر (اسکلت، هنوز فعال نیست).
 *
 * @stage   Placeholder — changeover/charger flags are 0.
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
 * @param  argument  [EN] Required by FreeRTOS, unused.
 *                   [FA] اجباری FreeRTOS، استفاده نمی‌شود.
 */
void TaskControl(void *argument)
{
    (void)argument;

    for (;;)
    {
#if (MODULE_CHANGEOVER || MODULE_CHARGER || MODULE_JITTER)
        {
            measurement_snapshot_t snap;
            fault_mask_t faults = FAULT_NONE;
            app_state_t state = APP_STATE_IDLE;

            snap.valid = false;
#if MODULE_MEASUREMENT
            (void)Measurement_GetSnapshot(&snap);
#endif
#if MODULE_FAULT
            faults = Fault_Get();
#endif
#if MODULE_JITTER
            Jitter_Run();
#endif
#if MODULE_CHANGEOVER
            state = Changeover_Evaluate(&snap, faults);
#endif
#if MODULE_CHARGER
            Charger_Evaluate(&snap, state);
#endif
            (void)state;
        }
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.control_period_ms));
#else
        vTaskDelay(pdMS_TO_TICKS(1000u));
#endif
    }
}
