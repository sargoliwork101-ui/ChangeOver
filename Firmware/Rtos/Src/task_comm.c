/**
 * @file    task_comm.c
 * @brief   [EN] FreeRTOS task for UART/ESP telemetry (placeholder). Full type naming, func_ prefix.
 *          [FA] تسک ارتباط UART/ESP (اسکلت، هنوز فعال نیست). نام تایپ کامل.
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "FreeRTOS.h"
#include "task.h"

#if MODULE_ESP
#include "esp_link.h"
#endif
#if MODULE_MEASUREMENT
#include "measurement.h"
#endif
#if MODULE_FAULT
#include "fault.h"
#endif

/**
 * @brief  [EN] Communication task entry. Idle loop until ESP is enabled.
 *         [FA] ورود تسک ارتباط. تا ESP روشن نشود کار نمی‌کند.
 * @param  void_ptr_argument [EN] Required by FreeRTOS, unused, type void* / آرگومان FreeRTOS
 */
void func_TaskComm(void *void_ptr_argument)
{
    (void)void_ptr_argument;

    for (;;)
    {
#if MODULE_ESP
        {
            measurement_snapshot_t measurement_snapshot_t_snap;
            fault_mask_t fault_mask_t_faults = FAULT_NONE;
            measurement_snapshot_t_snap.valid = false;
#if MODULE_MEASUREMENT
            (void)func_Measurement_GetSnapshot(&measurement_snapshot_t_snap);
#endif
#if MODULE_FAULT
            fault_mask_t_faults = func_Fault_Get();
#endif
            func_EspLink_Run(&measurement_snapshot_t_snap, APP_STATE_IDLE, fault_mask_t_faults);
        }
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.comm_period_ms));
#else
        vTaskDelay(pdMS_TO_TICKS(1000u));
#endif
    }
}

