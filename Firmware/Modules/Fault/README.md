/**
 * @file    README.md
 * @brief   [EN] Fault module sheet: latched fault bits.
 *          [FA] برگه ماژول Fault: بیت‌های خطای قفل‌شده.
 */

# ماژول Fault

## وضعیت

در build اعتبارسنجی برد فعال است: `MODULE_FAULT = 1`. تسک جدا ندارد؛ `func__Fault_Init` هنگام شروع RTOS اجرا می‌شود و `task_control` هر پاس ابتدا `func__Fault_Evaluate` و سپس ماسک را می‌خواند. تولید خودکار خطا هنوز در Protection فعال نیست (`MODULE_PROTECTION = 0`)؛ برای سناریوی Fault می‌توان `func__Fault_Set` را از مسیر تست تزریق کرد.

## تشخیص متمرکز قطع باتری (۲۰۲۶-۰۹-۱۹، دستور کاربر)

مالکیت تشخیص «باتری قطع» از Charger به این ماژول منتقل شد تا همه خطاها یک جا
جمع شوند؛ Charger و UI هر دو فقط **پرچم** را می‌خوانند. دو حالت، یک بیت
(`FAULT_CHARGER_BAT_LOST`، بیت ۶ در `app_types.h`):

| حالت | امضا | دبانس |
|---|---|---|
| شارژ فعال + سیم باتری قطع | هر نیم‌باتری بالای `FAULT_BAT_DISCONNECT_MV` (14.8V) - پالس‌های فلای‌بک خازن خروجی را پمپ می‌کنند | ۱۵۰ms (۱۵ پاس؛ مدین ولتاژ ۳→**۵** (برست دوفریمی هم می‌میرد) + فعال=فقط BULK/ABSORB؛ با این حال بوق فیکِ دوباره حین ابزورب دیده شد، پس دبانس علاوه‌بر سرعت به ۱۵۰ms رسید - گره پمپ‌شده ~۰٫۵s بالای ۱۴٫۸V می‌ماند و آستانه ۱۵٫۰V نمی‌شود چون با قطع اعتبار تداخل دارد - تاریخچه بنچ کاربر ۲۰۲۶-۰۹-۱۹) |
| ورودی سالم (21..28V) ولی باتری نیست | **هر دو** نیم‌باتری زیر `FAULT_BAT_ABSENT_MV` (6V - انتخاب کاربر) | ۱۰۰۰ms |

«هر دو نیم» بودنِ حالت دوم عمدی است: در تست رومیزی تک‌باتری، نیمِ نصب‌نشده ~0V
شناور است و نباید خطای کاذب بدهد.

**بازیابی مشترک:** برگشت هر دو نیم به پنجره سالم `[6V, 14.8V]` و پایدارماندن
`FAULT_BAT_RECOVER_MS` (۱s) ⇒ پاک‌شدن بیت؛ سپس خودبه‌خود: Changeover از FAULT
خارج می‌شود، Charger آینهٔ کانال را به OFF برمی‌گرداند و اولین پاس BULK نرم
از ۱٪ دیوتی شروع می‌کند، و UI سناریوی BatLost را متوقف می‌کند.

نکته مرزی: اگر باتری وقتی شارژ فعال نیست جدا شود، ماندۀ خازن روی گره از مسیر
دیوایدر آرام خالی می‌شود و تشخیص حالت دوم ~۲۰-۴۰ ثانیه طول می‌کشد (حین شارژ
همان ۱۵۰ms قاعده اول است). با نبودِ snapshot معتبر هیچ Set/Clear رخ نمی‌دهد.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-19 | تشخیص متمرکز قطع باتری (`func__Fault_Evaluate` + ثابت‌های `FAULT_BAT_*`) از Charger به اینجا منتقل شد؛ دو قاعده + بازیابی مشترک روی بیت ۶ |
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
| `func__Fault_Evaluate` | هر پاس از `task_control` (قبل از Get): تشخیص/پاک‌سازی متمرکز `FAULT_CHARGER_BAT_LOST` از snapshot |

بیت‌ها: `FAULT_ADC`، `FAULT_OVERCURRENT_1`، `FAULT_OVERCURRENT_2`، `FAULT_LOW_BATTERY`، `FAULT_JITTER_1`، `FAULT_JITTER_2`، `FAULT_CHARGER_BAT_LOST`.

## پایه‌ها

پایه ندارد. فقط RAM.

## پیش‌فرض امن

Init همه بیت‌ها را صفر می‌کند.

## درخت اتصال

صدا زده می‌شود از (در build اعتبارسنجی با Fault فعال):

```text
protection.c      func__Fault_Set
task_control.c    func__Fault_Evaluate + func__Fault_Get
task_comm.c       func__Fault_Get
charger.c         func__Fault_Get (آینهٔ CHG_STATE_BAT_LOST)
ui_led.c          func__Fault_Get (سناریو ۵ BatLost)
changeover.c      مقدار faults را از آرگومان می‌گیرد (خودش func__Fault_Get نمی‌زند)
```

این ماژول صدا می‌زند:

```text
fault.c
  fault.h → app_types.h    FAULT_*
  cmsis_os2.h             osKernelGetTickCount (تایمرهای دبانس)
  rtos_time.h             func__Rtos_MillisecondsToTicks
```

پایه و BSP ندارد.
