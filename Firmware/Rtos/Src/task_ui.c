#include "rtos_tasks.h"
#include "ui.h"
#include "FreeRTOS.h"
#include "task.h"

void TaskUi(void *argument)
{
    (void)argument;

    for (;;)
    {
        Ui_Run();
        vTaskDelay(pdMS_TO_TICKS(500u));
    }
}
