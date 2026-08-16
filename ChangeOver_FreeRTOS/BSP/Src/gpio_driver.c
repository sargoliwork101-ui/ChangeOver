/**
 * @file BSP/Src/gpio_driver.c
 * @brief C source for gpio_driver.
 * @details This file belongs to the BSP hardware abstraction and HAL wrapper layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "gpio_driver.h"

/**
 * @brief Updates the requested module output or state.
 * @function gpio_driver_write_logical
 * @param pin Input or state associated with the operation.
 * @param enabled Input or state associated with the operation.
 * @param active_high Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void gpio_driver_write_logical(const board_pin_t * pin,
                               bool enabled,
                               bool active_high)
{
    bool physical_high;

    if (pin != NULL)
    {
        physical_high = (active_high == enabled);
        gpio_driver_write_physical(pin, physical_high);
    }
}

/**
 * @brief Updates the requested module output or state.
 * @function gpio_driver_write_physical
 * @param pin Input or state associated with the operation.
 * @param high Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void gpio_driver_write_physical(const board_pin_t * pin, bool high)
{
    GPIO_PinState state;

    if (pin != NULL)
    {
        state = high ? GPIO_PIN_SET : GPIO_PIN_RESET;
        (void)HAL_GPIO_WritePin(pin->port, pin->pin, state);
    }
}

/**
 * @brief Returns the requested module state.
 * @function gpio_driver_read_logical
 * @param pin Input or state associated with the operation.
 * @param active_high Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool gpio_driver_read_logical(const board_pin_t * pin, bool active_high)
{
    bool enabled;
    bool physical_high;

    enabled = false;
    if (pin != NULL)
    {
        physical_high = gpio_driver_read_physical(pin);
        enabled = (active_high == physical_high);
    }
    return enabled;
}

/**
 * @brief Returns the requested module state.
 * @function gpio_driver_read_physical
 * @param pin Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool gpio_driver_read_physical(const board_pin_t * pin)
{
    bool high;

    high = false;
    if (pin != NULL)
    {
        high = (HAL_GPIO_ReadPin(pin->port, pin->pin) == GPIO_PIN_SET);
    }
    return high;
}
