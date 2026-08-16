/**
 * @file App/Inc/changeover_controller.h
 * @brief C interface for changeover_controller.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef CHANGEOVER_CONTROLLER_H
#define CHANGEOVER_CONTROLLER_H

#include "actuator_manager.h"
#include "app_types.h"

typedef struct
{
    actuator_manager_t * actuators;
    system_state_t state;
} changeover_controller_t;

/**
 * @brief Initializes the module state and its dependencies.
 * @function changeover_controller_init
 * @param controller Input or state associated with the operation.
 * @param actuators Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void changeover_controller_init(changeover_controller_t * controller,
                                actuator_manager_t * actuators);
/**
 * @brief Executes one deterministic update cycle.
 * @function changeover_controller_update
 * @param controller Input or state associated with the operation.
 * @param measurement Input or state associated with the operation.
 * @param faults Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void changeover_controller_update(changeover_controller_t * controller,
                                  const measurement_snapshot_t * measurement,
                                  fault_mask_t faults);
/**
 * @brief Returns the requested module state.
 * @function changeover_controller_get_state
 * @param controller Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
system_state_t changeover_controller_get_state(
    const changeover_controller_t * controller);

#endif /* CHANGEOVER_CONTROLLER_H */
