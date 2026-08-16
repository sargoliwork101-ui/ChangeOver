/**
 * @file App/Inc/fault_manager.h
 * @brief C interface for fault_manager.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include <stdbool.h>
#include "app_types.h"
#include "FreeRTOS.h"
#include "event_groups.h"

typedef struct
{
    StaticEventGroup_t storage;
    EventGroupHandle_t event_group;
} fault_manager_t;

/**
 * @brief Initializes the module state and its dependencies.
 * @function fault_manager_init
 * @param manager Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void fault_manager_init(fault_manager_t * manager);
/**
 * @brief Implements the module operation represented by this API.
 * @function fault_manager_replace_dynamic
 * @param manager Input or state associated with the operation.
 * @param faults Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void fault_manager_replace_dynamic(fault_manager_t * manager,
                                   fault_mask_t faults);
/**
 * @brief Implements the module operation represented by this API.
 * @function fault_manager_latch
 * @param manager Input or state associated with the operation.
 * @param faults Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void fault_manager_latch(fault_manager_t * manager, fault_mask_t faults);
/**
 * @brief Implements the module operation represented by this API.
 * @function fault_manager_clear_latched
 * @param manager Input or state associated with the operation.
 * @param faults Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void fault_manager_clear_latched(fault_manager_t * manager,
                                 fault_mask_t faults);
/**
 * @brief Implements the module operation represented by this API.
 * @function fault_manager_clear_all
 * @param manager Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void fault_manager_clear_all(fault_manager_t * manager);
/**
 * @brief Implements the module operation represented by this API.
 * @function fault_manager_get
 * @param manager Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
fault_mask_t fault_manager_get(const fault_manager_t * manager);
/**
 * @brief Implements the module operation represented by this API.
 * @function fault_manager_has
 * @param manager Input or state associated with the operation.
 * @param fault Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool fault_manager_has(const fault_manager_t * manager, fault_mask_t fault);

#endif /* FAULT_MANAGER_H */
