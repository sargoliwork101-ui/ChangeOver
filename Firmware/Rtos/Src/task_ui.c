/**
 * فایل FreeRTOS — الگوی LED این‌جا نیست.
 *
 * فقط: یک خط الگو را بزن، هر چقدر گفت بخواب.
 */

#include "rtos_tasks.h"
#include "ui.h"
#include "FreeRTOS.h"
#include "task.h"

void TaskUi(void *argument)
{
    uint32_t sleep_ms;

    (void)argument;

    for (;;)
    {
        sleep_ms = Ui_Run();
        vTaskDelay(pdMS_TO_TICKS(sleep_ms));
    }
}
