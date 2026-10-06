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
| `TaskProtection` | تسک فقط با `MODULE_PROTECTION=1` ساخته می‌شود؛ فعلاً `MODULE_PROTECTION=0` است و تسک/این ماژول در بیلد فعلی اجرا نمی‌شود |

حدهای بعدی (اضافه‌جریان و باتری کم) هنوز تعریف نشده‌اند. در مرتب‌سازی ۲۰۲۶-۱۰-۰۵ فیلدهای بی‌مصرف `overcurrent1_ma`، `overcurrent2_ma`، `low_battery_mv` و `low_battery_recover_mv` از `APP_CONFIG` حذف شدند؛ وقتی این ماژول واقعاً پیاده شد، آستانه‌هایش طبق قانون «جایگذاری ثابت‌های مرتبط با تابع» در `protection.h` و بالای همان تابع تعریف می‌شوند، نه در پیکربندی سراسری.

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

## تستر

`Tester/` — تست هاست نگهبان اعتبار اندازه‌گیری (۶۱ بررسی). اجرا با
`Tester/run_host_test_protection.sh`؛ داخل `tools/check_firmware_syntax.sh` هم اجرا
می‌شود. توضیح کامل در `Tester/README.md`.

## ماشین حالت

`Protection_State_Machine.xlsx` — حالت‌ها و گذارهای همین ماژول، رنگی و
راست‌به‌چپ با فونت وزیرمتن. از برگهٔ «ماشین حالت» در فایل اعتبارسنجی همین
پوشه هم به آن لینک هست و خودش به نمای سیستمی
(`Documentation/System_State_Machine.xlsx`) برمی‌گردد. بازتولید:
`python3 tools/make_module_state_machines.py`.
