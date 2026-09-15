/**
 * @file    ui_buzzer.h
 * @brief   [EN] One public UI buzzer service: periodic pattern from period, duty, count, and gap.
 *          [FA] یک سرویس عمومی بوق UI: الگوی دوره‌ای بر اساس دوره، دیوتی، تعداد و گپ.
 *
 * @note    [EN] Call func__Ui_Buzzer_Tick() from a task every UI_TICK_MS. The function is non-blocking.
 *          [FA] تابع func__Ui_Buzzer_Tick() را هر UI_TICK_MS از یک تسک صدا بزن؛ تابع غیرمسدودکننده است.
 */

#ifndef UI_BUZZER_H
#define UI_BUZZER_H

/* ==================== Includes ==================== */

#include <stdint.h>

/* ==================== Buzzer / Beep constants ==================== */

#define UI_BUZZER_PERCENT_SCALE         100u    /* [EN] Duty percentage scale / مقیاس درصد دیوتی */
#define UI_BUZZER_DUTY_MAX_PERCENT      100u    /* [EN] Maximum valid duty / بیشترین دیوتی مجاز */
#define UI_BUZZER_TICK_MS               10u     /* [EN] Service call interval / فاصله فراخوانی سرویس */

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
 *         A zero duty or zero count turns the buzzer off. Call every UI_TICK_MS.
 *         [FA] یک الگوی دوره‌ای بوق را بدون قفل کردن تسک اجرا می‌کند.
 *         الگو هر periodMs تکرار می‌شود. پنجره دیوتی برابر periodMs*dutyPercent/100 است.
 *         تعداد beepCount بوق این پنجره را تقسیم می‌کنند و gapMs بین بوق‌های مجاور قرار می‌گیرد.
 *         دیوتی صفر یا تعداد صفر بوق را خاموش می‌کند. هر UI_TICK_MS صدا زده شود.
 * @param  uint32_t__periodMs [EN] Pattern period in ms, non-zero / دوره الگو بر حسب ms، غیرصفر
 * @param  uint8_t__dutyPercent [EN] Duty window 0..100 percent / پنجره دیوتی از صفر تا صد درصد
 * @param  uint8_t__beepCount [EN] Number of pulses in duty window; zero disables / تعداد پالس در پنجره دیوتی؛ صفر یعنی خاموش
 * @param  uint32_t__gapMs [EN] Low gap between pulses in ms / گپ خاموش بین بوق‌ها بر حسب ms
 */
void func__Ui_Buzzer_Tick(uint32_t uint32_t__periodMs, uint8_t uint8_t__dutyPercent, uint8_t uint8_t__beepCount, uint32_t uint32_t__gapMs);

#endif /* UI_BUZZER_H */
