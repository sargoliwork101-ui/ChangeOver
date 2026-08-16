/**
 * @file RTOS/Src/freertos_hooks.c
 * @brief Static memory and fault hooks required by FreeRTOS.
 * @details This file provides the kernel-owned idle task memory and the
 *          stack-overflow safety hook when dynamic allocation is disabled.
 * @safety No heap allocation is performed in this file.
 * @misra  FreeRTOS hook signatures are external contracts and are documented
 *         as a third-party boundary in docs/deviation-record.md.
 */

#include <stddef.h>
#include "app_entry.h"
#include "FreeRTOS.h"
#include "task.h"

static StaticTask_t idle_task_control_block;
static StackType_t idle_task_stack[configMINIMAL_STACK_SIZE];

/**
 * @brief Supplies statically allocated memory for the FreeRTOS idle task.
 * @function vApplicationGetIdleTaskMemory
 * @param idle_tcb Address returned to FreeRTOS for the idle task TCB.
 * @param idle_stack Address returned to FreeRTOS for the idle task stack.
 * @param idle_stack_size Number of elements in the supplied stack.
 * @safety This function is called by the kernel during scheduler startup and
 *         must not allocate memory or block.
 * @misra  The function signature is fixed by the FreeRTOS static-allocation
 *         contract and is treated as a documented third-party boundary.
 */
void vApplicationGetIdleTaskMemory(StaticTask_t ** idle_tcb,
                                   StackType_t ** idle_stack,
                                   uint32_t * idle_stack_size)
{
    if ((idle_tcb != NULL) && (idle_stack != NULL) &&
        (idle_stack_size != NULL))
    {
        *idle_tcb = &idle_task_control_block;
        *idle_stack = idle_task_stack;
        *idle_stack_size = (uint32_t)configMINIMAL_STACK_SIZE;
    }
}

/**
 * @brief Handles a FreeRTOS stack-overflow report.
 * @function vApplicationStackOverflowHook
 * @param task Task handle reported by the kernel.
 * @param task_name Task name reported by the kernel.
 * @safety This is a terminal safety path. It must disable further application
 *         activity and must not attempt to recover by continuing operation.
 * @misra  The intentional terminal loop is a safety deviation and must be
 *         reviewed together with the board-specific safe indication.
 */
void vApplicationStackOverflowHook(TaskHandle_t task,
                                   char * task_name)
{
    (void)task;
    (void)task_name;
    firmware_report_stack_overflow();

    for (;;)
    {
        /* Integrate the board-specific safe indication here. */
    }
}
