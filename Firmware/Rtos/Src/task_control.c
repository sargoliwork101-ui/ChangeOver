/**
 * @file    task_control.c
 * @brief   [EN] FreeRTOS control task - fully RTOS non-blocking, chunked, vTaskDelayUntil.
 *          [FA] تسک کنترل کاملاً RTOS غیربلوکه.
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "FreeRTOS.h"
#include "task.h"

#if MODULE_MEASUREMENT
#include "measurement.h"
#endif
#if MODULE_FAULT
#include "fault.h"
#endif
#if MODULE_CHANGEOVER
#include "changeover.h"
#endif
#if MODULE_CHARGER
#include "charger.h"
#endif
#if MODULE_JITTER
#include "jitter.h"
#endif

/**
 * @brief  [EN] Control task - non-blocking periodic.
 *         [FA] تسک کنترل - دوره‌ای غیربلوکه.
 * @param  void_ptr__argument [EN] FreeRTOS arg / آرگومان
 */
/* ==================== TaskControl ==================== */

void func__TaskControl(void *void_ptr__argument)
{
    TickType_t ticktype__lastWakeTick;
    TickType_t ticktype__periodTicks;

    (void)void_ptr__argument;

#if (MODULE_CHANGEOVER || MODULE_CHARGER || MODULE_JITTER)
    ticktype__periodTicks = pdMS_TO_TICKS(APP_CONFIG.control_period_ms);
#else
    ticktype__periodTicks = pdMS_TO_TICKS(1000u);
#endif

    ticktype__lastWakeTick = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&ticktype__lastWakeTick, ticktype__periodTicks);

#if (MODULE_CHANGEOVER || MODULE_CHARGER || MODULE_JITTER)
        {
            measurement_snapshot_t measurement_snapshot_t__snap;
            fault_mask_t fault_mask_t__faults = FAULT_NONE;
            app_state_t app_state_t__state = APP_STATE_IDLE;

            measurement_snapshot_t__snap.valid = false;
#if MODULE_MEASUREMENT
            (void)func__Measurement_GetSnapshot(&measurement_snapshot_t__snap);
#endif
#if MODULE_FAULT
            fault_mask_t__faults = func__Fault_Get();
#endif
#if MODULE_JITTER
            func__Jitter_Run();
#endif
#if MODULE_CHANGEOVER
            app_state_t__state = func__Changeover_Evaluate(&measurement_snapshot_t__snap, fault_mask_t__faults);
#endif
#if MODULE_CHARGER
            func__Charger_Evaluate(&measurement_snapshot_t__snap, app_state_t__state);
#endif
            (void)app_state_t__state;
        }
#endif
    }
}