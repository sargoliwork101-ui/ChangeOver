/**
 * @file BSP/Inc/pwm_driver.h
 * @brief C interface for pwm_driver.
 * @details This file belongs to the BSP hardware abstraction and HAL wrapper layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef PWM_DRIVER_H
#define PWM_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f1xx_hal.h"

typedef struct
{
    TIM_HandleTypeDef * timer;
    uint32_t channel;
} pwm_driver_t;

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
                     uint32_t channel);
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
bool pwm_driver_start(const pwm_driver_t * driver);
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
bool pwm_driver_stop(const pwm_driver_t * driver);
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
                                  uint16_t permille);
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
                            uint32_t compare);

#endif /* PWM_DRIVER_H */
