/**
 * @file App/Src/fault_manager.c
 * @brief C source for fault_manager.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "fault_manager.h"

/**
 * @brief Initializes the module state and its dependencies.
 * @function fault_manager_init
 * @param manager Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void fault_manager_init(fault_manager_t * manager)
{
    if (manager != NULL)
    {
        manager->event_group = xEventGroupCreateStatic(&manager->storage);
    }
}

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
                                   fault_mask_t faults)
{
    if ((manager != NULL) && (manager->event_group != NULL))
    {
        (void)xEventGroupClearBits(manager->event_group, FAULT_DYNAMIC_MASK);
        (void)xEventGroupSetBits(manager->event_group,
                                 faults & FAULT_DYNAMIC_MASK);
    }
}

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
void fault_manager_latch(fault_manager_t * manager, fault_mask_t faults)
{
    if ((manager != NULL) && (manager->event_group != NULL))
    {
        (void)xEventGroupSetBits(manager->event_group, faults);
    }
}

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
                                 fault_mask_t faults)
{
    if ((manager != NULL) && (manager->event_group != NULL))
    {
        (void)xEventGroupClearBits(manager->event_group, faults);
    }
}

/**
 * @brief Implements the module operation represented by this API.
 * @function fault_manager_clear_all
 * @param manager Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void fault_manager_clear_all(fault_manager_t * manager)
{
    if ((manager != NULL) && (manager->event_group != NULL))
    {
        (void)xEventGroupClearBits(manager->event_group, UINT32_MAX);
    }
}

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
fault_mask_t fault_manager_get(const fault_manager_t * manager)
{
    fault_mask_t faults;

    faults = FAULT_NONE;
    if ((manager != NULL) && (manager->event_group != NULL))
    {
        faults = (fault_mask_t)xEventGroupGetBits(manager->event_group);
    }
    return faults;
}

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
bool fault_manager_has(const fault_manager_t * manager, fault_mask_t fault)
{
    return (fault_manager_get(manager) & fault) != FAULT_NONE;
}
