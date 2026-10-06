# گزارش پاس برنامه‌نویسی (Code Quality) — ۲۰۲۶-۱۰-۰۶
# Programmer-role code review — 2026-10-06

نقش: برنامه‌نویس. این پاس دنبال باگ رفتاری نبود (دو پاس قبل آن را پوشاندند)؛
دنبال **کیفیت کد** بود: تکرار، عدد جادویی، نام‌گذاری خارج از قاعده، اعلان‌های
گم‌شده، کد مرده، و سخت‌گیری دروازهٔ کامپایلر.

Role: programmer. Not a behaviour hunt (the two earlier passes covered that),
but a code-quality pass: duplication, magic numbers, naming, missing
declarations, dead code and compiler-gate strictness.

---

## ۱) اصلاح‌شده (هیچ تغییری در رفتار)
## 1) Fixed (zero behaviour change)

### P1 — عدد جادویی «۲» ۴۱ بار در `charger.c` تکرار شده بود
تعداد کانال‌های شارژر به‌صورت ادبی در ۳۱ حلقه/بررسی مرزی و ۱۰ بُعد آرایه نوشته
شده بود. ثابت تازهٔ `CHG_CHANNEL_COUNT` در `charger.h` اضافه و همهٔ ۴۱ مورد به آن
گره خورد. خروجی کامپایلر یکسان است؛ ولی حالا افزودن کانال سوم یک تغییر
تک‌نقطه‌ای است، نه شکار ۴۱ عدد.

EN: the channel count was hard-coded 41 times in `charger.c`; it now comes from
one `CHG_CHANNEL_COUNT` define in `charger.h`.

### P2 — تنها نام‌گذاری خارج از قاعدهٔ پروژه
`Firmware/Rtos/Src/freertos_hooks.c` چهار استاتیک به سبک فری‌آرتوس
(`s_idle_tcb`, `s_idle_stack`, `s_timer_tcb`, `s_timer_stack`) داشت — تنها جای
فرم‌ور که قاعدهٔ `TYPE__G__Name` را رعایت نمی‌کرد. به
`STATICTASK_T__G__IdleTcb` / `STACKTYPE_T__G__A__IdleStack` (و معادل تایمر)
تغییر نام یافت؛ همان حافظه، همان لینکیج.

### P3 — نقاط ورود بدون اعلان
چهار نماد با لینکیج بیرونی (`DMA1_Channel4_IRQHandler`, `USART1_IRQHandler`,
`vApplicationGetIdleTaskMemory`, `vApplicationStackOverflowHook`) هیچ اعلانی
نداشتند؛ یعنی کامپایلر نمی‌توانست تطابق امضای آن‌ها را با جدول بردار/هستهٔ
RTOS بررسی کند. اعلان کنار تعریف اضافه شد.

### P4 — دروازهٔ هشدار سخت‌گیرتر شد
`tools/check_firmware_syntax.sh` حالا برای سورس‌های `Firmware/` این‌ها را هم با
`-Werror` اعمال می‌کند:
`-Wshadow -Wundef -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wcast-qual`
درخت تولیدشدهٔ ST زیر `CubeIDE/` با مجموعهٔ پایه می‌ماند (هدر
`system_stm32f1xx.h` خودش `-Wredundant-decls` می‌دهد).
پس از اصلاح P1..P3، کل فرم‌ور زیر این مجموعه **صفر هشدار** دارد؛ پس این یک
دروازه است نه آرزو.

---

## ۲) بررسی‌شده و تمیز / Checked and clean

- **کد مرده:** هیچ تابع یا متغیر استاتیک بلااستفاده‌ای نیست (`-Wunused-*` ساکت).
- **نگهبان هدر:** هر ۳ هدر مشکوک (`bsp_iwdg.h`, `imbalance.h`, `cal_lut.h`) در
  واقع نگهبان کامل دارند؛ هشدار اولیه خطای ابزار من بود.
- **`TODO` / `FIXME` / `HACK`:** هیچ موردی در کل `Firmware/` نیست.
- **سایه‌افکنی متغیر، مقایسهٔ شناور، `undef` در ماکرو:** صفر مورد.

---

## ۳) گزارش‌شده، اصلاح‌نشده (نیاز به تأیید شما)
## 3) Reported, not changed — needs your approval

| # | مورد | چرا دست نزدم |
|---|---|---|
| Q1 | توابع بلند: `func__Charger_RegulateChannel` ۵۰۵ خط، `func__Charger_PidStep` ۴۳۰، `func__Charger_Evaluate` ۳۸۵، `func__Changeover_Evaluate` ۳۴۴، `func__Measurement_Run` ۳۲۱، `func__Imbalance_Evaluate` ۳۱۴ | شکستن‌شان یعنی دست‌زدن به **منطق اصلی** که صریحاً ممنوع کرده‌اید. بخش بزرگی از این خط‌ها کامنت دوزبانه است، ولی باز هم هر کدام چند مسئولیت دارند |
| Q2 | `-Wswitch-enum`: سه `switch` در `bsp_gpio.c` و `bsp_pwm.c` همهٔ مقادیر enum را نمی‌شمارند و به `default` تکیه می‌کنند | افزودن `case`های صریح رفتار را عوض نمی‌کند ولی کد را طولانی‌تر می‌کند؛ اگر بخواهید، این هشدار را هم به دروازه اضافه می‌کنم |
| Q3 | `func__EspLink_ApplyParam` / `GetParam` دو زنجیرهٔ موازی ۲۳۴ و ۱۸۲ خطی `if/else` روی همان شناسه‌ها هستند | جدول‌محور کردنشان بازنویسی منطق مسیر پنل است |

---

## ۴) اسناد به‌روز شد / Documentation updated

- `README.md` ریشه: درخت `tools/` (دروازهٔ سخت‌گیر، ۳۰۸ نامتغیر) و یک ردیف
  تاریخچه برای هر سه پاس امروز.
- `Firmware/Modules/Charger/README.md`: تک‌منبع بودن `CHG_CHANNEL_COUNT`.
- `Firmware/Modules/Measurement/README.md`: گیرهٔ «حداقل دو نقطه».
- `Firmware/Modules/Imbalance/README.md`: قرارداد تازهٔ `Init`، شناسه‌های
  ۱۲۳/۱۲۴، و شمار تازهٔ بررسی‌های تست هاست.
- گزارش‌های همین سه پاس: `Audit_Report_2026-10-06.md`،
  `Validation_Report_2026-10-06.md` و همین فایل.

## ۵) وضعیت دروازه‌ها / Gates

| گیت | نتیجه |
|---|---|
| `tools/check_ai_rules.sh` | ALL CHECKS PASSED |
| `tools/check_firmware_syntax.sh` (با مجموعهٔ سخت‌گیر تازه) | PASSED — ۵۰ تست شارژر، ۵۰۵۲ بررسی عدم‌توازن، ۰ هشدار |
| `tools/audit_consistency.py` | ۳۰۸ نامتغیر، ۰ یافته |
