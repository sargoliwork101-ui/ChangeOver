#include "rtos_tasks.h"
#include "ui.h"

/* 1 = سناریو 1 ، 2 = سناریو 2 */
#define UI_FLAG  1u

void TaskUi(void *argument)
{
    (void)argument;

    Ui_BoardTest();

    if (UI_FLAG == 1u)
    {
        Ui_Scenario1();
    }
    else
    {
        Ui_Scenario2();
    }
}
