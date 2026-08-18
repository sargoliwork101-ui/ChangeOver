/**
 * @file App/Src/system_controller.c
 * @brief C source for system_controller.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "system_controller.h"
#include "app_config.h"

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
                            charger_controller_t * charger)
{
    if (controller != NULL)
    {
        controller->measurements = measurements;
        controller->faults = faults;
        controller->changeover = changeover;
        controller->charger = charger;
        controller->state = SYSTEM_STATE_BOOT;
    }
}

/**
 * @brief Executes one deterministic update cycle.
 * @function system_controller_update
 * @param controller Input or state associated with the operation.
 * @safety The power-stage approval gate is checked before any Changeover or
 *         Charger action; the baseline configuration therefore remains safe.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void system_controller_update(system_controller_t * controller)
{
    measurement_snapshot_t measurement;
    fault_mask_t faults;

    if ((controller != NULL) &&
        (controller->measurements != NULL) &&
        (controller->faults != NULL) &&
        (controller->changeover != NULL) &&
        (controller->charger != NULL))
    {
        measurement = measurement_manager_get_snapshot(controller->measurements);
        faults = fault_manager_get(controller->faults);

        if (!APP_CONFIG.power_stage_enabled)
        {
            charger_controller_disable(controller->charger);
            actuator_manager_safe_off(controller->changeover->actuators);
            controller->state = SYSTEM_STATE_SELF_TEST;
        }
        else if (measurement.sequence == UINT32_C(0))
        {
            charger_controller_disable(controller->charger);
            controller->state = SYSTEM_STATE_SELF_TEST;
        }
        else
        {
            changeover_controller_update(controller->changeover,
                                         &measurement,
                                         faults);
            charger_controller_update(controller->charger,
                                      &measurement,
                                      faults);
            controller->state = changeover_controller_get_state(
                controller->changeover);
        }
    }
}

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
    const system_controller_t * controller)
{
    system_state_t state;

    state = SYSTEM_STATE_FAULT;
    if (controller != NULL)
    {
        state = controller->state;
    }
    return state;
}
