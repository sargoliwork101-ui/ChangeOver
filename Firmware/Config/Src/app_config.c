/**
 * @file    app_config.c
 * @brief   [EN] Default configuration values.
 *          [FA] مقادیر پیش‌فرض پیکربندی.
 */

#include "app_config.h"

const app_config_t APP_CONFIG =
{
    .ui_input_ok_poll_ms     = 500u,
    .ui_selftest_led_ms      = 500u,
    .ui_boot_beep_ms         = 150u,
    .ui_blink_period_ms      = 1000u,
    .ui_green_min_off_ms     = 10u,
    .ui_low_battery_percent  = 20u,
    .ui_warn_period_ms       = 1000u,
    .ui_warn_yellow_on_ms    = 500u,
    .ui_warn_beep_ms         = 250u,
    .ui_warn_beep_period_ms  = 30000u,
    .power_stage_enabled    = false,
    .esp_link_enabled       = false,
    .control_period_ms      = 10u,
    .protection_period_ms   = 5u,
    .comm_period_ms         = 100u,
    .low_battery_mv         = 20000u,
    .low_battery_recover_mv = 21000u,
    .overcurrent1_ma        = 3500u,
    .overcurrent2_ma        = 3500u,
    .pwm_max_duty_permille  = 0u
};
