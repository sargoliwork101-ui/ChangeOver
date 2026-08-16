/**
 * @file BSP/Src/uart_driver.c
 * @brief C source for uart_driver.
 * @details This file belongs to the BSP hardware abstraction and HAL wrapper layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include "uart_driver.h"

#include <stddef.h>

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
void uart_driver_init(uart_driver_t * driver, UART_HandleTypeDef * uart)
{
    if (driver != NULL)
    {
        driver->uart = uart;
    }
}

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
                       uint32_t timeout_ms)
{
    bool written;

    written = false;
    if ((driver != NULL) && (driver->uart != NULL) &&
        (data != NULL) && (length > UINT16_C(0)))
    {
        written = (HAL_UART_Transmit(driver->uart,
                                     (uint8_t *)data,
                                     length,
                                     timeout_ms) == HAL_OK);
    }
    return written;
}

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
                              uint32_t timeout_ms)
{
    bool written;
    uint16_t length;
    const char * cursor;

    written = false;
    length = UINT16_C(0);
    cursor = text;
    if ((driver != NULL) && (text != NULL))
    {
        while ((length < UINT16_MAX) && (*cursor != '\0'))
        {
            cursor++;
            length++;
        }
        if ((length < UINT16_MAX) && (*cursor == '\0'))
        {
            written = uart_driver_write(driver,
                                        (const uint8_t *)text,
                                        length,
                                        timeout_ms);
        }
    }
    return written;
}
