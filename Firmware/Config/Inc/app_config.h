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
    uint32_t ui_input_ok_poll_ms;      /* [EN] Re-check period while input is steady / دوره بازبینی حالت ورودی */
    uint32_t ui_selftest_led_ms;       /* board-test LED step / گام LED تست برد */
    uint32_t ui_boot_beep_ms;          /* board-test beep length / طول بوق تست برد */
    uint32_t ui_blink_period_ms;       /* [EN] Green battery-run blink period / دوره چشمک سبز */
    uint32_t ui_green_min_off_ms;      /* [EN] Minimum green off time (full battery) / حداقل خاموشی سبز */
    uint8_t  ui_low_battery_percent;   /* [EN] At/below this, low-battery warning starts / آستانه هشدار باتری */
    uint32_t ui_warn_period_ms;        /* [EN] Yellow warning blink period / دوره چشمک زرد هشدار */
    uint32_t ui_warn_yellow_on_ms;     /* [EN] Yellow on-time inside the warning period / زمان روشن‌بودن زرد */
    uint32_t ui_warn_beep_ms;          /* [EN] Warning beep length / طول بوق هشدار */
    uint32_t ui_warn_beep_period_ms;   /* [EN] Time between warning beeps / فاصله بوق‌های هشدار */
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
