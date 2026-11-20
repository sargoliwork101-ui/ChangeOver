# مدرک آزمون ESP_USER — ۲۰۲۶-۱۱-۰۴
## محدوده و نتیجه
- برنامهٔ تولیدی تحت آزمون: `3ca254c`؛ در این مرحله فقط تسترها و اسناد تکمیل شده‌اند. STM32، کالیبراسیون، منطق اجرا و پنل تولیدی تغییر نکرده‌اند.
- اجرای کامل `run_release_tests.sh` با کد خروج ۰ تمام شد؛ پیام نهایی آن در بخش خروجی پایین ثبت شده است. اجرای سبک قبلی نیز قبل از تغییر تسترها موفق بود.
- شمارش‌های زیر تعداد بررسی‌ها/ورودی‌های اجراشده‌اند، نه درصد پوشش کد یا تعداد خرابی‌های کشف‌شده. ادعای پوشش کامل کل پروژه یا تضمین نبود اشکال نداریم.
## محیط و بازتولید
- Linux، GCC/G++ 12.2.0، Node 22.22.3، jsdom 30.1.3، playwright-core 1.65.1، Chromium 153.0.8010.0؛ بسته‌های Node در `package-lock.json` ثابت هستند.
- منبع LittleFS: مخزن `littlefs-project/littlefs` در نسخهٔ `6a53d76e90af33f0656333c1db09bd337fa75d23`؛ اجرای اجباری هش `lfs.c`، `lfs.h`، `lfs_util.c` و `lfs_util.h` را نیز کنترل می‌کند.
- دستور اجرا در محیط این بررسی:
```sh
CHROMIUM_PATH=/tmp/chromium \
LITTLEFS_SOURCE=/tmp/memory-littlefs \
LD_LIBRARY_PATH=/tmp/al2023/lib \
bash ESP_USER/Tester/run_release_tests.sh
```
- مسیرها وابسته به محیط‌اند؛ برای اجرای مجدد، نصب بسته‌ها و دریافت منبع ثابت در [README.md](README.md) آمده است. `LD_LIBRARY_PATH` فقط برای کتابخانه‌های Chromium این محیط بود و الزام عمومی نیست.
- تست مرورگر پس از پایدارکردن دادهٔ نمونه و انتظار برای پاسخ/رندر غیرهم‌زمان، سه بار جداگانه نیز اجرا شد؛ هر سه بار ۳۰ بررسی موفق داشت. برنامه برای گذراندن تست تغییر نکرده است.
## خروجی ثبت‌شدهٔ اجرای کامل
```text
Ran 4 tests ... OK                       (mandatory-gate preflight failures)
== بررسی‌ها: 657، شکست‌ها: 0 ==
ALL 45 REAL CHARGE SESSION CHECKS PASSED
ALL 51 PRODUCTION PACK CHECKS PASSED
ALL 607 PACK EDGE CHECKS PASSED
ALL ESP_USER OV TESTS PASSED
ALL ESP_USER DIAG CLOCK TESTS PASSED
ALL 129 PANEL ACTION CHECKS PASSED
ALL 56 CHART REFRESH CHECKS PASSED
20 SETTINGS CHECKS; 0 FAILURES
28 BATTERY PERCENT CHECKS; 0 FAILURES
ALL 54 LOGIN / FORMAT CHECKS PASSED
ALL 55 SIMULATOR CALENDAR CHECKS PASSED
ALL 35 ABOUT CHECKS PASSED
ALL 70 BATTERY WEAR PREVIEW CHECKS PASSED
ALL 35 PRODUCTION PACK DOM CHECKS PASSED
ALL 28 PACK RACE / FAILURE CHECKS PASSED
Ran 10 tests ... OK                      (Python lab policy/routes)
REAL LITTLEFS 64KiB PASS: profile=0 minutes=129600 retained_days=4 reclaimed=86 peak_allocated=53248 blocked=0 write_errors=0
REAL LITTLEFS 64KiB PASS: profile=1 minutes=129600 retained_days=2 reclaimed=88 peak_allocated=53248 blocked=0 write_errors=0
REAL LITTLEFS 64KiB PASS: profile=2 minutes=129600 retained_days=2 reclaimed=88 peak_allocated=53248 blocked=0 write_errors=0
REAL LITTLEFS PACK POWER-CUT PASS: scenarios=2 interrupted_commits=8, physical_image=65536 bytes
ALL 30 LIVE PACK BROWSER CHECKS PASSED
ALL MANDATORY ESP RELEASE TESTS PASSED (host, DOM, real LittleFS, power cuts, live Chromium)
```
## هر مدرک چه چیزی را می‌سنجد؟
| مسیر | اثبات محدود به آزمون |
|---|---|
| `host_test_pack_review.cpp` | احراز هویت مسیر واقعی، ذخیره و بازخوانی، تأیید تعویض، مبنای مستقل ۰→۱، حفظ تاریخچه و شمارنده‌ها، شکست ذخیره، فرمت و کارخانه |
| `host_test_pack_edges.cpp` | CRC با بردار مستقل، تمام پیشوندهای ناقص و تغییر تک‌بیت، ۲۵۶ ورودی قطعی تمام‌بازه، سهم حافظه و خطای جایگزینی، گیت‌های ساعت/داده، نقش و توکن و کوکی و انقضا، جلوگیری از سرریز نسخه |
| `host_test_pack_races.js` | پاسخ قدیمی پیش از ذخیره، پاسخ دیررس پس از خروج، قفل نوشتن، خطای داده/نسخه/رکورد/حافظه/دسترسی، مهلت خواندن و منع نوشتن با تنظیمات نخوانده |
| `host_test_storage_littlefs.cpp` | سه مدل ۹۰روزهٔ هم‌زمان با تنظیم پک، لاگ، نمودار و تاریخچه روی LittleFS اصلی و تصویر ۶۴کیبی‌بایتی؛ پیشرفت ذخیره بدون خطای نوشتن و حفظ حساب مدیر و مبنای تعویض |
| `host_test_pack_powercut.cpp` | هشت قطع ناگهانی فرایند در مرز عملیات برنامه‌ریزی/پاک‌کردن هنگام دو ذخیره؛ پس از مونت فقط رکورد کامل قدیم یا جدید و سوابق/رمزهای بدون تغییر پذیرفته می‌شوند |
| `host_test_pack_browser.js` | HTML و دارایی تولیدی بدون افزونهٔ نمایشی، HTTP و ورود واقعی، فایل ذخیره‌شده، لغو/تأیید، نمایش و رفرش، پنج عرض ۳۲۰/۳۹۰/۶۴۰/۷۶۸/۱۲۸۰، توقف و شروع دوبارهٔ فرایند با UART قطع |
| `host_test_release_preflight.py` | نبود مرورگر یا منبع حافظه و منبع دستکاری‌شده، موفقیت محسوب نمی‌شود؛ آماده‌بودن ابزار هم با اجرای تست اشتباه نمی‌شود |
- تست مدل مستقل قدیمی و مدل پایتون، مدرک نوشتن روی ESP نیستند؛ در خروجی از مسیر تولیدی C++ و LittleFS تفکیک شده‌اند.
- عدد روزهای باقی‌مانده در فشار حافظه عمداً کمتر از ۳۰ است؛ این‌ها نتیجهٔ حذف قدیمی‌ها طبق سقف حافظه‌اند، نه تضمین نگهداری کامل ۳۰ روز.
## وضعیت بررسی‌های سراسری
این دو بررسی جدا از اجرای اجباری ESP دوباره اجرا شدند و هنوز ناموفق‌اند:
- `tools/check_ai_rules.sh`: یک یافتهٔ ممیزی سراسری: `panel pending-restore still accepts the derived id 70`؛ خروج ۱.
- `tools/check_firmware_syntax.sh`: آزمون کالیبراسیون `checkbox focus survives recalculation`؛ خروج ۱.
این خطاها در گزارش مرحلهٔ قبل نیز وجود داشتند. برای سبزکردن تست‌ها، رفتار تست یا کد کالیبراسیون دستکاری نشده است؛ فایل پیش‌نمایش کالیبراسیون که بررسی سراسری بازتولید کرد نیز وارد تغییرات نشده است. پس قبولی ESP به معنی سبزبودن همهٔ دروازه‌های مخزن نیست.
## محدودیت‌ها و آزمایش‌های باقی‌مانده
- ساخت هدف ESP8266/ESP32 و اندازه‌گیری RAM/Flash خروجی هدف در این محیط انجام نشد؛ ابزار هدف موجود نبود. کامپایل میزبان جای آن نیست.
- روی ESP فیزیکی چیزی نصب یا آزمایش نشد. صحت رادیو، فرسایش فلش، منبع تغذیه و رفتار گوشی واقعی اندازه‌گیری نشده‌اند.
- قطع برق شبیه‌سازی‌شده در مرز عملیات کامل برنامه‌ریزی/پاک‌کردن بود؛ قطع وسط سلول، همهٔ وضعیت‌های خرابی سخت‌افزار یا قطع برق وسط فرمت را تضمین نمی‌کند.
- شروع/مدت سابقهٔ شارژ از دادهٔ مشاهده‌شده می‌آید؛ این آزمون‌ها ظرفیت یا سلامت واقعی باتری را تأیید نمی‌کنند.
- قبل از نصب نهایی: ساخت هدف، آزمایش روشن/خاموش واقعی هنگام ذخیره و پس از ذخیره، حافظهٔ نزدیک سقف، ورود/خروج و قطع ارتباط روی ESP و گوشی واقعی لازم است.
