#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "FreeRTOS.h"
#include "task.h"

#if MODULE_PROTECTION
#include "protection.h"
#include "measurement.h"
#endif

void TaskProtection(void *argument)
{
    (void)argument;

    for (;;)
    {
#if MODULE_PROTECTION
        {
            measurement_snapshot_t snap;
            snap.valid = false;
#if MODULE_MEASUREMENT
            (void)Measurement_GetSnapshot(&snap);
#endif
            Protection_Run(&snap);
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.protection_period_ms));
    }
}
