/**
 * @file App/Inc/charger_controller.h
 * @brief C interface for charger_controller.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef CHARGER_CONTROLLER_H
#define CHARGER_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>
#include "actuator_manager.h"
#include "app_types.h"

typedef struct
{
    actuator_manager_t * actuators;
    bool enabled;
    uint16_t duty1_permille;
    uint16_t duty2_permille;
} charger_controller_t;

/**
 * @brief Initializes the module state and its dependencies.
 * @function charger_controller_init
 * @param controller Input or state associated with the operation.
 * @param actuators Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void charger_controller_init(charger_controller_t * controller,
                             actuator_manager_t * actuators);
/**
 * @brief Implements the module operation represented by this API.
 * @function charger_controller_enable
 * @param controller Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void charger_controller_enable(charger_controller_t * controller);
/**
 * @brief Implements the module operation represented by this API.
 * @function charger_controller_disable
 * @param controller Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void charger_controller_disable(charger_controller_t * controller);
/**
 * @brief Updates the requested module output or state.
 * @function charger_controller_set_duty1
 * @param controller Input or state associated with the operation.
 * @param duty_permille Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void charger_controller_set_duty1(charger_controller_t * controller,
                                   uint16_t duty_permille);
/**
 * @brief Updates the requested module output or state.
 * @function charger_controller_set_duty2
 * @param controller Input or state associated with the operation.
 * @param duty_permille Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void charger_controller_set_duty2(charger_controller_t * controller,
                                   uint16_t duty_permille);
/**
 * @brief Executes one deterministic update cycle.
 * @function charger_controller_update
 * @param controller Input or state associated with the operation.
 * @param measurement Input or state associated with the operation.
 * @param faults Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void charger_controller_update(charger_controller_t * controller,
                               const measurement_snapshot_t * measurement,
                               fault_mask_t faults);
/**
 * @brief Reports the requested module condition.
 * @function charger_controller_is_enabled
 * @param controller Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool charger_controller_is_enabled(const charger_controller_t * controller);

#endif /* CHARGER_CONTROLLER_H */
