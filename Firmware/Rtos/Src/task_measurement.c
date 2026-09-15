/**
 * @file    task_measurement.c
 * @brief   [EN] FreeRTOS task for ADC sampling (placeholder). Full type naming, func_ prefix.
 *          [FA] تسک نمونه‌برداری ADC (اسکلت). نام تایپ کامل.
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
 * @param  void_ptr_argument [EN] Required by FreeRTOS, unused / آرگومان
 */
void func_TaskMeasurement(void *void_ptr_argument)
{
    (void)void_ptr_argument;

    for (;;)
    {
#if MODULE_MEASUREMENT
        func_Measurement_Run();
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.control_period_ms));
#else
        vTaskDelay(pdMS_TO_TICKS(1000u));
#endif
    }
}

