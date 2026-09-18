/**
 * @file    task_protection.c
 * @brief   [EN] CMSIS-RTOS2 protection thread - simple RTOS with osDelay.
 *          [FA] تسک حفاظت ساده RTOS.
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "cmsis_os2.h"
#include "rtos_time.h"


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
        func__Rtos_DelayMilliseconds(APP_CONFIG.protection_period_ms);
#else
        func__Rtos_DelayMilliseconds(1000u);
#endif
    }
}
