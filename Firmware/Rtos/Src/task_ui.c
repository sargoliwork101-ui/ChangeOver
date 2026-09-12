#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "FreeRTOS.h"
#include "task.h"

#if MODULE_UI
#include "ui.h"
#endif
#if MODULE_FAULT
#include "fault.h"
#endif

void TaskUi(void *argument)
{
    (void)argument;

    for (;;)
    {
#if MODULE_UI
        {
            fault_mask_t faults = FAULT_NONE;
#if MODULE_FAULT
            faults = Fault_Get();
#endif
            Ui_Show(APP_STATE_IDLE, faults);
            Ui_Run();
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_period_ms));
    }
}
