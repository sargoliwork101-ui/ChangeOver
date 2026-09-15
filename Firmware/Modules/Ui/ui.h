/**
 * @file    ui.h
 * @brief   [EN] LED/buzzer scenarios - single source for all UI thresholds (ui_config.h removed per user request).
 *          Fully RTOS, no delay inside module, non-blocking state machine.
 *          [FA] سناریوهای LED/بازر - تک فایل برای همه آستانه‌ها (ui_config.h حذف شد). کاملاً RTOS بدون delay.
 *
 * @note    [EN] Battery 0% = 21V (21000mV) = 0%, 100% = 28V (28000mV). Input present if V_in >=20V.
 *          Naming: after type double underscore __, func__ prefix, e.g., uint32_t__batteryMv, func__Ui_Init.
 *          [FA] باتری صفر درصد ۲۱V، فول ۲۸V. نام‌گذاری با __ بعد تایپ.
 */

#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== Battery voltage mapping ==================== */
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

/* ==================== Buzzer Pattern ==================== */
#define UI_BUZZER_DEFAULT_GAP_PERCENT   20u     /* [EN] Default gap 20% of onTime when repeat>1 / گپ پیش‌فرض ۲۰٪ */

/* ==================== Percent helpers ==================== */
#define UI_PERCENT_FULL                 100u
#define UI_PERCENT_SCALE                100u

/* ==================== RTOS tick ==================== */
#define UI_TICK_MS                      10u     /* [EN] Ui task tick 10ms, non-blocking / تیکه ۱۰ میلی‌ثانیه */

/**
 * @brief  [EN] Drive all UI outputs low (safe state).
 *         [FA] همه خروجی‌های UI خاموش (حالت امن).
 */
void func__Ui_Init(void);

/**
 * @brief  [EN] One-shot wiring check: red, yellow, green, short beep using new buzzer pattern (non-blocking state machine).
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز، بوق با تابع جدید (استیت ماشین بدون delay).
 */
void func__Ui_BoardTest_Start(void);

/**
 * @brief  [EN] Board test tick - call every UI_TICK_MS, non-blocking.
 *         [FA] تیکه تست برد - هر ۱۰ms صدا بزن، بدون delay.
 * @return bool [EN] true=still running, false=finished / در حال اجرا یا تمام
 */
bool func__Ui_BoardTest_Tick(void);

/**
 * @brief  [EN] Convert battery voltage to percent 0..100. 0%=21V 100%=28V.
 *         [FA] تبدیل ولتاژ باتری به درصد.
 * @param  uint32_t__batteryMv [EN] Battery voltage in mV, range 0..40000mV, clamped / ولتاژ باتری میلی‌ولت
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func__Ui_BatteryVoltageToPercent(uint32_t uint32_t__batteryMv);

/* ===== Buzzer pattern - separate scenario, non-blocking ===== */

/**
 * @brief  [EN] Buzzer pattern with gap ms - start pattern.
 *         Inputs: period (repeat time), onTime, repeat inside onTime, gap ms.
 *         If repeat=1 gap ignored. Example: onTime=1000ms repeat=2 gap=200ms => ON400 OFF200 ON400.
 *         Non-blocking: call Start once, then Tick every 10ms.
 *         [FA] الگوی بازر با گپ میلی‌ثانیه - شروع الگو، بدون delay.
 * @param  uint32_t__periodMs [EN] Period between pattern starts, 0=once, 0..60000ms / دوره تناوب
 * @param  uint32_t__onTimeMs [EN] Total ON including gaps, 10..10000ms / زمان روشن بودن بوق
 * @param  uint8_t__repeatCount [EN] Repeat inside ON 1..10 / تکرار زمان روشن بودن
 * @param  uint32_t__gapMs [EN] Gap ms 0..5000, ignored if repeat=1 / گپ میلی‌ثانیه
 */
void func__Ui_BuzzerPatternMs_Start(uint32_t uint32_t__periodMs, uint32_t uint32_t__onTimeMs, uint8_t uint8_t__repeatCount, uint32_t uint32_t__gapMs);

/**
 * @brief  [EN] Buzzer pattern tick - call every UI_TICK_MS.
 *         [FA] تیکه الگوی بازر - هر ۱۰ms صدا بزن.
 * @return bool [EN] true=still running, false=finished / در حال اجرا یا تمام
 */
bool func__Ui_BuzzerPatternMs_Tick(void);

/**
 * @brief  [EN] Buzzer pattern with gap percent - start.
 *         [FA] الگوی بازر با گپ درصدی - شروع.
 * @param  uint32_t__periodMs [EN] Period ms / دوره تناوب
 * @param  uint32_t__onTimeMs [EN] ON time ms / زمان روشن
 * @param  uint8_t__repeatCount [EN] Repeat inside ON / تکرار داخل روشن
 * @param  uint8_t__gapPercent [EN] Gap percent 0..90, ignored if repeat=1 / گپ درصدی
 */
void func__Ui_BuzzerPatternPercent_Start(uint32_t uint32_t__periodMs, uint32_t uint32_t__onTimeMs, uint8_t uint8_t__repeatCount, uint8_t uint8_t__gapPercent);

/**
 * @brief  [EN] Buzzer pattern percent tick.
 *         [FA] تیکه الگوی بازر درصدی.
 * @return bool [EN] true=running / در حال اجرا
 */
bool func__Ui_BuzzerPatternPercent_Tick(void);

/**
 * @brief  [EN] Stop buzzer pattern immediately.
 *         [FA] توقف فوری الگوی بازر.
 */
void func__Ui_BuzzerPattern_Stop(void);

/* ===== Scenarios - non-blocking ===== */

/**
 * @brief  [EN] InputOk: green steady, others off. Non-blocking, call once or every tick.
 *         [FA] ورودی عادی: سبز ثابت، بدون delay.
 */
void func__Ui_ScenarioInputOk(void);

/**
 * @brief  [EN] BatteryRun: green blink, yellow OFF, smart beep using new buzzer pattern. Non-blocking tick.
 *         Must be called every UI_TICK_MS with current battery voltage.
 *         [FA] دشارژ: سبز چشمک، زرد خاموش، بوق هوشمند با تابع جدید، بدون delay، هر ۱۰ms صدا بزن.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, 21000=0% 28000=100% / ولتاژ باتری
 */
void func__Ui_ScenarioBatteryRun_Tick(uint32_t uint32_t__batteryMv);

/**
 * @brief  [EN] Charging: green steady, yellow remaining to full. Non-blocking tick.
 *         [FA] شارژ: سبز ثابت، زرد مانده تا فول، بدون delay.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV / ولتاژ باتری
 */
void func__Ui_ScenarioCharging_Tick(uint32_t uint32_t__batteryMv);

/**
 * @brief  [EN] Ui main tick - decides which scenario based on input and battery, non-blocking.
 *         Call every UI_TICK_MS from task.
 *         [FA] تیکه اصلی UI - تصمیم سناریو بر اساس ورودی و باتری، بدون delay، هر ۱۰ms.
 * @param  uint32_t__inputVoltageMv [EN] Input voltage mV / ولتاژ ورودی
 * @param  uint32_t__batteryVoltageMv [EN] Battery voltage mV / ولتاژ باتری
 */
void func__Ui_Tick(uint32_t uint32_t__inputVoltageMv, uint32_t uint32_t__batteryVoltageMv);

#endif /* UI_H */
