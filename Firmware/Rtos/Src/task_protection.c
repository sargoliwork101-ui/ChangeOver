/**
 * @file    task_protection.c
 * @brief   [EN] FreeRTOS protection task - fully RTOS non-blocking, vTaskDelayUntil.
 *          [FA] تسک حفاظت کاملاً RTOS غیربلوکه.
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
 * @brief  [EN] Protection task - non-blocking.
 *         [FA] تسک حفاظت - غیربلوکه.
 * @param  void_ptr__argument [EN] FreeRTOS arg / آرگومان
 */
/* ==================== TaskProtection ==================== */

void func__TaskProtection(void *void_ptr__argument)
{
    TickType_t ticktype__lastWakeTick;
    TickType_t ticktype__periodTicks;

    (void)void_ptr__argument;

#if MODULE_PROTECTION
    ticktype__periodTicks = pdMS_TO_TICKS(APP_CONFIG.protection_period_ms);
#else
    ticktype__periodTicks = pdMS_TO_TICKS(1000u);
#endif

    ticktype__lastWakeTick = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&ticktype__lastWakeTick, ticktype__periodTicks);

#if MODULE_PROTECTION
        {
            measurement_snapshot_t measurement_snapshot_t__snap;
            measurement_snapshot_t__snap.valid = false;
#if MODULE_MEASUREMENT
            (void)func__Measurement_GetSnapshot(&measurement_snapshot_t__snap);
#endif
            func__Protection_Run(&measurement_snapshot_t__snap);
        }
#endif
    }
}