/**
 * @file Integration/Src/firmware_app.c
 * @brief C source for firmware_app.
 * @details This file belongs to the CubeMX and application integration layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "firmware_app.h"

#include "board_pins.h"

/**
 * @brief Initializes the module state and its dependencies.
 * @function firmware_app_init
 * @param app Input or state associated with the operation.
 * @param adc Input or state associated with the operation.
 * @param pwm_timer1 Input or state associated with the operation.
 * @param pwm_timer2 Input or state associated with the operation.
 * @param uart Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool firmware_app_init(firmware_app_t * app,
                       ADC_HandleTypeDef * adc,
                       TIM_HandleTypeDef * pwm_timer1,
                       TIM_HandleTypeDef * pwm_timer2,
                       UART_HandleTypeDef * uart)
{
    bool result;

    result = false;
    if ((app != NULL) && (adc != NULL) &&
        (pwm_timer1 != NULL) && (pwm_timer2 != NULL) &&
        (uart != NULL))
    {
        adc_driver_init(&app->adc, adc);
        pwm_driver_init(&app->pwm1, pwm_timer1, TIM_CHANNEL_1);
        pwm_driver_init(&app->pwm2, pwm_timer2, TIM_CHANNEL_1);
        uart_driver_init(&app->uart, uart);

        actuator_manager_init(&app->actuators,
                              &app->pwm1,
                              &app->pwm2);
        measurement_manager_init(&app->measurements);
        fault_manager_init(&app->faults);
        diagnostics_init(&app->diagnostics);
        diagnostics_report(&app->diagnostics,
                           DIAG_BOOT_STARTED,
                           DIAG_SEVERITY_INFO,
                           UINT32_C(0),
                           FAULT_NONE,
                           SYSTEM_STATE_BOOT,
                           UINT32_C(0));
        jitter_detector_init(&app->jitter);
        changeover_controller_init(&app->changeover,
                                   &app->actuators);
        charger_controller_init(&app->charger,
                                &app->actuators);
        user_interface_init(&app->user_interface,
                            &app->actuators);
        esp8266_service_init(&app->esp,
                             &app->uart,
                             &app->diagnostics);
        system_controller_init(&app->system,
                               &app->measurements,
                               &app->faults,
                               &app->changeover,
                               &app->charger);
        actuator_manager_initialize_safe_state(&app->actuators);

        result = true;
        if (!measurement_task_create(&app->measurement_task,
                                     &app->adc,
                                     &app->measurements,
                                     &app->diagnostics))
        {
            result = false;
        }
        if (!protection_task_create(&app->protection_task,
                                    &app->measurements,
                                    &app->faults,
                                    &app->diagnostics))
        {
            result = false;
        }
        if (!control_task_create(&app->control_task,
                                 &app->jitter,
                                 &app->measurements,
                                 &app->actuators,
                                 &app->system,
                                 &app->faults,
                                 &app->diagnostics))
        {
            result = false;
        }
        if (!communication_task_create(&app->communication_task,
                                       &app->measurements,
                                       &app->faults,
                                       &app->system,
                                       &app->esp))
        {
            result = false;
        }
        if (!ui_task_create(&app->ui_task,
                            &app->system,
                            &app->faults,
                            &app->user_interface))
        {
            result = false;
        }
        if (!result)
        {
            actuator_manager_safe_off(&app->actuators);
            diagnostics_report(&app->diagnostics,
                               DIAG_INIT_FAILED,
                               DIAG_SEVERITY_FATAL,
                               UINT32_C(0),
                               FAULT_HARDWARE,
                               SYSTEM_STATE_FAULT,
                               UINT32_C(0));
        }
    }
    return result;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function firmware_app_on_gpio_exti
 * @param app Input or state associated with the operation.
 * @param gpio_pin Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void firmware_app_on_gpio_exti(firmware_app_t * app, uint16_t gpio_pin)
{
    if (app != NULL)
    {
        if (gpio_pin == BOARD_PIN_JITTER1.pin)
        {
            jitter_detector_notify_edge_from_isr(&app->jitter,
                                                 JITTER_CHANNEL_1);
        }
        else if (gpio_pin == BOARD_PIN_JITTER2.pin)
        {
            jitter_detector_notify_edge_from_isr(&app->jitter,
                                                 JITTER_CHANNEL_2);
        }
        else
        {
            // Input presence is sampled by ControlTask.
        }
    }
}
