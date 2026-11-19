# Simple panel host tests / تست‌های هاست پنل ساده

## FA / فارسی

هر چیز دیگری که پنل ساده را بررسی می‌کند، آن را به‌صورت **متن** می‌خواند:
`audit_consistency.py` جدول‌ها و شناسه‌ها را مقابله می‌کند و شبیه‌ساز زنده فقط
«بالا آمدن» برنامه را نشان می‌دهد. هیچ‌کدام تله‌متری نیست: یک فریم با CRC اشتباه
یا پژواکی که خانهٔ صف را آزاد نمی‌کند، تا لحظه‌ای که روی برد واقعی نوشتن انجام
شود دیده نمی‌شود.

گام ولتاژ لاگ در آزمون واقعی روی ۵۰۰ میلی‌ولت قفل شده است: ۲۰۰ و ۴۹۹ ردیف نمی‌سازند، ۵۰۰ ردیف می‌سازد؛ محدودیت دقیقه‌ای، استثنای بالا آمدن در دشارژ و ثبت مستقل قطع برق نیز بررسی می‌شوند.

این پوشه تست‌هایی را نگه می‌دارد که **همان توابع خود برنامه** را روی PC صدا
می‌زنند (بدون کپی) و روی بایت‌هایی که رد و بدل می‌شود ادعا می‌گذارند.

| فایل | چه چیزی را اثبات می‌کند |
|---|---|
| `host_test_latch.cpp` | صف و فریم ریست قفل‌ها، CRC16، پژواک/NAK، مسیر عادی پارامترها، ترتیب سناریوی ۵ (۲۰۰، ۲۰۱، سپس ۲۰۲)، اینکه پنل فقط با پژواک تازهٔ شناسهٔ هدف و تعداد پاسخ‌های لازم موفقیت می‌گوید، اینکه تله‌متری کهنه فقط در نمودار زنده صفر می‌شود و نمونهٔ جعلی روی فلش نمی‌سازد، و نشست یکتا: ورود هم‌سطح رد، دسترسی بالاتر جایگزین، کوکی صاحب نشست، خروج و پایان مهلت. |
| `host_test_battery_percent.js` | رندر واقعی با حدهای مستقل شارژ/دشارژ برای پک و هر دو باتری، تغییر زندهٔ حدها، تعویض حالت، حذف محدودشدن دشارژ با سقف شارژ و حفظ رفتار FLOAT/پنهان‌شدن عدد حین شارژ. تست C++ نیز دریافت قاب پارامترها، انتشار هر دو جفت حد و استقلال درصد نمودار از سقف شارژ را می‌سنجد. |
| `host_test_ov.js` | آلارم اضافه‌ولتاژ پنل از سقف ورودی برد و هیسترزیس زنده ساخته می‌شود، نه از عدد ثابت. |
| `host_test_diag_clock.js` | ساعت زندهٔ کارت آمار: زمان کل شارژ در بی‌کاری، لینک قطع یا داده کهنه جلو نمی‌رود؛ با شارژ فعال و داده تازه جلو می‌رود؛ کارکرد برد جداست. |
| `host_test_chart_refresh.js` | رفرش صفحه و بازکردن تب، بوم پنهان، آماده‌نبودن context، تغییر اندازه و تراکم پیکسل، حذف درخواست تکراری، پاسخ دیررس بازهٔ قبلی، حفظ داده در خطای شبکه و جلوگیری از بازگشت تاریخچه پس از فرمت. |
| `host_test_log_codec.h` | ذخیره‌سازی فشردهٔ تاریخچه (پورت روش کالیبراسیون): سرصفحهٔ نسخه‌دار، رفت‌وبرگشت متن↔عدد با مقادیر مرزی، چک‌پوینت هر ۱۶ رکورد، بریدن فایل در هر بایت (فقط پیشوند کامل می‌ماند)، برگشت یک بیت (شناسایی، بدون عدد جعلی، بازیابی از چک‌پوینت بعدی)، رد فریم CRC-درستِ بدشکل، نوشتن نیمه‌کاره و فلش پر (بدون ردیف تکراری)، روز متنی قدیمی که متن می‌ماند، و کاهش حجم ذخیره‌شده در برابر CSV. |

تست `host_test_panel_actions.js` خود HTML و اسکریپت فرم‌ویر را بدون لایهٔ
شبیه‌ساز اجرا می‌کند: آیکون محلی، CSS سالم، قفل کلیک تکراری، پاسخ ذخیره،
پایان نشست، خطای شبکه و مهلت خواندن بدنه، دانلود تک‌روز و همهٔ روزها از حافظه، و ورود.
خروجی CSV روی نمودارها حذف شده است؛ تست DOM نبودِ دکمه و کد مخصوص آن را بررسی می‌کند.
اطلاعات فنی فقط داخل مدیریتِ تنظیمات، با برچسب انگلیسی و چیدمان چپ‌به‌راست نمایش داده می‌شود؛ پیش از ورود و پس از خروج مدیر پنهان است.
برای اجرای تست‌های DOM باید `node` و `jsdom` در دسترس باشند؛ اسکریپت در نبودشان
صریحاً `SKIP` چاپ می‌کند.

تست `host_test_settings.js` دسته‌بندی تنظیمات، نام‌گذاری دکمه‌های خطی، حفظ مسیرهای
ریست ۵/۶/۸، تأیید حذف، پاک‌شدن رمز فرم هنگام خروج و بسته‌شدن ورود، و ترتیب پاسخ‌های
فهرست فایل و لاگ را بررسی می‌کند. خواندن دیررس بدنه پس از خروج نباید فایل خصوصی را برگرداند.

تست اختیاری مرورگر واقعی، تنظیمات عمومی و مدیریت را در عرض‌های ۳۲۰، ۳۶۰، ۳۹۰، ۷۶۸ و
۱۳۶۶ پیکسل از نظر سرریز افقی، اندازهٔ دکمه و خطای اسکریپت می‌سنجد. این تست فقط چیدمان
را بررسی می‌کند؛ ورود و عملیات حساس در تست‌های جداگانهٔ بالا پوشش دارند.
با نصب `playwright-core` و یک Chromium سازگار اجرا شود:

```sh
CHROMIUM_PATH=/path/to/chromium node ESP_USER/Tester/host_test_settings_browser.js
```

متغیر اختیاری `SCREENSHOT_DIR` مسیر پوشهٔ موجود برای ذخیرهٔ تصویرهاست؛ تصویرهای آزمون
را داخل Git نگذارید.

تست `host_test_login_format.js` کلیک واقعی روی پیش‌نمایش تولیدشده را می‌سنجد:
ورود با رمز اولیه، رد مسیر تغییر رمز، ورود مجدد، فرمت با تأیید، انصراف، خطای فرمت و پایان نشست.
تست میزبان C++ هم ورود در حافظهٔ بازنشده، منع فرمت بدون مدیر، شکست واقعی فرمت و
حفظ رمز سفارشی پس از فرمت را بررسی می‌کند.

تست نشست بدون کوکی در C++ و DOM اضافه شده است: انتقال شناسه با هدر، رد دستگاه
دوم، ورود مجدد صاحب نشست، تازه‌شدن مهلت، خروج واقعی، حفظ نشست پس از فرمت و پاک‌کردن،
بازیابی پس از تازه‌کردن تب، خطای حافظه و شبکه و نمایش آیکون فرمت فقط برای مدیر.

### چرا وجود دارد

مسیر «ریست قفل‌ها» تنها مسیر **نوشتن** پنل ساده است و همین تازه بودن، خطرناکش
می‌کند: اگر ترتیب بایت‌های مقدار اشتباه شود یا CRC سرِ جایش نباشد، برد فریم را
دور می‌ریزد، پنل «نشد» نشان می‌دهد و کاربر فکر می‌کند قفل پاک شده است. هیچ grep و
هیچ اجرای شبیه‌ساز این را نمی‌گیرد؛ یک تست که خود بایت‌ها را می‌شمارد می‌گیرد.

نکتهٔ طراحی که تست ثبت می‌کند: بازهٔ شناسه‌ها (۲۰۰..۲۰۴) دقیقاً به‌اندازهٔ طول
صف است، پس با ادغام تکراری‌ها صف هرگز سرریز نمی‌شود و شمارندهٔ «صف پر» صفر
می‌ماند. اگر روزی شناسهٔ تازه‌ای اضافه شود، همین بررسی می‌شکند و به روزکردن
طول صف هشدار می‌دهد.

### اجرا

```sh
bash ESP_USER/Tester/run_simple_tests.sh
```

خروجی صفر یعنی قبول. برای دیدن جزئیات هر بررسی، همان اسکریپت را دستی اجرا کنید؛
هر بررسی یک خط `ok`/`FAIL` چاپ می‌کند و در پایان شمارش می‌دهد.

قبل از کامپایل، خود اسکریپت جاوااسکریپت درون `simple_panel.h` را هم به پارسر
`node` می‌سپارد (همان نگهبان «صفحهٔ سفید» که ممیزی برای پنل فنی دارد). اگر
`node` نصب نباشد این مرحله فقط هشدار می‌دهد و بقیهٔ تست اجرا می‌شود.

### محدودیت‌ها

- این تست «لینک واقعی» ندارد: فریم‌ها با همان مسیر گیرندهٔ برنامه تزریق می‌شوند،
  ولی تایمینگ UART و رفتار برد واقعی اینجا آزمایش نمی‌شود.
- برای دیدن کل زنجیره (پنل → `/latch/reset` → صف → فریم → پژواک شبیه‌برد)
  شبیه‌ساز زنده بالا بیاورید: `bash ESP_USER/Simulator/run_sim.sh`.

فرمت اکنون با خطای نوشتن واقعی، شکست mount، شکست ذخیرهٔ فایل حساب و تلاش مجدد
آزمایش می‌شود. DOM جداگانه پیام هر خطا، نتیجهٔ نامشخص در قطع ارتباط و کنترل
وضعیت نهایی حافظه را بررسی می‌کند. این پوشش، اجرای فرمت روی سخت‌افزار نیست.

تست حذف تکی: مجوز مدیر، فهرست عمومی/خصوصی، رد مسیر خراب و NUL، حذف دقیق فایل،
خطای واقعی remove، فایل ناموجود، حفظ نشست، حفظ فایل‌های دیگر، حذف حساب فقط از فلش،
بازسازی روز جاری، صفحه‌بندی و نام UTF-8/نقل‌قول‌دار. DOM تأیید و انصراف، تزریق
HTML در نام، خطای شبکه و پاسخ دیررس پس از خروج را بررسی می‌کند. کلیک واقعی
پیش‌نمایش آفلاین و درخواست HTTP به کد واقعی C++ نیز بررسی شدند.
ممیزی JSON وای‌فای با نام/رمز نقل‌قول‌دار، بک‌اسلش، کنترل و فارسی پوشش دارد؛
رمز در پاسخ وای‌فای وجود ندارد.

فرمت کامل، تاریخچهٔ خطا/قطعی/شارژ و همهٔ بازه‌های نمودار را از رم و فایل پاک می‌کند؛
پاک‌کردن دیاگ برد با همان فرمان موجود ارسال می‌شود و پاسخ کهنه تا تأیید خالی‌شدن پذیرفته نمی‌شود.
شکست خود فرمت داده را نگه می‌دارد؛ شکست mount پس از فرمت، رم کهنه را نگه نمی‌دارد.

تست `host_test_credentials.h` از runner اصلی C++ اجرا می‌شود: حفظ حساب موجود و شبکه در
نوشتن ناقص، بازنشدن فایل، شکست خواندن مجدد یا rename، بوت با فایل موقت نیمه، نبود
ریستارت/موفقیت جعلی، ماندگاری پس از ذخیرهٔ موفق، رمزنگاری فایل و محرمانگی لاگ شبیه‌ساز.
استاب `String::toInt` سقف علامت‌دار ۳۲ بیتی ESP را مدل می‌کند تا تفاوت `long` در PC
پنهان نماند. بازکردن فایل‌های معتبر با nonceهای ۰، ۲۱۴۷۴۸۳۶۴۷، ۲۱۴۷۴۸۳۶۴۸ و
۴۲۹۴۹۶۷۲۹۵، و رد nonce خراب/سرریز پوشش داده می‌شوند. آزمون DOM تنظیمات هم پیام
خطای ذخیره و حفظ ورودی برای تلاش دوباره را بررسی می‌کند. این تست‌ها جای آزمون
قطع تغذیه روی LittleFS واقعی نیستند.

## Battery retention regression

`host_test_battery_retention.h` runs inside the existing sanitized C++ suite.
It resets clock/log RAM between boots and feeds CRC-framed DIAG_DATA with the
existing reset counter (and HELLO_ACK for live advancement)
through the production parser. A 540-wake trace advances STM32 uptime by 180 seconds
per cycle, with another reply 58 seconds into each powered session: 97,258 seconds,
two midnight crossings, no browser or valid board epoch, exactly 28 stored rows.
It covers first-frame-before-Tick initialization, empty-new-day restoration,
excluded handshake/mains flags, hourly heartbeats, the 250 ms mains flush,
pending update/event preservation, phone sync before HELLO, silent-boot saves,
delayed saves after a board reply, board reset/re-anchor, and five clock/stamp
storage fault stages. These are accelerated host tests, not elapsed hardware tests.

## Chart journal and trigger regressions

`host_test_chart_journal.h` is included by the existing sanitized host executable.
It covers exact 3599/3600-second and 499/500-mV boundaries on either voltage in both
directions; cumulative changes from the committed (not incoming-frame) baseline;
no percentage-only or per-boot sample; strict clock gate; immediate persistence; replay;
every truncation of a new journal frame; short writes, post-write readback failure,
open/rename/no-space failures and retries; checkpoint-before-journal-removal power
loss; temporary-file isolation; file deletion/recreation; clear/reset; malformed
CRC-valid snapshots/deltas; 64-bit aggregation sums; 288-point HTTP serialization;
540 cold battery wakes; rejection of the former 300-second trigger; and a 90-day
trace of 2,161 hourly samples from 25,921 observations on a 64,000-byte budget
alongside 30 normal log days and 8,000 bytes of other files. The test File shim adds
a binary short-write limit; it is not firmware. The DOM chart tests remain in the
same runner. All timing/power-cut tests are accelerated host models, not hardware.

## Read-only administrator password

The credential fixture seeds a previously provisioned encrypted password without
using a firmware password setter. Regressions cover 540 credential reloads with a
legacy recovery marker, byte-for-byte unchanged account files, custom-password
login, 404 for guest/admin password-edit requests, unchanged active sessions and
custom-password retention through full format. Source checks cover the actual
sketch startup/loop as well as removal of the production setter/recovery code.
Direct account-save fault tests remain; only tests of the removed editing feature
were replaced. DOM tests assert editing controls/handlers are absent while the
login, Wi-Fi controls, password clearing, reset actions and format still work.

## Delayed verified clock / durable warning

`host_test_clock_gap.h` uses the real parser, clock, chart and log code. It covers
8-second rejection, no deadline-based trust, 39/40-second warning boundaries,
undated public log/CSV output, pending-marker survival across boots, delayed
same-reset recovery across midnight without a browser, one dated warning and a
fresh chart sample (no backfill). It also covers misleading larger uptimes after
changed reset identity, rejection of old retained board epochs, initial missing
clock, adjacent local fault/power histories, marker staging failures at all five
stages, retries, failed dated append retention, explicit format, and recovery
before 40 seconds without a spurious warning. DOM tests render the exact Persian
message and the unknown-time label.

`host_test_clock_loop.cpp` includes the real simulator entry/sketch and runs its
actual `setup()` and `loop()`. Advancing host milliseconds through 40 seconds
without board responses verifies repeated existing DIAG requests, no stale
TIME_SYNC even with stored diagnostics and live telemetry, no dated samples,
a durable warning, and recovery via a later CRC-framed UART response without any
browser clock request. Both C++ executables run under ASan/UBSan in the existing
runner. Time is accelerated; these are not hardware elapsed-time measurements.

## Shared 64 KiB storage regressions
`host_test_storage64.h` runs with the existing ASan/UBSan host suite: snapshot
failures, unchanged dirty flags, payload ceilings, durable clear/replay, current-day
compaction and honest deletion failures. Previous byte-count-only assertions are
not physical flash-fit proofs.
`host_test_storage_littlefs.cpp` runs production storage code on real LittleFS
with 16 erase blocks of 4096 bytes, 64-byte read/program/cache, block_cycles=16.
The in-memory NOR enforces erase-before-program; GNU stdio wrappers adapt the
existing File shim without copying production storage code. No network is used
by the runner. Supply upstream LittleFS sources at ESP8266's pinned commit:
`6a53d76e90af33f0656333c1db09bd337fa75d23`.
Run `LITTLEFS_SOURCE=/path/to/littlefs bash ESP_USER/Tester/run_storage_littlefs.sh`.
Three independent 90-day workloads run at one-minute observation intervals:
steady mains, one daily 2-hour outage, two daily 2-hour outages. Each includes
clock writes, hourly/voltage chart persistence, snapshots, final replay/clear and
1000 within-day burst rows. Assertions require zero ENOSPC writes, no pending
chart/dirty history, no RAM log loss, bounded day files and unchanged account
bytes. This is software validation, not physical power-cut or endurance testing.

## 18-column user log

Current log policy is today plus the preceding 29 local calendar days (at most 30 day files) and 200 ordinary rows, with the
4-KiB minimally trimmed day limit and 28-KiB total budget. Chart range remains 90
days. Schema tests verify the calibration column order without battery percent.
Day-start note 7 is not an event slot; clock-gap warning is note 8. Battery-wake
and midnight regressions ensure day markers do not reset hourly scheduling.
The old 22-column logs must be cleared before deploying; they are not migrated.

Row-cap regression: production RAM drain retains exactly events 8..207 from a
191+16 burst, preserving the start marker outside the 200-event cap. It also
checks a previously overfull day, temporary-file open/read/write/rename failure,
short writes, retry without duplicates, reopening and a cross-midnight drain.
Byte-limit eviction now measures successive re-encoded suffixes and commits only
the minimum necessary deletion together with the incoming event. The ordinary
day-full stop is unchanged.

Recovery/retention regressions cover every header/start-marker cut boundary,
verified-clock gating, 29/30/60-day age boundaries, local midnight/leap/year
transitions, expiration failure/retry and protection of account/Wi-Fi/chart files.
High-entropy byte-pressure rows verify the exact newest suffix, that one more
old event would not fit after re-encoding, and atomic failure preservation.

Audited retry regressions also check calendar cleanup/recount after failed day
initialization; cached and uncached 9-byte appends into a 4082-byte day without
staging/rename; exact full/delta size previews; and a stable clock-warning intent
before append. Warning delivery is tested across staging failure, append failure,
marker-removal failure, reboot, local-day/timezone changes and a later distinct
incident. The pending file remains in place on unreadable intent data. The
owner explicitly left the previous-day RAM start-marker behavior unchanged.

Factory reset coverage: admin plus explicit erase token; format/mount failures;
all ESP files removed without credential re-save; session revocation; default
admin/Wi-Fi/theme on reload; zero outgoing board commands and unchanged board
counters. The real setup/loop harness checks the post-erase barrier with dirty
RAM and the deferred ESP restart. Offline native clicks and Python lab checks
mirror the destructive route independently of ordinary credential-preserving format.
Chart DOM coverage includes untouched normal wheel, Ctrl-wheel, two-pointer
pinch, horizontal drag, bounds, full-view/range reset, polling/resize retention,
visible-sample tooltips, original-array preservation, and empty/single samples.

The simulator-only calendar test covers experiment isolation from firmware and
production HTML, Jalali month/leap lengths and Saturday-first placement, native
day/month controls, per-channel recorded counts, chronological daily detail,
unknown days, polling/focus retention, erase invalidation and malformed records.
The Python test verifies that the lab serves the exact production page and the
same calendar flash asset. No proposed status cards are implemented.

Calendar duration checks cover supplied seconds in Persian hours/minutes/seconds,
unknown zero/missing/invalid/out-of-range values, duration-only poll updates,
completion-time stability, original-data preservation and erase invalidation.
No duration is inferred from adjacent completions or cumulative board counters.

Start-time tests cover explicit source timestamps, prior-day date labels,
start-only polling changes, invalid/missing values without duration-based guesses,
input preservation and erase invalidation. Python fixtures expose separate start
and completion timestamps and never report an in-progress cycle as completed.

## زوم دو‌محوره و بازهٔ تقویم
- زوم ولتاژ با Ctrl+اسکرول یا دو انگشت، هر دو محور را تغییر می‌دهد؛ کشیدن نمودار در هر دو راستا کار می‌کند. محور عمودی از مقیاس کامل همان داده‌ها ساخته می‌شود و با پاسخ تکراری سرور جابه‌جا نمی‌شود. برچسب‌های ولتاژ تا میلی‌ولت دقیق‌اند و خطوط بیرون بازه بریده می‌شوند، نه اینکه به لبه بچسبند.
- آیکون خطی «نمایش کامل» در بالا چپ نمودار، هر دو محور را برمی‌گرداند؛ تغییر بازه و پاک‌کردن تاریخچه نیز زوم را بازنشانی می‌کنند.
- تقویم فقط امروز و ۲۹ روز قبل را قابل انتخاب می‌کند؛ ماه‌های خارج بازه بسته‌اند و داده‌های قدیمی‌تر یا روزهای آینده در شمارش نمی‌آیند. تست‌ها مرز نیمه‌شب، ماه‌های کبیسه، حفظ انتخاب و غیرفعال‌بودن روزهای خارج بازه را می‌سنجند. سوابق منقضی روی ESP نیز فقط پس از تأیید ساعت حذف می‌شوند؛ سهم حافظه ثابت است و نگهداری همهٔ نوبت‌های ۳۰ روز تضمین نیست.

## درباره ما و وضعیت نوبت‌های شارژ
- `host_test_about.js`: فوتر دائمی و مشترک همهٔ تب‌ها، دریافت خودکار از منبع مستقل، پنهان‌شدن عنوان/آیکون/ردیف خالی، شماره‌گیر و کپی (Clipboard و جایگزین HTTP)، شکست واقعی کپی، اعداد فارسی/عربی، معرفی اختیاری بدون متن ساختگی، لینک‌های ایمیل/واتس‌اپ/سایت و شبکهٔ اجتماعی، منع اجرای HTML/آدرس ناامن، جلوگیری از درخواست تکراری و تلاش دوباره، و منبع یکسان در پیش‌نمایش آفلاین.
- `host_test_clock_loop.cpp`: مسیرهای واقعی و عمومی `/about` و `/about.js` دقیقاً JSON اطلاعات `about_content.h` و کد `about_footer.h` را با `no-store` برمی‌گردانند.
- تقویم: وضعیت‌های کامل، نیمه‌تمام و جاری جدا هستند؛ زمان پایانِ نیمه‌تمام، آخرین ثبتِ جاری، مدتِ گذشته، شمارش همهٔ نوبت‌ها، تغییر وضعیت بدون ردیف تکراری، حفظ دادهٔ ورودی و متن تأییدشده بررسی می‌شوند. تولید سابقهٔ روی ESP از تله‌متری مشاهده‌شده است؛ تست مستقل C++ همین مسیر را می‌سنجد.

## ثبت واقعی نوبت شارژ در ESP
- `host_test_charge_sessions.cpp` همان کد برنامه را با ASan/UBSan اجرا می‌کند: شروع دو کانال، تغییر BULK/ABSORB بدون ردیف تکراری، تکمیل و نیمه‌تمام، توقف حرارتی/تلاش دوباره بدون احتساب مدت توقف، شروع مشاهده از وسط شارژ، وقفهٔ UART، تغییر ساعت، نبود ساعت، ریست/بازخوانی نوبت زنده، نوشتن ناموفق و تلاش دوباره، پاک‌کردن ماندگار، قالب ناقص/قدیمی/سرریز، سقف حافظه بدون حذف نوبت زندهٔ دیگر، مرز نیمه‌شب محلی و منع حذف با ساعت تأییدنشده، استقلال شمارنده‌های برد، خروجی HTTP و دریافت قاب CRC-درست از UART.
- مرورگر باید صفحه و دارایی تولیدی را در دو سرور، چیدمان ۳۲۰/۳۹۰/۶۴۰/۱۲۸۰، هم‌جواری راهنمای ولتاژ و آیکون، عدد/واحد یک‌خطی، خلاصهٔ کوچک و سادهٔ بدون بولد/کادر و حذف متن محور بررسی کند. تست روی میزبان جایگزین کامپایل هدف و آزمایش ESP فیزیکی نیست.

تست فوتر دو جای مستقل شماره، تماس و کپی مستقل، مخفی‌شدن شمارهٔ خالی و آیکون کپیِ بدون نوشته را می‌سنجد. راهنمای بازه و انتخاب روز در تست تقویم یک پاراگراف واحد است.
تست `host_test_battery_wear_preview.js` فقط پیش‌نمایش مستقل کارکرد باتری را بررسی می‌کند: مرزهای فرضی، نبود داده/حد، نوار محدودشده، نبود درخواست یا ذخیره‌سازی، و نبود این افزونه در فایل‌های برنامهٔ ESP و پیش‌نمایش عادی. این تست جزو اجرای `run_simple_tests.sh` است و تشخیص واقعی سلامت باتری را اثبات نمی‌کند.
