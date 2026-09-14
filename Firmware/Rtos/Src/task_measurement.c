/**
 * @file    task_measurement.c
 * @brief   [EN] FreeRTOS task for ADC sampling (placeholder).
 *          [FA] تسک نمونه‌برداری ADC (اسکلت، هنوز فعال نیست).
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#if MODULE_MEASUREMENT
#include "measurement.h"
#endif

/**
 * @brief  [EN] Measurement task entry. Idle loop until the module is enabled.
 *         [FA] ورود تسک اندازه‌گیری. تا ماژول روشن نشود کار نمی‌کند.
 * @param  argument  [EN] Required by FreeRTOS, unused.
 *                   [FA] اجباری FreeRTOS، استفاده نمی‌شود.
 */
void TaskMeasurement(void *argument)
{
    (void)argument;

    for (;;)
    {
#if MODULE_MEASUREMENT
        Measurement_Run();
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.control_period_ms));
#else
        vTaskDelay(pdMS_TO_TICKS(1000u));
#endif
    }
}
