/**
 * @file BSP/Src/pwm_driver.c
 * @brief C source for pwm_driver.
 * @details This file belongs to the BSP hardware abstraction and HAL wrapper layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "pwm_driver.h"

/**
 * @brief Initializes the module state and its dependencies.
 * @function pwm_driver_init
 * @param driver Input or state associated with the operation.
 * @param timer Input or state associated with the operation.
 * @param channel Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void pwm_driver_init(pwm_driver_t * driver,
                     TIM_HandleTypeDef * timer,
                     uint32_t channel)
{
    if (driver != NULL)
    {
        driver->timer = timer;
        driver->channel = channel;
    }
}

/**
 * @brief Starts or stops the requested hardware service.
 * @function pwm_driver_start
 * @param driver Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool pwm_driver_start(const pwm_driver_t * driver)
{
    bool started;

    started = false;
    if ((driver != NULL) && (driver->timer != NULL))
    {
        started = (HAL_TIM_PWM_Start(driver->timer, driver->channel) == HAL_OK);
    }
    return started;
}

/**
 * @brief Starts or stops the requested hardware service.
 * @function pwm_driver_stop
 * @param driver Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool pwm_driver_stop(const pwm_driver_t * driver)
{
    bool stopped;

    stopped = false;
    if ((driver != NULL) && (driver->timer != NULL))
    {
        stopped = (HAL_TIM_PWM_Stop(driver->timer, driver->channel) == HAL_OK);
    }
    return stopped;
}

/**
 * @brief Updates the requested module output or state.
 * @function pwm_driver_set_duty_permille
 * @param driver Input or state associated with the operation.
 * @param permille Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool pwm_driver_set_duty_permille(const pwm_driver_t * driver,
                                  uint16_t permille)
{
    bool result;
    uint32_t period;
    uint32_t compare;
    uint64_t full_scale;

    result = false;
    if ((driver != NULL) && (driver->timer != NULL))
    {
        if (permille > UINT16_C(1000))
        {
            permille = UINT16_C(1000);
        }

        period = __HAL_TIM_GET_AUTORELOAD(driver->timer);
        full_scale = (uint64_t)period + UINT64_C(1);
        compare = (uint32_t)((full_scale * (uint64_t)permille) /
                             UINT64_C(1000));
        if (compare > period)
        {
            compare = period;
        }
        result = pwm_driver_set_compare(driver, compare);
    }
    return result;
}

/**
 * @brief Updates the requested module output or state.
 * @function pwm_driver_set_compare
 * @param driver Input or state associated with the operation.
 * @param compare Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool pwm_driver_set_compare(const pwm_driver_t * driver,
                            uint32_t compare)
{
    bool result;
    uint32_t period;

    result = false;
    if ((driver != NULL) && (driver->timer != NULL))
    {
        period = __HAL_TIM_GET_AUTORELOAD(driver->timer);
        if (compare <= period)
        {
            __HAL_TIM_SET_COMPARE(driver->timer, driver->channel, compare);
            result = true;
        }
    }
    return result;
}
