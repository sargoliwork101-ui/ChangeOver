#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#if MODULE_MEASUREMENT
#include "measurement.h"
#endif

void TaskMeasurement(void *argument)
{
    (void)argument;

    for (;;)
    {
#if MODULE_MEASUREMENT
        Measurement_Run();
#endif
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.control_period_ms));
    }
}
