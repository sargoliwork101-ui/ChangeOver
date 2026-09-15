/**
 * @file    task_protection.c
 * @brief   [EN] FreeRTOS task for over-current / low-battery checks (placeholder). Full type naming, func_ prefix.
 *          [FA] تسک حفاظت جریان و ولتاژ (اسکلت). نام تایپ کامل.
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
 * @param  void_ptr_argument [EN] Required by FreeRTOS, unused / آرگومان
 */
void func_TaskProtection(void *void_ptr_argument)
{
    (void)void_ptr_argument;

    for (;;)
    {
#if MODULE_PROTECTION
        {
            measurement_snapshot_t measurement_snapshot_t_snap;
            measurement_snapshot_t_snap.valid = false;
#if MODULE_MEASUREMENT
            (void)func_Measurement_GetSnapshot(&measurement_snapshot_t_snap);
#endif
            func_Protection_Run(&measurement_snapshot_t_snap);
        }
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.protection_period_ms));
#else
        vTaskDelay(pdMS_TO_TICKS(1000u));
#endif
    }
}

