/**
 * @file App/Inc/user_interface.h
 * @brief C interface for user_interface.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>
#include "actuator_manager.h"
#include "app_types.h"

typedef struct
{
    actuator_manager_t * actuators;
    uint32_t update_counter;
    bool initialized;
} user_interface_t;

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
                         actuator_manager_t * actuators);
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
                           fault_mask_t faults);

#endif /* USER_INTERFACE_H */
