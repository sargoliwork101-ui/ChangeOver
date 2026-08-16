/**
 * @file App/Inc/esp8266_service.h
 * @brief C interface for esp8266_service.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef ESP8266_SERVICE_H
#define ESP8266_SERVICE_H

#include <stdbool.h>
#include "app_types.h"
#include "diagnostics.h"
#include "uart_driver.h"

typedef struct
{
    uart_driver_t * uart;
    diagnostics_t * diagnostics;
    bool enabled;
} esp8266_service_t;

/**
 * @brief Initializes the module state and its dependencies.
 * @function esp8266_service_init
 * @param service Input or state associated with the operation.
 * @param uart UART driver used for the transport.
 * @param diagnostics Diagnostic context used for D-lines.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void esp8266_service_init(esp8266_service_t * service,
                          uart_driver_t * uart,
                          diagnostics_t * diagnostics);
/**
 * @brief Implements the module operation represented by this API.
 * @function esp8266_service_enable
 * @param service Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void esp8266_service_enable(esp8266_service_t * service, bool enabled);
/**
 * @brief Implements the module operation represented by this API.
 * @function esp8266_service_send_telemetry
 * @param service Input or state associated with the operation.
 * @param measurement Input or state associated with the operation.
 * @param state Input or state associated with the operation.
 * @param faults Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool esp8266_service_send_telemetry(const esp8266_service_t * service,
                                    const measurement_snapshot_t * measurement,
                                    system_state_t state,
                                    fault_mask_t faults);

#endif /* ESP8266_SERVICE_H */
