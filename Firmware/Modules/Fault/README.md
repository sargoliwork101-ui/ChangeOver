/**
 * @file    README.md
 * @brief   [EN] Fault module sheet: latched fault bits.
 *          [FA] برگه ماژول Fault: بیت‌های خطای قفل‌شده.
 */

# ماژول Fault

## وضعیت

در build اعتبارسنجی برد فعال است: `MODULE_FAULT = 1`. تسک جدا ندارد؛ `func__Fault_Init` هنگام شروع RTOS اجرا می‌شود و `task_control` هر پاس ابتدا `func__Fault_Evaluate(&snap)` را اجرا و سپس ماسک را می‌خواند. تشخیص **قطع باتری** از ۲۰۲۶-۰۹-۱۹ در مالکیت مرکزی این ماژول است (قاعدهٔ پمپ >۱۴٫۸V فقط وقتی مسلح که پمپی فعال باشد؛ قاعدهٔ غیبت: هر نیم‌سلِ کانالِ **نصب‌شده** زیر ۷V با ورودی ۲۱..۲۸V؛ پاک‌شدن با ۱۰۰۰ms پایداری همهٔ نیم‌سل‌های نصب‌شدهٔ ≥۷V) و شارژر فقط بیت `FAULT_CHARGER_BAT_LOST` را آینه می‌کند.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09-20 | بازسازی تشخیص قطع باتری (از دست‌رفتهٔ سینک): قواعد فقط روی نیم‌سل کانال‌های نصب‌شده + مسلح‌بودن قاعدهٔ پمپ با `IsAnyChannelActive` + تبدیل ms فقط از `rtos_time.h` (باگ بنچ: قفل ابدی و زرد الکی) |
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
| `func__Fault_Evaluate` | تشخیص مرکزی قطع باتری روی snapshot (هر پاس task_control) |

بیت‌ها: `FAULT_ADC`، `FAULT_OVERCURRENT_1`، `FAULT_OVERCURRENT_2`، `FAULT_LOW_BATTERY`، `FAULT_JITTER_1`، `FAULT_JITTER_2`، `FAULT_CHARGER_BAT_LOST` (مالکیت مرکزی قطع باتری).

## پایه‌ها

پایه ندارد. فقط RAM.

## پیش‌فرض امن

Init همه بیت‌ها را صفر می‌کند.

## درخت اتصال

صدا زده می‌شود از (در build اعتبارسنجی با Fault فعال):

```text
protection.c      func__Fault_Set
task_control.c    func__Fault_Evaluate(&snap) سپس func__Fault_Get
task_comm.c       func__Fault_Get
charger.c         آینهٔ func__Fault_Get() & FAULT_CHARGER_BAT_LOST
changeover.c      مقدار faults را از آرگومان می‌گیرد (خودش func__Fault_Get نمی‌زند)
```

این ماژول صدا می‌زند:

```text
fault.c
  fault.h    → app_types.h    FAULT_*
  charger.h  → CHG_INSTALLED_CHANNEL_MASK + func__Charger_IsAnyChannelActive (مسلح‌سازی پمپ)
  rtos_time.h → func__Rtos_MillisecondsToTicks
```

پایه و BSP ندارد.
