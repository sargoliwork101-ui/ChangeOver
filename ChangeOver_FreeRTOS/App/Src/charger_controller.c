/**
 * @file App/Src/charger_controller.c
 * @brief C source for charger_controller.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "charger_controller.h"

#include "app_config.h"

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
                             actuator_manager_t * actuators)
{
    if (controller != NULL)
    {
        controller->actuators = actuators;
        controller->enabled = false;
        controller->duty1_permille = APP_CONFIG.pwm_off_permille;
        controller->duty2_permille = APP_CONFIG.pwm_off_permille;
    }
}

/**
 * @brief Implements the module operation represented by this API.
 * @function charger_controller_enable
 * @param controller Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void charger_controller_enable(charger_controller_t * controller)
{
    if (controller != NULL)
    {
        controller->enabled = true;
    }
}

/**
 * @brief Implements the module operation represented by this API.
 * @function charger_controller_disable
 * @param controller Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void charger_controller_disable(charger_controller_t * controller)
{
    if (controller != NULL)
    {
        controller->enabled = false;
        if (controller->actuators != NULL)
        {
            actuator_manager_set_pwm1(controller->actuators,
                                       APP_CONFIG.pwm_off_permille);
            actuator_manager_set_pwm2(controller->actuators,
                                       APP_CONFIG.pwm_off_permille);
            actuator_manager_set_charger_relay(controller->actuators, false);
        }
    }
}

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
                                   uint16_t duty_permille)
{
    if (controller != NULL)
    {
        controller->duty1_permille = duty_permille;
        if (controller->duty1_permille > APP_CONFIG.pwm_max_permille)
        {
            controller->duty1_permille = APP_CONFIG.pwm_max_permille;
        }
    }
}

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
                                   uint16_t duty_permille)
{
    if (controller != NULL)
    {
        controller->duty2_permille = duty_permille;
        if (controller->duty2_permille > APP_CONFIG.pwm_max_permille)
        {
            controller->duty2_permille = APP_CONFIG.pwm_max_permille;
        }
    }
}

/**
 * @brief Executes one deterministic update cycle.
 * @function charger_controller_update
 * @param controller Input or state associated with the operation.
 * @param measurement Input or state associated with the operation.
 * @param faults Input or state associated with the operation.
 * @safety The current limits are rechecked locally so a delayed Fault update
 *         cannot leave the PWM stage enabled during a protection transition.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void charger_controller_update(charger_controller_t * controller,
                               const measurement_snapshot_t * measurement,
                               fault_mask_t faults)
{
    bool direct_over_current;

    direct_over_current = false;
    if ((controller != NULL) && (measurement != NULL))
    {
        direct_over_current =
            (measurement->current1_ma > APP_CONFIG.over_current_1_ma) ||
            (measurement->current2_ma > APP_CONFIG.over_current_2_ma);
        if ((!controller->enabled) || (!measurement->input_present) ||
            ((faults & FAULT_BLOCKING_MASK) != FAULT_NONE) ||
            direct_over_current)
        {
            charger_controller_disable(controller);
        }
        else if (controller->actuators != NULL)
        {
            // CC/CV control is intentionally not enabled in the skeleton.
            // Keep these values at zero until the power stage is validated.
            actuator_manager_start_pwm(controller->actuators);
            actuator_manager_set_charger_relay(controller->actuators, true);
            actuator_manager_set_pwm1(controller->actuators,
                                       controller->duty1_permille);
            actuator_manager_set_pwm2(controller->actuators,
                                       controller->duty2_permille);
        }
    }
}

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
bool charger_controller_is_enabled(const charger_controller_t * controller)
{
    bool enabled;

    enabled = false;
    if (controller != NULL)
    {
        enabled = controller->enabled;
    }
    return enabled;
}
