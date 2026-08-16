/**
 * @file App/Inc/app_types.h
 * @brief C interface for app_types.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SYSTEM_STATE_BOOT = 0,
    SYSTEM_STATE_SELF_TEST,
    SYSTEM_STATE_INPUT_SOURCE,
    SYSTEM_STATE_BATTERY_SOURCE,
    SYSTEM_STATE_CHARGING,
    SYSTEM_STATE_LOW_BATTERY,
    SYSTEM_STATE_OVER_CURRENT,
    SYSTEM_STATE_FAULT
} system_state_t;

typedef uint32_t fault_mask_t;

#define FAULT_NONE             (UINT32_C(0))
#define FAULT_LOW_BATTERY      (UINT32_C(1) << 0U)
#define FAULT_OVER_CURRENT     (UINT32_C(1) << 1U)
#define FAULT_ADC_OUT_OF_RANGE (UINT32_C(1) << 2U)
#define FAULT_INPUT_UNDERVOLT  (UINT32_C(1) << 3U)
#define FAULT_JITTER_LOST      (UINT32_C(1) << 4U)
#define FAULT_COMMUNICATION    (UINT32_C(1) << 5U)
#define FAULT_HARDWARE         (UINT32_C(1) << 6U)

#define FAULT_DYNAMIC_MASK (FAULT_LOW_BATTERY | FAULT_OVER_CURRENT | \
                            FAULT_ADC_OUT_OF_RANGE | FAULT_INPUT_UNDERVOLT | \
                            FAULT_JITTER_LOST)
#define FAULT_BLOCKING_MASK (FAULT_OVER_CURRENT | FAULT_ADC_OUT_OF_RANGE | \
                             FAULT_HARDWARE)

typedef struct
{
    uint32_t sequence;
    uint32_t raw_current1;
    uint32_t raw_input24v;
    uint32_t raw_battery24v;
    uint32_t raw_battery12v;
    uint32_t raw_current2;

    uint32_t current1_ma;
    uint32_t input24v_mv;
    uint32_t battery24v_mv;
    uint32_t battery12v_mv;
    uint32_t current2_ma;

    bool input_present;
    bool jitter1_active;
    bool jitter2_active;
} measurement_snapshot_t;

typedef enum
{
    JITTER_CHANNEL_1 = 0,
    JITTER_CHANNEL_2
} jitter_channel_t;

#endif /* APP_TYPES_H */
