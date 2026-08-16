/**
 * @file BSP/Inc/uart_driver.h
 * @brief C interface for uart_driver.
 * @details This file belongs to the BSP hardware abstraction and HAL wrapper layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f1xx_hal.h"

typedef struct
{
    UART_HandleTypeDef * uart;
} uart_driver_t;

/**
 * @brief Initializes the module state and its dependencies.
 * @function uart_driver_init
 * @param driver Input or state associated with the operation.
 * @param uart Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void uart_driver_init(uart_driver_t * driver, UART_HandleTypeDef * uart);
/**
 * @brief Implements the module operation represented by this API.
 * @function uart_driver_write
 * @param driver Input or state associated with the operation.
 * @param data Input or state associated with the operation.
 * @param length Input or state associated with the operation.
 * @param timeout_ms Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool uart_driver_write(const uart_driver_t * driver,
                       const uint8_t * data,
                       uint16_t length,
                       uint32_t timeout_ms);
/**
 * @brief Updates the requested module output or state.
 * @function uart_driver_write_string
 * @param driver Input or state associated with the operation.
 * @param text Input or state associated with the operation.
 * @param timeout_ms Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool uart_driver_write_string(const uart_driver_t * driver,
                              const char * text,
                              uint32_t timeout_ms);

#endif /* UART_DRIVER_H */
