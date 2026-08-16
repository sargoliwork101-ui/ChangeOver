/**
 * @file App/Src/changeover_controller.c
 * @brief C source for changeover_controller.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "changeover_controller.h"
#include "app_config.h"

/**
 * @brief Implements the module operation represented by this API.
 * @function changeover_controller_apply_input
 * @param controller Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static void changeover_controller_apply_input(changeover_controller_t * controller)
{
    if ((controller != NULL) && (controller->actuators != NULL))
    {
        controller->state = SYSTEM_STATE_INPUT_SOURCE;
        actuator_manager_set_battery_switch(controller->actuators, false);
        actuator_manager_set_battery_protection(controller->actuators, false);
    }
}

/**
 * @brief Implements the module operation represented by this API.
 * @function changeover_controller_apply_battery
 * @param controller Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static void changeover_controller_apply_battery(changeover_controller_t * controller)
{
    if ((controller != NULL) && (controller->actuators != NULL))
    {
        controller->state = SYSTEM_STATE_BATTERY_SOURCE;
        actuator_manager_set_battery_protection(controller->actuators, false);
        actuator_manager_set_battery_switch(controller->actuators, true);
    }
}

/**
 * @brief Implements the module operation represented by this API.
 * @function changeover_controller_apply_low_battery
 * @param controller Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static void changeover_controller_apply_low_battery(
    changeover_controller_t * controller)
{
    if ((controller != NULL) && (controller->actuators != NULL))
    {
        controller->state = SYSTEM_STATE_LOW_BATTERY;
        actuator_manager_set_battery_switch(controller->actuators, false);
        actuator_manager_set_battery_protection(controller->actuators, true);
    }
}

/**
 * @brief Implements the module operation represented by this API.
 * @function changeover_controller_apply_fault
 * @param controller Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static void changeover_controller_apply_fault(changeover_controller_t * controller)
{
    if ((controller != NULL) && (controller->actuators != NULL))
    {
        controller->state = SYSTEM_STATE_FAULT;
        actuator_manager_safe_off(controller->actuators);
    }
}

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
                                actuator_manager_t * actuators)
{
    if (controller != NULL)
    {
        controller->actuators = actuators;
        controller->state = SYSTEM_STATE_SELF_TEST;
        if (actuators != NULL)
        {
            actuator_manager_set_battery_switch(actuators, false);
            actuator_manager_set_battery_protection(actuators, false);
        }
    }
}

/**
 * @brief Executes one deterministic update cycle.
 * @function changeover_controller_update
 * @param controller Input or state associated with the operation.
 * @param measurement Input or state associated with the operation.
 * @param faults Input or state associated with the operation.
 * @safety The controller independently rechecks immediate Low Battery and
 *         Over Current conditions so a delayed ProtectionTask cannot enable
 *         a power path during its first scheduling interval.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void changeover_controller_update(changeover_controller_t * controller,
                                  const measurement_snapshot_t * measurement,
                                  fault_mask_t faults)
{
    bool direct_low_battery;
    bool direct_over_current;

    direct_low_battery = false;
    direct_over_current = false;
    if ((controller != NULL) && (measurement != NULL))
    {
        direct_low_battery = (!measurement->input_present) &&
                             (measurement->battery24v_mv > UINT32_C(100)) &&
                             (measurement->battery24v_mv <
                              APP_CONFIG.low_battery_24v_mv);
        direct_over_current =
            (measurement->current1_ma > APP_CONFIG.over_current_1_ma) ||
            (measurement->current2_ma > APP_CONFIG.over_current_2_ma);

        if (((faults & FAULT_BLOCKING_MASK) != FAULT_NONE) ||
            direct_over_current)
        {
            changeover_controller_apply_fault(controller);
        }
        else if (((faults & FAULT_LOW_BATTERY) != FAULT_NONE) ||
                 direct_low_battery)
        {
            changeover_controller_apply_low_battery(controller);
        }
        else if (measurement->input_present)
        {
            // Placeholder policy: prefer external 24V input.
            changeover_controller_apply_input(controller);
        }
        else
        {
            // Placeholder policy: use battery if input is absent.
            changeover_controller_apply_battery(controller);
        }
    }
}

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
    const changeover_controller_t * controller)
{
    system_state_t state;

    state = SYSTEM_STATE_FAULT;
    if (controller != NULL)
    {
        state = controller->state;
    }
    return state;
}
