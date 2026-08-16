/**
 * @file App/Src/battery_protection.c
 * @brief C source for battery_protection.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "battery_protection.h"

#include "app_config.h"

/**
 * @brief Implements the module operation represented by this API.
 * @function battery_protection_evaluate
 * @param measurement Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
fault_mask_t battery_protection_evaluate(const measurement_snapshot_t * measurement)
{
    fault_mask_t faults;

    faults = FAULT_NONE;
    if (measurement != NULL)
    {
        if ((!measurement->input_present) &&
            (measurement->battery24v_mv > UINT32_C(100)) &&
            (measurement->battery24v_mv < APP_CONFIG.low_battery_24v_mv))
        {
            faults |= FAULT_LOW_BATTERY;
        }

        if ((measurement->current1_ma > APP_CONFIG.over_current_1_ma) ||
            (measurement->current2_ma > APP_CONFIG.over_current_2_ma))
        {
            faults |= FAULT_OVER_CURRENT;
        }
    }
    return faults;
}
