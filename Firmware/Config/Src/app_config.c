#include "app_config.h"

const app_config_t APP_CONFIG =
{
    .power_stage_enabled    = false,
    .esp_link_enabled       = false,

    .ui_period_ms           = 100u,
    .control_period_ms      = 10u,
    .protection_period_ms   = 5u,
    .comm_period_ms         = 100u,

    .low_battery_mv         = 20000u,  /* PLACEHOLDER */
    .low_battery_recover_mv = 21000u,  /* PLACEHOLDER */
    .overcurrent1_ma        = 3500u,   /* PLACEHOLDER matches LM358 headroom ~3.5 A */
    .overcurrent2_ma        = 3500u,
    .pwm_max_duty_permille  = 0u       /* no pulse until you set this */
};
