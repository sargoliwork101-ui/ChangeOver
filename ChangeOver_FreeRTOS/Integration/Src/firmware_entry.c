/**
 * @file Integration/Src/firmware_entry.c
 * @brief C source for firmware_entry.
 * @details This file belongs to the CubeMX and application integration layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "app_entry.h"
#include "firmware_app.h"

#include "stm32f1xx_hal.h"

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart1;

static firmware_app_t firmware_app;
static bool firmware_initialized = false;

/**
 * @brief Initializes the module state and its dependencies.
 * @function firmware_init
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void firmware_init(void)
{
    firmware_initialized = firmware_app_init(&firmware_app,
                                             &hadc1,
                                             &htim2,
                                             &htim3,
                                             &huart1);
    (void)firmware_initialized;
}

/**
 * @brief Reports whether application initialization succeeded.
 * @function firmware_is_ready
 * @return true when all required static objects and tasks were created.
 * @safety Read-only status query; no hardware or RTOS state is modified.
 * @misra  The main integration layer shall check this status before starting
 *         the scheduler.
 */
bool firmware_is_ready(void)
{
    return firmware_initialized;
}

/**
 * @brief Reports a kernel stack overflow through the diagnostic context.
 * @function firmware_report_stack_overflow
 * @safety This function is called from a terminal kernel hook. It must avoid
 *         recovery actions and only record the event when the context exists.
 * @misra  The hook is intentionally minimal and is followed by terminal Safe
 *         State in `vApplicationStackOverflowHook`.
 */
void firmware_report_stack_overflow(void)
{
    diagnostics_report_emergency(&firmware_app.diagnostics,
                                 DIAG_STACK_OVERFLOW,
                       DIAG_SEVERITY_FATAL,
                       UINT32_C(0),
                       FAULT_HARDWARE,
                       SYSTEM_STATE_FAULT,
                       UINT32_C(0));
}

/**
 * @brief Handles a hardware or RTOS event in callback/ISR context.
 * @function HAL_GPIO_EXTI_Callback
 * @param GPIO_Pin Input or state associated with the operation.
 * @safety ISR/callback rule: use only ISR-safe APIs, do not block,
 *         and keep the execution path deterministic.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (firmware_initialized)
    {
        firmware_app_on_gpio_exti(&firmware_app, GPIO_Pin);
    }
}
