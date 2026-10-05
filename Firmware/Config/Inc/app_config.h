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

/* [EN] Clean-up 2026-10-05: this struct had grown to nineteen fields of which
 *      the firmware read only five. The other fourteen were historical - UI
 *      timings that moved into ui_led.h / ui_buzzer.h when the UI was split,
 *      the legacy battery and over-current limits, two enable flags and a PWM
 *      cap that were never wired to anything. They are gone, because a config
 *      value nobody consumes is worse than no value: it reads like a knob,
 *      and turning it does nothing. What is left is exactly what the firmware
 *      reads today - the two one-shot board-test timings and the three task
 *      periods. The removal is compile-checked: every gate, the four host
 *      testers and the 270-invariant consistency audit run green after it.
 *
 *      CONTRACT from here on: do not re-add a field for a value a module
 *      already owns. Every threshold the ESP panel can tune lives with its
 *      module and is persisted in NVM; a second copy in APP_CONFIG is how
 *      the panel and the firmware end up disagreeing about one quantity.
 *      The Protection limits deliberately did not come back here - that
 *      module is still a skeleton and its thresholds will live next to its
 *      own functions; see Firmware/Modules/Protection/README.md.
 * [FA] مرتب‌سازی ۲۰۲۶-۱۰-۰۵: این ساختار نوزده فیلد شده بود که فرم‌ور فقط پنج
 *      تای آن را می‌خواند. چهارده تای دیگر تاریخی بودند - زمان‌بندی‌های UI که
 *      هنگام تفکیک UI به ‎ui_led.h/ui_buzzer.h‎ رفتند، حدهای قدیمی باتری و
 *      اضافه‌جریان، دو پرچم فعال‌سازی و یک سقف PWM که به جایی وصل نبودند.
 *      حذف شدند، چون مقدار پیکربندیِ بی‌مصرف از نبودنش بدتر است: شبیه یک پیچ
 *      تنظیم دیده می‌شود ولی چرخاندنش هیچ اثری ندارد. آنچه مانده دقیقاً همان
 *      است که فرم‌ور امروز می‌خواند. حذف با کامپایل بررسی شده است: همهٔ
 *      دروازه‌ها، چهار تستر هاست و ممیزی ۲۷۰ نامتغیری بعد از آن سبزند.
 *
 *      قرارداد از این به بعد: برای مقداری که یک ماژول خودش دارد دوباره فیلدی
 *      اینجا اضافه نکنید. هر آستانه‌ای که پنل ESP تنظیم می‌کند پیش خود ماژول
 *      است و در NVM می‌ماند؛ نسخهٔ دوم در ‎APP_CONFIG‎ باعث می‌شود پنل و فرم‌ور
 *      دربارهٔ یک کمیت اختلاف پیدا کنند. */
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
