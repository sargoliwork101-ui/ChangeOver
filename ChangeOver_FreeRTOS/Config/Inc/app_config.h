/**
 * @file Config/Inc/app_config.h
 * @brief C interface for app_config.
 * @details This file belongs to the centralized configuration and calibration data layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t top_resistance_ohm;
    uint32_t bottom_resistance_ohm;
} voltage_divider_t;

typedef struct
{
    uint32_t adc_max_code;
    uint32_t adc_channel_count;
    uint32_t adc_reference_mv;

    uint32_t measurement_period_ms;
    uint32_t protection_period_ms;
    uint32_t control_period_ms;
    uint32_t communication_period_ms;
    uint32_t ui_period_ms;
    uint32_t jitter_timeout_ms;

    uint32_t low_battery_24v_mv;
    uint32_t low_battery_recovery_24v_mv;
    uint32_t over_current_1_ma;
    uint32_t over_current_2_ma;

    uint16_t pwm_off_permille;
    uint16_t pwm_max_permille;

    bool battery_switch_active_high;
    bool charger_relay_active_high;
    bool battery_protection_active_high;
    bool led_active_high;
    bool buzzer_active_high;
    bool input_detect_active_high;
    bool esp_chip_enable_active_high;

    /* Explicit approval gates; all are disabled in the baseline sample. */
    bool power_stage_enabled;
    bool esp_link_enabled;
    bool diagnostics_enabled;

    voltage_divider_t input_24v_divider;
    voltage_divider_t battery_24v_divider;
    voltage_divider_t battery_12v_divider;

    uint32_t current_adc_divider_num;
    uint32_t current_adc_divider_den;
    uint32_t current_1_mv_per_amp;
    uint32_t current_2_mv_per_amp;
    uint32_t current_1_offset_mv;
    uint32_t current_2_offset_mv;
} app_config_t;

extern const app_config_t APP_CONFIG;

#endif /* APP_CONFIG_H */
