/**
 * @file RTOS/Inc/control_task.h
 * @brief C interface for control_task.
 * @details This file belongs to the FreeRTOS task integration layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include <stdbool.h>
#include "actuator_manager.h"
#include "diagnostics.h"
#include "fault_manager.h"
#include "jitter_detector.h"
#include "measurement_manager.h"
#include "rtos_task_config.h"
#include "system_controller.h"
#include "FreeRTOS.h"
#include "task.h"

typedef struct
{
    jitter_detector_t * jitter;
    measurement_manager_t * measurements;
    actuator_manager_t * actuators;
    system_controller_t * system;
    fault_manager_t * faults;
    diagnostics_t * diagnostics;
    system_state_t last_state;
    bool last_input_present;
    bool last_jitter1_active;
    bool last_jitter2_active;
    TaskHandle_t handle;
    StaticTask_t control_block;
    StackType_t stack[CONTROL_TASK_STACK_WORDS];
} control_task_t;

/**
 * @brief Creates the statically allocated FreeRTOS object.
 * @function control_task_create
 * @param task Input or state associated with the operation.
 * @param jitter Input or state associated with the operation.
 * @param measurements Input or state associated with the operation.
 * @param actuators Input or state associated with the operation.
 * @param system System controller context.
 * @param faults Fault manager context.
 * @param diagnostics Diagnostic event context.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool control_task_create(control_task_t * task,
                         jitter_detector_t * jitter,
                         measurement_manager_t * measurements,
                         actuator_manager_t * actuators,
                         system_controller_t * system,
                         fault_manager_t * faults,
                         diagnostics_t * diagnostics);

#endif /* CONTROL_TASK_H */
