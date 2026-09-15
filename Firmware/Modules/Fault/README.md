/**
 * @file    README.md
 * @brief   [EN] Fault module sheet: latched fault bits.
 *          [FA] برگه ماژول Fault: بیت‌های خطای قفل‌شده.
 */

# ماژول Fault

## وضعیت

اسکلت. `MODULE_FAULT = 0`. تسک جدا ندارد. App الان `func_Fault_Init` را صدا نمی‌زند. فایل را پاک نکن.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت ماسک `func_Fault_Set` / `Clear` / `Get` / `Any` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `fault.h` / `fault.c` | `s_mask` |
| `../../Config/Inc/app_types.h` | `FAULT_*` |

به بیلد LED لازم نیست.

## توابع

| نام | کار |
|---|---|
| `func_Fault_Init` | `s_mask = FAULT_NONE` |
| `func_Fault_Set` | بیت‌ها را OR می‌کند (قفل) |
| `func_Fault_Clear` | بیت‌ها را پاک می‌کند |
| `func_Fault_Get` | ماسک فعلی |
| `func_Fault_Any` | اگر چیزی غیر از NONE باشد true |

بیت‌ها: `FAULT_ADC`، `FAULT_OVERCURRENT_1`، `FAULT_OVERCURRENT_2`، `FAULT_LOW_BATTERY`، `FAULT_JITTER_1`، `FAULT_JITTER_2`.

## پایه‌ها

پایه ندارد. فقط RAM.

## پیش‌فرض امن

Init همه بیت‌ها را صفر می‌کند.

## درخت اتصال

صدا زده می‌شود از (وقتی فلگ‌ها ۱ شوند):

```text
protection.c      func_Fault_Set
task_control.c    func_Fault_Get
task_comm.c       func_Fault_Get
changeover.c      مقدار faults را از آرگومان می‌گیرد (خودش func_Fault_Get نمی‌زند)
```

این ماژول صدا می‌زند:

```text
fault.c
  fault.h → app_types.h    FAULT_*
```

پایه و BSP ندارد.
