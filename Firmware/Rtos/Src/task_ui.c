#include "rtos_tasks.h"
#include "ui.h"

void TaskUi(void *argument)
{
    (void)argument;

    /* یکی را باز کن، آن یکی را ببند. */
    Ui_Scenario1();
    /* Ui_Scenario2(); */
}
