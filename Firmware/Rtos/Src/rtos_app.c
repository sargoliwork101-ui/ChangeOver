/**
 * @file    rtos_app.c
 * @brief   [EN] Creates statically allocated CMSIS-RTOS2 threads and starts the kernel.
 *          [FA] تسک‌های CMSIS-RTOS2 را با تخصیص استاتیک می‌سازد و کرنل را شروع می‌کند.
 *
 * @note    [EN] FreeRTOS remains the current CMSIS-RTOS2 backend, but application
 *          code uses the CMSIS-RTOS2 interface and supplies all thread memory.
 *          [FA] FreeRTOS در این مرحله Backend داخلی CMSIS-RTOS2 است، اما کد برنامه
 *          از رابط CMSIS-RTOS2 استفاده می‌کند و حافظهٔ همهٔ تسک‌ها را خودش می‌دهد.
 */

#include "rtos_app.h"
#include "rtos_tasks.h"
#include "rtos_config.h"
#include "modules_enable.h"

#include "cmsis_os2.h"
#include "rtos_backend_memory.h"

#if MODULE_FAULT
#include "fault.h"
#endif

static rtos_stack_word_t STACKTYPE_T__G__UiStack[TASK_STACK_UI];
static rtos_thread_control_block_t STATICTASK_T__G__UiTcb;
static const osThreadAttr_t OS_THREAD_ATTR_T__G__Ui =
{
    .name = "ui",
    .attr_bits = 0u,
    .cb_mem = &STATICTASK_T__G__UiTcb,
    .cb_size = sizeof(STATICTASK_T__G__UiTcb),
    .stack_mem = STACKTYPE_T__G__UiStack,
    .stack_size = sizeof(STACKTYPE_T__G__UiStack),
    .priority = TASK_PRIO_UI,
    .tz_module = 0u,
    .reserved = 0u
};

#if MODULE_MEASUREMENT
static rtos_stack_word_t STACKTYPE_T__G__MeasStack[TASK_STACK_MEASUREMENT];
static rtos_thread_control_block_t STATICTASK_T__G__MeasTcb;
static const osThreadAttr_t OS_THREAD_ATTR_T__G__Measurement =
{
    .name = "meas",
    .attr_bits = 0u,
    .cb_mem = &STATICTASK_T__G__MeasTcb,
    .cb_size = sizeof(STATICTASK_T__G__MeasTcb),
    .stack_mem = STACKTYPE_T__G__MeasStack,
    .stack_size = sizeof(STACKTYPE_T__G__MeasStack),
    .priority = TASK_PRIO_MEASUREMENT,
    .tz_module = 0u,
    .reserved = 0u
};
#endif

#if MODULE_PROTECTION
static rtos_stack_word_t STACKTYPE_T__G__ProtStack[TASK_STACK_PROTECTION];
static rtos_thread_control_block_t STATICTASK_T__G__ProtTcb;
static const osThreadAttr_t OS_THREAD_ATTR_T__G__Protection =
{
    .name = "prot",
    .attr_bits = 0u,
    .cb_mem = &STATICTASK_T__G__ProtTcb,
    .cb_size = sizeof(STATICTASK_T__G__ProtTcb),
    .stack_mem = STACKTYPE_T__G__ProtStack,
    .stack_size = sizeof(STACKTYPE_T__G__ProtStack),
    .priority = TASK_PRIO_PROTECTION,
    .tz_module = 0u,
    .reserved = 0u
};
#endif

#if (MODULE_CHANGEOVER || MODULE_CHARGER || MODULE_JITTER)
static rtos_stack_word_t STACKTYPE_T__G__CtrlStack[TASK_STACK_CONTROL];
static rtos_thread_control_block_t STATICTASK_T__G__CtrlTcb;
static const osThreadAttr_t OS_THREAD_ATTR_T__G__Control =
{
    .name = "ctrl",
    .attr_bits = 0u,
    .cb_mem = &STATICTASK_T__G__CtrlTcb,
    .cb_size = sizeof(STATICTASK_T__G__CtrlTcb),
    .stack_mem = STACKTYPE_T__G__CtrlStack,
    .stack_size = sizeof(STACKTYPE_T__G__CtrlStack),
    .priority = TASK_PRIO_CONTROL,
    .tz_module = 0u,
    .reserved = 0u
};
#endif

#if MODULE_ESP
static rtos_stack_word_t STACKTYPE_T__G__CommStack[TASK_STACK_COMM];
static rtos_thread_control_block_t STATICTASK_T__G__CommTcb;
static const osThreadAttr_t OS_THREAD_ATTR_T__G__Comm =
{
    .name = "comm",
    .attr_bits = 0u,
    .cb_mem = &STATICTASK_T__G__CommTcb,
    .cb_size = sizeof(STATICTASK_T__G__CommTcb),
    .stack_mem = STACKTYPE_T__G__CommStack,
    .stack_size = sizeof(STACKTYPE_T__G__CommStack),
    .priority = TASK_PRIO_COMM,
    .tz_module = 0u,
    .reserved = 0u
};
#endif

/**
 * @brief  [EN] Stop in a deterministic state if the kernel or a required thread cannot start.
 *         [FA] اگر کرنل یا یکی از تسک‌های لازم شروع نشد، در وضعیت مشخص متوقف می‌شود.
 */
static void func__Rtos_Fatal(void)
{
    for (;;)
    {
    }
}

/**
 * @brief  [EN] Create enabled CMSIS-RTOS2 threads with static memory, then start the kernel.
 *         [FA] تسک‌های روشن CMSIS-RTOS2 را با حافظهٔ ثابت می‌سازد و سپس کرنل را شروع می‌کند.
 */
void func__Rtos_Start(void)
{
    if (osKernelInitialize() != osOK)
    {
        func__Rtos_Fatal();
    }

#if MODULE_FAULT
    /* [EN] Initialize the enabled fault mask before any task can report or read it.
       [FA] ماسک خطای فعال را قبل از شروع تسک‌ها مقداردهی می‌کند. */
    func__Fault_Init();
#endif

#if MODULE_UI
    if (osThreadNew(func__TaskUi, NULL, &OS_THREAD_ATTR_T__G__Ui) == NULL)
    {
        func__Rtos_Fatal();
    }
#endif
#if MODULE_MEASUREMENT
    if (osThreadNew(func__TaskMeasurement, NULL, &OS_THREAD_ATTR_T__G__Measurement) == NULL)
    {
        func__Rtos_Fatal();
    }
#endif
#if MODULE_PROTECTION
    if (osThreadNew(func__TaskProtection, NULL, &OS_THREAD_ATTR_T__G__Protection) == NULL)
    {
        func__Rtos_Fatal();
    }
#endif
#if (MODULE_CHANGEOVER || MODULE_CHARGER || MODULE_JITTER)
    if (osThreadNew(func__TaskControl, NULL, &OS_THREAD_ATTR_T__G__Control) == NULL)
    {
        func__Rtos_Fatal();
    }
#endif
#if MODULE_ESP
    if (osThreadNew(func__TaskComm, NULL, &OS_THREAD_ATTR_T__G__Comm) == NULL)
    {
        func__Rtos_Fatal();
    }
#endif

    if (osKernelStart() != osOK)
    {
        func__Rtos_Fatal();
    }

    for (;;)
    {
    }
}
