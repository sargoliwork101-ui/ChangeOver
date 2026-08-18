/**
 * @file Config/Src/app_config.c
 * @brief C source for app_config.
 * @details This file belongs to the centralized configuration and calibration data layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "app_config.h"

const app_config_t APP_CONFIG =
{
    .adc_max_code = UINT32_C(4095),
    .adc_channel_count = UINT32_C(5),
    .adc_reference_mv = UINT32_C(3300),

    .measurement_period_ms = UINT32_C(10),
    .protection_period_ms = UINT32_C(5),
    .control_period_ms = UINT32_C(10),
    .communication_period_ms = UINT32_C(100),
    .ui_period_ms = UINT32_C(100),
    .jitter_timeout_ms = UINT32_C(250),

    .low_battery_24v_mv = UINT32_C(20000),
    .low_battery_recovery_24v_mv = UINT32_C(21000),
    .over_current_1_ma = UINT32_C(10000),
    .over_current_2_ma = UINT32_C(10000),

    .pwm_off_permille = UINT16_C(0),
    .pwm_max_permille = UINT16_C(1000),

    .battery_switch_active_high = true,
    .charger_relay_active_high = true,
    .battery_protection_active_high = true,
    .led_active_high = true,
    .buzzer_active_high = true,
    .input_detect_active_high = true,
    .esp_chip_enable_active_high = true,

    // No unapproved power or ESP control is enabled in the baseline sample.
    .power_stage_enabled = false,
    .esp_link_enabled = false,
    .diagnostics_enabled = true,

    // External 68K/33K resistors from the block diagram are included.
    .input_24v_divider = { UINT32_C(69200), UINT32_C(6800) },
    .battery_24v_divider = { UINT32_C(69200), UINT32_C(6800) },
    .battery_12v_divider = { UINT32_C(34200), UINT32_C(6800) },

    // Current ADC node: 1K series and 10K to GND.
    .current_adc_divider_num = UINT32_C(11000),
    .current_adc_divider_den = UINT32_C(10000),
    .current_1_mv_per_amp = UINT32_C(1000),
    .current_2_mv_per_amp = UINT32_C(1000),
    .current_1_offset_mv = UINT32_C(0),
    .current_2_offset_mv = UINT32_C(0)
};
