/**
 * @file    ui_buzzer.h
 * @brief   [EN] One public UI buzzer service: periodic pattern from period, duty, count, and gap.
 *          [FA] یک سرویس عمومی بوق UI: الگوی دوره‌ای بر اساس دوره، دیوتی، تعداد و گپ.
 *
 * @note    [EN] The return value is the recommended next-call delay in milliseconds.
 *          [FA] مقدار بازگشتی، زمان پیشنهادی مراجعه بعدی بر حسب میلی‌ثانیه است.
 */

#ifndef UI_BUZZER_H
#define UI_BUZZER_H

/* ==================== Includes ==================== */

#include <stdint.h>

/* ==================== Buzzer / Beep constants ==================== */

#define UI_BUZZER_PERCENT_SCALE         100u    /* [EN] Duty percentage scale / مقیاس درصد دیوتی */
#define UI_BUZZER_DUTY_MAX_PERCENT      100u    /* [EN] Maximum valid duty / بیشترین دیوتی مجاز */
#define UI_BUZZER_MIN_PERIOD_MS         1000u   /* [EN] Minimum valid period / کمترین دوره مجاز */
#define UI_BUZZER_MIN_GAP_MS            100u    /* [EN] Minimum gap when count is above one / کمترین گپ برای تعداد بیشتر از یک */
#define UI_BUZZER_CHECK_PERCENT         10u     /* [EN] Recommended check fraction / درصد پیشنهادی مراجعه */
#define UI_BUZZER_MIN_CHECK_MS          1u      /* [EN] Minimum valid next-call delay / کمترین تأخیر مراجعه بعدی */
#define UI_BUZZER_OFF_RESULT            0       /* [EN] Valid disabled pattern / الگوی خاموش معتبر */
#define UI_BUZZER_INVALID_RESULT        (-1)    /* [EN] Invalid configuration / تنظیمات نامعتبر */

/* ==================== Legacy default constants kept for configuration ==================== */

#define UI_BOOT_BEEP_MS                 150u    /* [EN] Reserved boot-test default / پیش‌فرض رزرو تست راه‌اندازی */
#define UI_BEEP_BASE_MS                 250u    /* [EN] Reserved explicit-beep default / پیش‌فرض رزرو بوق صریح */
#define UI_BEEP_DOUBLE_THRESH_PCT       20u     /* [EN] Reserved threshold / آستانه رزرو */
#define UI_BEEP_START_PCT               50u     /* [EN] Reserved threshold / آستانه رزرو */
#define UI_BEEP_MIN_INTERVAL_CYCLES     1u      /* [EN] Minimum interval when a caller chooses zero percent / حداقل فاصله رزرو */
#define UI_BUZZER_DEFAULT_GAP_PERCENT   20u     /* [EN] Reserved default for a future caller / پیش‌فرض رزرو گپ */

/* ==================== Shared UI tick ==================== */

#define UI_TICK_MS                      10u     /* [EN] Application service tick / تیک سرویس برنامه */

/* ==================== Buzzer service ==================== */

/**
 * @brief  [EN] Service one periodic buzzer pattern without blocking the task.
 *         The pattern repeats every periodMs. The duty window is periodMs*dutyPercent/100.
 *         beepCount pulses share that duty window; gapMs is inserted between adjacent pulses.
 *         A zero period, zero duty, or zero count turns the buzzer off and returns
 *         UI_BUZZER_OFF_RESULT. A non-zero period below UI_BUZZER_MIN_PERIOD_MS,
 *         an out-of-range duty, a too-small gap for multiple pulses, or a pattern
 *         without positive time for every pulse turns the buzzer off and returns
 *         UI_BUZZER_INVALID_RESULT. Valid patterns return the recommended next-call
 *         delay: UI_BUZZER_CHECK_PERCENT of the smallest positive pattern segment.
 *         [FA] یک الگوی دوره‌ای بوق را بدون قفل کردن تسک اجرا می‌کند.
 *         الگو هر periodMs تکرار می‌شود و پنجره دیوتی برابر periodMs*dutyPercent/100 است.
 *         تعداد beepCount بوق این پنجره را تقسیم می‌کنند و gapMs بین بوق‌های مجاور قرار می‌گیرد.
 *         دوره صفر، دیوتی صفر یا تعداد صفر، بوق را خاموش و UI_BUZZER_OFF_RESULT را برمی‌گرداند.
 *         دوره غیرصفر کمتر از UI_BUZZER_MIN_PERIOD_MS، دیوتی خارج از محدوده،
 *         گپ کمتر از حد مجاز برای چند بوق، یا نبود زمان مثبت برای همه بوق‌ها،
 *         بوق را خاموش و UI_BUZZER_INVALID_RESULT را برمی‌گرداند.
 *         در الگوی معتبر، زمان مراجعه بعدی برابر UI_BUZZER_CHECK_PERCENT درصد
 *         کوچک‌ترین بخش مثبت الگو است.
 * @param  uint32_t__periodMs [EN] Pattern period in ms / دوره الگو بر حسب ms
 * @param  uint8_t__dutyPercent [EN] Duty window 0..100 percent / پنجره دیوتی از صفر تا صد درصد
 * @param  uint8_t__beepCount [EN] Number of pulses; zero disables / تعداد پالس؛ صفر یعنی خاموش
 * @param  uint32_t__gapMs [EN] Low gap between adjacent pulses in ms / گپ خاموش بین پالس‌های مجاور بر حسب ms
 * @return int32_t [EN] Next-call delay, zero for valid off, or -1 for invalid configuration.
 *         [FA] زمان مراجعه بعدی، صفر برای خاموشی معتبر، یا منفی یک برای تنظیم نامعتبر.
 */
int32_t func__Ui_Buzzer_Tick(uint32_t uint32_t__periodMs, uint8_t uint8_t__dutyPercent, uint8_t uint8_t__beepCount, uint32_t uint32_t__gapMs);

#endif /* UI_BUZZER_H */
