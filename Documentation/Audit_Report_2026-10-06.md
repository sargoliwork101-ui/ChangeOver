# گزارش ممیزی نهایی — سناریوی ۷ — ۲۰۲۶-۱۰-۰۶
# Final consistency audit — scenario 7 — 2026-10-06

## دامنه / Scope

این گزارش وضعیت نهایی ممیزی پس از افزودن خطای فنی برد، هماهنگ‌سازی
مستندات پروتکل/NVM، پنل و اصلاح کامنت‌های RTL را ثبت می‌کند. ممیزی هم
زنجیرهٔ فرم‌ور و هم قرارداد پنل را بررسی می‌کند؛ تست فیزیکی برد در این محیط
انجام نشده است.

This is the final post-edit audit for the technical-board fault scenario,
the protocol/NVM documentation sync, the ESP panel and RTL-comment cleanup.
It covers firmware/panel contracts; no physical board test was performed.

## یافته‌ها و نتیجه / Findings and result

- detector اتصال‌کوتاه/سوختن: رلهٔ شارژر باز، duty اعمالی `0‰` و JIT همان
  کانال ثبت‌شده؛ detector قطع‌شدن/سوختن: duty اعمالی `>200‰` و جریان همان
  کانال دقیقاً `0mA`.
- پس از تشخیص، lockout در RAM در هر دو مسیر PWM/شارژ باقی می‌ماند و فقط
  `Charger_Init()` در reset/power-cycle آن را آزاد می‌کند؛ پنل، retry، app state
  و NVM این قفل را آزاد نمی‌کنند.
- `FAULT_CHARGER_TECHNICAL` بیت ۷ است و UI آن را پیش از overvoltage و BatLost
  انتخاب می‌کند. سه LED از phase مشترک استفاده می‌کنند.
- تنظیمات مستقل سناریوی ۷ شناسه‌های `137..142` هستند؛ count/gap از
  `q132..q135` یا سناریوهای ۵/۶ borrow نمی‌شود.
- قرارداد لینک فعلی: `ESP_PARAM_COUNT=143`، شناسه‌های `0..142`، سقف payload
  برابر ۵۱۲ بایت و حداکثر ۱۰۲ جفت id/value در هر chunk؛ NVM نسخهٔ ۱۲ با ظرفیت
  ۱۴۴ entry و رکورد `12 + 144 x 8 + 4 = 1168 B`.
- قوانین پنل، از جمله عدم POST خودکار، صف staged و ارسال فقط با دکمهٔ سراسری،
  clamp/import/export، reset factory و گزارش موفقیت/خطا حفظ شده‌اند.
- گاردهای مرزی جدید Flash/NVM، statusهای HAL در UART، انتشار atomic فریم TX،
  guard نرخ تیک صفر، saturation شمارندهٔ dead-battery و قراردادهای NULL برای
  APIهای indexed نیز در همین پاس اصلاح و با ۵۲ تست Charger pin شدند.

### خروج ابزارها / Tool output

| ابزار | نتیجهٔ واقعی پس از آخرین ویرایش |
|---|---|
| `python3 tools/audit_consistency.py` | **PASS — 438 invariant، 0 finding** |
| `bash tools/check_ai_rules.sh` | **PASS — ALL CHECKS PASSED**؛ شامل RTL comment check |
| `python3 tools/fix_rtl_comments.py --check` | **PASS** |
| `bash tools/check_firmware_syntax.sh` | **PASS**؛ ESP 93، Charger 52، Imbalance 3805، و همهٔ testerهای Changeover/Fault/Protection/Jitter/McuPowerPath/CalLut/Measurement سبز |
| Panel simulator matrix | **PASS**؛ `/`، `/f.css`، `/t`، `/m` با HTTP 200؛ telemetry=31، params=143، clamp/write مستقل `137..142` و `/lut` status |
| `host_test_panel_click.js` | **SKIP اختیاری**؛ `jsdom` نصب نیست |
| `host_test_scenario_cards.js` | **SKIP اختیاری**؛ `jsdom` نصب نیست |

## موارد خارج از محیط / Not verifiable here

1. build واقعی STM32/ARM به `arm-none-eabi-gcc` و خروجی `.map` از CubeIDE نیاز
   دارد؛ این toolchain در محیط حاضر موجود نیست.
2. تست رله، PWM، JIT، سنسور جریان، باتری و power-cycle روی برد واقعی انجام
   نشده است. Host tests جایگزین فیزیکی هستند، نه ادعای validation سخت‌افزار.
3. دو تست DOM به دلیل نبود `jsdom` اجرا نشدند؛ تست parser جاوااسکریپت و تست
   متنی/سازگاری پنل اجرا و موفق شدند.

## نتیجه / Conclusion

ممیزی سازگاری و دروازهٔ قوانین پس از آخرین اصلاحات **قبول** است و stale claim
شناخته‌شده‌ای در قرارداد سناریوی ۷، protocol/NVM یا simulator باقی نمانده است.
محدودیت‌های ARM، DOM و سخت‌افزار واقعی صریحاً باز نگه داشته شده‌اند.

## پیگیری hardening پنل — ۲۰۲۶-۱۰-۰۶ / Panel hardening follow-up

بازهٔ عددی هر کنترل سناریو اکنون در خطی visible زیر همان کادر نمایش داده می‌شود؛
این خط از `min/max` همان ورودی ساخته می‌شود و preview تولیدشده نیز همین رفتار را
دارد. بررسی `q69` نشان داد `۰..۱۰۰۰۰ ms` بازهٔ معتبر پارامتر Firmware است؛ مقدار
`۵ ms` پذیرفتنی است اما به‌دلیل کف رفتاری ۲٪، در دورهٔ ۱۰۰۰ ms اثر واقعی کمتر از
`۲۰ ms` نمی‌شود. این تفاوت در کارت شارژ و simulator صریح شده است، نه اینکه
ورودی به‌غلط به حد ۲۰ تغییر داده شود.

سناریوی ۶ اکنون دو بخش مستقل دارد: چراغ قرمز با `q130/q131` و بوق با
`q128/q129/q134/q135`. خلاصهٔ simulator هم این دو را جدا برچسب می‌زند. صف staged،
import/clamp، ارسال فقط با global send، گزارش موفقیت/خطا و تنظیم مستقل سناریوی ۷
دست‌نخورده باقی مانده‌اند.

آخرین gate واقعی: `audit_consistency.py` با **438 invariant و 0 finding**،
`check_ai_rules.sh` با **ALL CHECKS PASSED**، syntax/host با **ESP 93 و Charger 52**
و سایر suiteها سبز؛ suite DOM به‌دلیل نبود `jsdom` با قرارداد پروژه SKIP شد و
تست فیزیکی برد انجام نشده است.
