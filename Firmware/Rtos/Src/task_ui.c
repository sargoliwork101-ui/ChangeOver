/**
 * @file task_ui.c
 * @brief این فایل فقط ساعت است، نه الگوی LED.
 *
 * هر 100 ms زنگ می‌زند: Ui_Run() را صدا کن، بعد بخواب.
 * این‌که سبز باشد یا قرمز، داخل ui.c است.
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#if MODULE_UI
#include "ui.h"
#endif

void TaskUi(void *argument)
{
    uint32_t delay_ms;

    (void)argument;

    for (;;)
    {
#if MODULE_UI
        Ui_Run();
#endif

        delay_ms = APP_CONFIG.ui_period_ms;
        if (delay_ms == 0u)
        {
            delay_ms = 1u;
        }

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}
