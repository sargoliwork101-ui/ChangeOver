/**
 * @file    app_config.h
 * @brief   همه عددهای قابل تنظیم در یک جا. ماژول‌ها عدد خام نداشته باشند.
 *
 * MISRA: magic number وسط منطق ممنوع. زمان چشمک را این‌جا عوض کن، نه در ui.c.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    /* دوره اجرای Task UI بر حسب میلی‌ثانیه. 100 یعنی Ui_Run هر 0.1 ثانیه. */
    uint32_t ui_period_ms;

    /* هر LED در خودآزمایی چند ms روشن بماند. */
    uint32_t ui_selftest_led_ms;

    /* طول بوق شروع. کوتاه باشد تا آزاردهنده نشود. */
    uint32_t ui_boot_beep_ms;

    /* نصف دوره چشمک سبز بعد از تست (روشن یا خاموش بودن). */
    uint32_t ui_heartbeat_half_ms;

    /* از این به بعد برای مرحله‌های بعد است؛ در LED استفاده نمی‌شود. */
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

/* تعریف واقعی در app_config.c — const تا در ram عمداً عوض نشود. */
extern const app_config_t APP_CONFIG;

#endif /* APP_CONFIG_H */
