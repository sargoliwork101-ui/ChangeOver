# گزارش ممیزی دقیق سناریوی ۷ — ۲۰۲۶-۱۰-۰۶
# Detailed scenario-7 audit — 2026-10-06

## دامنه / Scope

این ممیزی مسیر کامل «خرابی فنی برد / سوختن ترانزیستور شارژر» را بررسی کرد:

```text
ADC snapshot + JIT
  -> Charger technical-fault evaluator
  -> Fault bit 7
  -> Changeover/Charger safety lockout
  -> UI synchronized three-LED face + independent buzzer
  -> ESP protocol/NVM/panel/import-export/factory reset/simulator
```

مرجع‌های سیمی سناریوی ۷:

| id | مالک | معنی | پیش‌فرض | بازه |
|---:|---|---|---:|---|
| 137 | UI | دورهٔ بوق | 3000 ms | 0 یا 1000..600000 ms |
| 138 | UI | طول هر بوق | 200 ms | 0..600000 ms |
| 139 | UI | تعداد بوق | 3 | 0..10 |
| 140 | UI | گپ بین بوق‌ها | 100 ms | 0..5000 ms |
| 141 | UI | دورهٔ مشترک سه LED | 1000 ms | 100..10000 ms |
| 142 | UI | سهم روشنی مشترک سه LED | 50% | 0..100% |

## یافته‌های ممیزی و اصلاحات

### A1 — صحت تشخیص و lockout

- امضای اتصال‌کوتاه/سوختن فقط با ترکیب رلهٔ باز، duty واقعی صفر و JIT ثبت‌شده
  پذیرفته می‌شود؛ صرفاً بازشدن رله یا صفرشدن duty کافی نیست.
- امضای قطع‌شدن/سوختن برای هر کانال مستقل است: duty واقعی بیشتر از `200‰` و
  جریان همان کانال دقیقاً صفر.
- پس از تشخیص، بیت `FAULT_CHARGER_TECHNICAL` (بیت ۷) و قفل RAM شارژر ست می‌شوند.
- قفل در هر پاس `FinalDisconnect` را اجرا می‌کند؛ هر دو PWM صفر و رله باز می‌مانند.
- فقط `func__Charger_Init()` در reset/power-cycle قفل شارژر را آزاد می‌کند.
  Fault نیز فقط با `func__Fault_Init()` در reset پاک می‌شود.
- ترتیب task اصلاح و بررسی شد: `Jitter_Run` سپس technical evaluator، سپس
  Changeover/Charger control.

### A2 — اولویت نمایش UI

در بازبینی دقیق مشخص شد که شرط OverVoltage پیش از fault فنی بررسی می‌شد. این
با الزام «هر سه LED هنگام فعال‌بودن fault» ناسازگار بود، چون اضافه‌ولتاژ هم‌زمان
می‌توانست چهرهٔ سناریوی ۷ را پنهان کند. اصلاح شد:

```text
Technical fault -> OverVoltage -> BatLost -> سایر سناریوها
```

هر سه LED در سناریوی ۷ از یک `bool__on` و یک phase مطلق استفاده می‌کنند؛ بنابراین
روشن/خاموش‌شدن آن‌ها هم‌زمان است. تنظیمات بوق سناریوی ۷ به فیلدهای مستقل خودش
متصل است و از ids `132..135` استفاده نمی‌کند.

### A3 — ممیزی پروتکل، NVM و پنل

- `ESP_PARAM_COUNT` و `ESPLINK_PARAM_COUNT` برابر `143` هستند.
- `TxOrder` و جدول‌های min/max تا id `142` کامل‌اند.
- `CDEF` شامل ۲۴ ورودی ext و ids `119..142` را پوشش می‌دهد.
- NVM format/version فعلی `12` و ظرفیت رکورد `144` entry است؛ سناریوی ۷ در
  NVM به‌عنوان تنظیمات مستقل ذخیره می‌شود، اما fault lockout خودش NVM نیست.
- bulk پارامترها chunk می‌شوند؛ سقف payload برابر `512` و هر chunk حداکثر
  `102` آیتم id/value است.
- clamp مستقل ids `137..142` در پنل preview و فرم پنل با firmware همسان است.
- import/export بر اساس شمای پارامتر انجام می‌شود؛ factory reset مقدارها را
  در صف محلی می‌نشاند و POST فقط با دکمهٔ global انجام می‌شود.
- stamp پنل با markup برابر است، markup متوازن است و سقف انتقال فعلی
  `380000` بایت است.

### A4 — اصلاحات مرزی، هم‌زمانی و API

- گارد برنامه‌ریزی فلش اکنون به‌جای جمعِ قابل‌سرریز، `END - byte_count` را
  مقایسه می‌کند و Erase/Program پیش از شروع و در پایان عملیات `WaitIdle` دارند.
- اعتبارسنجی NVM شناسهٔ ذخیره‌شده را پیش از cast به `uint8_t` بررسی می‌کند؛
  شناسهٔ نیم‌کلمه‌ای بیرون از فضای wire دیگر به شناسهٔ معتبر دیگری alias نمی‌شود.
- UART مقدارهای HAL را در تمام مراحل init بررسی می‌کند و پس از failure
  initialized اعلام نمی‌شود. پرچم producer، pump وقفه را تا کپی کامل فریم TX
  متوقف می‌کند.
- زمان RTOS در نرخ تیک صفر به تقسیم بر صفر نمی‌رسد و ارزیابی زمان‌محور
  عدم‌توازن در همان پاس متوقف می‌شود؛ شمارندهٔ باتری خراب نیز در مرز دقیق
  `UINT32_MAX` saturate می‌شود.
- getter/setterهای indexed که خروجی الزامی دارند، `NULL` را پیش از dereference
  رد می‌کنند؛ قرارداد setterهای replay-only که خروجی اختیاری دارند حفظ شده است.

### A5 — ممیزی مستندات و hygiene

این موارد به‌روزرسانی شدند:

- `Firmware/Modules/Charger/README.md`
- `Firmware/Modules/Fault/README.md`
- `Firmware/Modules/Ui/README.md`
- `Firmware/Modules/EspLink/README.md`
- `esp_link_panel/README.md`
- توضیحات جاری NVM/protocol در `esp_link_nvm.h`، `esp_link.h` و
  `plink_config.h`
- دو خط کامنت RTL فاقد direction mark در دو فایل با
  `tools/fix_rtl_comments.py`
- این گزارش ممیزی

## نتایج گیت‌ها

| بررسی | نتیجه |
|---|---|
| `python3 tools/audit_consistency.py` | **PASS — 436 invariant، 0 finding** |
| `bash tools/check_ai_rules.sh` | **PASS — ALL CHECKS PASSED؛ RTL comment check passed** |
| `bash tools/check_firmware_syntax.sh` | **PASS — syntax، ESP، UI و همهٔ Host suites موفق** |
| `python3 Firmware/Modules/Charger/Tester/host_test_charger.py` | **PASS — 52 تست، شامل سناریوی ۷، lockout و edge-hardening contracts** |
| Host تست‌های واقعی برد | جایگزین نرم‌افزاری؛ برد فیزیکی تست نشده |
| Build ARM/CubeIDE | در این محیط انجام نشده؛ `arm-none-eabi-gcc` موجود نیست |
| DOM تست پنل | در صورت نبود `jsdom`، SKIP اختیاری است |

## محدودیت‌های باقی‌مانده

1. تست واقعی رله، PWM، LM393/JIT، جریان صفر و هم‌زمانی LED با اسیلوسکوپ و
   current probe هنوز باید روی برد و در Excel اعتبارسنجی ماژول ثبت شود.
2. build نهایی CubeIDE به toolchain `arm-none-eabi-gcc` یا محیط CubeIDE نیاز دارد.
3. تست‌های DOM پنل به نصب اختیاری `jsdom` نیاز دارند؛ تست متنی، simulator server
   و audit مستقل از آن اجرا می‌شوند.

این گزارش ادعای تست سخت‌افزاری ندارد؛ Host tests فقط قرارداد و منطق قابل‌آزمون
بدون برد را اثبات می‌کنند.
