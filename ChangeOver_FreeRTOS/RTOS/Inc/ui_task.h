/**
 * @file RTOS/Inc/ui_task.h
 * @brief C interface for ui_task.
 * @details This file belongs to the FreeRTOS task integration layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef UI_TASK_H
#define UI_TASK_H

#include "fault_manager.h"
#include "rtos_task_config.h"
#include "system_controller.h"
#include "user_interface.h"
#include "FreeRTOS.h"
#include "task.h"

typedef struct
{
    system_controller_t * system;
    fault_manager_t * faults;
    user_interface_t * user_interface;
    TaskHandle_t handle;
    StaticTask_t control_block;
    StackType_t stack[UI_TASK_STACK_WORDS];
} ui_task_t;

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
                    user_interface_t * user_interface);

#endif /* UI_TASK_H */
