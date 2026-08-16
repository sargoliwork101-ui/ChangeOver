/**
 * @file BSP/Inc/adc_driver.h
 * @brief C interface for adc_driver.
 * @details This file belongs to the BSP hardware abstraction and HAL wrapper layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

typedef struct
{
    ADC_HandleTypeDef * adc;
    TaskHandle_t notification_task;
} adc_driver_t;

/**
 * @brief Initializes the module state and its dependencies.
 * @function adc_driver_init
 * @param driver Input or state associated with the operation.
 * @param adc Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void adc_driver_init(adc_driver_t * driver, ADC_HandleTypeDef * adc);
/**
 * @brief Starts or stops the requested hardware service.
 * @function adc_driver_start_scan_dma
 * @param driver Input or state associated with the operation.
 * @param buffer Input or state associated with the operation.
 * @param length Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool adc_driver_start_scan_dma(const adc_driver_t * driver,
                               uint32_t * buffer,
                               uint32_t length);
/**
 * @brief Starts or stops the requested hardware service.
 * @function adc_driver_stop_scan_dma
 * @param driver Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool adc_driver_stop_scan_dma(const adc_driver_t * driver);
/**
 * @brief Implements the module operation represented by this API.
 * @function adc_driver_attach_task
 * @param driver Input or state associated with the operation.
 * @param task Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void adc_driver_attach_task(adc_driver_t * driver, TaskHandle_t task);
/**
 * @brief Handles a hardware or RTOS event in callback/ISR context.
 * @function adc_driver_conversion_complete_callback
 * @param adc Input or state associated with the operation.
 * @safety ISR/callback rule: use only ISR-safe APIs, do not block,
 *         and keep the execution path deterministic.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void adc_driver_conversion_complete_callback(ADC_HandleTypeDef * adc);

#endif /* ADC_DRIVER_H */
