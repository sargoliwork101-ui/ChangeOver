/**
 * @file    ui_led.h
 * @brief   [EN] UI LED scenarios - green/red/yellow, battery percent, InputOk/Charging/BatteryRun.
 *          Split from UI into LED and BUZZER per user request. Constants for LED in its own header.
 *          RTOS simple readable, non-linear formulas, markers above each function in h and c.
 *          [FA] سناریوهای LED ماژول UI - ثابت‌های LED در هدر خودش، فرمول غیرخطی، RTOS ساده.
 *
 * @note    [EN] LED defaults live in this header; since v1.16 the live
 *          cadence is the persisted ui_alarm_t (ids 38..76) and runtime
 *          logic reads it - APP_CONFIG keeps only the one-shot BoardTest
 *          timings. Naming __ after type, func__ prefix.
 *          CMSIS-RTOS2: osDelay allowed, HAL_Delay forbidden. Formulas broken into steps.
 *          [FA] پیش‌فرض‌های LED در این هدر هستند؛ از v1.16 منطق زمان اجرا
 *          از ui_alarm_t ماندگار (۳۸..۷۶) می‌خواند و APP_CONFIG فقط
 *          زمان‌بندی تست برد را نگه می‌دارد.
 */

#ifndef UI_LED_H
#define UI_LED_H

/* ==================== Includes / شامل‌ها ==================== */

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"

/* ==================== Battery voltage mapping constants / ثابت‌های نگاشت ولتاژ باتری ==================== */

/**
 * @brief  [EN] Battery voltage mapped to zero percent, in millivolts.
 *         [FA] ولتاژ باتری متناظر با صفر درصد، بر حسب میلی‌ولت.
 */
#define UI_BAT_V_MIN_MV                 21000u

/**
 * @brief  [EN] Battery voltage mapped to one hundred percent, in millivolts.
 *              Raised 28000 -> 29000 by user order 2026-09-20: the Charging
 *              yellow blink keeps pacing until the battery truly reaches 29 V.
 *         [FA] ولتاژ باتری متناظر با صد درصد. ۲۸۰۰۰ → ۲۹۰۰۰ به دستور کاربر:
 *              چشمک زرد شارژ تا رسیدن واقعی به ۲۹V ادامه دارد.
 */
#define UI_BAT_V_MAX_MV                 29000u

/* ==================== Input voltage thresholds and hysteresis / آستانه‌ها و هیسترزیس ولتاژ ورودی ==================== */

/**
 * @brief  [EN] Input voltage at or above which the input is considered connected, in millivolts.
 *         [FA] ولتاژ ورودی که از آن به بعد ورودی متصل در نظر گرفته می‌شود، بر حسب میلی‌ولت.
 */
#define UI_INPUT_CONNECTED_THRESHOLD_MV 21000u

/**
 * @brief  [EN] Hysteresis band for input connected/disconnected detection, in millivolts.
 *         [FA] پهنای هیسترزیس تشخیص وصل/قطع ورودی، بر حسب میلی‌ولت.
 */
#define UI_INPUT_HYSTERESIS_MV          1000u

/**
 * @brief  [EN] Input voltage at or below which the input is considered disconnected, in millivolts.
 *         Derived from the connected threshold minus the hysteresis band.
 *         [FA] ولتاژ ورودی که از آن به پایین ورودی قطع در نظر گرفته می‌شود، بر حسب میلی‌ولت.
 *         از کم‌کردن هیسترزیس از آستانه وصل به دست می‌آید.
 */
#define UI_INPUT_DISCONNECTED_THRESHOLD_MV \
    (UI_INPUT_CONNECTED_THRESHOLD_MV - UI_INPUT_HYSTERESIS_MV)

/* ==================== Input overvoltage error constants / ثابت‌های خطای اضافه‌ولتاژ ورودی ==================== */

/**
 * @brief  [EN] Input voltage above which the overvoltage error is activated, in millivolts.
 *         The comparison is strict: values greater than this threshold activate the error.
 *         [FA] ولتاژ ورودی که بیشتر از آن خطای اضافه‌ولتاژ فعال می‌شود، بر حسب میلی‌ولت.
 *         مقایسه strict است؛ مقدار بزرگ‌تر از این آستانه خطا را فعال می‌کند.
 */
#define UI_INPUT_OVERVOLTAGE_THRESHOLD_MV 28000u

/**
 * @brief  [EN] Hysteresis band used to clear the input overvoltage error, in millivolts.
 *         [FA] پهنای هیسترزیس پاک‌کردن خطای اضافه‌ولتاژ ورودی، بر حسب میلی‌ولت.
 */
#define UI_INPUT_OVERVOLTAGE_HYSTERESIS_MV 1000u

/**
 * @brief  [EN] Input voltage at or below which the overvoltage error is cleared, in millivolts.
 *         Derived from the overvoltage threshold minus its hysteresis band.
 *         [FA] ولتاژ ورودی که از آن به پایین خطای اضافه‌ولتاژ پاک می‌شود، بر حسب میلی‌ولت.
 *         از کم‌کردن هیسترزیس خطا از آستانه اضافه‌ولتاژ به دست می‌آید.
 */
#define UI_INPUT_OVERVOLTAGE_CLEAR_THRESHOLD_MV \
    (UI_INPUT_OVERVOLTAGE_THRESHOLD_MV - UI_INPUT_OVERVOLTAGE_HYSTERESIS_MV)

/* ==================== Scenario timing constants / ثابت‌های زمانی سناریوها ==================== */

/**
 * @brief  [EN] Red LED period while the input overvoltage error is displayed, in milliseconds.
 *         [FA] دوره چشمک LED قرمز هنگام نمایش خطای اضافه‌ولتاژ ورودی، بر حسب میلی‌ثانیه.
 */
#define UI_INPUT_OVERVOLTAGE_LED_PERIOD_MS 1000u

/**
 * @brief  [EN] Red LED duty while the input overvoltage error is displayed, in percent.
 *         [FA] دیوتی چشمک LED قرمز هنگام نمایش خطای اضافه‌ولتاژ ورودی، بر حسب درصد.
 */
#define UI_INPUT_OVERVOLTAGE_LED_DUTY_PERCENT 50u

/**
 * @brief  [EN] Buzzer pattern period for the input overvoltage error, in milliseconds.
 *         [FA] دوره الگوی بوق خطای اضافه‌ولتاژ ورودی، بر حسب میلی‌ثانیه.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_PERIOD_MS 10000u

/**
 * @brief  [EN] Buzzer ON duration for one input overvoltage warning, in milliseconds.
 *         [FA] مدت روشن‌بودن بوق در هر هشدار اضافه‌ولتاژ ورودی، بر حسب میلی‌ثانیه.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_DURATION_MS 1000u

/**
 * @brief  [EN] Buzzer duty derived from the overvoltage warning ON duration and period.
 *         [FA] دیوتی بوق که از مدت روشن‌بودن و دوره هشدار اضافه‌ولتاژ به دست می‌آید.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_DUTY_PERCENT \
    ((UI_INPUT_OVERVOLTAGE_BEEP_DURATION_MS * UI_PERCENT_SCALE) / UI_INPUT_OVERVOLTAGE_BEEP_PERIOD_MS)

/**
 * @brief  [EN] Number of buzzer pulses in one input overvoltage warning pattern.
 *         [FA] تعداد پالس بوق در الگوی هشدار اضافه‌ولتاژ ورودی.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_COUNT 1u

/**
 * @brief  [EN] Gap between adjacent overvoltage pulses; one pulse does not use a gap.
 *         [FA] گپ بین پالس‌های اضافه‌ولتاژ؛ یک پالس گپ ندارد.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_GAP_MS 0u

/* ==================== Battery-lost error constants / ثابت‌های خطای قطع باتری ==================== */

/**
 * @brief  [EN] Battery-lost scenario (2026-09-19): shown only while the
 *         central fault bit FAULT_CHARGER_BAT_LOST is latched by the Fault
 *         module. Design chosen with the user: red fast blink (defaults 500 ms ON / 500 ms OFF - clearly
 *         different from the 1 s input-overvoltage pulse; ids 44/45),
 *         green steady (input is present in both detection cases), and a
 *         repeating buzzer pattern (defaults: THREE short beeps then a silence; ids 46..49). It can
 *         never mix with the BatteryRun critical beep: that one runs only with
 *         the input ABSENT, and in func__Ui_Tick this scenario is checked
 *         (after overvoltage) before every normal scenario and returns.
 *         [FA] سناریوی قطع باتری: فقط تا وقتی پرچم متمرکز قفل است؛ قرمز
 *         چشمک‌تند (پیش‌فرض نیم‌ثانیه/نیم‌ثانیه؛ ۴۴/۴۵)، سبز ثابت و بوق
 *         دوره‌ای و تکرارشونده (پیش‌فرض سه بیپ کوتاه + مکث؛ ۴۶..۴۹).
 *         با بوق بحرانی دشارژ هرگز قاطی نمی‌شود - آن فقط بی‌ورودی
 *         است و این سناریو قبل از همه سناریوهای نرمال چک و return می‌شود.
 */
#define UI_BAT_LOST_LED_PERIOD_MS 1000u

/**
 * @brief  [EN] Red LED duty while the battery-lost error is displayed, percent.
 *         [FA] دیوتی LED قرمز هنگام نمایش خطای قطع باتری، درصد.
 */
#define UI_BAT_LOST_LED_DUTY_PERCENT 50u

/**
 * @brief  [EN] Battery-lost buzzer pattern: one full cycle (beeps + silence),
 *         in milliseconds.
 *         [FA] دوره کامل الگوی بوق قطع باتری (بیپ‌ها + سکوت)، میلی‌ثانیه.
 */
#define UI_BAT_LOST_BEEP_PERIOD_MS 3000u

/**
 * @brief  [EN] Total beep window inside one battery-lost cycle (the three
 *         beeps share it), remainder is silence, in milliseconds.
 *         [FA] پنجره بیپ‌ها در یک دوره (سه بیپ در این پنجره تقسیم می‌شوند).
 */
#define UI_BAT_LOST_BEEP_DURATION_MS 900u

/**
 * @brief  [EN] Buzzer duty derived from the beep window and the full period.
 *         [FA] دیوتی بوق برگرفته از پنجره بیپ و دوره کامل.
 */
#define UI_BAT_LOST_BEEP_DUTY_PERCENT \
    ((UI_BAT_LOST_BEEP_DURATION_MS * UI_PERCENT_SCALE) / UI_BAT_LOST_BEEP_PERIOD_MS)

/**
 * @brief  [EN] Number of buzzer pulses per battery-lost cycle (three beeps).
 *         [FA] تعداد پالس بوق در هر دوره (سه بیپ).
 */
#define UI_BAT_LOST_BEEP_COUNT 3u

/**
 * @brief  [EN] Silence between adjacent battery-lost beeps, in milliseconds.
 *         [FA] سکوت بین بیپ‌های مجاور، میلی‌ثانیه.
 */
#define UI_BAT_LOST_BEEP_GAP_MS 100u

/**
 * @brief  [EN] Delay used while InputOk holds the green LED steady.
 *         [FA] تأخیر سناریوی InputOk هنگام ثابت نگه‌داشتن LED سبز.
 */
#define UI_INPUT_OK_POLL_MS             500u

/**
 * @brief  [EN] Duration of each LED step in the one-shot BoardTest.
 *         [FA] مدت هر مرحله LED در تست یک‌باره برد.
 */
#define UI_SELFTEST_LED_MS              500u

/**
 * @brief  [EN] Complete green blink period used by BatteryRun, in milliseconds.
 *         [FA] دوره کامل چشمک سبز در BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BLINK_PERIOD_MS              1000u

/**
 * @brief  [EN] Minimum green LED OFF time used to keep the full-battery blink visible.
 *         [FA] کمترین زمان خاموشی LED سبز برای قابل‌مشاهده ماندن چشمک باتری فول.
 */
#define UI_GREEN_MIN_OFF_MS             10u

/**
 * @brief  [EN] Complete yellow blink period used by Charging, in milliseconds.
 *         [FA] دوره کامل چشمک زرد در Charging، بر حسب میلی‌ثانیه.
 */
#define UI_CHARGING_BLINK_PERIOD_MS     1000u

/**
 * @brief  [EN] Minimum yellow LED OFF time near full charge, in milliseconds.
 *         [FA] کمترین زمان خاموشی LED زرد نزدیک شارژ کامل، بر حسب میلی‌ثانیه.
 */
#define UI_CHARGING_YELLOW_MIN_OFF_MS   10u

/* ==================== BatteryRun warning constants / ثابت‌های هشدار BatteryRun ==================== */

/**
 * @brief  [EN] Battery percentage below which the standard BatteryRun warning beep is enabled.
 *         [FA] درصد باتری که پایین‌تر از آن بوق هشدار معمول BatteryRun فعال می‌شود.
 */
#define UI_BATTERY_RUN_BEEP_START_PERCENT 40u

/**
 * @brief  [EN] Battery percentage below which the standard warning uses two beeps.
 *         [FA] درصد باتری که پایین‌تر از آن هشدار معمول با دو بوق اجرا می‌شود.
 */
#define UI_BATTERY_RUN_BEEP_DOUBLE_PERCENT 20u

/**
 * @brief  [EN] Battery percentage below which the warning uses three beeps every 20 seconds.
 *         [FA] درصد باتری که پایین‌تر از آن هشدار با سه بوق هر ۲۰ ثانیه اجرا می‌شود.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_PERCENT 10u

/**
 * @brief  [EN] Battery percentage below which the critical ten-second beep is used.
 *         [FA] درصد باتری که پایین‌تر از آن بوق بحرانی ده‌ثانیه‌ای اجرا می‌شود.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_PERCENT 1u

/**
 * @brief  [EN] Interval shared by the one-beep and two-beep BatteryRun warnings, in milliseconds.
 *         [FA] فاصله مشترک هشدار یک‌بوق و دو‌بوق BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS 60000u

/**
 * @brief  [EN] Interval of the three-beep BatteryRun warning, in milliseconds.
 *         [FA] فاصله هشدار سه‌بوق BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS 20000u

/**
 * @brief  [EN] Complete period used for the single critical ten-second beep, in milliseconds.
 *         [FA] دوره کامل بوق بحرانی ده‌ثانیه‌ای، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_PERIOD_MS 10000u

/**
 * @brief  [EN] Duty of the continuous critical BatteryRun beep, in percent.
 *         [FA] دیوتی بوق ممتد بحرانی BatteryRun، بر حسب درصد.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_DUTY_PERCENT 100u

/**
 * @brief  [EN] Pulse count of the critical BatteryRun beep.
 *         [FA] تعداد پالس بوق بحرانی BatteryRun.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_COUNT 1u

/**
 * @brief  [EN] Duration of each one-beep or two-beep BatteryRun pulse, in milliseconds.
 *         [FA] مدت هر بوق در هشدار یک‌بوق یا دو‌بوق BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS 1000u

/**
 * @brief  [EN] Approximate duty for one standard beep, derived from duration and interval.
 *         [FA] دیوتی تقریبی یک بوق معمول که از مدت و فاصله محاسبه می‌شود.
 */
#define UI_BATTERY_RUN_BEEP_STANDARD_DUTY_PERCENT \
    ((UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS * UI_PERCENT_SCALE + UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS - 1u) / UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS)

/**
 * @brief  [EN] Approximate duty for two standard-duration beeps and their gap.
 *         [FA] دیوتی تقریبی دو بوق معمول به‌همراه گپ بین آن‌ها.
 */
#define UI_BATTERY_RUN_BEEP_DOUBLE_DUTY_PERCENT \
    ((((UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS * UI_BATTERY_RUN_BEEP_DOUBLE_COUNT) + \
       (UI_BATTERY_RUN_BEEP_GAP_MS * (UI_BATTERY_RUN_BEEP_DOUBLE_COUNT - 1u))) * \
      UI_PERCENT_SCALE + UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS - 1u) / \
     UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS)

/**
 * @brief  [EN] Approximate duty for three triple-duration beeps and their gaps.
 *         [FA] دیوتی تقریبی سه بوق مدت‌دار به‌همراه گپ‌های بین آن‌ها.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_DUTY_PERCENT \
    ((((UI_BATTERY_RUN_BEEP_TRIPLE_DURATION_MS * UI_BATTERY_RUN_BEEP_TRIPLE_COUNT) + \
       (UI_BATTERY_RUN_BEEP_GAP_MS * (UI_BATTERY_RUN_BEEP_TRIPLE_COUNT - 1u))) * \
      UI_PERCENT_SCALE + UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS - 1u) / \
     UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS)

/**
 * @brief  [EN] Duration of each pulse in the three-beep BatteryRun warning, in milliseconds.
 *         [FA] مدت هر بوق در هشدار سه‌بوق BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_DURATION_MS 2000u

/**
 * @brief  [EN] Duration of the single critical BatteryRun beep, in milliseconds.
 *         [FA] مدت بوق بحرانی تک‌باره BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_DURATION_MS 10000u

/**
 * @brief  [EN] Pulse count for the standard BatteryRun warning.
 *         [FA] تعداد پالس در هشدار معمول BatteryRun.
 */
#define UI_BATTERY_RUN_BEEP_STANDARD_COUNT 1u

/**
 * @brief  [EN] Pulse count for the low-battery BatteryRun warning.
 *         [FA] تعداد پالس در هشدار باتری پایین BatteryRun.
 */
#define UI_BATTERY_RUN_BEEP_DOUBLE_COUNT 2u

/**
 * @brief  [EN] Pulse count for the critical BatteryRun warning above the empty threshold.
 *         [FA] تعداد پالس در هشدار بحرانی BatteryRun بالاتر از آستانه خالی.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_COUNT 3u

/**
 * @brief  [EN] Low gap between adjacent BatteryRun warning pulses, in milliseconds.
 *         [FA] فاصله خاموش بین بوق‌های متوالی BatteryRun، بر حسب میلی‌ثانیه.
 */
#define UI_BATTERY_RUN_BEEP_GAP_MS 100u

/* ==================== Battery percent hysteresis / هیسترزیس درصد باتری ==================== */

/**
 * @brief  [EN] Hysteresis for BatteryRun display/timing percent, in percent points.
 *         Stable percent changes only when raw percent differs by at least 2.
 *         Jitter 56↔57 or 57↔58 does not change blink timing; 57→55 or 57→59 does.
 *         This hysteresis applies only to BatteryRun green blink and buzzer timing, not to input, overvoltage or Low Battery Alarm.
 *         [FA] هیسترزیس درصد نمایش/زمان‌بندی BatteryRun، بر حسب واحد درصد.
 *         درصد پایدار فقط وقتی اختلاف درصد خام و پایدار حداقل 2 باشد تغییر می‌کند.
 */
#define UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT 2u

/**
 * @brief  [EN] Legacy alias kept for compatibility. Use UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT.
 *         [FA] نام قدیمی برای سازگاری؛ از UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT استفاده کن.
 */
#define UI_BATTERY_PERCENT_HYSTERESIS_PERCENT UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT

/**
 * @brief  [EN] Hysteresis for Charging yellow blink timing, in percent points.
 *         Charging stable percent changes only when raw differs by at least 5.
 *         Example: stable 57, raw 53..61 keeps 57; outside range moves to new raw.
 *         [FA] هیسترزیس زمان چشمک زرد شارژ، بر حسب واحد درصد.
 *         مثال: پایدار 57، خام 53 تا 61 همان 57 می‌ماند؛ خارج از محدوده به مقدار جدید می‌رود.
 */
#define UI_CHARGING_PERCENT_HYSTERESIS_PERCENT 5u

/**
 * @brief  [EN] Raw percent threshold to exit the critical 0% state.
 *         While stable is 0, it stays 0 until raw reaches at least 2; then it moves to 1 first, not directly to 2.
 *         [FA] آستانه درصد خام برای خروج از حالت بحرانی 0 درصد.
 *         تا وقتی پایدار 0 است، تا raw حداقل 2 نشده روی 0 می‌ماند؛ پس از خروج ابتدا به 1 می‌رود.
 */
#define UI_BATTERY_ZERO_EXIT_THRESHOLD        2u

/**
 * @brief  [EN] Raw percent threshold to exit the 1% state upward.
 *         While stable is 1: raw==0 → 0, raw>=3 → 2, otherwise keep 1. Prevents chatter between 0 and 1.
 *         [FA] آستانه خروج از حالت 1 درصد به سمت بالا.
 *         وقتی پایدار 1 است: raw 0 → 0، حداقل 3 → 2، otherwise 1 حفظ شود.
 */
#define UI_BATTERY_ONE_EXIT_THRESHOLD         3u

/**
 * @brief  [EN] Raw battery percent at which Charging may enter InputOk (full) state.
 *         InputOk is entered only when raw reaches 100%.
 *         [FA] درصد خام باتری که در آن Charging می‌تواند وارد حالت InputOk (فول) شود.
 *         ورود به InputOk فقط وقتی خام به 100٪ برسد مجاز است.
 */
#define UI_CHARGING_FULL_ENTER_PERCENT        100u

/**
 * @brief  [EN] Raw battery percent below which InputOk exits back to Charging.
 *         While InputOk is active it stays until raw falls below 95%.
 *         [FA] درصد خام باتری که پایین‌تر از آن InputOk به Charging برمی‌گردد.
 *         تا وقتی InputOk فعال است تا کمتر از 95٪ در همان حالت می‌ماند.
 */
#define UI_CHARGING_FULL_EXIT_PERCENT         95u

/* ==================== Low Battery Alarm / آلارم باتری کم ==================== */

/**
 * @brief  [EN] Battery voltage below which the UI low-battery alarm becomes active, in millivolts.
 *         When snapshot is valid and v_bat24_mv is below this threshold the UI asserts
 *         BOOL__G__UiBatteryAlarmIssued continuously until the clear threshold is reached.
 *         [FA] ولتاژی که پایین‌تر از آن آلارم کم‌بود باتری UI فعال می‌شود، بر حسب میلی‌ولت.
 * @note   [EN] Production battery voltage is taken ONLY from snapshot.v_bat24_mv.
 *         [FA] ولتاژ باتری تولید فقط از snapshot.v_bat24_mv خوانده می‌شود.
 */
#define UI_LOW_BATTERY_ALARM_THRESHOLD_MV 21000u

/**
 * @brief  [EN] Battery voltage at or above which the UI low-battery alarm is cleared, in millivolts.
 *         Provides hysteresis with the threshold (21000 -> 21200) to keep the flag continuous.
 *         [FA] ولتاژی که در آن یا بالاتر از آن آلارم کم‌بود باتری پاک می‌شود، بر حسب میلی‌ولت.
 */
#define UI_LOW_BATTERY_ALARM_CLEAR_MV   21200u

/* ==================== UI Global Battery Alarm Flag / فلگ سراسری آلارم باتری UI ==================== */

/**
 * @brief  [EN] Global flag owned by the UI: true while a valid low-battery alarm is active.
 *         Changeover reads it only; UI owns and updates it. Continuous level, not a pulse.
 *         [FA] فلگ سراسری در مالکیت UI: هنگام آلارم معتبر باتری کم مقدار true دارد.
 */
extern volatile bool BOOL__G__UiBatteryAlarmIssued;

/* ==================== Percentage constants / ثابت‌های درصد ==================== */

/**
 * @brief  [EN] Full battery percentage and upper bound of percentage calculations.
 *         [FA] درصد شارژ کامل و حد بالای محاسبات درصد.
 */
#define UI_PERCENT_FULL                 100u

/**
 * @brief  [EN] Scale used to convert a ratio into a percentage.
 *         [FA] مقیاس تبدیل نسبت به درصد.
 */
#define UI_PERCENT_SCALE                100u

/* ==================== RTOS tick / تیک RTOS ==================== */

/**
 * @brief  [EN] Base UI task delay in milliseconds.
 *         [FA] تأخیر پایه تسک UI بر حسب میلی‌ثانیه.
 */
#define UI_TICK_MS                      10u

/* ==================== Battery Voltage To Percent / تبدیل ولتاژ باتری به درصد ==================== */

/**
 * @brief  [EN] Convert battery voltage to percent 0..100. Non-linear broken into steps: range, offset, scaled, percent.
 *         [FA] تبدیل ولتاژ باتری به درصد - غیرخطی ۴ گام.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, range 0..40000mV, clamped / ولتاژ باتری میلی‌ولت
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func__Ui_BatteryVoltageToPercent(uint32_t uint32_t__batteryMv);

/* ==================== Ui Init / مقداردهی اولیه UI ==================== */

/**
 * @brief  [EN] Drive all UI outputs low (safe state).
 *         [FA] همه خروجی‌های UI خاموش (حالت امن).
 */
void func__Ui_Init(void);

/* ==================== Board Test Start / شروع تست برد ==================== */

/**
 * @brief  [EN] One-shot wiring check: red, yellow, green and the previous short buzzer check via the independent buzzer service.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز و بوق کوتاه قبلی با سرویس مستقل بوق.
 */
void func__Ui_BoardTest_Start(void);

/* ==================== Scenario InputOk / سناریوی ورودی عادی ==================== */

/**
 * @brief  [EN] InputOk: green steady, red/yellow/buzzer off. CMSIS-RTOS2 delay, MCU not locked.
 *         [FA] ورودی عادی: سبز ثابت، قرمز/زرد/بوق خاموش، ساده RTOS.
 */
void func__Ui_ScenarioInputOk(void);

/* ==================== Scenario Charging Tick / تیک سناریوی شارژ ==================== */

/**
 * @brief  [EN] Charging: green steady, yellow remaining to full non-linear (remainingPercent, periodPerPercent, yellowOnMs/offMs).
 *         CMSIS-RTOS2 delay.
 *         [FA] شارژ: سبز ثابت، زرد مانده تا فول غیرخطی، ساده RTOS.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV / ولتاژ باتری
 */
void func__Ui_ScenarioCharging_Tick(uint32_t uint32_t__batteryMv);

/* ==================== Scenario BatLost Tick / تیک سناریوی قطع باتری ==================== */

/**
 * @brief  [EN] BatLost: red fast blink + green steady + periodic beeps
 *         (defaults: three short beeps and a pause, UI_BAT_LOST_*; ids 44..49).
 *         Called every Ui pass while FAULT_CHARGER_BAT_LOST is latched; no latch lives in the UI.
 *         [FA] قطع باتری: قرمز چشمک‌تند + سبز ثابت + بوق دوره‌ای
 *         (پیش‌فرض سه بیپ کوتاه و مکث؛ ۴۴..۴۹)؛ تا
 *         وقتی پرچم متمرکز قفل است هر پاس صدا زده می‌شود؛ UI چیزی لچ نمی‌کند.
 */
void func__Ui_ScenarioBatLost_Tick(void);

/* ==================== Scenario BatteryRun Tick / تیک سناریوی دشارژ ==================== */

/**
 * @brief  [EN] BatteryRun: green blink from the runtime 74/75 percent map and four buzzer bands (50..53).
 *         Below the crit band (53) all LEDs turn off and the critical pattern (56/57/58/65)
 *         plays once for the latch length (61), then silence until the battery recovers.
 *         [FA] دشارژ: سبز بر اساس نگاشت درصد ۷۴/۷۵ و چهار بازه بوق (۵۰..۵۳) چشمک می‌زند.
 *         زیر باند بحرانی (۵۳) همهٔ LEDها خاموش و الگوی بحرانی (۵۶/۵۷/۵۸/۶۵) فقط یک‌بار
 *         به‌اندازهٔ طول یک‌باره (۶۱) پخش می‌شود، بعد سکوت تا برگشت باتری.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, 21000=0% 29000=100% / ولتاژ باتری
 */
void func__Ui_ScenarioBatteryRun_Tick(uint32_t uint32_t__batteryMv);

/* ==================== Ui Tick / تیک اصلی UI ==================== */

/**
 * @brief  [EN] Ui main tick - decides which scenario from the real Measurement snapshot.
 *         The snapshot is obtained via func__Measurement_GetSnapshot(); valid is checked
 *         before any decision. If invalid, UI enters safe-off, alarm flag is cleared and
 *         no stale/manual values are used. Battery production source is snapshot.v_bat24_mv only.
 *         [FA] تیک اصلی UI - تصمیم سناریو را از snapshot واقعی Measurement می‌گیرد.
 * @param  measurement_snapshot_t__snap [EN] Pointer to snapshot, may be NULL / اشاره‌گر snapshot
 */
void func__Ui_Tick(const measurement_snapshot_t *measurement_snapshot_t__snap);

/* ==================== Runtime UI cadence (v1.16) ==================== */
/* [EN] v1.16 (user order 2026-09-26: "draw the LEDs with real blinking,
 *      show the buzzer with a mute cross, and make every alarm number
 *      editable - ranges, beep times, beep counts"): the 39 UI_* numbers
 *      below become runtime ids 38..76, editable from the ESP panel,
 *      persisted to STM32 flash like the charge profile (~1.5 s debounce)
 *      and clamped as a set on every write. The macros stay as BOOT
 *      DEFAULTS only. Id 76 (mute) is panel-session only since v1.16b
 *      (RAM, cleared on reboot - the board returns to its own scenario);
 *      only the one-shot BoardTest wiring beep ignores the mute.
 * [FA] v1.16 (دستور کاربر ۲۰۲۶-۰۹-۲۶: «LEDها کشیده شوند با چشمک واقعی،
 *      بازر با ضربدر میوت نشان داده شود و همهٔ اعداد آلارم - بازه‌ها،
 *      زمان و تعداد بوق‌ها - قابل اصلاح باشند»): ۳۹ عدد UI_* شناسه‌های
 *      زمان‌اجرای ۳۸..۷۶ می‌شوند، از پنل ESP قابل اصلاح‌اند، مثل پروفایل
 *      شارژ روی فلش STM32 می‌مانند و با هر نوشتن به‌صورت مجموعه گیره
 *      می‌خورند. ماکروها فقط پیش‌فرض بوت می‌مانند. شناسهٔ ۷۶ (میوت) از
 *      v1.16b فقط جلسه‌ای است (RAM، با ریبوت پاک می‌شود - برد به سناریوی
 *      خودش برمی‌گردد)؛ فقط بوق یک‌بارهٔ تست سیم‌کشی برد میوت را نادیده
 *      می‌گیرد. */
#define UI_ALARM_PARAM_OV_LED_PERIOD_MS    38u  /* ms, 100..10000 */
#define UI_ALARM_PARAM_OV_LED_DUTY_PCT     39u  /* %, 0..100 */
#define UI_ALARM_PARAM_OV_BEEP_PERIOD_MS   40u  /* ms, 0=off else 1000..600000 */
#define UI_ALARM_PARAM_OV_BEEP_DUR_MS      41u  /* ms per beep, 0..window-fit vs 40/42/43 */
#define UI_ALARM_PARAM_OV_BEEP_COUNT       42u  /* n, 0..10, 0=off */
#define UI_ALARM_PARAM_OV_BEEP_GAP_MS      43u  /* ms, 0..5000, >=100 when 42>1 */
#define UI_ALARM_PARAM_BL_LED_PERIOD_MS    44u  /* ms, 100..10000 */
#define UI_ALARM_PARAM_BL_LED_DUTY_PCT     45u  /* %, 0..100 */
#define UI_ALARM_PARAM_BL_BEEP_PERIOD_MS   46u  /* ms, 0=off else 1000..600000 */
#define UI_ALARM_PARAM_BL_BEEP_DUR_MS      47u  /* ms per beep, def 233 (legacy 900 window shared), 0..fit vs 46/48/49 */
#define UI_ALARM_PARAM_BL_BEEP_COUNT       48u  /* n, 0..10, 0=off */
#define UI_ALARM_PARAM_BL_BEEP_GAP_MS      49u  /* ms, 0..5000, >=100 when 48>1 */
#define UI_ALARM_PARAM_RUN_BEEP_START_PCT  50u  /* %, 0..100, >= 51 */
#define UI_ALARM_PARAM_RUN_BEEP_DOUBLE_PCT 51u  /* %, 0..100, <= 50, >= 52 */
#define UI_ALARM_PARAM_RUN_BEEP_TRIPLE_PCT 52u  /* %, 0..100, <= 51, >= 53 */
#define UI_ALARM_PARAM_RUN_BEEP_CRIT_PCT   53u  /* %, 0..100, <= 52 */
#define UI_ALARM_PARAM_RUN_STD_INTERVAL_MS 54u  /* ms, 0=off else 1000..600000 */
#define UI_ALARM_PARAM_RUN_TRI_INTERVAL_MS 55u  /* ms, 0=off else 1000..600000 */
#define UI_ALARM_PARAM_RUN_CRIT_PERIOD_MS  56u  /* ms, 0=off else 1000..600000 */
#define UI_ALARM_PARAM_RUN_CRIT_DUTY_PCT   57u  /* %, 0..100 */
#define UI_ALARM_PARAM_RUN_CRIT_COUNT      58u  /* n, 0..10 */
#define UI_ALARM_PARAM_RUN_STD_DUR_MS      59u  /* ms per beep, 0..fit vs 54/62/63/65 */
#define UI_ALARM_PARAM_RUN_TRI_DUR_MS      60u  /* ms per beep, 0..fit vs 55/64/65 */
#define UI_ALARM_PARAM_RUN_CRIT_DUR_MS     61u  /* ms, 0..120000 one-shot latch length */
#define UI_ALARM_PARAM_RUN_STD_COUNT       62u  /* n, 0..10 */
#define UI_ALARM_PARAM_RUN_DOUBLE_COUNT    63u  /* n, 0..10 */
#define UI_ALARM_PARAM_RUN_TRI_COUNT       64u  /* n, 0..10 */
#define UI_ALARM_PARAM_RUN_GAP_MS          65u  /* ms, 0..5000, >=100 when any band count>1 */
#define UI_ALARM_PARAM_GREEN_PERIOD_MS     66u  /* ms, 100..10000 */
#define UI_ALARM_PARAM_GREEN_MIN_OFF_MS    67u  /* ms, 0..66 */
#define UI_ALARM_PARAM_YELLOW_PERIOD_MS    68u  /* ms, 100..10000 */
#define UI_ALARM_PARAM_YELLOW_MIN_OFF_MS   69u  /* ms, 0..68 */
#define UI_ALARM_PARAM_OV_THRESH_MV        70u  /* mV, 24000..32000 */
#define UI_ALARM_PARAM_OV_HYST_MV          71u  /* mV, 0..2000 */
#define UI_ALARM_PARAM_LOWBAT_THRESH_MV    72u  /* mV, 15000..24000, <= 73 */
#define UI_ALARM_PARAM_LOWBAT_CLEAR_MV     73u  /* mV, 15000..24000, >= 72 */
#define UI_ALARM_PARAM_PCT_VMIN_MV         74u  /* mV, 15000..25000, <= 75-100 */
#define UI_ALARM_PARAM_PCT_VMAX_MV         75u  /* mV, 25000..32000, >= 74+100 */
#define UI_ALARM_PARAM_BUZZER_MUTE         76u  /* 0/1, panel-session only (RAM); never persisted, cleared on reboot; scenarios only */
#define UI_ALARM_PARAM_MIN_ID              38u
#define UI_ALARM_PARAM_MAX_ID              76u

/**
 * @brief  [EN] Live UI cadence set (one struct, like the fault alarms).
 *              Scenarios read these, never the macros.
 *         [FA] مجموعهٔ زندهٔ اعداد UI (یک struct مثل آلارم‌های فالت).
 *              سناریوها این‌ها را می‌خوانند، نه ماکروها.
 */
typedef struct
{
    uint32_t uint32_t__ovLedPeriodMs;
    uint32_t uint32_t__ovLedDutyPct;
    uint32_t uint32_t__ovBeepPeriodMs;
    uint32_t uint32_t__ovBeepDurMs;
    uint32_t uint32_t__ovBeepCount;
    uint32_t uint32_t__ovBeepGapMs;
    uint32_t uint32_t__blLedPeriodMs;
    uint32_t uint32_t__blLedDutyPct;
    uint32_t uint32_t__blBeepPeriodMs;
    uint32_t uint32_t__blBeepDurMs;
    uint32_t uint32_t__blBeepCount;
    uint32_t uint32_t__blBeepGapMs;
    uint32_t uint32_t__runBeepStartPct;
    uint32_t uint32_t__runBeepDoublePct;
    uint32_t uint32_t__runBeepTriplePct;
    uint32_t uint32_t__runBeepCritPct;
    uint32_t uint32_t__runStdIntervalMs;
    uint32_t uint32_t__runTriIntervalMs;
    uint32_t uint32_t__runCritPeriodMs;
    uint32_t uint32_t__runCritDutyPct;
    uint32_t uint32_t__runCritCount;
    uint32_t uint32_t__runStdDurMs;
    uint32_t uint32_t__runTriDurMs;
    uint32_t uint32_t__runCritDurMs;
    uint32_t uint32_t__runStdCount;
    uint32_t uint32_t__runDoubleCount;
    uint32_t uint32_t__runTriCount;
    uint32_t uint32_t__runGapMs;
    uint32_t uint32_t__greenPeriodMs;
    uint32_t uint32_t__greenMinOffMs;
    uint32_t uint32_t__yellowPeriodMs;
    uint32_t uint32_t__yellowMinOffMs;
    uint32_t uint32_t__ovThreshMv;
    uint32_t uint32_t__ovHystMv;
    uint32_t uint32_t__lowBatThreshMv;
    uint32_t uint32_t__lowBatClearMv;
    uint32_t uint32_t__pctVminMv;
    uint32_t uint32_t__pctVmaxMv;
    uint32_t uint32_t__buzzerMute;
} ui_alarm_t;

/**
 * @brief  [EN] Write one UI cadence value (38..76): store, re-clamp the
 *              whole set, report the applied value.
 *         [FA] نوشتن یک عدد UI (۳۸..۷۶): ذخیره، گیرهٔ کل مجموعه، گزارش
 *              مقدار اعمال‌شده.
 * @param  uint8_t__paramId [EN] 38..76 / شناسه
 * @param  uint32_t__value [EN] Requested value / مقدار درخواستی
 * @param  uint32_t__appliedValue [EN] Applied value out / مقدار اعمال‌شده
 * @return bool [EN] true when the id is 38..76 / شناسه معتبر بود
 */
bool func__Ui_SetAlarmParam(uint8_t uint8_t__paramId,
                            uint32_t uint32_t__value,
                            uint32_t *uint32_t__appliedValue);

/**
 * @brief  [EN] Read one live UI cadence value (38..76).
 *         [FA] خواندن یک عدد زندهٔ UI (۳۸..۷۶).
 * @param  uint8_t__paramId [EN] 38..76 / شناسه
 * @param  uint32_t__value [EN] Value out / مقدار
 * @return bool [EN] true when the id is 38..76 / شناسه معتبر بود
 */
bool func__Ui_GetAlarmParam(uint8_t uint8_t__paramId,
                            uint32_t *uint32_t__value);

#endif /* UI_LED_H */
