#include "rtos_app.h"
#include "rtos_tasks.h"
#include "rtos_config.h"

#include "FreeRTOS.h"
#include "task.h"

static StackType_t s_ui_stack[TASK_STACK_UI];
static StaticTask_t s_ui_tcb;

void Rtos_Start(void)
{
    (void)xTaskCreateStatic(TaskUi,
                            "ui",
                            TASK_STACK_UI,
                            0,
                            TASK_PRIO_UI,
                            s_ui_stack,
                            &s_ui_tcb);

    vTaskStartScheduler();

    for (;;)
    {
    }
}
