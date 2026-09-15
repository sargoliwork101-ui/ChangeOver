/**
 * @file    task_measurement.c
 * @brief   [EN] FreeRTOS measurement task - simple RTOS with vTaskDelay, readable, no HAL_Delay.
 *          [FA] تسک اندازه‌گیری ساده RTOS با vTaskDelay.
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#if MODULE_MEASUREMENT
#include "measurement.h"
#endif

/* ==================== Task Measurement ==================== */

void func__TaskMeasurement(void *void_ptr__argument)
{
    (void)void_ptr__argument;

    for (;;)
    {
#if MODULE_MEASUREMENT
        func__Measurement_Run();
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.control_period_ms));
#else
        vTaskDelay(pdMS_TO_TICKS(1000u));
#endif
    }
}
