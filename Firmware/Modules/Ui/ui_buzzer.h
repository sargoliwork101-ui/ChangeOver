/**
 * @file    ui_buzzer.h
 * @brief   [EN] UI buzzer patterns - separate from LED, constants for buzzer in its own header per user request.
 *          Non-linear formulas broken into steps, RTOS simple readable, markers above each function.
 *          [FA] الگوهای بازر ماژول UI - ثابت‌های بازر در هدر خودش، فرمول غیرخطی، RTOS ساده.
 *
 * @note    [EN] Buzzer defaults live in this header; app_config.c copies tunable values into const APP_CONFIG. Fixed pattern-policy constants stay here. Naming __ after type, func__ prefix.
 *          RTOS: vTaskDelay allowed, HAL_Delay forbidden. Formulas broken into steps.
 *          [FA] پیش‌فرض‌های بازر در این هدر هستند؛ app_config.c مقدارهای قابل تنظیم را به APP_CONFIG ثابت منتقل می‌کند و ثابت‌های سیاست الگو همین‌جا می‌مانند.
 */

#ifndef UI_BUZZER_H
#define UI_BUZZER_H

/* ==================== Includes ==================== */

#include <stdint.h>
#include <stdbool.h>

/* ==================== Buzzer / Beep ==================== */

#define UI_BOOT_BEEP_MS                 150u    /* [EN] Board test beep / بوق تست برد */
#define UI_BEEP_BASE_MS                 250u    /* [EN] Base beep / طول بوق پایه */
#define UI_BEEP_DOUBLE_THRESH_PCT       20u     /* [EN] Below this, duration x2 / زیر این ۲ برابر */
#define UI_BEEP_START_PCT               50u     /* [EN] Below this, periodic beep starts / زیر این بوق دوره‌ای */
#define UI_BEEP_MIN_INTERVAL_CYCLES     1u      /* [EN] Zero-percent beep interval floor / حداقل فاصله بوق در صفر درصد */

/* ==================== Buzzer Pattern ==================== */

#define UI_BUZZER_DEFAULT_GAP_PERCENT   20u     /* [EN] Default gap 20% of onTime when repeat>1 / گپ پیش‌فرض ۲۰٪ */

/* ==================== RTOS tick ==================== */

#define UI_TICK_MS                      10u     /* [EN] Ui task tick 10ms, simple RTOS / تیکه ۱۰ میلی‌ثانیه - common with LED */

/* ==================== Percent helpers ==================== */

#define UI_PERCENT_FULL                 100u

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

#endif /* UI_BUZZER_H */
