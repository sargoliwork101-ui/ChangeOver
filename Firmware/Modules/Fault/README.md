/**
 * @file    README.md
 * @brief   [EN] Fault module sheet: latched fault bits.
 *          [FA] برگه ماژول Fault: بیت‌های خطای قفل‌شده.
 */

# ماژول Fault

## وضعیت

در build اعتبارسنجی برد فعال است: `MODULE_FAULT = 1`. تسک جدا ندارد؛ `func__Fault_Init` هنگام شروع RTOS اجرا می‌شود و `task_control` هر پاس ابتدا `func__Fault_Evaluate` و سپس ماسک را می‌خواند. تولید خودکار خطا هنوز در Protection فعال نیست (`MODULE_PROTECTION = 0`)؛ برای سناریوی Fault می‌توان `func__Fault_Set` را از مسیر تست تزریق کرد.

**فریز حین مود تست دستی (v1.2، ۲۰۲۶-۰۹-۲۳):** وقتی شارژر در مود دستی است
(`func__Charger_IsManualTestModeActive() == true`)، `func__Fault_Evaluate` در همان
ابتدا برمی‌گردد: هیچ Set/Clear و هیچ پیشرفت دبانسی انجام نمی‌شود و هر سه تمپ
دبانس صفر می‌شوند؛ ماسک خطا همان‌طور که بود فریز می‌ماند (خروج از مود دستی،
تشخیص را با تمپ‌های صفر از نو شروع می‌کند). دلیل: در مود دستی، پمپِ عمدی روی
خازن خروجی امضای «قطع باتری» نیست و نباید بیت `FAULT_CHARGER_BAT_LOST` را قفل
کند؛ قطع سخت‌افزاری ۱۵٫۰V و JIT همچنان مستقل از این ماژول فعال‌اند.

## تشخیص متمرکز قطع باتری (۲۰۲۶-۰۹-۱۹، دستور کاربر)

مالکیت تشخیص «باتری قطع» از Charger به این ماژول منتقل شد تا همه خطاها یک جا
جمع شوند؛ Charger و UI هر دو فقط **پرچم** را می‌خوانند. دو حالت، یک بیت
(`FAULT_CHARGER_BAT_LOST`، بیت ۶ در `app_types.h`):

| حالت | امضا | دبانس |
|---|---|---|
| شارژ فعال + سیم باتری قطع | هر نیم‌باتری بالای `FAULT_BAT_DISCONNECT_MV` (14.8V) - پالس‌های فلای‌بک خازن خروجی را پمپ می‌کنند | ۱۵۰ms (۱۵ پاس؛ مدین ولتاژ ۳→**۵** (برست دوفریمی هم می‌میرد) + فعال=فقط BULK/ABSORB؛ با این حال بوق فیکِ دوباره حین ابزورب دیده شد، پس دبانس علاوه‌بر سرعت به ۱۵۰ms رسید - گره پمپ‌شده ~۰٫۵s بالای ۱۴٫۸V می‌ماند و آستانه ۱۵٫۰V نمی‌شود چون با قطع اعتبار تداخل دارد - تاریخچه بنچ کاربر ۲۰۲۶-۰۹-۱۹) |
| ورودی سالم (21..28V) ولی هرکدام از نیم‌باتری‌ها زیر ۶V | **هرکدام** زیر `FAULT_BAT_ABSENT_MV` (6V - جداسازی آستانه به دستور کاربر ۲۰۲۶-۰۹-۲۲) | ۱۰۰۰ms |

چرا «هرکدام» و نه «هر دو»: خرابی واقعی بنچ (یک سیم قطع) نیمِ سالم را ~۱۳V نگه
می‌دارد و «هر دو غایب» هرگز فایر نمی‌شد؛ با شارژر پارک‌شده امضای پمپ هم نیست،
پس باتری قطع‌شده کاملاً ساکت می‌ماند (گزارش کاربر ۲۰۲۶-۰۹-۱۹). از ۲۰۲۶-۰۹-۲۲
(دستور کاربر) تشخیص روی ۶V (`FAULT_BAT_ABSENT_MV`) و بازیابی روی ۷V
(`FAULT_BATTERY_BACK_MV`) است؛ فاصلهٔ ۱V هیسترزیس تمیز جفت Set/Clear است -
باتری ۱۲V واقعی حتی دشارژشده بالاتر از هر دو مرز است و عمیق‌دشارژِ در حال
شارژ تا نزدیک ۶V می‌افتد بدون تریپ.

**بازیابی مشترک:** برگشت **هر دو** نیم به بالای ۷V بدون پمپ و پایدارماندن
`FAULT_BAT_RECOVER_MS` (۱s) ⇒ پاک‌شدن بیت؛ سپس خودبه‌خود: Changeover از FAULT
خارج می‌شود، Charger آینهٔ کانال را به OFF برمی‌گرداند و پس از گیت ۱۵ ثانیه‌ای
ثبات اتصال BULK نرم از ۱٪ دیوتی شروع می‌کند، و UI سناریوی BatLost را متوقف
می‌کند.

نکته مرزی: اگر باتری وقتی شارژ فعال نیست جدا شود، ماندۀ خازن روی گره از مسیر
دیوایدر آرام خالی می‌شود و تشخیص حالت دوم ~۲۰-۴۰ ثانیه طول می‌کشد (حین شارژ
همان ۱۵۰ms قاعده اول است). با نبودِ snapshot معتبر هیچ Set/Clear رخ نمی‌دهد.

### نشانهٔ زنده برای ریست تعویض باتری

`func__Fault_IsBatteryAbsent()` یک نشانهٔ زنده و جدا از بیت قفل‌شدهٔ خطای ۶
می‌دهد. این نشانه فقط پس از `func__Fault_Evaluate()` و snapshot معتبر به‌روز
می‌شود؛ snapshot نامعتبر و مود تست دستی آن را خاموش می‌کنند. امضاها:

- نیمِ نصب‌شده زیر ۳V، حتی وقتی ورودی برق نیست؛
- نیمِ نصب‌شده زیر ۶V با ورودی سالم؛
- نیمِ متناظر با کانال شارژ فعال، بالاتر از حد قطع قابل‌تنظیم (پیش‌فرض ۱۴٫۸V).

این getter خودش `FAULT_CHARGER_BAT_LOST` را set یا clear نمی‌کند. در
`task_control.c` سیگنال به `bool__batAbsent` داده می‌شود؛ Imbalance نبود پیوسته
را ۳ ثانیه می‌سنجد و سپس شناسه‌های ماندگار ۲۰۰ تا ۲۰۲ را پاک می‌کند. پس debounce
۱۵۰ms/۱s خطای مشترک با زمان ۳s ریست یکی نیست. حد سخت ۳V هنوز روی برد اندازه‌گیری
و تأیید نشده است.

## `FAULT_CHARGER_TECHNICAL` (بیت ۷) — خطای فنی برد / Scenario 7

ماژول Fault فقط مالک بیت مرکزی `FAULT_CHARGER_TECHNICAL` (بیت ۷) است. تشخیص امضای ترانزیستور در `Charger` انجام می‌شود، سپس
`func__Fault_Set()` این بیت را latch می‌کند تا UI و Changeover همان fault را
ببینند. این بیت در NVM ذخیره نمی‌شود و `func__Fault_Init()` آن را مثل تمام
بیت‌های دیگر در reset پاک می‌کند؛ قفل فیزیکی PWM/رله را خود Charger نگه می‌دارد.

## فایل‌ها

| فایل | نقش |
|---|---|
| `fault.h` / `fault.c` | ماسک خطای قفل‌شده (`FAULT_MASK_T__G__Mask`) و `func__Fault_Evaluate` |
| `../../Config/Inc/app_types.h` | `FAULT_*`، شامل `FAULT_CHARGER_TECHNICAL` (بیت ۷) |
| `../Charger/charger.c` | تشخیص امضای سناریوی ۷ و فراخوانی `func__Fault_Set`; آینهٔ `FAULT_CHARGER_BAT_LOST` (بیت ۶) به `CHG_STATE_BAT_LOST` |

برای این ماژول بیلد جدا لازم نیست؛ با همان بیلد اعتبارسنجی برد (MODULE_FAULT=1) کار می‌کند.

## توابع

| نام | کار |
|---|---|
| `func__Fault_Init` | `s_mask = FAULT_NONE` |
| `func__Fault_Set` | بیت‌ها را OR می‌کند (قفل) |
| `func__Fault_Clear` | بیت‌ها را پاک می‌کند |
| `func__Fault_Get` | ماسک فعلی |
| `func__Fault_Any` | اگر چیزی غیر از NONE باشد true |
| `func__Fault_Evaluate` | هر پاس از `task_control` (قبل از Get): تشخیص/پاک‌سازی متمرکز `FAULT_CHARGER_BAT_LOST` از snapshot |
| `func__Fault_IsBatteryAbsent` | خواندن نشانهٔ زندهٔ reset-only؛ مستقل از بیت خطای ۶ و فقط با snapshot معتبر |

بیت‌ها: `FAULT_ADC`، بیت‌های رزرو جریان/جیتر، `FAULT_CHARGER_BAT_LOST` (بیت ۶) و
`FAULT_CHARGER_TECHNICAL` (بیت ۷، سناریوی خطای فنی برد).

## پایه‌ها

پایه ندارد. فقط RAM.

## پیش‌فرض امن

Init همه بیت‌ها را صفر می‌کند.

## درخت اتصال

صدا زده می‌شود از (در build اعتبارسنجی با Fault فعال):

```text
protection.c      func__Fault_Set
task_control.c    func__Fault_Evaluate + func__Fault_Get + func__Fault_IsBatteryAbsent
task_comm.c       func__Fault_Get
charger.c         func__Fault_Set (سناریوی ۷) + func__Fault_Get (آینهٔ CHG_STATE_BAT_LOST)
ui_led.c          func__Fault_Get (سناریو ۵ و سناریو ۷)
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

## تستر

`Tester/` — تست هاست ماژول خطا با ساعت و شارژر بدلی (۶۳ بررسی: API ماسک، دو
آشکارساز قفل‌شونده، getter زندهٔ reset-only، مرز ۳V، snapshot نامعتبر، مود تست
دستی، گیرهٔ پارامترها و سرریز تیک). اجرا با `Tester/run_host_test_fault.sh`؛
داخل `tools/check_firmware_syntax.sh` هم اجرا می‌شود. تست یکپارچهٔ
`../Imbalance/Tester/run_host_test_battery_swap.sh` مسیر Fault→TaskControl→Imbalance
را با ۱۳۰ بررسی میزبان و آزمون اتصال کد پوشش می‌دهد. نتیجه‌ها جای تست برد را
نمی‌گیرند.

## ماشین حالت

`Fault_State_Machine.xlsx` — حالت‌ها و گذارهای همین ماژول، رنگی و
راست‌به‌چپ با فونت وزیرمتن. از برگهٔ «ماشین حالت» در فایل اعتبارسنجی همین
پوشه هم به آن لینک هست و خودش به نمای سیستمی
(`Documentation/System_State_Machine.xlsx`) برمی‌گردد. بازتولید:
`python3 tools/make_module_state_machines.py`.

## تاریخچه

تاریخچهٔ تغییرات در `git log` همین مخزن نگهداری می‌شود.
