/**
 * @file Core/Inc/app_entry.h
 * @brief C interface for app_entry.
 * @details This file belongs to the CubeMX core integration contract layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef APP_ENTRY_H
#define APP_ENTRY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the module state and its dependencies.
 * @function firmware_init
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void firmware_init(void);

/**
 * @brief Reports whether all required static RTOS objects were created.
 * @function firmware_is_ready
 * @return true when firmware_init completed successfully.
 * @safety This function is read-only and does not access hardware.
 * @misra  The status is exposed explicitly so main.c can refuse to start
 *         the scheduler after an initialization failure.
 */
bool firmware_is_ready(void);

/**
 * @brief Reports a FreeRTOS stack overflow to the diagnostic service.
 * @function firmware_report_stack_overflow
 * @safety May be called from the kernel fault hook; it must not block.
 * @misra  The hook path records the event before entering terminal Safe State.
 */
void firmware_report_stack_overflow(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_ENTRY_H */
