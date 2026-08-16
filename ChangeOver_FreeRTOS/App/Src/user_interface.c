/**
 * @file App/Src/user_interface.c
 * @brief C source for user_interface.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "user_interface.h"

/**
 * @brief Reports the requested module condition.
 * @function user_interface_is_critical_fault
 * @param faults Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static bool user_interface_is_critical_fault(fault_mask_t faults)
{
    return (faults & (FAULT_OVER_CURRENT | FAULT_HARDWARE |
                      FAULT_ADC_OUT_OF_RANGE)) != FAULT_NONE;
}

/**
 * @brief Reports the requested module condition.
 * @function user_interface_is_low_battery
 * @param faults Input or state associated with the operation.
 * @param state Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static bool user_interface_is_low_battery(fault_mask_t faults,
                                          system_state_t state)
{
    return ((faults & FAULT_LOW_BATTERY) != FAULT_NONE) ||
           (state == SYSTEM_STATE_LOW_BATTERY);
}

/**
 * @brief Reports the requested module condition.
 * @function user_interface_is_active_source
 * @param state Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static bool user_interface_is_active_source(system_state_t state)
{
    return (state == SYSTEM_STATE_INPUT_SOURCE) ||
           (state == SYSTEM_STATE_BATTERY_SOURCE) ||
           (state == SYSTEM_STATE_CHARGING);
}

/**
 * @brief Implements the module operation represented by this API.
 * @function user_interface_should_beep
 * @param faults Input or state associated with the operation.
 * @param state Input or state associated with the operation.
 * @param counter Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static bool user_interface_should_beep(fault_mask_t faults,
                                       system_state_t state,
                                       uint32_t counter)
{
    bool beep;
    uint32_t phase;

    beep = false;
    phase = counter % UINT32_C(20);
    if (user_interface_is_critical_fault(faults))
    {
        // Two update periods on, then eighteen periods off.
        beep = phase < UINT32_C(2);
    }
    else if (user_interface_is_low_battery(faults, state))
    {
        // One short notification approximately every two seconds at 100 ms.
        beep = phase == UINT32_C(0);
    }
    return beep;
}

/**
 * @brief Initializes the module state and its dependencies.
 * @function user_interface_init
 * @param interface Input or state associated with the operation.
 * @param actuators Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void user_interface_init(user_interface_t * interface,
                         actuator_manager_t * actuators)
{
    if (interface != NULL)
    {
        interface->actuators = actuators;
        interface->update_counter = UINT32_C(0);
        interface->initialized = true;
    }
}

/**
 * @brief Executes one deterministic update cycle.
 * @function user_interface_update
 * @param interface Input or state associated with the operation.
 * @param state Input or state associated with the operation.
 * @param faults Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void user_interface_update(user_interface_t * interface,
                           system_state_t state,
                           fault_mask_t faults)
{
    bool low_battery;
    bool source_active;
    bool green;
    bool yellow;
    bool red;
    bool buzzer;

    if ((interface != NULL) && interface->initialized &&
        (interface->actuators != NULL))
    {
        low_battery = user_interface_is_low_battery(faults, state);
        source_active = user_interface_is_active_source(state);

        red = faults != FAULT_NONE;
        yellow = low_battery || (state == SYSTEM_STATE_CHARGING);
        green = (!red) && source_active;
        buzzer = user_interface_should_beep(faults,
                                             state,
                                             interface->update_counter);

        actuator_manager_set_led_red(interface->actuators, red);
        actuator_manager_set_led_yellow(interface->actuators, yellow);
        actuator_manager_set_led_green(interface->actuators, green);
        actuator_manager_set_buzzer(interface->actuators, buzzer);
        interface->update_counter++;
    }
}
