#include "rtos_app.h"
#include "rtos_tasks.h"
#include "rtos_config.h"
#include "modules_enable.h"

#include "FreeRTOS.h"
#include "task.h"

static StackType_t s_ui_stack[TASK_STACK_UI];
static StaticTask_t s_ui_tcb;

#if MODULE_MEASUREMENT
static StackType_t s_meas_stack[TASK_STACK_MEASUREMENT];
static StaticTask_t s_meas_tcb;
#endif

#if MODULE_PROTECTION
static StackType_t s_prot_stack[TASK_STACK_PROTECTION];
static StaticTask_t s_prot_tcb;
#endif

#if (MODULE_CHANGEOVER || MODULE_CHARGER || MODULE_JITTER)
static StackType_t s_ctrl_stack[TASK_STACK_CONTROL];
static StaticTask_t s_ctrl_tcb;
#endif

#if MODULE_ESP
static StackType_t s_comm_stack[TASK_STACK_COMM];
static StaticTask_t s_comm_tcb;
#endif

void Rtos_Start(void)
{
#if MODULE_UI
    (void)xTaskCreateStatic(TaskUi, "ui", TASK_STACK_UI, 0,
                            TASK_PRIO_UI, s_ui_stack, &s_ui_tcb);
#endif
#if MODULE_MEASUREMENT
    (void)xTaskCreateStatic(TaskMeasurement, "meas", TASK_STACK_MEASUREMENT, 0,
                            TASK_PRIO_MEASUREMENT, s_meas_stack, &s_meas_tcb);
#endif
#if MODULE_PROTECTION
    (void)xTaskCreateStatic(TaskProtection, "prot", TASK_STACK_PROTECTION, 0,
                            TASK_PRIO_PROTECTION, s_prot_stack, &s_prot_tcb);
#endif
#if (MODULE_CHANGEOVER || MODULE_CHARGER || MODULE_JITTER)
    (void)xTaskCreateStatic(TaskControl, "ctrl", TASK_STACK_CONTROL, 0,
                            TASK_PRIO_CONTROL, s_ctrl_stack, &s_ctrl_tcb);
#endif
#if MODULE_ESP
    (void)xTaskCreateStatic(TaskComm, "comm", TASK_STACK_COMM, 0,
                            TASK_PRIO_COMM, s_comm_stack, &s_comm_tcb);
#endif

    vTaskStartScheduler();

    for (;;)
    {
        /* scheduler returned: config error */
    }
}
