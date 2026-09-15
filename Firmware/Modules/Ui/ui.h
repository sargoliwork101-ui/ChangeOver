/**
 * @file    ui.h
 * @brief   [EN] LED/buzzer scenarios - single source for all UI thresholds (ui_config.h removed).
 *          Fully RTOS simple & readable, non-linear formulas broken into steps.
 *          [FA] سناریوهای LED/بازر - تک فایل برای همه آستانه‌ها، RTOS ساده خوانا، فرمول غیرخطی.
 *
 * @note    [EN] Battery 0% = 21V (21000mV) = 0%, 100% = 28V (28000mV). Input present if V_in >=20V.
 *          Naming: after type double underscore __, func__ prefix, e.g., uint32_t__batteryMv, func__Ui_Init.
 *          RTOS: vTaskDelay allowed (does not lock MCU), HAL_Delay forbidden. Formulas non-linear.
 *          [FA] باتری 0% 21V، 100% 28V. نام‌گذاری با __ بعد تایپ. RTOS ساده با vTaskDelay.
 */

#ifndef UI_H
#define UI_H

/* ==================== Includes ==================== */

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

#define UI_TICK_MS                      10u     /* [EN] Ui task tick 10ms, simple RTOS / تیکه ۱۰ میلی‌ثانیه */

/* ==================== Ui Init ==================== */

/**
 * @brief  [EN] Drive all UI outputs low (safe state).
 *         [FA] همه خروجی‌های UI خاموش (حالت امن).
 */
void func__Ui_Init(void);

/* ==================== Board Test Start ==================== */

/**
 * @brief  [EN] One-shot wiring check: red, yellow, green, short beep using new buzzer pattern, RTOS simple with vTaskDelay.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز، بوق با vTaskDelay ساده.
 */
void func__Ui_BoardTest_Start(void);

/* ==================== Board Test Tick ==================== */

/**
 * @brief  [EN] Board test tick - for compatibility, returns false (test done in Start).
 *         [FA] تیکه تست برد - برای سازگاری false برمی‌گرداند.
 * @return bool [EN] true=still running, false=finished / در حال اجرا یا تمام
 */
bool func__Ui_BoardTest_Tick(void);

/* ==================== Battery Voltage To Percent ==================== */

/**
 * @brief  [EN] Convert battery voltage to percent 0..100. Non-linear broken into steps: range, offset, scaled, percent.
 *         [FA] تبدیل ولتاژ باتری به درصد - غیرخطی ۴ گام.
 * @param  uint32_t__batteryMv [EN] Battery voltage in mV, range 0..40000mV, clamped / ولتاژ باتری میلی‌ولت
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func__Ui_BatteryVoltageToPercent(uint32_t uint32_t__batteryMv);

/* ==================== Buzzer Pattern Ms Start ==================== */

/**
 * @brief  [EN] Buzzer pattern with gap ms - start pattern. Non-linear gap handling.
 *         Inputs: period (repeat time), onTime, repeat inside onTime, gap ms. If repeat=1 gap ignored.
 *         Example: onTime=1000ms repeat=2 gap=200ms => ON400 OFF200 ON400. RTOS simple.
 *         [FA] الگوی بازر با گپ میلی‌ثانیه - شروع الگو، غیرخطی.
 * @param  uint32_t__periodMs [EN] Period between pattern starts, 0=once, 0..60000ms / دوره تناوب
 * @param  uint32_t__onTimeMs [EN] Total ON including gaps, 10..10000ms / زمان روشن بودن بوق
 * @param  uint8_t__repeatCount [EN] Repeat inside ON 1..10 / تکرار زمان روشن بودن
 * @param  uint32_t__gapMs [EN] Gap ms 0..5000, ignored if repeat=1 / گپ میلی‌ثانیه
 */
void func__Ui_BuzzerPatternMs_Start(uint32_t uint32_t__periodMs, uint32_t uint32_t__onTimeMs, uint8_t uint8_t__repeatCount, uint32_t uint32_t__gapMs);

/* ==================== Buzzer Pattern Ms Tick ==================== */

/**
 * @brief  [EN] Buzzer pattern tick - call every UI_TICK_MS, non-blocking.
 *         [FA] تیکه الگوی بازر - هر ۱۰ms صدا بزن.
 * @return bool [EN] true=still running, false=finished / در حال اجرا یا تمام
 */
bool func__Ui_BuzzerPatternMs_Tick(void);

/* ==================== Buzzer Pattern Percent Start ==================== */

/**
 * @brief  [EN] Buzzer pattern with gap percent - start. Non-linear: gap = onTime*percent/100 broken into steps.
 *         [FA] الگوی بازر با گپ درصدی - شروع، غیرخطی.
 * @param  uint32_t__periodMs [EN] Period ms / دوره تناوب
 * @param  uint32_t__onTimeMs [EN] ON time ms / زمان روشن
 * @param  uint8_t__repeatCount [EN] Repeat inside ON / تکرار داخل روشن
 * @param  uint8_t__gapPercent [EN] Gap percent 0..90, ignored if repeat=1 / گپ درصدی
 */
void func__Ui_BuzzerPatternPercent_Start(uint32_t uint32_t__periodMs, uint32_t uint32_t__onTimeMs, uint8_t uint8_t__repeatCount, uint8_t uint8_t__gapPercent);

/* ==================== Buzzer Pattern Percent Tick ==================== */

/**
 * @brief  [EN] Buzzer pattern percent tick.
 *         [FA] تیکه الگوی بازر درصدی.
 * @return bool [EN] true=running / در حال اجرا
 */
bool func__Ui_BuzzerPatternPercent_Tick(void);

/* ==================== Buzzer Pattern Stop ==================== */

/**
 * @brief  [EN] Stop buzzer pattern immediately.
 *         [FA] توقف فوری الگوی بازر.
 */
void func__Ui_BuzzerPattern_Stop(void);

/* ==================== Scenario InputOk ==================== */

/**
 * @brief  [EN] InputOk: green steady, others off. RTOS simple with vTaskDelay, MCU not locked.
 *         [FA] ورودی عادی: سبز ثابت، ساده RTOS.
 */
void func__Ui_ScenarioInputOk(void);

/* ==================== Scenario BatteryRun Tick ==================== */

/**
 * @brief  [EN] BatteryRun: green blink non-linear (remainingPercent, periodPerPercent, greenOnMs/offMs), yellow OFF, smart beep.
 *         RTOS simple with vTaskDelay.
 *         [FA] دشارژ: سبز چشمک غیرخطی، زرد خاموش، بوق هوشمند، ساده RTOS.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV, 21000=0% 28000=100% / ولتاژ باتری
 */
void func__Ui_ScenarioBatteryRun_Tick(uint32_t uint32_t__batteryMv);

/* ==================== Scenario Charging Tick ==================== */

/**
 * @brief  [EN] Charging: green steady, yellow remaining to full non-linear (remainingPercent, periodPerPercent, yellowOnMs/offMs).
 *         RTOS simple with vTaskDelay.
 *         [FA] شارژ: سبز ثابت، زرد مانده تا فول غیرخطی، ساده RTOS.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV / ولتاژ باتری
 */
void func__Ui_ScenarioCharging_Tick(uint32_t uint32_t__batteryMv);

/* ==================== Ui Tick ==================== */

/**
 * @brief  [EN] Ui main tick - decides which scenario based on input and battery, RTOS simple readable.
 *         Call every UI_TICK_MS from task.
 *         [FA] تیکه اصلی UI - تصمیم سناریو بر اساس ورودی و باتری، ساده خوانا.
 * @param  uint32_t__inputVoltageMv [EN] Input voltage mV / ولتاژ ورودی
 * @param  uint32_t__batteryVoltageMv [EN] Battery voltage mV / ولتاژ باتری
 */
void func__Ui_Tick(uint32_t uint32_t__inputVoltageMv, uint32_t uint32_t__batteryVoltageMv);

#endif /* UI_H */
