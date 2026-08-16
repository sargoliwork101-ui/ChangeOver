/**
 * @file Integration/Inc/firmware_app.h
 * @brief C interface for firmware_app.
 * @details This file belongs to the CubeMX and application integration layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef FIRMWARE_APP_H
#define FIRMWARE_APP_H

#include <stdbool.h>
#include <stdint.h>
#include "adc_driver.h"
#include "actuator_manager.h"
#include "battery_protection.h"
#include "charger_controller.h"
#include "changeover_controller.h"
#include "communication_task.h"
#include "diagnostics.h"
#include "control_task.h"
#include "esp8266_service.h"
#include "fault_manager.h"
#include "gpio_driver.h"
#include "user_interface.h"
#include "jitter_detector.h"
#include "measurement_manager.h"
#include "measurement_task.h"
#include "protection_task.h"
#include "pwm_driver.h"
#include "system_controller.h"
#include "uart_driver.h"
#include "ui_task.h"

typedef struct
{
    adc_driver_t adc;
    pwm_driver_t pwm1;
    pwm_driver_t pwm2;
    uart_driver_t uart;

    actuator_manager_t actuators;
    measurement_manager_t measurements;
    fault_manager_t faults;
    diagnostics_t diagnostics;
    jitter_detector_t jitter;
    changeover_controller_t changeover;
    charger_controller_t charger;
    user_interface_t user_interface;
    esp8266_service_t esp;
    system_controller_t system;

    measurement_task_t measurement_task;
    protection_task_t protection_task;
    control_task_t control_task;
    communication_task_t communication_task;
    ui_task_t ui_task;
} firmware_app_t;

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
                       UART_HandleTypeDef * uart);
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
void firmware_app_on_gpio_exti(firmware_app_t * app, uint16_t gpio_pin);

#endif /* FIRMWARE_APP_H */
