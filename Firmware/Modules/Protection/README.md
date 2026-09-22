/**
 * @file    README.md
 * @brief   [EN] Protection module sheet: over-current and low battery.
 *          [FA] برگه ماژول Protection: اضافه جریان و باتری ضعیف.
 */

# ماژول Protection

## وضعیت

اسکلت. `MODULE_PROTECTION = 0`. backend رله و حفاظت شارژر در BSP موجود است، اما مالکیت policy خروجی با Changeover/Charger تعیین می‌شود. فایل را پاک نکن.

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
| `func__Protection_Init` | فعلاً خالی |
| `func__Protection_Run` | `FAULT_ADC` را به‌صورت **وضعیت لحظه‌ای** می‌نویسد: snap تهی/نامعتبر → Set، snap معتبر → Clear (ممیزی کل برنامه ۲۰۲۶-۰۹-۲۲؛ قبلاً فقط Set بود و چون هیچ‌جا پاک نمی‌شد، با روشن‌شدن آیندهٔ این ماژول بیت در بوت قفل و شارژر برای همیشه safe-idle می‌شد). مقایسه جریان هنوز نیست |
| `TaskProtection` | تا فلگ صفر Idle |

حدهای بعدی در `APP_CONFIG`: `overcurrent1_ma`، `overcurrent2_ma`، `low_battery_mv`، `low_battery_recover_mv`.

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
