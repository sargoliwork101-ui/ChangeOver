/**
 * @file    rtos_app.c
 * @brief   [EN] Creates FreeRTOS tasks with static allocation and starts the scheduler. Full type naming, func__ prefix.
 *          [FA] تسک‌های FreeRTOS را با تخصیص استاتیک می‌سازد و scheduler را شروع می‌کند. نام تایپ کامل.
 *
 * @note    [EN] xTaskCreateStatic does not use malloc. Idle task RAM is in freertos_hooks.c.
 *          [FA] از malloc استفاده نمی‌شود. RAM تسک Idle در freertos_hooks.c است.
 */

#include "rtos_app.h"
#include "rtos_tasks.h"
#include "rtos_config.h"
#include "modules_enable.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stddef.h>

static StackType_t STACKTYPE_T__G__UiStack[TASK_STACK_UI];
static StaticTask_t STATICTASK_T__G__UiTcb;

#if MODULE_MEASUREMENT
static StackType_t STACKTYPE_T__G__MeasStack[TASK_STACK_MEASUREMENT];
static StaticTask_t STATICTASK_T__G__MeasTcb;
#endif

#if MODULE_PROTECTION
static StackType_t STACKTYPE_T__G__ProtStack[TASK_STACK_PROTECTION];
static StaticTask_t STATICTASK_T__G__ProtTcb;
#endif

#if (MODULE_CHANGEOVER || MODULE_CHARGER || MODULE_JITTER)
static StackType_t STACKTYPE_T__G__CtrlStack[TASK_STACK_CONTROL];
static StaticTask_t STATICTASK_T__G__CtrlTcb;
#endif

#if MODULE_ESP
static StackType_t STACKTYPE_T__G__CommStack[TASK_STACK_COMM];
static StaticTask_t STATICTASK_T__G__CommTcb;
#endif

/**
 * @brief  [EN] Create enabled tasks, then start the scheduler.
 *         [FA] تسک‌های روشن را بساز، بعد زمان‌بند را شروع کن.
 */
void func__Rtos_Start(void)
{
#if MODULE_UI
    (void)xTaskCreateStatic(func__TaskUi, "ui", TASK_STACK_UI, NULL,
                            TASK_PRIO_UI, STACKTYPE_T__G__UiStack, &STATICTASK_T__G__UiTcb);
#endif
#if MODULE_MEASUREMENT
    (void)xTaskCreateStatic(func__TaskMeasurement, "meas", TASK_STACK_MEASUREMENT, NULL,
                            TASK_PRIO_MEASUREMENT, STACKTYPE_T__G__MeasStack, &STATICTASK_T__G__MeasTcb);
#endif
#if MODULE_PROTECTION
    (void)xTaskCreateStatic(func__TaskProtection, "prot", TASK_STACK_PROTECTION, NULL,
                            TASK_PRIO_PROTECTION, STACKTYPE_T__G__ProtStack, &STATICTASK_T__G__ProtTcb);
#endif
#if (MODULE_CHANGEOVER || MODULE_CHARGER || MODULE_JITTER)
    (void)xTaskCreateStatic(func__TaskControl, "ctrl", TASK_STACK_CONTROL, NULL,
                            TASK_PRIO_CONTROL, STACKTYPE_T__G__CtrlStack, &STATICTASK_T__G__CtrlTcb);
#endif
#if MODULE_ESP
    (void)xTaskCreateStatic(func__TaskComm, "comm", TASK_STACK_COMM, NULL,
                            TASK_PRIO_COMM, STACKTYPE_T__G__CommStack, &STATICTASK_T__G__CommTcb);
#endif

    vTaskStartScheduler();

    for (;;)
    {
    }
}

