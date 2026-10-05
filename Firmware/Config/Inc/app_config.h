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

/* ==================== Application configuration block ==================== */

/* [EN] WHICH OF THESE FIELDS ARE ACTUALLY READ - full-program audit
 *      2026-10-05. Only five of the nineteen fields below are read by the
 *      firmware today; the other fourteen are historical, left over from
 *      versions where the UI thresholds, the legacy battery limits and the
 *      overcurrent limits lived here before each module grew its own
 *      runtime-tunable parameter set (reachable from the ESP panel and
 *      persisted in NVM). The dead fields are marked UNUSED one by one
 *      below and were deliberately NOT deleted: APP_CONFIG is a const
 *      aggregate in Flash, so an unread member costs a few bytes of Flash
 *      and nothing at runtime, while removing members renumbers the
 *      initializer in app_config.c and would be a silent, high-risk edit
 *      against a struct that external tooling also reads. Treat the UNUSED
 *      markers as the contract: do not start reading one of them again -
 *      take the module's own parameter instead, otherwise the panel and the
 *      firmware will disagree about the same quantity.
 * [FA] کدام یک از این فیلدها واقعاً خوانده می‌شوند - ممیزی کل برنامه
 *      ۲۰۲۶-۱۰-۰۵. امروز فقط پنج فیلد از نوزده فیلد زیر توسط فرم‌ور خوانده
 *      می‌شوند؛ چهارده تای دیگر تاریخی‌اند و از نسخه‌هایی مانده‌اند که
 *      آستانه‌های رابط کاربری، حدهای قدیمی باتری و حدهای اضافه‌جریان اینجا
 *      بودند، پیش از آنکه هر ماژول مجموعهٔ پارامتر قابل تنظیم در زمان اجرای
 *      خودش را پیدا کند (قابل دسترس از پنل ESP و ماندگار در NVM). فیلدهای
 *      مرده پایین تک‌تک با برچسب UNUSED مشخص شده‌اند و عمداً حذف نشده‌اند:
 *      ‎APP_CONFIG‎ یک تجمیعِ const در فلش است، پس عضو خوانده‌نشده فقط چند بایت
 *      فلش می‌گیرد و در زمان اجرا هیچ هزینه‌ای ندارد، در حالی که حذف اعضا
 *      مقداردهی اولیه را در ‎app_config.c‎ جابه‌جا می‌کند و ویرایشی بی‌صدا و
 *      پرخطر روی ساختاری است که ابزارهای بیرونی هم می‌خوانندش. برچسب‌های
 *      UNUSED را قرارداد بدانید: دوباره از یکی از آن‌ها نخوانید؛ به‌جایش
 *      پارامتر خود ماژول را بردارید، وگرنه پنل و فرم‌ور دربارهٔ یک کمیت با هم
 *      اختلاف پیدا می‌کنند. */
typedef struct
{
    /* [EN] UNUSED (audit 2026-10-05): the UI task runs on its own cadence.
       [FA] استفاده‌نشده: تسک UI آهنگ خودش را دارد. */
    uint32_t ui_input_ok_poll_ms;      /* [EN] Re-check period while input is steady / دوره بازبینی حالت ورودی */

    /* [‎EN] LIVE: read by the UI board self-test‎. / زنده: خودآزمون برد. */
    uint32_t ui_selftest_led_ms;       /* board-test LED step / گام LED تست برد */

    /* [‎EN] LIVE: read by the UI board self-test‎. / زنده: خودآزمون برد. */
    uint32_t ui_boot_beep_ms;          /* board-test beep length / طول بوق تست برد */

    /* [EN] UNUSED (audit 2026-10-05): ui_led.c owns its own blink timing.
       [FA] استفاده‌نشده: زمان‌بندی چشمک مال خود ‎ui_led.c‎ است. */
    uint32_t ui_blink_period_ms;       /* [EN] Green battery-run blink period / دوره چشمک سبز */

    /* [EN] UNUSED (audit 2026-10-05): ui_led.c owns its own blink timing.
       [FA] استفاده‌نشده: زمان‌بندی چشمک مال خود ‎ui_led.c‎ است. */
    uint32_t ui_green_min_off_ms;      /* [EN] Minimum green off time (full battery) / حداقل خاموشی سبز */

    /* [EN] UNUSED (audit 2026-10-05): the battery window is a UI constant.
       [FA] استفاده‌نشده: پنجرهٔ باتری ثابتِ خود ماژول UI است. */
    uint32_t ui_bat_v_min_mv;          /* [EN] 0% battery voltage (e.g. 21V) / ولتاژ صفر درصد باتری */

    /* [EN] UNUSED (audit 2026-10-05): the battery window is a UI constant.
       [FA] استفاده‌نشده: پنجرهٔ باتری ثابتِ خود ماژول UI است. */
    uint32_t ui_bat_v_max_mv;          /* [EN] 100% battery voltage (e.g. 28V) / ولتاژ فول باتری */

    /* [EN] UNUSED (audit 2026-10-05): charging blink timing lives in ui_led.c.
       [FA] استفاده‌نشده: زمان‌بندی چشمک شارژ در ‎ui_led.c‎ است. */
    uint32_t ui_charging_blink_period_ms; /* [EN] Yellow blink period in charging / دوره چشمک زرد شارژ */

    /* [EN] UNUSED (audit 2026-10-05): charging blink timing lives in ui_led.c.
       [FA] استفاده‌نشده: زمان‌بندی چشمک شارژ در ‎ui_led.c‎ است. */
    uint32_t ui_charging_yellow_min_on_ms; /* [EN] Min yellow ON blip near full / حداقل روشنی زرد */

    /* [EN] UNUSED (audit 2026-10-05): the build-time MODULE_* switches in
       modules_enable.h decide what exists; a runtime copy would be a second
       source of truth for the same question.
       [FA] استفاده‌نشده: سوییچ‌های ‎MODULE_*‎ زمان کامپایل تصمیم می‌گیرند؛ کپی
       زمان‌اجرا منبع حقیقت دوم برای یک پرسش می‌شد. */
    bool     power_stage_enabled;       /* [EN] Power stage enable flag / پرچم فعال‌سازی مرحله توان */

    /* [EN] UNUSED (audit 2026-10-05): see power_stage_enabled, MODULE_ESP owns this.
       [FA] استفاده‌نشده: مانند بالا، تصمیمش با ‎MODULE_ESP‎ است. */
    bool     esp_link_enabled;           /* [EN] ESP link enable flag / پرچم فعال‌سازی ارتباط ESP */

    /* [EN] LIVE: the control task period. / زنده: دورهٔ تسک کنترل. */
    uint32_t control_period_ms;          /* [EN] Control task period / دوره تسک کنترل */

    /* [EN] LIVE: the protection task period. / زنده: دورهٔ تسک حفاظت. */
    uint32_t protection_period_ms;       /* [EN] Protection task period / دوره تسک حفاظت */

    /* [EN] LIVE: the communication task period. / زنده: دورهٔ تسک ارتباط. */
    uint32_t comm_period_ms;             /* [EN] Communication task period / دوره تسک ارتباط */

    /* [EN] UNUSED (audit 2026-10-05): superseded by the Fault module's
       runtime alarm set, which the panel can tune and NVM persists.
       [FA] استفاده‌نشده: مجموعهٔ آلارم زمان‌اجرای ماژول فالت جایش را گرفته. */
    uint32_t low_battery_mv;             /* [EN] Legacy low-battery threshold / آستانه قدیمی باتری پایین */

    /* [EN] UNUSED (audit 2026-10-05): superseded by the Fault alarm set.
       [FA] استفاده‌نشده: مجموعهٔ آلارم فالت جایش را گرفته. */
    uint32_t low_battery_recover_mv;     /* [EN] Legacy recovery threshold / آستانه قدیمی بازیابی باتری */

    /* [EN] UNUSED (audit 2026-10-05): the real over-current reaction is the
       hardware LM393 comparator plus the Jitter module, not a soft limit.
       [FA] استفاده‌نشده: واکنش واقعی اضافه‌جریان، مقایسه‌گر سخت‌افزاری LM393 و
       ماژول جیتر است، نه یک حد نرم‌افزاری. */
    uint32_t overcurrent1_ma;             /* [EN] First overcurrent limit / حد اول اضافه‌جریان */

    /* [EN] UNUSED (audit 2026-10-05): see overcurrent1_ma.
       [FA] استفاده‌نشده: مانند بالا. */
    uint32_t overcurrent2_ma;             /* [EN] Second overcurrent limit / حد دوم اضافه‌جریان */

    /* [EN] UNUSED (audit 2026-10-05): the charger owns its own duty ceiling
       so the limit and the regulator that must respect it stay together.
       [FA] استفاده‌نشده: سقف دیوتی مال خود شارژر است تا حد و تنظیم‌کننده‌ای که
       باید رعایتش کند کنار هم بمانند. */
    uint16_t pwm_max_duty_permille;      /* [EN] PWM duty limit / حد دیوتی PWM */
} app_config_t;

extern const app_config_t APP_CONFIG;

#endif /* APP_CONFIG_H */
