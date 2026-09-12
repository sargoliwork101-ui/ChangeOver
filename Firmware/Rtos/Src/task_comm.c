#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "FreeRTOS.h"
#include "task.h"

#if MODULE_ESP
#include "esp_link.h"
#endif
#if MODULE_MEASUREMENT
#include "measurement.h"
#endif
#if MODULE_FAULT
#include "fault.h"
#endif

void TaskComm(void *argument)
{
    (void)argument;

    for (;;)
    {
#if MODULE_ESP
        {
            measurement_snapshot_t snap;
            fault_mask_t faults = FAULT_NONE;
            snap.valid = false;
#if MODULE_MEASUREMENT
            (void)Measurement_GetSnapshot(&snap);
#endif
#if MODULE_FAULT
            faults = Fault_Get();
#endif
            EspLink_Run(&snap, APP_STATE_IDLE, faults);
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.comm_period_ms));
    }
}
