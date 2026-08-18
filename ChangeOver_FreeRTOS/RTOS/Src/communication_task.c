/**
 * @file RTOS/Src/communication_task.c
 * @brief C source for communication_task.
 * @details This file belongs to the FreeRTOS task integration layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "communication_task.h"

#include "app_config.h"

/**
 * @brief Implements the module operation represented by this API.
 * @function communication_task_entry
 * @param context Input or state associated with the operation.
 * @safety This is a FreeRTOS task entry. It may block only through
 *         approved RTOS scheduling calls and must not return normally.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static void communication_task_entry(void * context)
{
    communication_task_t * task;
    TickType_t last_wake;
    measurement_snapshot_t measurement;
    system_state_t state;
    fault_mask_t faults;

    task = (communication_task_t *)context;
    if ((task != NULL) && (task->measurements != NULL) &&
        (task->faults != NULL) && (task->system != NULL) &&
        (task->esp != NULL))
    {
        esp8266_service_enable(task->esp, APP_CONFIG.esp_link_enabled);
        last_wake = xTaskGetTickCount();
        for (;;)
        {
            measurement = measurement_manager_get_snapshot(task->measurements);
            state = system_controller_get_state(task->system);
            faults = fault_manager_get(task->faults);
            (void)esp8266_service_send_telemetry(task->esp,
                                                  &measurement,
                                                  state,
                                                  faults);
            vTaskDelayUntil(
                &last_wake,
                pdMS_TO_TICKS(APP_CONFIG.communication_period_ms));
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Creates the statically allocated FreeRTOS object.
 * @function communication_task_create
 * @param task Input or state associated with the operation.
 * @param measurements Input or state associated with the operation.
 * @param faults Input or state associated with the operation.
 * @param system Input or state associated with the operation.
 * @param esp Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool communication_task_create(communication_task_t * task,
                               measurement_manager_t * measurements,
                               fault_manager_t * faults,
                               system_controller_t * system,
                               esp8266_service_t * esp)
{
    bool created;

    created = false;
    if ((task != NULL) && (measurements != NULL) &&
        (faults != NULL) && (system != NULL) && (esp != NULL))
    {
        task->measurements = measurements;
        task->faults = faults;
        task->system = system;
        task->esp = esp;
        task->handle = NULL;
        task->handle = xTaskCreateStatic(communication_task_entry,
                                         "communication",
                                         COMMUNICATION_TASK_STACK_WORDS,
                                         task,
                                         COMMUNICATION_TASK_PRIORITY,
                                         task->stack,
                                         &task->control_block);
        created = task->handle != NULL;
    }
    return created;
}
