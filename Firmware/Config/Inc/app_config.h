/**
 * @file    app_config.h
 * @brief   [EN] Tunable values in one place (MISRA: no magic numbers in logic).
 *          [FA] اعداد قابل تنظیم در یک جا (عدد جادویی وسط منطق ممنوع).
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint32_t ui_input_ok_poll_ms;      /* [EN] Re-check period while input is steady / دوره بازبینی حالت ورودی */
    uint32_t ui_selftest_led_ms;       /* board-test LED step / گام LED تست برد */
    uint32_t ui_boot_beep_ms;          /* board-test beep length / طول بوق تست برد */
    uint32_t ui_blink_period_ms;       /* [EN] Green battery-run blink period / دوره چشمک سبز */
    uint32_t ui_green_min_off_ms;      /* [EN] Minimum green off time (full battery) / حداقل خاموشی سبز */
    uint8_t  ui_low_battery_percent;   /* [EN] Deprecated, kept for compat / قدیمی */
    uint32_t ui_warn_period_ms;        /* [EN] Deprecated / قدیمی */
    uint32_t ui_warn_yellow_on_ms;     /* [EN] Deprecated / قدیمی */
    uint32_t ui_warn_beep_ms;          /* [EN] Base beep length, used by new logic too / طول بوق پایه */
    uint32_t ui_warn_beep_period_ms;   /* [EN] Deprecated / قدیمی */
    uint32_t ui_input_threshold_mv;    /* [EN] Below this, input considered lost (<20V) / آستانه ولتاژ ورودی */
    uint32_t ui_bat_v_min_mv;          /* [EN] 0% battery voltage (e.g. 21V) / ولتاژ صفر درصد باتری */
    uint32_t ui_bat_v_max_mv;          /* [EN] 100% battery voltage (e.g. 28V) / ولتاژ فول باتری */
    uint32_t ui_charging_blink_period_ms; /* [EN] Yellow blink period in charging / دوره چشمک زرد شارژ */
    uint32_t ui_charging_yellow_min_off_ms; /* [EN] Min off for yellow full / حداقل خاموشی زرد */
    uint32_t ui_beep_base_ms;          /* [EN] Base beep duration / طول بوق پایه */
    uint8_t  ui_beep_double_thresh_pct;/* [EN] Below this, beep duration x2 / زیر این درصد بوق ۲ برابر */
    uint8_t  ui_beep_start_pct;        /* [EN] Below this, periodic beep starts / زیر این درصد بوق دوره‌ای */
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
