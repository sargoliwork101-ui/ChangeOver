/**
 * @file    README.md
 * @brief   [EN] Protection module sheet: over-current and low battery.
 *          [FA] برگه ماژول Protection: اضافه جریان و باتری ضعیف.
 */

# ماژول Protection

## وضعیت

پیاده‌سازی اولیه حفاظت: `MODULE_PROTECTION = 0` در build فعلی (غیرفعال)، اما منطق آن کامل شد تا با فعال‌سازی فلگ قابل تست باشد. `snapshot==NULL` یا `!valid` → `FAULT_ADC` ست، در غیر این صورت `FAULT_ADC` پاک و جریان‌ها و ولتاژ باتری با `APP_CONFIG` مقایسه و `FAULT_OVERCURRENT_1/2` و `FAULT_LOW_BATTERY` ست می‌شوند (latched). قطع مسیر باتری همچنان در اختیار Changeover است؛ این ماژول فقط بیت خطا را قفل می‌کند.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت `func__Protection_Init` / `func__Protection_Run` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `protection.h` / `protection.c` | مقایسه با حد |
| `../Fault/fault.c` | قفل بیت |
| `../../Rtos/Src/task_protection.c` | تسک |
| `../../Config/Src/app_config.c` | حد جریان/ولتاژ |

`protection.c` را به بیلد LED اضافه نکن.

## توابع

| نام | کار |
|---|---|
| `func__Protection_Init` | خالی؛ latch در Fault است |
| `func__Protection_Run` | اگر `snap==NULL` یا `!valid` → `FAULT_ADC` ست؛ وگرنه `FAULT_ADC` پاک و اگر `i_ch1_ma>overcurrent1` → `FAULT_OVERCURRENT_1`، اگر `i_ch2_ma>overcurrent2` → `FAULT_OVERCURRENT_2`، اگر `v_bat24<low_battery` → `FAULT_LOW_BATTERY`؛ همه latched |
| `TaskProtection` | تا فلگ صفر Idle، با فلگ 1 هر `protection_period_ms` یک Run |

حدها در `APP_CONFIG`: `overcurrent1_ma=3500`, `overcurrent2_ma=3500`, `low_battery_mv=20000`, `low_battery_recover_mv=21000`.

## پایه‌ها

پایهٔ GPIO اختصاصی ندارد. قطع مسیر باتری مال Changeover است؛ `PB11` فقط مرجع فیزیکی برد فعلی است و نباید وارد منطق Protection شود.

## پیش‌فرض امن

Init چیزی را High نمی‌کند. بدون نمونه معتبر، بعداً باید خطا ADC قفل شود نه PWM/رله.

## درخت اتصال

صدا زده می‌شود از (وقتی فلگ ۱ شود):

```text
rtos_app.c → TaskProtection → task_protection.c
  func__Protection_Run(&snap)
```

این ماژول صدا می‌زند:

```text
protection.c
  measurement.h / Measurement_GetSnapshot   (از تسک)
  fault.h / func__Fault_Set
  app_config.h                              حدها
  app_types.h                               snapshot ، FAULT_*
```
