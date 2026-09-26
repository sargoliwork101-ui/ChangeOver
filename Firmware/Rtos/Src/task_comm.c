/**
 * @file    task_comm.c
 * @brief   [EN] CMSIS-RTOS2 communication thread - simple RTOS with osDelay.
 *          [FA] تسک ارتباط ساده RTOS.
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "cmsis_os2.h"
#include "rtos_time.h"


#if MODULE_ESP
#include "esp_link.h"
#endif
#if MODULE_MEASUREMENT
#include "measurement.h"
#endif
#if MODULE_FAULT
#include "fault.h"
#endif

/* ==================== Task Comm ==================== */

void func__TaskComm(void *void_ptr__argument)
{
    (void)void_ptr__argument;

#if MODULE_ESP
    /* [EN] The comm thread owns the link lifecycle: bring the UART backend,
       parser and ESP power up once before the periodic loop (ESP panel,
       user order 2026-09-22).
       [FA] تسک ارتباط مالک چرخهٔ حیات لینک است: قبل از حلقهٔ دوره‌ای،
       backend ی UART و پارسر و تغذیهٔ ESP را یک‌بار بالا می‌آورد (پنل
       ESP، دستور کاربر ۲۰۲۶-۰۹-۲۲). */
    func__EspLink_Init();
#endif

    for (;;)
    {
#if MODULE_ESP
        {
            measurement_snapshot_t measurement_snapshot_t__snap;
            fault_mask_t fault_mask_t__faults = FAULT_NONE;
            measurement_snapshot_t__snap.valid = false;
#if MODULE_MEASUREMENT
            (void)func__Measurement_GetSnapshot(&measurement_snapshot_t__snap);
#endif
#if MODULE_FAULT
            fault_mask_t__faults = func__Fault_Get();
#endif
            func__EspLink_Run(&measurement_snapshot_t__snap, fault_mask_t__faults);
        }
        func__Rtos_DelayMilliseconds(APP_CONFIG.comm_period_ms);
#else
        func__Rtos_DelayMilliseconds(1000u);
#endif
    }
}
