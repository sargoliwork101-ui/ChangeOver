/**
 * @file RTOS/Inc/rtos_task_config.h
 * @brief C interface for rtos_task_config.
 * @details This file belongs to the FreeRTOS task integration layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef RTOS_TASK_CONFIG_H
#define RTOS_TASK_CONFIG_H

enum
{
    MEASUREMENT_TASK_STACK_WORDS = 256,
    PROTECTION_TASK_STACK_WORDS = 192,
    CONTROL_TASK_STACK_WORDS = 256,
    COMMUNICATION_TASK_STACK_WORDS = 256,
    UI_TASK_STACK_WORDS = 192,
    MEASUREMENT_TASK_PRIORITY = 5,
    PROTECTION_TASK_PRIORITY = 6,
    CONTROL_TASK_PRIORITY = 5,
    COMMUNICATION_TASK_PRIORITY = 3,
    UI_TASK_PRIORITY = 1
};

#endif /* RTOS_TASK_CONFIG_H */
