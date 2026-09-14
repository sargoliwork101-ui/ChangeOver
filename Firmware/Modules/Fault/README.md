/**
 * @file    README.md
 * @brief   [EN] Fault module: latched bit-mask of faults.
 *          [FA] ماژول خطا: بیت‌ماسک قفل‌شدهٔ خطاها.
 */

# ماژول Fault

توضیح کامل **همین ماژول** همین‌جاست.

الان **تسک جدا ندارد**. `MODULE_FAULT` در `modules_enable.h` صفر است و App آن را Init نمی‌کند. فایل‌ها را پاک نکن. Protection بعداً همین API را صدا می‌زند.

## کار ماژول

چند خطا هم‌زمان ممکن است. به‌جای یک enum، یک ماسک بیت است تا OR/AND روشن باشد.

نوع: `fault_mask_t` در `app_types.h`.

| بیت | معنی |
|---|---|
| `FAULT_NONE` | هیچ |
| `FAULT_ADC` | نمونه نامعتبر |
| `FAULT_OVERCURRENT_1` / `_2` | اضافه جریان |
| `FAULT_LOW_BATTERY` | باتری ضعیف |
| `FAULT_JITTER_1` / `_2` | تریپ جیتر |

`1u << n` یعنی بیت شماره n. چند تا را با `|` جمع می‌کنند.

قفل (latch): `Fault_Set` بیت را روشن می‌کند و خاموش نمی‌کند مگر `Fault_Clear` یا `Fault_Init`. یک پالس کوتاه خطا گم نمی‌شود.

## فایل‌ها

| فایل | نقش |
|---|---|
| `fault.h` / `fault.c` | ماسک `s_mask` |
| `../../Config/Inc/app_types.h` | تعریف بیت‌ها |

`fault.c` را به بیلد LED لازم نیست اضافه کنی؛ هنوز کسی صدا نمی‌زند.

## توابع همین الان در کد

- `Fault_Init` — `s_mask = FAULT_NONE`.
- `Fault_Set` — `s_mask |= bits` (بیت‌های قبلی می‌مانند).
- `Fault_Clear` — `s_mask &= ~bits`.
- `Fault_Get` — ماسک فعلی.
- `Fault_Any` — true اگر غیر از `FAULT_NONE`.

Changeover_Evaluate همین حالا اگر `faults != FAULT_NONE` به `APP_STATE_FAULT` می‌رود. یعنی این ماژول منبع حقیقت خطا است، نه کپی در چند فایل.
