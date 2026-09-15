/**
 * @file    task_protection.c
 * @brief   [EN] FreeRTOS protection task - simple RTOS with vTaskDelay.
 *          [FA] تسک حفاظت ساده RTOS.
 */

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

/* ==================== Task Protection ==================== */

void func__TaskProtection(void *void_ptr__argument)
{
    (void)void_ptr__argument;

    for (;;)
    {
#if MODULE_PROTECTION
        {
            measurement_snapshot_t measurement_snapshot_t__snap;
            measurement_snapshot_t__snap.valid = false;
#if MODULE_MEASUREMENT
            (void)func__Measurement_GetSnapshot(&measurement_snapshot_t__snap);
#endif
            func__Protection_Run(&measurement_snapshot_t__snap);
        }
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.protection_period_ms));
#else
        vTaskDelay(pdMS_TO_TICKS(1000u));
#endif
    }
}
