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

void TaskControl(void *argument)
{
    (void)argument;

    for (;;)
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
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.control_period_ms));
    }
}
