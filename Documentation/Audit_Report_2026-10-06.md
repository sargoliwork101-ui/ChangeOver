# گزارش ممیزی دور دوم — ۲۰۲۶-۱۰-۰۶
# Audit report, round 2 — 2026-10-06

نقش: برنامه‌نویس فرم‌ور امبدد. روش: خواندن خط‌به‌خط فایل‌ها (نه فقط grep)، بررسی
سرریز صحیح، گیره‌ها، مرزهای آرایه، بخش‌های بحرانی و ترتیب مقداردهی اولیه.

Role: embedded firmware engineer. Method: line-by-line reading (not grep-only),
checking integer overflow, clamps, array bounds, critical sections and
initialisation order.

---

## الف) ایرادهای یافته‌شده و اصلاح‌شده (بدون تغییر منطق اصلی)
## A) Defects found and fixed (no change to the main logic)

### A1 — بحرانی: تنظیم‌های سناریو ۵ (عدم‌توازن) با هر بار روشن‌شدن پاک می‌شد
**فایل:** `Firmware/Modules/Imbalance/imbalance.c`

- `func__EspLink_NvmInit()` پیش از شروع scheduler مقادیر ذخیره‌شدهٔ پنل
  (شناسه‌های ۱۰۸ به بعد) را با `func__Imbalance_SetParam()` بازپخش می‌کرد.
- `func__Imbalance_Init()` بعداً داخل نخ کنترل اجرا می‌شد و با یک حلقهٔ
  بی‌قید، **کل جدول پارامتر را به پیش‌فرض کامپایل برمی‌گرداند**.
- نتیجه: هر تنظیم عدم‌توازنی که کاربر از پنل ذخیره کرده بود، با هر
  power-cycle از بین می‌رفت — خلاف قاعدهٔ مستند پروژه که در
  `func__Charger_Init()` صریحاً نوشته شده: «Init ماژول هیچ‌وقت مقدار
  پنل/فلش را برنمی‌گرداند».

**اصلاح:** تابع `func__Imbalance_SeedDefaultsOnce()` اضافه شد؛ پیش‌فرض‌ها فقط
یک‌بار و در نخستین دسترسی به جدول نوشته می‌شوند. این تابع در ابتدای
`Init` / `SetParam` / `GetParam` / `ReadParam` فراخوانی می‌شود. برای بردِ نو
(بدون رکورد NVM) رفتار دقیقاً مثل قبل است؛ فقط مقدار بازیابی‌شده دیگر پاک
نمی‌شود. منطق خود سناریو ۵ دست نخورده است.

EN: the unconditional default loop in `func__Imbalance_Init()` wiped every
panel/NVM-restored scenario-5 parameter on each boot, because the NVM replay
runs before the scheduler and Init runs after. Defaults are now seeded exactly
once on first touch of the table. Fresh boards behave identically.

### A2 — سخت‌سازی: نبود گیرهٔ «حداقل دو نقطه» در درون‌یابی جدول بنچ
**فایل:** `Firmware/Modules/Measurement/measurement.c`،
`func__Measurement_BenchLutInterp()`

قرارداد تابع `points >= 2` است ولی خودش آن را بررسی نمی‌کرد؛ شاخهٔ انتهایی
`[points - 2]` را می‌خواند و با `points` برابر ۰ یا ۱ کم‌ریزی بدون‌علامت
رخ می‌داد و حافظه‌ای بسیار بیرون از جدول خوانده می‌شد. امروز از بیرون
غیرقابل‌دسترس است (`func__CalLut_Active()` با `CAL_LUT_POINTS_MIN == 2`
نگهبانی می‌کند) ولی گیره اضافه شد: `if (points < 2u) return 0u;`
خروجی ورودی‌های معتبر بیت‌به‌بیت بدون تغییر است.

---

## ب) فایل‌ها و توابعی که خط‌به‌خط خوانده شد و سالم بود
## B) Read line-by-line, verified clean

| فایل / تابع | نتیجه |
|---|---|
| `bsp_exti.c` | take-and-clear با محافظت PRIMASK، شاخص منبع مرزبندی‌شده — سالم |
| `freertos_hooks.c` | idle task استاتیک، hook سرریز پشته متوقف‌کننده — سالم |
| `bsp_pwm.c` | CCR2 = CCR1/2 فقط زیر ~۲ شمارش duty تباه می‌شود؛ زیر کف ۱٪ شارژر غیرقابل‌دسترس |
| `bsp_uart.c` | حلقهٔ DMA درست؛ «نبود ذخیرهٔ بایت» هشدار کاذب بود (خط ۴۱۲ موجود است) |
| `bsp_flash.c` | پاک/برنامه هر دو به بازهٔ storage مقید، هم‌ترازی‌شده، سقف ۱۰۲۴ هاف‌ورد، spin محدود، قفل دوباره در همهٔ مسیرها |
| `bsp_iwdg.c` | وتوی reload با کهنه‌بودن هر اسلات، `last == 0` همان مهلت بوت، ماسک از همان `MODULE_*` |
| `bsp_adc.c` (`GetRaw`, `Start`) | انتخاب نیمهٔ DMA با CNDTR، بررسی قبل/بعد، کپی داخل PRIMASK، نمونهٔ سنکرون بیرون از بخش بحرانی — سالم |
| `charger.c :: func__Charger_PidStep` | کل حساب صحیح/گیره‌ها بررسی شد؛ `appliedPermille` اثباتاً منفی نمی‌شود |
| `measurement.c :: func__Measurement_GetSnapshot` | کپی زیر `osKernelLock`، NULL-check، بازگرداندن قفل — سالم |
| `measurement.c :: func__Measurement_Publish` (~۱۲۸۷) | انتشار زیر قفل، `valid` آخر از همه نوشته می‌شود — سالم |
| `measurement.c :: func__Measurement_Init` | فقط وضعیت زمان‌اجرا را صفر می‌کند، هیچ استاتیک قابل‌تنظیمی را نه — سالم |
| `charger.c :: func__Charger_Init` | قاعدهٔ «Init مقدار پنل را برنمی‌گرداند» را درست رعایت می‌کند — سالم |

---

## ج) موارد باز از دور اول (بدون تغییر)
## C) Still-open items from round 1

- **ج-۵:** یک پاس شارژر حین ذخیرهٔ ~۱۰۰ms در NVM از دست می‌رود — اولویت پایین.
- **ج-۶:** `func__Fault_DebounceDone` تیک `0` را «تایمر خاموش» می‌گیرد؛ یک دورهٔ
  کنترل، یک‌بار در هر ۴۹٫۷ روز — آرایشی.
- تصمیم‌های کاربر ۲۰۲۶-۱۰-۰۵: ج-۱ اصلاح شد، ج-۲ و ج-۴ همین‌طور بماند،
  ج-۳ بیت‌های رزرو با برچسب «رزرو» نگه داشته شد.

## د) قابل بررسی نیست در این محیط
## D) Not verifiable here

بودجهٔ فلش (نیاز به `.map` از CubeIDE)، دو سوییت `jsdom`، آنالیزور استاتیک بیرونی.

---

## نتیجهٔ ابزارها پس از اصلاح‌ها / Tooling after the fixes

- `tools/check_ai_rules.sh` → **ALL CHECKS PASSED**
- `tools/check_firmware_syntax.sh` → **ALL HOST TESTS PASSED** (۵۰ تست شارژر، تست عدم‌توازن ۵۰۳۴ بررسی / ۰ خطا)
- `tools/audit_consistency.py` → ۲۷۰ ناوردا، **۰ یافته**
