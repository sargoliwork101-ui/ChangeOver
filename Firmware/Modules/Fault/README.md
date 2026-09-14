/**
 * @file    README.md
 * @brief   [EN] Fault module sheet: latched fault bits.
 *          [FA] برگه ماژول Fault: بیت‌های خطای قفل‌شده.
 */

# ماژول Fault

## وضعیت

اسکلت. `MODULE_FAULT = 0`. تسک جدا ندارد. App الان `Fault_Init` را صدا نمی‌زند. فایل را پاک نکن.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت ماسک `Fault_Set` / `Clear` / `Get` / `Any` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `fault.h` / `fault.c` | `s_mask` |
| `../../Config/Inc/app_types.h` | `FAULT_*` |

به بیلد LED لازم نیست.

## توابع

| نام | کار |
|---|---|
| `Fault_Init` | `s_mask = FAULT_NONE` |
| `Fault_Set` | بیت‌ها را OR می‌کند (قفل) |
| `Fault_Clear` | بیت‌ها را پاک می‌کند |
| `Fault_Get` | ماسک فعلی |
| `Fault_Any` | اگر چیزی غیر از NONE باشد true |

بیت‌ها: `FAULT_ADC`، `FAULT_OVERCURRENT_1`، `FAULT_OVERCURRENT_2`، `FAULT_LOW_BATTERY`، `FAULT_JITTER_1`، `FAULT_JITTER_2`.

## پایه‌ها

پایه ندارد. فقط RAM.

## پیش‌فرض امن

Init همه بیت‌ها را صفر می‌کند.
