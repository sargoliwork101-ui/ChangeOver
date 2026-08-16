/**
 * @file App/Inc/system_controller.h
 * @brief C interface for system_controller.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef SYSTEM_CONTROLLER_H
#define SYSTEM_CONTROLLER_H

#include "app_types.h"
#include "charger_controller.h"
#include "changeover_controller.h"
#include "fault_manager.h"
#include "measurement_manager.h"

typedef struct
{
    measurement_manager_t * measurements;
    fault_manager_t * faults;
    changeover_controller_t * changeover;
    charger_controller_t * charger;
    system_state_t state;
} system_controller_t;

/**
 * @brief Initializes the module state and its dependencies.
 * @function system_controller_init
 * @param controller Input or state associated with the operation.
 * @param measurements Input or state associated with the operation.
 * @param faults Input or state associated with the operation.
 * @param changeover Input or state associated with the operation.
 * @param charger Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void system_controller_init(system_controller_t * controller,
                            measurement_manager_t * measurements,
                            fault_manager_t * faults,
                            changeover_controller_t * changeover,
                            charger_controller_t * charger);
/**
 * @brief Executes one deterministic update cycle.
 * @function system_controller_update
 * @param controller Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void system_controller_update(system_controller_t * controller);
/**
 * @brief Returns the requested module state.
 * @function system_controller_get_state
 * @param controller Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
system_state_t system_controller_get_state(
    const system_controller_t * controller);

#endif /* SYSTEM_CONTROLLER_H */
