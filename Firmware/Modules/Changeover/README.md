/**
 * @file    README.md
 * @brief   [EN] Changeover module: input vs battery path.
 *          [FA] ماژول تغییر مسیر: ورودی یا باتری.
 */

# ماژول Changeover

توضیح کامل **همین ماژول** همین‌جاست.

الان **خاموش** است (`MODULE_CHANGEOVER 0`). GPIO رله و سوئیچ باتری را Enable نکن. فایل‌ها را پاک نکن.

## کار ماژول

تصمیم می‌گیرد بار از ورودی ۲۴ ولت تغذیه شود یا از باتری. ورودی‌اش snapshot و ماسک خطا است؛ خروجی‌اش `app_state_t`.

حالت‌ها در `app_types.h`: `BOOT`، `IDLE`، `INPUT`، `BATTERY`، `FAULT`، `SAFE`.

وقتی فعال شود، از روی همین state شارژر و UI می‌فهمند سیستم کجاست.

## پایه‌هایی که بعداً مال این ماژول‌اند

از `board_pins.h` — قطبیت از **شماتیک** است، روی برد هنوز اندازه نشده. حالا نزن:

| پایه | شماتیک |
|---|---|
| PB5 | High = باتری از تغذیهٔ کنترل جدا |
| PB11 | High = مسیر باتری خاموش (Q17) |
| PB7 | High = رلهٔ شارژر وصل |

منطق قدرت اینجا نوشته می‌شود، نه داخل `main.c`.

## فایل‌ها

| فایل | نقش |
|---|---|
| `changeover.h` / `changeover.c` | ماشین حالت |
| `../../Rtos/Src/task_control.c` | تسک مشترک با Charger/Jitter؛ فلگ صفر = Idle |
| `../../Config/Inc/app_types.h` | `app_state_t` |

`changeover.c` را به بیلد LED اضافه نکن.

## توابع همین الان در کد

- `Changeover_Init` — `s_state = APP_STATE_BOOT`.
- `Changeover_Evaluate` — `snap` را فعلاً استفاده نمی‌کند (`(void)snap`). اگر `faults != FAULT_NONE` برود `FAULT`، وگرنه `IDLE`. مسیر INPUT/BATTERY هنوز نیست.

تسک کنترل فقط وقتی یکی از `MODULE_CHANGEOVER` / `MODULE_CHARGER` / `MODULE_JITTER` یک باشد ساخته می‌شود.
