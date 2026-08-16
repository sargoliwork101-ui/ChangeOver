/**
 * @file RTOS/Inc/communication_task.h
 * @brief C interface for communication_task.
 * @details This file belongs to the FreeRTOS task integration layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef COMMUNICATION_TASK_H
#define COMMUNICATION_TASK_H

#include "esp8266_service.h"
#include "fault_manager.h"
#include "measurement_manager.h"
#include "rtos_task_config.h"
#include "system_controller.h"
#include "FreeRTOS.h"
#include "task.h"

typedef struct
{
    measurement_manager_t * measurements;
    fault_manager_t * faults;
    system_controller_t * system;
    esp8266_service_t * esp;
    TaskHandle_t handle;
    StaticTask_t control_block;
    StackType_t stack[COMMUNICATION_TASK_STACK_WORDS];
} communication_task_t;

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
                               esp8266_service_t * esp);

#endif /* COMMUNICATION_TASK_H */
