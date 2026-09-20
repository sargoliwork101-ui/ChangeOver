/**
 * @file    README.md
 * @brief   [EN] Fault module sheet: latched fault bits.
 *          [FA] برگه ماژول Fault: بیت‌های خطای قفل‌شده.
 */

# ماژول Fault

## وضعیت

در build اعتبارسنجی برد فعال است: `MODULE_FAULT = 1`. تسک جدا ندارد؛ `func__Fault_Init` هنگام شروع RTOS اجرا می‌شود و `task_control` ماسک را می‌خواند. تولید خودکار خطا هنوز در Protection فعال نیست (`MODULE_PROTECTION = 0`)؛ برای سناریوی Fault می‌توان `func__Fault_Set` را از مسیر تست تزریق کرد.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت ماسک `func__Fault_Set` / `Clear` / `Get` / `Any` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `fault.h` / `fault.c` | `s_mask` |
| `../../Config/Inc/app_types.h` | `FAULT_*` |

به بیلد LED لازم نیست.

## توابع

| نام | کار |
|---|---|
| `func__Fault_Init` | `s_mask = FAULT_NONE` |
| `func__Fault_Set` | بیت‌ها را OR می‌کند (قفل) |
| `func__Fault_Clear` | بیت‌ها را پاک می‌کند |
| `func__Fault_Get` | ماسک فعلی |
| `func__Fault_Any` | اگر چیزی غیر از NONE باشد true |

بیت‌ها: `FAULT_ADC`، `FAULT_OVERCURRENT_1`، `FAULT_OVERCURRENT_2`، `FAULT_LOW_BATTERY`، `FAULT_JITTER_1`، `FAULT_JITTER_2`.

## پایه‌ها

پایه ندارد. فقط RAM.

## پیش‌فرض امن

Init همه بیت‌ها را صفر می‌کند.

## درخت اتصال

صدا زده می‌شود از (در build اعتبارسنجی با Fault فعال):

```text
protection.c      func__Fault_Set
task_control.c    func__Fault_Get
task_comm.c       func__Fault_Get
changeover.c      مقدار faults را از آرگومان می‌گیرد (خودش func__Fault_Get نمی‌زند)
```

این ماژول صدا می‌زند:

```text
fault.c
  fault.h → app_types.h    FAULT_*
```

پایه و BSP ندارد.
