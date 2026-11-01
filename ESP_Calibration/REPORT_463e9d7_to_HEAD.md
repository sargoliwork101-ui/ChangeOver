# گزارش کار: از `463e9d7` تا `HEAD` شاخهٔ `arena/ad080916-changeover`

دامنهٔ گزارش: **۵ کامیت**، همه در `ESP_Calibration/` (پنل کالیبراسیون ESP8266).
فایل‌های بیرون این پوشه (`Firmware/`, `ESP_USER/`, `Documentation/`, `Tester/`های دیگر، `Simulator/`) دست نخورده‌اند.
شمارهٔ خط‌ها همان خط‌های **HEAD** است (`1302aa6`).

## خلاصه

| کامیت | موضوع | تست هاست |
| --- | --- | --- |
| `659b947` | ضربان ساعتی روی ساعت لاگ، نه روی آپ‌تایم پنل | ۴۰۸ → **۴۱۹** |
| `7e429b0` | اصلاح کامنت «۶ ساعت» | ۴۱۹ (بی‌تغییر) |
| `9854f5d` | قفسهٔ RAM تاریخچه از ۱۶۰ به ۲۵۶ بایت | ۴۱۹ (بی‌تغییر) |
| `74e5aa8` | ذخیرهٔ اتمیک ساعت + نسخه ۱۱۴ + حذف شاخهٔ ESP32 | ۴۱۹ → **۴۲۵** |
| `1302aa6` | یک تابع مشترک برای نوشتن ساعت + مستند اندازه‌ها | ۴۲۵ → **۴۳۲** |

مبنا در `463e9d7`: **۴۰۸** بررسی. در HEAD: **۴۳۲** بررسی، صفر شکست.

---

## ۱) `659b947` — «Count the ESP log heartbeat on the log clock, not on panel uptime»

**فایل‌های تغییرکرده**

| فایل | تغییر |
| --- | --- |
| `ESP_Calibration/plink_config.h` | +۱۲/−۱: تعریف `ESP_LOG_HEARTBEAT_SEC` و توضیح آن |
| `ESP_Calibration/plink_log.h` | +۱۳۵/−۹: دو متغیر `LogLastBeat*`، پارامتر خواندن/نوشتن فیلد پنجم، `HeartbeatDue` تازه، اتصال در `WriteRow`/`Tick`/`ResetStateAfterFormat` |
| `ESP_Calibration/Tester/host_test_esp_link.cpp` | +۱۱۲: ۱۱ بررسی تازه (`beat:`) |
| `ESP_Calibration/README.md` | +۲۵: بخش v1.114 |

**تغییر رفتاری**
پنجرهٔ یک‌ساعتهٔ ردیف «زنده‌ام» از `millis()` (آپ‌تایم خودِ پنل) به **ثانیهٔ دیواری ساعت لاگ** منتقل شد، و ثانیهٔ آخرین ردیف به‌عنوان **فیلد پنجم** `/evt-clock.txt` ذخیره می‌شود. اگر ساعتی معلوم نباشد، همان قاعدهٔ قدیمی پشتیبان است.

**دلیل**
بررسی خط‌به‌خط: ردیف‌ها تغییر‌محورند (`DetectChanges`)، و تنها ردیف دوره‌ای به آپ‌تایم پنل گره خورده بود. برد روی باتری هر چند دقیقه تغذیهٔ ESP را قطع می‌کند، پس شمارنده از صفر شروع می‌شد و ساعت هرگز کامل نمی‌شد → تاریخچه روی باتری خالی می‌ماند.

**شاهد (`فایل:خط`)**

- `plink_config.h:345` — `ESP_LOG_HEARTBEAT_MS 3600000u` (پنجرهٔ یک‌ساعته)
- `plink_config.h:353` — `ESP_LOG_HEARTBEAT_SEC (ESP_LOG_HEARTBEAT_MS / 1000u)` (همان پنجره روی ساعت لاگ)
- `plink_log.h:170` — `UINT32_T__G__LogLastBeatSec` / `BOOL__G__LogLastBeatValid`
- `plink_log.h:2170` — `func__EspLog_HeartbeatDue`: مقایسه با `func__EspLog_NowSec()` و `ESP_LOG_HEARTBEAT_SEC`؛ بازگردانی به عقب، پنجره را از نو شروع می‌کند
- `plink_log.h:556` — نوشتن ۵ فیلد در فایل ساعت (`%lu,%ld,%lu,%u,%lu`)
- `plink_log.h:645-654` — خواندن فیلد پنجم در `ClockLoad` با اعتبارسنجی `ESP_LOG_CLOCK_VALID_MIN`
- `plink_log.h:1651` — هر ردیف نوشته‌شده، پنجرهٔ ساعت را از نو می‌نشاند
- `plink_log.h:2325` — پس از نوشتن ردیف ضربان، ساعت **فوراً** ذخیره می‌شود
- `plink_log.h:3026` — `ResetStateAfterFormat` وضعیت ضربان را پاک می‌کند
- `plink_log.h:781` — `FwStampWrite` فیلد پنجم را دست‌نخورده نگه می‌دارد

**تست و نتیجه**
`bash ESP_Calibration/Tester/run_esp_tests.sh` → **ALL 419 ESP HOST TESTS PASSED**، `exit 0` (مبنا ۴۰۸ → ۱۱ بررسی تازه).
تست سرخ پیش از اصلاح (نسخهٔ دارای قاعدهٔ قدیمی در `/tmp/red_check`): **3 of 419 CHECKS FAILED**، `exit 1` — دقیقاً همان سه بررسی `beat:`.

---

## ۲) `7e429b0` — «Say the heartbeat is one hour on the log clock, not six hours on uptime»

**فایل تغییرکرده:** `ESP_Calibration/plink_log.h` (+۲/−۱)

**تغییر رفتاری:** هیچ. فقط متن.

**دلیل:** کامنت سرصفحه هنوز «ضربان شش ساعته» می‌گفت، در حالی که مقدار واقعی یک ساعت است؛ مستنداتِ دروغ، خوانندهٔ بعدی را به نتیجهٔ غلط می‌رساند.

**شاهد:** `plink_log.h:34-35` — «one-hour heartbeat (counted on the log clock, so a battery power cut cannot reset it)» در مقابل `plink_config.h:345`.

**تست و نتیجه:** `ALL 419 ESP HOST TESTS PASSED`، `exit 0` (بی‌تغییر، چون فقط کامنت است).

---

## ۳) `9854f5d` — «Hold a whole log row in the RAM ring instead of cutting it at 160 bytes»

**فایل‌های تغییرکرده**

| فایل | تغییر |
| --- | --- |
| `ESP_Calibration/plink_config.h` | +۸/−۱: `ESP_LOG_RAM_ROW_MAX` از ۱۶۰ به ۲۵۶ + توضیح |
| `ESP_Calibration/plink_log.h` | +۷: `static_assert` نگهبان |

**تغییر رفتاری**
ردیفی که فلش رد می‌کند، در حلقهٔ RAM کامل نگه داشته می‌شود. پیش‌تر `strncpy(..., 159)` بی‌صدا می‌برید و همان ردیف نصفه بعداً داخل فایل روز نوشته می‌شد (ستون‌های گم‌شده، بدون هیچ خطا یا شمارنده).

**دلیل:** اندازهٔ قفسه (۱۶۰) از اندازهٔ خودِ ردیف کوچک‌تر بود.

**شاهد**

- `plink_config.h:314` — `ESP_LOG_RAM_ROW_MAX 256u`
- `plink_log.h:962-965` — `strncpy(..., ESP_LOG_RAM_ROW_MAX - 1u)` و بستن رشته
- `plink_log.h:239` — نگهبان قدیمی برای ردیف اصلی (`ESP_LOG_ROW_MAX >= ESP_LOG_ROW_WORST_CASE`)
- `plink_log.h:246` — نگهبان تازه: `static_assert(ESP_LOG_RAM_ROW_MAX >= ESP_LOG_ROW_WORST_CASE, ...)`

**اندازه‌گیری (اجرای خودِ اسکچ روی هاست، همهٔ فیلدها روی حداکثر):** بدترین ردیف = **۱۸۱ بایت**؛ قفسهٔ ۱۶۰ بایتی ۲۲ بایت آخر را می‌برید؛ قفسهٔ ۲۵۶ بایتی کامل جا می‌دهد.

**تست و نتیجه:** `ALL 419 ESP HOST TESTS PASSED`، `exit 0`. این کامیت **تست تازه ندارد**؛ شاهدش دو چیز است: `static_assert` (خطای کامپایل اگر دوباره کوچک شود) و اندازه‌گیری بالا.

---

## ۴) `74e5aa8` — «Save the clock atomically, bump the version to 114, drop the ESP32 branch»

**فایل‌های تغییرکرده**

| فایل | تغییر |
| --- | --- |
| `ESP_Calibration/plink_log.h` | +۳۹/−۱۰۰: ذخیرهٔ اتمیک ساعت، حذف ۳ بلوک `#if defined(ESP32)` |
| `ESP_Calibration/plink_config.h` | +۷/−۳: نسخه ۱۱۲→۱۱۴، مسیر فایل موقت، اصلاح سند |
| `ESP_Calibration/plink_link.h` | +۲/−۳: حذف شاخهٔ `WiFi.setSleep(false)` |
| `ESP_Calibration/ESP_Calibration.ino` | +۱۸/−۱۸: یک هدف (ESP8266) و `#error` گویا |
| `ESP_Calibration/Tester/stub_arduino.h` | +۱۲: `StubFS::rename` (هم‌رفتار با `lfs_rename`) |
| `ESP_Calibration/Tester/host_test_esp_link.cpp` | +۳۴: ۶ بررسی تازه |
| `ESP_Calibration/README.md` | +۲۷/−۱: بخش تازه + اصلاح جملهٔ «نسخه عوض نشد» |

**تغییر رفتاری (سه مورد)**

1. **فایل ساعت اتمیک شد:** نوشتن در `/evt-clock.tmp` و سپس `LittleFS.rename` روی `/evt-clock.txt`.
2. **نسخه ۱۱۴ شد:** `FwCheck` مهر ذخیره‌شده را با همین عدد مقایسه می‌کند → ردیف «بروزرسانی فرم‌ور» دوباره نوشته می‌شود و `/t` نسخهٔ درست را می‌گوید.
3. **ESP32 حذف شد:** انتخاب برد اشتباه حالا با `#error` متوقف می‌شود، نه با خطای مبهم وسط `LittleFS`.

**دلیل**
`open(..., "w")` همان لحظه فایل قدیمی را خالی می‌کند؛ اگر نوشتن شکست بخورد (قطع برق روی باتری، فلش پر) ساعت + منطقه‌زمانی + ثانیهٔ ضربان با هم می‌رفتند. نسخهٔ ۱۱۲ با مستندات v1.114 نمی‌خواند و `FwCheck` را از کار انداخته بود. شاخهٔ ESP32 هیچ‌جا کامپایل نمی‌شد.

**شاهد**

- `plink_log.h:504` — `LittleFS.rename(ESP_LOG_CLOCK_TMP_PATH, ESP_LOG_CLOCK_PATH)`
- `plink_config.h:332` — `ESP_LOG_CLOCK_TMP_PATH "/evt-clock.tmp"`
- `plink_config.h:77` — `ESP_ESP_FW_VERSION 114u`
- `plink_log.h:805-809` — مقایسهٔ مهر با نسخه در `FwCheck` (نتیجه: `BOOL__G__LogUpdatePending`)
- `ESP_Calibration.ino:75` — `#error "This panel is ESP8266 only / این پنل فقط ESP8266 است"`
- `plink_link.h:1271` — تنها شاخهٔ باقی‌مانده: `WiFi.setSleepMode(WIFI_NONE_SLEEP)`
- بررسی پایانی: تعداد `#if defined(ESP32)` در همهٔ فایل‌های `ESP_Calibration/` = **صفر**

**تست و نتیجه**
`ALL 425 ESP HOST TESTS PASSED`، `exit 0` (۶ بررسی تازه).
سرخ‌آزمایی در `/tmp/redc` (برگرداندن به نوشتن مستقیم روی فایل اصلی): **1 of 425 CHECKS FAILED**، `exit 1` — `clock file: a refused save leaves the OLD file untouched`.

---

## ۵) `1302aa6` — «Write the clock file through one atomic helper, and document the I/O ranges»

**فایل‌های تغییرکرده**

| فایل | تغییر |
| --- | --- |
| `ESP_Calibration/plink_log.h` | +۴۰/−۴۵: تابع مشترک `ClockStore` و استفاده در **دو** نویسنده |
| `ESP_Calibration/Tester/host_test_esp_link.cpp` | +۲۹: ۷ بررسی تازه |
| `ESP_Calibration/README.md` | +۴۴/−۲: جدول اندازه‌های سنجیده‌شده و اعداد حافظه/فلش |

**تغییر رفتاری**
یک تابع مشترک (`func__EspLog_ClockStore`) هر دو نویسندهٔ فایل ساعت را پوشش می‌دهد: `ClockSave` و `FwStampWrite`. پیش‌تر فقط `ClockSave` اتمیک شده بود و `FwStampWrite` همان نوشتن مستقیم را داشت، یعنی مهر نسخه هم می‌توانست با یک قطع برق ساعت را پاک کند. اگر نوشتن یا `rename` شکست بخورد، فایل موقت پاک می‌شود و ساعت قبلی دست‌نخورده می‌ماند.

**دلیل**
در بازبینی «چک کن ورودی/خروجی‌هات درست باشد» معلوم شد یک مسیر نوشتن از قلم افتاده است.

**شاهد**

- `plink_log.h:482` — `func__EspLog_ClockStore`
- `plink_log.h:491` — نوشتن در فایل موقت
- `plink_log.h:501` — دور ریختن فایل نصفه
- `plink_log.h:504` — `rename` اتمیک
- `plink_log.h:563` — `ClockSave` از همین تابع می‌رود
- `plink_log.h:794` — `FwStampWrite` هم از همین تابع می‌رود
- `ESP_Calibration/README.md:26` — بخش «ورودی/خروجی و رنج (اندازه‌گیری‌شده)»

**تست و نتیجه**
`ALL 432 ESP HOST TESTS PASSED`، `exit 0` (۷ بررسی تازه: مهر ردشده ساعت را دست نمی‌زند، فایل موقتِ جامانده روز حساب نمی‌شود، `/evt/fs` نشانش می‌دهد، `/evt/clean` پاکش می‌کند و ساعت اصلی می‌ماند).
سرخ‌آزمایی در `/tmp/red1302` (برگرداندن `ClockStore` به نوشتن مستقیم): **1 of 432 CHECKS FAILED**، `exit 1` — `clock file: a refused save leaves the OLD file untouched`.

---

## اندازه‌های سنجیده‌شده (اجرای خودِ اسکچ روی هاست)

| بررسی | نتیجه |
| --- | --- |
| `GET /t` | ۱۲۹۴ بایت در بافر ۲۶۲۴ |
| `GET /m` | ۳۰۴ بایت در همان بافر |
| `GET /lut` | ۱۵۳ بایت در همان بافر |
| `GET /evt` | ۳۶۸ بایت در بافر ۳۶۰۰ |
| `/evt/days` | یک روز ۸ رقمی (`20261107`) |
| فایل ساعت | ۵ فیلد (۴ کاما)، ۲۵ بایت، با `\n` |
| بدترین ردیف در قفسهٔ RAM | ۱۸۱ بایت از ۲۵۶ |
| آرایه‌های بزرگ اسکچ | ۱۵٫۷ کیلوبایت؛ رشد این تغییرها ۱٫۵ کیلوبایت |
| فلش | تغییری نکرد (هیچ متن تازه‌ای در `PROGMEM` اضافه نشد) |

---

## آنچه این گزارش ثابت نمی‌کند

- تست‌ها روی **هاست** اجرا شده‌اند؛ `Serial`، `WiFi`، `LittleFS` و وب‌سرور شبیه‌سازی‌اند. رفتار روی برد واقعی (تایمینگ، مصرف، فلش واقعی) تأیید **نشده** است.
- تست DOM پنل اجرا **نشد** (`jsdom` نصب نیست) → SKIP، نه PASS.
- حجم نهایی sketch و حافظهٔ آزاد (`ESP.getFreeHeap()`) با زنجیرهٔ ابزار Arduino و روی برد اندازه‌گیری **نشده** است.
- صحت شبکهٔ شماتیک نسبت به پایه‌های واقعی (**net→pin**) ثابت **نشده** است.
