/**
 * @file App/Inc/measurement_manager.h
 * @brief C interface for measurement_manager.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef MEASUREMENT_MANAGER_H
#define MEASUREMENT_MANAGER_H

#include <stdint.h>
#include "app_types.h"
#include "FreeRTOS.h"
#include "semphr.h"

typedef struct
{
    StaticSemaphore_t mutex_storage;
    SemaphoreHandle_t mutex;
    measurement_snapshot_t snapshot;
} measurement_manager_t;

/**
 * @brief Initializes the module state and its dependencies.
 * @function measurement_manager_init
 * @param manager Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void measurement_manager_init(measurement_manager_t * manager);
/**
 * @brief Implements the module operation represented by this API.
 * @function measurement_manager_update_from_dma
 * @param manager Input or state associated with the operation.
 * @param raw Input or state associated with the operation.
 * @param count Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void measurement_manager_update_from_dma(measurement_manager_t * manager,
                                         const uint32_t * raw,
                                         uint32_t count);
/**
 * @brief Updates the requested module output or state.
 * @function measurement_manager_set_digital_inputs
 * @param manager Input or state associated with the operation.
 * @param input_present Input or state associated with the operation.
 * @param jitter1_active Input or state associated with the operation.
 * @param jitter2_active Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void measurement_manager_set_digital_inputs(measurement_manager_t * manager,
                                            bool input_present,
                                            bool jitter1_active,
                                            bool jitter2_active);
/**
 * @brief Updates the requested module output or state.
 * @function measurement_manager_set_input_present
 * @param manager Input or state associated with the operation.
 * @param input_present Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void measurement_manager_set_input_present(measurement_manager_t * manager,
                                           bool input_present);
/**
 * @brief Returns the requested module state.
 * @function measurement_manager_get_snapshot
 * @param manager Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
measurement_snapshot_t measurement_manager_get_snapshot(
    const measurement_manager_t * manager);

#endif /* MEASUREMENT_MANAGER_H */
