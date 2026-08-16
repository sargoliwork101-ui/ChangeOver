/**
 * @file App/Inc/actuator_manager.h
 * @brief C interface for actuator_manager.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef ACTUATOR_MANAGER_H
#define ACTUATOR_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "pwm_driver.h"

typedef struct
{
    pwm_driver_t * pwm1;
    pwm_driver_t * pwm2;
    bool initialized;
} actuator_manager_t;

/**
 * @brief Initializes the module state and its dependencies.
 * @function actuator_manager_init
 * @param manager Input or state associated with the operation.
 * @param pwm1 Input or state associated with the operation.
 * @param pwm2 Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_init(actuator_manager_t * manager,
                           pwm_driver_t * pwm1,
                           pwm_driver_t * pwm2);
/**
 * @brief Implements the module operation represented by this API.
 * @function actuator_manager_initialize_safe_state
 * @param manager Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_initialize_safe_state(actuator_manager_t * manager);
/**
 * @brief Implements the module operation represented by this API.
 * @function actuator_manager_safe_off
 * @param manager Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_safe_off(actuator_manager_t * manager);
/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_battery_switch
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_battery_switch(const actuator_manager_t * manager,
                                         bool enabled);
/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_charger_relay
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_charger_relay(const actuator_manager_t * manager,
                                        bool enabled);
/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_battery_protection
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_battery_protection(const actuator_manager_t * manager,
                                             bool enabled);
/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_esp_chip_enable
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_esp_chip_enable(const actuator_manager_t * manager,
                                          bool enabled);
/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_pwm1
 * @param manager Input or state associated with the operation.
 * @param duty_permille Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_pwm1(const actuator_manager_t * manager,
                               uint16_t duty_permille);
/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_pwm2
 * @param manager Input or state associated with the operation.
 * @param duty_permille Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_pwm2(const actuator_manager_t * manager,
                               uint16_t duty_permille);
/**
 * @brief Starts or stops the requested hardware service.
 * @function actuator_manager_start_pwm
 * @param manager Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_start_pwm(const actuator_manager_t * manager);
/**
 * @brief Starts or stops the requested hardware service.
 * @function actuator_manager_stop_pwm
 * @param manager Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_stop_pwm(const actuator_manager_t * manager);
/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_led_red
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_led_red(const actuator_manager_t * manager,
                                  bool enabled);
/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_led_yellow
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_led_yellow(const actuator_manager_t * manager,
                                     bool enabled);
/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_led_green
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_led_green(const actuator_manager_t * manager,
                                    bool enabled);
/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_buzzer
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_buzzer(const actuator_manager_t * manager,
                                 bool enabled);
/**
 * @brief Implements the module operation represented by this API.
 * @function actuator_manager_input_present
 * @param manager Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool actuator_manager_input_present(const actuator_manager_t * manager);

#endif /* ACTUATOR_MANAGER_H */
