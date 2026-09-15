/**
 * @file    app_config.h
 * @brief   [EN] Tunable values in one place (MISRA: no magic numbers in logic).
 *          [FA] اعداد قابل تنظیم در یک جا (عدد جادویی وسط منطق ممنوع).
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ==================== Includes / شامل‌ها ==================== */
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint32_t ui_input_ok_poll_ms;      /* [EN] Re-check period while input is steady / دوره بازبینی حالت ورودی */
    uint32_t ui_selftest_led_ms;       /* board-test LED step / گام LED تست برد */
    uint32_t ui_boot_beep_ms;          /* board-test beep length / طول بوق تست برد */
    uint32_t ui_blink_period_ms;       /* [EN] Green battery-run blink period / دوره چشمک سبز */
    uint32_t ui_green_min_off_ms;      /* [EN] Minimum green off time (full battery) / حداقل خاموشی سبز */
    uint32_t ui_bat_v_min_mv;          /* [EN] 0% battery voltage (e.g. 21V) / ولتاژ صفر درصد باتری */
    uint32_t ui_bat_v_max_mv;          /* [EN] 100% battery voltage (e.g. 28V) / ولتاژ فول باتری */
    uint32_t ui_charging_blink_period_ms; /* [EN] Yellow blink period in charging / دوره چشمک زرد شارژ */
    uint32_t ui_charging_yellow_min_off_ms; /* [EN] Min off for yellow full / حداقل خاموشی زرد */
    bool     power_stage_enabled;       /* [EN] Power stage enable flag / پرچم فعال‌سازی مرحله توان */
    bool     esp_link_enabled;           /* [EN] ESP link enable flag / پرچم فعال‌سازی ارتباط ESP */
    uint32_t control_period_ms;          /* [EN] Control task period / دوره تسک کنترل */
    uint32_t protection_period_ms;       /* [EN] Protection task period / دوره تسک حفاظت */
    uint32_t comm_period_ms;             /* [EN] Communication task period / دوره تسک ارتباط */
    uint32_t low_battery_mv;             /* [EN] Legacy low-battery threshold / آستانه قدیمی باتری پایین */
    uint32_t low_battery_recover_mv;     /* [EN] Legacy recovery threshold / آستانه قدیمی بازیابی باتری */
    uint32_t overcurrent1_ma;             /* [EN] First overcurrent limit / حد اول اضافه‌جریان */
    uint32_t overcurrent2_ma;             /* [EN] Second overcurrent limit / حد دوم اضافه‌جریان */
    uint16_t pwm_max_duty_permille;      /* [EN] PWM duty limit / حد دیوتی PWM */
} app_config_t;

extern const app_config_t APP_CONFIG;

#endif /* APP_CONFIG_H */
