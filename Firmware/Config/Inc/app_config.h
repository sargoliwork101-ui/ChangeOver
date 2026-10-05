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

/* [EN] Tidy-up 2026-10-05: this struct had grown 11 fields that no line of
 *      code ever read - UI timings that moved into ui_led.h / ui_buzzer.h
 *      when the UI was split, plus two enable flags and a PWM cap that were
 *      never wired to anything. A config value nobody consumes is worse
 *      than no value: it reads like a knob, and turning it does nothing.
 *      What is left is exactly what the firmware reads today: the two
 *      one-shot board-test timings and the three task periods.
 *      The Protection limits (over-current, low-battery) deliberately did
 *      NOT move here - that module is still a skeleton and its thresholds
 *      live with it; see Firmware/Modules/Protection/README.md.
 * [FA] مرتب‌سازی: این ساختار ۱۱ فیلد داشت که هیچ خطی از کد نمی‌خواند -
 *      زمان‌بندی‌های UI که موقع تفکیک UI به ‎ui_led.h/ui_buzzer.h‎ منتقل شدند،
 *      به‌علاوهٔ دو پرچم فعال‌سازی و یک سقف PWM که به جایی وصل نبودند. مقدار
 *      پیکربندیِ بی‌مصرف از نبودنش بدتر است: شبیه یک پیچ تنظیم دیده می‌شود
 *      ولی چرخاندنش هیچ اثری ندارد. آنچه مانده دقیقاً همان است که فرم‌ور
 *      امروز می‌خواند. */
typedef struct
{
    /* ==================== Board-test one-shots / تست برد ==================== */
    uint32_t ui_selftest_led_ms;         /* [EN] Board-test LED step, ms / گام LED تست برد */
    uint32_t ui_boot_beep_ms;            /* [EN] Board-test beep length, ms / طول بوق تست برد */

    /* ==================== Task periods / دورهٔ تسک‌ها ==================== */
    uint32_t control_period_ms;          /* [EN] Control task period, ms / دوره تسک کنترل */
    uint32_t protection_period_ms;       /* [EN] Protection task period, ms / دوره تسک حفاظت */
    uint32_t comm_period_ms;             /* [EN] Communication task period, ms / دوره تسک ارتباط */
} app_config_t;

extern const app_config_t APP_CONFIG;

#endif /* APP_CONFIG_H */
