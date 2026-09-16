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

/* ==================== Includes / شامل‌ها ==================== */

#include <stdint.h>

/* ==================== Buzzer timing constants / ثابت‌های زمانی بازر ==================== */

/**
 * @brief  [EN] Percentage scale used by duty and check calculations.
 *         100 means that a duty value is expressed as a percentage.
 *         [FA] مقیاس درصد برای محاسبه دیوتی و زمان مراجعه.
 *         مقدار ۱۰۰ یعنی ورودی دیوتی به‌صورت درصد بیان می‌شود.
 */
#define UI_BUZZER_PERCENT_SCALE         100u

/**
 * @brief  [EN] Maximum accepted duty-window percentage.
 *         Values above this limit are invalid and turn the buzzer off.
 *         [FA] بیشترین درصد مجاز پنجره دیوتی.
 *         مقدار بیشتر از این حد نامعتبر است و بوق را خاموش می‌کند.
 */
#define UI_BUZZER_DUTY_MAX_PERCENT      100u

/**
 * @brief  [EN] Minimum accepted non-zero complete pattern period in milliseconds.
 *         This prevents very fast periodic switching of the buzzer GPIO.
 *         [FA] کمترین دوره کامل غیرصفر الگو بر حسب میلی‌ثانیه.
 *         این حد از سوئیچ سریع پایه GPIO بوق جلوگیری می‌کند.
 */
#define UI_BUZZER_MIN_PERIOD_MS         1000u

/**
 * @brief  [EN] Minimum gap between adjacent pulses when beepCount is above one.
 *         A single pulse has no adjacent gap and does not use this limit.
 *         [FA] کمترین گپ بین پالس‌های مجاور وقتی تعداد بوق بیشتر از یک است.
 *         یک بوق گپ مجاور ندارد و این محدودیت را استفاده نمی‌کند.
 */
#define UI_BUZZER_MIN_GAP_MS            100u

/**
 * @brief  [EN] Percentage of the smallest positive pattern segment used to recommend the next RTOS check.
 *         [FA] درصد کوچک‌ترین بخش مثبت الگو برای پیشنهاد زمان مراجعه بعدی RTOS.
 */
#define UI_BUZZER_CHECK_PERCENT         10u

/**
 * @brief  [EN] Minimum positive delay returned to a caller between buzzer checks.
 *         [FA] کمترین تأخیر مثبت که بین دو بررسی بوق به caller برگردانده می‌شود.
 */
#define UI_BUZZER_MIN_CHECK_MS          1u

/* ==================== Buzzer result constants / ثابت‌های نتیجه بازر ==================== */

/**
 * @brief  [EN] Return value for a valid command that intentionally disables the buzzer.
 *         [FA] مقدار بازگشتی برای فرمان معتبر خاموش‌کردن عمدی بوق.
 */
#define UI_BUZZER_OFF_RESULT            0

/**
 * @brief  [EN] Return value for an unsafe or invalid buzzer configuration.
 *         The GPIO is forced LOW when this result is returned.
 *         [FA] مقدار بازگشتی برای تنظیمات ناامن یا نامعتبر بوق.
 *         هنگام این نتیجه، GPIO بوق روی LOW قرار می‌گیرد.
 */
#define UI_BUZZER_INVALID_RESULT        (-1)

/* ==================== BoardTest compatibility constant / ثابت سازگاری تست برد ==================== */

/**
 * @brief  [EN] Default one-shot beep duration used by the BoardTest scenario.
 *         APP_CONFIG copies this default and the scenario converts it to a valid duty window.
 *         [FA] مدت پیش‌فرض بوق تک‌باره در سناریوی تست برد.
 *         APP_CONFIG این پیش‌فرض را کپی می‌کند و سناریو آن را به پنجره دیوتی معتبر تبدیل می‌کند.
 */
#define UI_BOOT_BEEP_MS                 150u

/* ==================== Buzzer service / سرویس بازر ==================== */

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
