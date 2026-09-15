/**
 * @file    ui_config.h
 * @brief   [EN] Single place for all UI tunable thresholds and timings (min/max, beep, blink).
 *          [FA] یک جای واحد برای همه آستانه‌ها و تایم‌های UI (مین/ماکس، بوق، چشمک).
 *
 * @note    [EN] Why one file? User asked to avoid duplication across 2-3 files.
 *                All min/max and timings that were duplicated in ui.c and task_ui.c are now here.
 *                Change here and whole UI behavior tunes without touching logic.
 *                This file is included by ui.c, task_ui.c, and app_config.c (so APP_CONFIG stays in sync).
 *          [FA] چرا یک فایل؟ کاربر خواست تکراری در ۲-۳ فایل نباشد.
 *                همه مین/ماکس و تایم‌هایی که در ui.c و task_ui.c تکراری بودند الان اینجاست.
 *                اینجا را عوض کن تا رفتار UI تنظیم شود بدون دست زدن به منطق.
 */

#ifndef UI_CONFIG_H
#define UI_CONFIG_H

/* ==================== Battery voltage mapping ==================== */
/* [EN] 0% is NOT 0V, it is Vmin. 100% is Vmax. / صفر درصد صفر ولت نیست */
#define UI_BAT_V_MIN_MV                 21000u  /* [EN] 0% = 21V / صفر درصد = ۲۱ ولت */
#define UI_BAT_V_MAX_MV                 28000u  /* [EN] 100% = 28V / فول = ۲۸ ولت */

/* ==================== Input voltage threshold ==================== */
#define UI_INPUT_THRESHOLD_MV           20000u  /* [EN] <20V = no input, >=20V = present / زیر ۲۰ ولت نداریم */

/* ==================== Blink / Poll timings ==================== */
#define UI_INPUT_OK_POLL_MS             500u    /* [EN] InputOk steady hold / سبز ثابت ورودی وصل */
#define UI_SELFTEST_LED_MS              500u    /* [EN] Board test LED step / گام تست برد */
#define UI_BOOT_BEEP_MS                 150u    /* [EN] Board test beep / بوق تست برد */
#define UI_BLINK_PERIOD_MS              1000u   /* [EN] Green blink period BatteryRun / دوره چشمک سبز دشارژ */
#define UI_GREEN_MIN_OFF_MS             10u     /* [EN] Min off for green full / حداقل خاموشی سبز فول */
#define UI_CHARGING_BLINK_PERIOD_MS     1000u   /* [EN] Yellow blink period Charging / دوره چشمک زرد شارژ */
#define UI_CHARGING_YELLOW_MIN_OFF_MS   10u     /* [EN] Min off yellow almost full / حداقل خاموشی زرد */

/* ==================== Buzzer / Beep ==================== */
#define UI_BEEP_BASE_MS                 250u    /* [EN] Base beep / طول بوق پایه */
#define UI_BEEP_DOUBLE_THRESH_PCT       20u     /* [EN] Below this, duration x2 / زیر این ۲ برابر */
#define UI_BEEP_START_PCT               50u     /* [EN] Below this, periodic beep starts / زیر این بوق دوره‌ای */

/* ==================== Buzzer Pattern (new) ==================== */
/* [EN] Buzzer pattern: separate scenario with period, onTime, repeat inside onTime, gap.
/   Example: onTime=1000ms, repeat=2, gap=20% => gap=200ms, each beep=400ms => ON 400 OFF 200 ON 400
/   If repeat=1, gap ignored.
/   [FA] الگوی بازر: سناریو جدا با دوره تناوب، زمان روشن، تکرار داخل روشن، گپ */
#define UI_BUZZER_DEFAULT_GAP_PERCENT   20u     /* [EN] Default gap 20% of onTime when repeat>1 / گپ پیش‌فرض ۲۰٪ */
#define UI_BUZZER_MAX_REPEAT            10u     /* [EN] Max repeat inside onTime, 1..10 / حداکثر تکرار داخل روشن */
#define UI_BUZZER_MIN_ON_MS             10u     /* [EN] Min beep on inside pattern / حداقل روشن */
#define UI_BUZZER_MAX_ON_MS             10000u  /* [EN] Max onTime per pattern / حداکثر روشن */
#define UI_BUZZER_MAX_PERIOD_MS         60000u  /* [EN] Max period / حداکثر دوره */
#define UI_BUZZER_MAX_GAP_MS            5000u   /* [EN] Max gap between beeps / حداکثر گپ */

/* ==================== Percent helpers ==================== */
#define UI_PERCENT_FULL                 100u
#define UI_PERCENT_SCALE                100u

#endif /* UI_CONFIG_H */
