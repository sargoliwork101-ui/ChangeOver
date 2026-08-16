/**
 * @file App/Src/actuator_manager.c
 * @brief C source for actuator_manager.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "actuator_manager.h"

#include "app_config.h"
#include "board_pins.h"
#include "gpio_driver.h"

/**
 * @brief Initializes the module state and its dependencies.
 * @function actuator_manager_init
 * @param manager Input or state associated with the operation.
 * @param pwm1 Input or state associated with the operation.
 * @param pwm2 Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_init(actuator_manager_t * manager,
                           pwm_driver_t * pwm1,
                           pwm_driver_t * pwm2)
{
    if (manager != NULL)
    {
        manager->pwm1 = pwm1;
        manager->pwm2 = pwm2;
        manager->initialized = true;
    }
}

/**
 * @brief Implements the module operation represented by this API.
 * @function actuator_manager_initialize_safe_state
 * @param manager Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_initialize_safe_state(actuator_manager_t * manager)
{
    if ((manager != NULL) && manager->initialized)
    {
        actuator_manager_safe_off(manager);
        actuator_manager_set_led_red(manager, false);
        actuator_manager_set_led_yellow(manager, false);
        actuator_manager_set_led_green(manager, false);
        actuator_manager_set_buzzer(manager, false);
        actuator_manager_set_esp_chip_enable(manager, false);
    }
}

/**
 * @brief Implements the module operation represented by this API.
 * @function actuator_manager_safe_off
 * @param manager Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_safe_off(actuator_manager_t * manager)
{
    if ((manager != NULL) && manager->initialized)
    {
        actuator_manager_set_pwm1(manager, APP_CONFIG.pwm_off_permille);
        actuator_manager_set_pwm2(manager, APP_CONFIG.pwm_off_permille);
        actuator_manager_stop_pwm(manager);
        actuator_manager_set_battery_switch(manager, false);
        actuator_manager_set_charger_relay(manager, false);
        actuator_manager_set_battery_protection(manager, false);
    }
}

/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_battery_switch
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_battery_switch(const actuator_manager_t * manager,
                                         bool enabled)
{
    if ((manager != NULL) && manager->initialized)
    {
        gpio_driver_write_logical(&BOARD_PIN_BATTERY_SWITCH,
                                  enabled,
                                  APP_CONFIG.battery_switch_active_high);
    }
}

/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_charger_relay
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_charger_relay(const actuator_manager_t * manager,
                                        bool enabled)
{
    if ((manager != NULL) && manager->initialized)
    {
        gpio_driver_write_logical(&BOARD_PIN_CHARGER_RELAY,
                                  enabled,
                                  APP_CONFIG.charger_relay_active_high);
    }
}

/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_battery_protection
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_battery_protection(const actuator_manager_t * manager,
                                             bool enabled)
{
    if ((manager != NULL) && manager->initialized)
    {
        gpio_driver_write_logical(&BOARD_PIN_BATTERY_PROTECTION,
                                  enabled,
                                  APP_CONFIG.battery_protection_active_high);
    }
}

/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_esp_chip_enable
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_esp_chip_enable(const actuator_manager_t * manager,
                                          bool enabled)
{
    if ((manager != NULL) && manager->initialized)
    {
        gpio_driver_write_logical(&BOARD_PIN_ESP_CHIP_ENABLE,
                                  enabled,
                                  APP_CONFIG.esp_chip_enable_active_high);
    }
}

/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_pwm1
 * @param manager Input or state associated with the operation.
 * @param duty_permille Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_pwm1(const actuator_manager_t * manager,
                               uint16_t duty_permille)
{
    if ((manager != NULL) && manager->initialized &&
        (manager->pwm1 != NULL))
    {
        (void)pwm_driver_set_duty_permille(manager->pwm1, duty_permille);
    }
}

/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_pwm2
 * @param manager Input or state associated with the operation.
 * @param duty_permille Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_pwm2(const actuator_manager_t * manager,
                               uint16_t duty_permille)
{
    if ((manager != NULL) && manager->initialized &&
        (manager->pwm2 != NULL))
    {
        (void)pwm_driver_set_duty_permille(manager->pwm2, duty_permille);
    }
}

/**
 * @brief Starts or stops the requested hardware service.
 * @function actuator_manager_start_pwm
 * @param manager Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_start_pwm(const actuator_manager_t * manager)
{
    if ((manager != NULL) && manager->initialized)
    {
        if (manager->pwm1 != NULL)
        {
            (void)pwm_driver_start(manager->pwm1);
        }
        if (manager->pwm2 != NULL)
        {
            (void)pwm_driver_start(manager->pwm2);
        }
    }
}

/**
 * @brief Starts or stops the requested hardware service.
 * @function actuator_manager_stop_pwm
 * @param manager Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_stop_pwm(const actuator_manager_t * manager)
{
    if ((manager != NULL) && manager->initialized)
    {
        if (manager->pwm1 != NULL)
        {
            (void)pwm_driver_stop(manager->pwm1);
        }
        if (manager->pwm2 != NULL)
        {
            (void)pwm_driver_stop(manager->pwm2);
        }
    }
}

/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_led_red
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_led_red(const actuator_manager_t * manager,
                                  bool enabled)
{
    if ((manager != NULL) && manager->initialized)
    {
        gpio_driver_write_logical(&BOARD_PIN_LED_RED,
                                  enabled,
                                  APP_CONFIG.led_active_high);
    }
}

/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_led_yellow
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_led_yellow(const actuator_manager_t * manager,
                                     bool enabled)
{
    if ((manager != NULL) && manager->initialized)
    {
        gpio_driver_write_logical(&BOARD_PIN_LED_YELLOW,
                                  enabled,
                                  APP_CONFIG.led_active_high);
    }
}

/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_led_green
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_led_green(const actuator_manager_t * manager,
                                    bool enabled)
{
    if ((manager != NULL) && manager->initialized)
    {
        gpio_driver_write_logical(&BOARD_PIN_LED_GREEN,
                                  enabled,
                                  APP_CONFIG.led_active_high);
    }
}

/**
 * @brief Updates the requested module output or state.
 * @function actuator_manager_set_buzzer
 * @param manager Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void actuator_manager_set_buzzer(const actuator_manager_t * manager,
                                 bool enabled)
{
    if ((manager != NULL) && manager->initialized)
    {
        gpio_driver_write_logical(&BOARD_PIN_BUZZER,
                                  enabled,
                                  APP_CONFIG.buzzer_active_high);
    }
}

/**
 * @brief Implements the module operation represented by this API.
 * @function actuator_manager_input_present
 * @param manager Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool actuator_manager_input_present(const actuator_manager_t * manager)
{
    bool present;

    present = false;
    if ((manager != NULL) && manager->initialized)
    {
        present = gpio_driver_read_logical(&BOARD_PIN_INPUT_DETECT,
                                           APP_CONFIG.input_detect_active_high);
    }
    return present;
}
