/**
 * @file RTOS/Inc/measurement_task.h
 * @brief C interface for measurement_task.
 * @details This file belongs to the FreeRTOS task integration layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef MEASUREMENT_TASK_H
#define MEASUREMENT_TASK_H

#include <stdint.h>
#include "adc_driver.h"
#include "diagnostics.h"
#include "app_config.h"
#include "measurement_manager.h"
#include "rtos_task_config.h"
#include "FreeRTOS.h"
#include "task.h"

typedef struct
{
    adc_driver_t * adc;
    measurement_manager_t * measurements;
    diagnostics_t * diagnostics;
    TaskHandle_t handle;
    StaticTask_t control_block;
    StackType_t stack[MEASUREMENT_TASK_STACK_WORDS];
    uint32_t dma_buffer[5U];
} measurement_task_t;

/**
 * @brief Creates the statically allocated FreeRTOS object.
 * @function measurement_task_create
 * @param task Input or state associated with the operation.
 * @param adc Input or state associated with the operation.
 * @param measurements Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool measurement_task_create(measurement_task_t * task,
                             adc_driver_t * adc,
                             measurement_manager_t * measurements,
                             diagnostics_t * diagnostics);

#endif /* MEASUREMENT_TASK_H */
