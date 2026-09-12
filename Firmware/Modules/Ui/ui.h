/**
 * @file    ui.h
 * @brief   رابط کاربر همین مرحله: سه LED و یک بازر.
 *
 * چرا این ماژول جدا است؟
 *   در آردوینو معمولاً digitalWrite را وسط loop می‌گذاری.
 *   این‌جا UI یک کتابخانه است تا فردا که Changeover اضافه شد،
 *   چشمک و بوق با منطق قدرت قاطی نشود.
 *
 * قانون MISRA مرتبط:
 *   - رابط عمومی فقط در هدر (Rule 8.4)
 *   - Include guard تا تعریف تکراری نشود (Rule 5.x / Header hygiene)
 */

#ifndef UI_H
#define UI_H

/**
 * @brief همه خروجی‌های UI را خاموش می‌کند و تست را از صفر شروع می‌کند.
 *
 * چه زمانی صدا زده شود: یک‌بار در App_Init، قبل از Start شدن scheduler.
 *
 * Safety:
 *   بازر را خاموش می‌گذارد تا بعد از Reset سوت ممتد نکشد.
 */
void Ui_Init(void);

/**
 * @brief یک قدم از الگوی LED/بازر. هیچ delayی داخلش نیست.
 *
 * چه زمانی صدا زده شود: دوره‌ای از TaskUi (الان هر 100 ms).
 *
 * چرا delay این‌جا ممنوع است؟
 *   delay کل CPU را می‌گیرد. در FreeRTOS صبر کردن کار Task است
 *   با vTaskDelay، نه کار کتابخانه UI.
 */
void Ui_Run(void);

#endif /* UI_H */
