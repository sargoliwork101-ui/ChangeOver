/**
 * @file RTOS/Src/ui_task.c
 * @brief C source for ui_task.
 * @details This file belongs to the FreeRTOS task integration layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "ui_task.h"

#include "app_config.h"

/**
 * @brief Implements the module operation represented by this API.
 * @function ui_task_entry
 * @param context Input or state associated with the operation.
 * @safety This is a FreeRTOS task entry. It may block only through
 *         approved RTOS scheduling calls and must not return normally.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static void ui_task_entry(void * context)
{
    ui_task_t * task;
    TickType_t last_wake;
    system_state_t state;
    fault_mask_t faults;

    task = (ui_task_t *)context;
    if ((task != NULL) && (task->system != NULL) &&
        (task->faults != NULL) && (task->user_interface != NULL))
    {
        last_wake = xTaskGetTickCount();
        for (;;)
        {
            state = system_controller_get_state(task->system);
            faults = fault_manager_get(task->faults);
            user_interface_update(task->user_interface, state, faults);
            vTaskDelayUntil(&last_wake,
                            pdMS_TO_TICKS(APP_CONFIG.ui_period_ms));
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Creates the statically allocated FreeRTOS object.
 * @function ui_task_create
 * @param task Input or state associated with the operation.
 * @param system Input or state associated with the operation.
 * @param faults Input or state associated with the operation.
 * @param user_interface Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool ui_task_create(ui_task_t * task,
                    system_controller_t * system,
                    fault_manager_t * faults,
                    user_interface_t * user_interface)
{
    bool created;

    created = false;
    if ((task != NULL) && (system != NULL) &&
        (faults != NULL) && (user_interface != NULL))
    {
        task->system = system;
        task->faults = faults;
        task->user_interface = user_interface;
        task->handle = NULL;
        task->handle = xTaskCreateStatic(ui_task_entry,
                                         "ui",
                                         UI_TASK_STACK_WORDS,
                                         task,
                                         UI_TASK_PRIORITY,
                                         task->stack,
                                         &task->control_block);
        created = task->handle != NULL;
    }
    return created;
}
