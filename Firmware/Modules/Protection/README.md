/**
 * @file    README.md
 * @brief   [EN] Protection module sheet: over-current and low battery.
 *          [FA] برگه ماژول Protection: اضافه جریان و باتری ضعیف.
 */

# ماژول Protection

## وضعیت

اسکلت. `MODULE_PROTECTION = 0`. رله را برای این ماژول Enable نکن. فایل را پاک نکن.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت `Protection_Init` / `Protection_Run` |

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
| `Protection_Init` | فعلاً خالی |
| `Protection_Run` | اگر `snap` تهی یا نامعتبر باشد `Fault_Set(FAULT_ADC)`. مقایسه جریان هنوز نیست |
| `TaskProtection` | تا فلگ صفر Idle |

حدهای بعدی در `APP_CONFIG`: `overcurrent1_ma`، `overcurrent2_ma`، `low_battery_mv`، `low_battery_recover_mv`.

## پایه‌ها

پایهٔ GPIO اختصاصی ندارد. قطع مسیر باتری مال Changeover است (`PB11`).

## پیش‌فرض امن

Init چیزی را High نمی‌کند. بدون نمونه معتبر، بعداً باید خطا ADC قفل شود نه PWM/رله.
