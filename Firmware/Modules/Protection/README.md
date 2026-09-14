/**
 * @file    README.md
 * @brief   [EN] Protection module: over-current and low-battery checks.
 *          [FA] ماژول حفاظت: اضافه جریان و باتری ضعیف.
 */

# ماژول Protection

توضیح کامل **همین ماژول** همین‌جاست.

الان **خاموش** است (`MODULE_PROTECTION 0`). رله و قطع باتری را در CubeMX برای این مرحله نزن. فایل‌ها را پاک نکن.

## کار ماژول

نمونهٔ Measurement را با حدهای `APP_CONFIG` مقایسه می‌کند. اگر خطر بود بیت خطا را در ماژول Fault قفل می‌کند. خودش GPIO قدرت را نمی‌زند؛ تصمیم قطع مسیر با Changeover است.

حدها (الان در `app_config.c` تعریف شده‌اند، هنوز استفاده نمی‌شوند):

- `overcurrent1_ma` / `overcurrent2_ma`
- `low_battery_mv` / `low_battery_recover_mv`

بیت‌ها در `app_types.h`: `FAULT_ADC`، `FAULT_OVERCURRENT_1`، `FAULT_OVERCURRENT_2`، `FAULT_LOW_BATTERY`.

## فایل‌ها

| فایل | نقش |
|---|---|
| `protection.h` / `protection.c` | مقایسه و صدا زدن `Fault_Set` |
| `../Fault/fault.c` | قفل بیت خطا |
| `../../Rtos/Src/task_protection.c` | تسک؛ با فلگ صفر Idle است |
| `../../Config/Src/app_config.c` | حدها |

`protection.c` را به بیلد LED اضافه نکن.

## توابع همین الان در کد

- `Protection_Init` — خالی؛ جایی برای state بعدی.
- `Protection_Run` — اگر `snap` برابر `NULL` یا `valid == false` باشد `Fault_Set(FAULT_ADC)` می‌زند و برمی‌گردد. مقایسه جریان/ولتاژ هنوز نیست.

چرا `NULL` چک است: تسک نباید روی اشاره‌گر خالی بخواند.

پایهٔ بعدی مربوط به قطع مسیر باتری (از شماتیک، هنوز اندازه نشده): `PB11` در `board_pins.h`. این ماژول آن پایه را مستقیم نمی‌زند.
