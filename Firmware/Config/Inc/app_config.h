/**
 * @file    app_config.h
 * @brief   [EN] Tunable values in one place (MISRA: no magic numbers in logic).
 *          [FA] اعداد قابل تنظیم در یک جا (عدد جادویی وسط منطق ممنوع).
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint32_t ui_selftest_led_ms;
    uint32_t ui_boot_beep_ms;
    uint32_t ui_scen1_on_ms;
    uint32_t ui_scen1_off_ms;
    uint32_t ui_scen2_green_on_ms;
    uint32_t ui_scen2_red_ms;
    bool     power_stage_enabled;
    bool     esp_link_enabled;
    uint32_t control_period_ms;
    uint32_t protection_period_ms;
    uint32_t comm_period_ms;
    uint32_t low_battery_mv;
    uint32_t low_battery_recover_mv;
    uint32_t overcurrent1_ma;
    uint32_t overcurrent2_ma;
    uint16_t pwm_max_duty_permille;
} app_config_t;

extern const app_config_t APP_CONFIG;

#endif /* APP_CONFIG_H */
