#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/**
 * All tunables live here. No magic numbers in modules.
 * Values marked PLACEHOLDER are not approved for power testing.
 */
typedef struct
{
    bool     power_stage_enabled;   /* must stay false until step 8 */
    bool     esp_link_enabled;

    uint32_t ui_period_ms;
    uint32_t control_period_ms;
    uint32_t protection_period_ms;
    uint32_t comm_period_ms;

    /* PLACEHOLDER — replace after calibration */
    uint32_t low_battery_mv;
    uint32_t low_battery_recover_mv;
    uint32_t overcurrent1_ma;
    uint32_t overcurrent2_ma;
    uint16_t pwm_max_duty_permille; /* 0..1000 , start at 0 */
} app_config_t;

extern const app_config_t APP_CONFIG;

#endif /* APP_CONFIG_H */
