/**
 * @file    task_protection.c
 * @brief   [EN] FreeRTOS task for over-current / low-battery checks (placeholder).
 *          [FA] تسک حفاظت جریان و ولتاژ (اسکلت، هنوز فعال نیست).
 *
 * @stage   Placeholder — MODULE_PROTECTION is 0.
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

/**
 * @brief  [EN] Protection task entry. Idle loop until the module is enabled.
 *         [FA] ورود تسک حفاظت. تا ماژول روشن نشود کار نمی‌کند.
 * @param  argument  [EN] Required by FreeRTOS, unused.
 *                   [FA] اجباری FreeRTOS، استفاده نمی‌شود.
 */
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
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.protection_period_ms));
#else
        vTaskDelay(pdMS_TO_TICKS(1000u));
#endif
    }
}
