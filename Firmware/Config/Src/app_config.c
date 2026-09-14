/**
 * @file    app_config.c
 * @brief   [EN] Default configuration values. Uses ui_config.h as single source for UI thresholds to avoid duplication.
 *          [FA] مقادیر پیش‌فرض پیکربندی. برای جلوگیری از تکرار، آستانه‌های UI از ui_config.h می‌آید.
 */

#include "app_config.h"
#include "ui_config.h"  /* [EN] Single source for UI min/max/timings / منبع واحد مین/ماکس و تایم‌های UI */

const app_config_t APP_CONFIG =
{
    .ui_input_ok_poll_ms           = UI_INPUT_OK_POLL_MS,
    .ui_selftest_led_ms            = UI_SELFTEST_LED_MS,
    .ui_boot_beep_ms               = UI_BOOT_BEEP_MS,
    .ui_blink_period_ms            = UI_BLINK_PERIOD_MS,
    .ui_green_min_off_ms           = UI_GREEN_MIN_OFF_MS,
    .ui_low_battery_percent        = 20u,  /* deprecated */
    .ui_warn_period_ms             = 1000u, /* deprecated */
    .ui_warn_yellow_on_ms          = 500u,  /* deprecated */
    .ui_warn_beep_ms               = UI_BEEP_BASE_MS,
    .ui_warn_beep_period_ms        = 30000u, /* deprecated */
    .ui_input_threshold_mv         = UI_INPUT_THRESHOLD_MV,
    .ui_bat_v_min_mv               = UI_BAT_V_MIN_MV,
    .ui_bat_v_max_mv               = UI_BAT_V_MAX_MV,
    .ui_charging_blink_period_ms   = UI_CHARGING_BLINK_PERIOD_MS,
    .ui_charging_yellow_min_off_ms = UI_CHARGING_YELLOW_MIN_OFF_MS,
    .ui_beep_base_ms               = UI_BEEP_BASE_MS,
    .ui_beep_double_thresh_pct     = UI_BEEP_DOUBLE_THRESH_PCT,
    .ui_beep_start_pct             = UI_BEEP_START_PCT,
    .power_stage_enabled           = false,
    .esp_link_enabled              = false,
    .control_period_ms             = 10u,
    .protection_period_ms          = 5u,
    .comm_period_ms                = 100u,
    .low_battery_mv                = 20000u,
    .low_battery_recover_mv        = 21000u,
    .overcurrent1_ma               = 3500u,
    .overcurrent2_ma               = 3500u,
    .pwm_max_duty_permille         = 0u
};
