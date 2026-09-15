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
 * @brief  [EN] One-shot wiring check: red, yellow, green, short beep using new buzzer pattern.
 *         [FA] تست یک‌باره سیم‌کشی: قرمز، زرد، سبز، بوق با تابع جدید.
 */
void func_Ui_BoardTest(void);

/**
 * @brief  [EN] Convert battery voltage to percent 0..100. 0%=21V 100%=28V.
 *         [FA] تبدیل ولتاژ باتری به درصد.
 * @param  uint32_t_batteryMv [EN] Battery voltage in mV, range 0..40000mV, clamped via Vmin/Vmax / ولتاژ باتری میلی‌ولت
 * @return uint8_t [EN] Percent 0..100 / درصد
 */
uint8_t func_Ui_BatteryVoltageToPercent(uint32_t uint32_t_batteryMv);

/* ===== Buzzer pattern - separate scenario (2 funcs) ===== */

/**
 * @brief  [EN] Buzzer pattern with gap in ms - separate scenario.
 *         Inputs: period (repeat time), onTime, repeat inside onTime, gap ms.
 *         If repeat=1, gap ignored. Example: onTime=1000ms repeat=2 gap=200ms => ON400 OFF200 ON400.
 *         If period=0 or period<=onTime, only pattern once, no extra off.
 *         [FA] الگوی بازر با گپ میلی‌ثانیه - سناریو جدا.
 *         ورودی: دوره تناوب، زمان روشن، تکرار داخل روشن، گپ میلی‌ثانیه. اگر تکرار ۱ بود گپ بکار نمی‌رود.
 * @param  uint32_t_periodMs [EN] Period between pattern starts, 0=once, range 0..60000ms / دوره تناوب تکرار بوق
 * @param  uint32_t_onTimeMs [EN] Total ON time including gaps, range 10..10000ms / زمان روشن بودن بوق
 * @param  uint8_t_repeatCount [EN] How many ON pulses inside onTime, 1..10, 1=continuous / تکرار زمان روشن بودن
 * @param  uint32_t_gapMs [EN] Gap between pulses ms, 0..5000ms, ignored if repeat=1 / گپ روشن بودن میلی‌ثانیه، اگر تکرار ۱ بود نادیده
 */
void func_Ui_BuzzerPatternMs(uint32_t uint32_t_periodMs, uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint32_t uint32_t_gapMs);

/**
 * @brief  [EN] Buzzer pattern with gap percent - separate scenario.
 *         gapMs = onTime * gapPercent /100, ignored if repeat=1.
 *         Example: onTime=1000ms repeat=2 gapPercent=20 => gap=200ms each beep 400ms.
 *         [FA] الگوی بازر با گپ درصدی - سناریو جدا.
 *         گپ = درصد * زمان روشن. اگر تکرار ۱ بود گپ حساب نمی‌شود.
 * @param  uint32_t_periodMs [EN] Period ms, 0=once, 0..60000 / دوره تناوب
 * @param  uint32_t_onTimeMs [EN] ON time ms including gaps, 10..10000 / زمان روشن
 * @param  uint8_t_repeatCount [EN] Repeat inside ON, 1..10 / تکرار داخل روشن
 * @param  uint8_t_gapPercent [EN] Gap percent of onTime, 0..90%, ignored if repeat=1 / گپ درصدی، اگر تکرار ۱ بود نادیده
 */
void func_Ui_BuzzerPatternPercent(uint32_t uint32_t_periodMs, uint32_t uint32_t_onTimeMs, uint8_t uint8_t_repeatCount, uint8_t uint8_t_gapPercent);

/**
 * @brief  [EN] Scenario InputOk: green steady, others off. One cycle = 500ms.
 *         [FA] سناریوی ورودی عادی: سبز ثابت.
 */
void func_Ui_ScenarioInputOk(void);

/**
 * @brief  [EN] Scenario BatteryRun: green blink, yellow OFF, smart beep using new buzzer pattern.
 *         [FA] سناریوی دشارژ: سبز چشمک، زرد خاموش، بوق هوشمند با تابع جدید بازر.
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
