/**
 * @file    ui.h
 * @brief   [EN] LED/buzzer scenarios: InputOk, BatteryRun (smart beep), Charging (yellow). All our functions have func_ prefix per AI rule.
 *          [FA] سناریوهای LED/بازر: ورودی عادی، دشارژ با بوق هوشمند، شارژ با زرد. همه توابع خودمان با پیشوند func_ طبق قانون AI.
 *
 * @note    [EN] Battery 0% = 21V (21000mV) = 0%, 100% = 28V (28000mV). Input present if V_in >=20V. Thresholds in ui_config.h single file.
 *          [FA] باتری صفر درصد ۲۱V، فول ۲۸V. ورودی وصل اگر >=۲۰V. آستانه‌ها در ui_config.h.
 */

#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  [EN] Drive all UI outputs low (safe state).
 *         [FA] همه خروجی‌های UI خاموش (حالت امن).
 */
void func_Ui_Init(void);

/**
 * @brief  [EN] One-shot wiring check: red, yellow, green, short beep.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز، بوق.
 */
void func_Ui_BoardTest(void);

/**
 * @brief  [EN] Separate buzzer beep - callable from any scenario.
 *         [FA] تابع جدا بازر - از هر سناریو قابل صدا زدن.
 * @param  uint32_t_durationMs [EN] Beep duration in ms, 0=use base 250ms. Range 0..5000ms / طول بوق میلی‌ثانیه
 */
void func_Ui_BuzzerBeep(uint32_t uint32_t_durationMs);

/* ===== Buzzer pattern - separate scenario with multiple functions ===== */

/**
 * @brief  [EN] Buzzer pattern once: beep inside onTime with repeat and gap in ms.
 *         If repeat=1, gap is ignored and buzzer stays ON for onTime.
 *         Example: onTime=1000ms, repeat=2, gap=200ms => ON 400ms OFF 200ms ON 400ms.
 *         [FA] الگوی بازر یک‌باره با گپ میلی‌ثانیه: داخل زمان روشن چند بار قطع و وصل.
 *         اگر تکرار ۱ بود گپ حساب نمی‌شود.
 * @param  uint32_t_onTimeMs [EN] Total ON time including gaps, range 10..10000ms / کل زمان روشن شامل گپ‌ها
 * @param  uint8_t_repeatCount [EN] How many ON pulses inside onTime, 1..10, 1=continuous / تعداد تکرار داخل روشن
 * @param  uint32_t_gapMs [EN] Gap between pulses in ms, range 0..5000ms, ignored if repeat=1 / گپ بین بوق‌ها میلی‌ثانیه، اگر تکرار ۱ بود نادیده
 */
void func_Ui_BuzzerPatternOnceMs(uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint32_t uint32_t_gapMs);

/**
 * @brief  [EN] Buzzer pattern once: gap as percent of onTime.
 *         gapMs = onTime * gapPercent / 100. If repeat=1 gap ignored.
 *         Example: onTime=1000ms, repeat=2, gapPercent=20 => gap=200ms, each beep 400ms.
 *         [FA] الگوی بازر یک‌باره با گپ درصدی: گپ = درصد * زمان روشن.
 *         اگر تکرار ۱ بود گپ بکار نمی‌رود.
 * @param  uint32_t_onTimeMs [EN] Total ON time including gaps, 10..10000ms / کل زمان روشن
 * @param  uint8_t_repeatCount [EN] Repeat inside ON, 1..10, 1=continuous / تکرار داخل روشن
 * @param  uint8_t_gapPercent [EN] Gap as percent of onTime, 0..90%, ignored if repeat=1 / گپ درصدی از کل زمان روشن
 */
void func_Ui_BuzzerPatternOncePercent(uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint8_t uint8_t_gapPercent);

/**
 * @brief  [EN] Buzzer pattern periodic: one cycle = pattern (onTime with repeats) + off = period-onTime.
 *         If period=0 or period<=onTime, only pattern once, no extra off.
 *         If repeat=1 gap ignored.
 *         [FA] الگوی بازر دوره‌ای: یک سیکل = الگو + خاموشی تا دوره کامل شود.
 *         اگر دوره صفر یا کوچکتر از زمان روشن بود فقط یک‌بار الگو اجرا می‌شود.
 * @param  uint32_t_periodMs [EN] Period between pattern starts, 0=once, range 0..60000ms / دوره تناوب تکرار بوق
 * @param  uint32_t_onTimeMs [EN] Total ON time including gaps, 10..10000ms / زمان روشن بودن بوق
 * @param  uint8_t_repeatCount [EN] Repeat inside ON, 1..10 / تکرار زمان روشن بودن
 * @param  uint32_t_gapMs [EN] Gap between pulses ms, 0..5000ms, ignored if repeat=1 / گپ روشن بودن میلی‌ثانیه
 */
void func_Ui_BuzzerPatternPeriodicMs(uint32_t uint32_t_periodMs, uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint32_t uint32_t_gapMs);

/**
 * @brief  [EN] Buzzer pattern periodic with gap percent.
 *         gapMs = onTime * gapPercent /100, ignored if repeat=1.
 *         [FA] الگوی بازر دوره‌ای با گپ درصدی.
 * @param  uint32_t_periodMs [EN] Period ms, 0=once, 0..60000 / دوره تناوب
 * @param  uint32_t_onTimeMs [EN] ON time including gaps, 10..10000ms / زمان روشن
 * @param  uint8_t_repeatCount [EN] Repeat inside ON, 1..10 / تکرار داخل روشن
 * @param  uint8_t_gapPercent [EN] Gap percent of onTime, 0..90%, ignored if repeat=1 / گپ درصدی
 */
void func_Ui_BuzzerPatternPeriodicPercent(uint32_t uint32_t_periodMs, uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint8_t uint8_t_gapPercent);

/**
 * @brief  [EN] Buzzer pattern repeated N times with period.
 *         Calls periodic pattern repeatTimes times. If repeatTimes=0, once.
 *         [FA] الگوی بازر N بار تکرار با دوره.
 * @param  uint32_t_periodMs [EN] Period ms, 0..60000 / دوره تناوب
 * @param  uint32_t_onTimeMs [EN] ON time ms, 10..10000 / زمان روشن
 * @param  uint8_t_repeatCount [EN] Repeat inside ON, 1..10 / تکرار داخل روشن
 * @param  uint32_t_gapMs [EN] Gap ms, 0..5000, ignored if repeat=1 / گپ
 * @param  uint32_t_repeatTimes [EN] How many periods to repeat, 0=1, 1..1000 / تعداد دفعات تکرار دوره
 */
void func_Ui_BuzzerPatternRepeatMs(uint32_t uint32_t_periodMs, uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint32_t uint32_t_gapMs, uint32_t uint32_t_repeatTimes);

/**
 * @brief  [EN] Same as RepeatMs but gap as percent.
 *         [FA] تکرار با گپ درصدی.
 * @param  uint32_t_periodMs [EN] Period ms / دوره
 * @param  uint32_t_onTimeMs [EN] ON time ms / زمان روشن
 * @param  uint8_t_repeatCount [EN] Repeat inside ON / تکرار داخل روشن
 * @param  uint8_t_gapPercent [EN] Gap percent / گپ درصدی
 * @param  uint32_t_repeatTimes [EN] Repeat times / تعداد تکرار
 */
void func_Ui_BuzzerPatternRepeatPercent(uint32_t uint32_t_periodMs, uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint8_t uint8_t_gapPercent, uint32_t uint32_t_repeatTimes);

/**
 * @brief  [EN] Convert battery voltage to percent 0..100. 0%=21V 100%=28V.
 *         [FA] تبدیل ولتاژ باتری به درصد.
 * @param  uint32_t_batteryMv [EN] Battery voltage in mV, range 0..40000mV, clamped via Vmin/Vmax / ولتاژ باتری میلی‌ولت
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func_Ui_BatteryVoltageToPercent(uint32_t uint32_t_batteryMv);

/**
 * @brief  [EN] Scenario InputOk: green steady, others off. One cycle = 500ms.
 *         [FA] سناریوی ورودی عادی: سبز ثابت.
 */
void func_Ui_ScenarioInputOk(void);

/**
 * @brief  [EN] Scenario BatteryRun: green blink, yellow OFF, smart beep.
 *         [FA] سناریوی دشارژ: سبز چشمک، زرد خاموش، بوق هوشمند.
 * @param  uint32_t_batteryMv [EN] Battery voltage mV, 21000=0% 28000=100%, range 21000..28000 / ولتاژ باتری
 */
void func_Ui_ScenarioBatteryRun(uint32_t uint32_t_batteryMv);

/**
 * @brief  [EN] Scenario Charging: green steady, yellow remaining to full. 0%=ON 100%=OFF.
 *         [FA] سناریوی شارژ: سبز ثابت، زرد مانده تا فول.
 * @param  uint32_t_batteryMv [EN] Battery voltage mV, 21000=0% ON, 28000=100% OFF, range 21000..28000 / ولتاژ باتری
 */
void func_Ui_ScenarioCharging(uint32_t uint32_t_batteryMv);

#endif /* UI_H */
