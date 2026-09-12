#include "rtos_tasks.h"
#include "ui.h"
#include "FreeRTOS.h"
#include "task.h"

void TaskUi(void *argument)
{
    (void)argument;

    for (;;)
    {
        Ui_Scenario1();
        /* برای سناریو 2 این خط را بگذار: Ui_Scenario2(); */
    }
}
