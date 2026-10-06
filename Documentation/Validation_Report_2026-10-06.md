# گزارش اعتبارسنجی نهایی — سناریوی ۷ — ۲۰۲۶-۱۰-۰۶
# Final validation report — scenario 7 — 2026-10-06

## پرسش اعتبارسنجی / Validation question

هدف این پاس ثبت شواهد قابل تکرار برای سناریوی خطای فنی برد است، نه ادعای
تست روی سخت‌افزار واقعی. Host test جایگزین اجرای برد در این محیط است و این
دو موضوع نباید با هم اشتباه شوند.

The goal is to record repeatable evidence for the technical-board fault
scenario, not to claim a real-board run. Host testing is the available
substitute here, not a physical-board result.

## پوشش end-to-end / End-to-end coverage

| لایه | شواهد و نتیجه |
|---|---|
| Charger detector | `host_test_charger.py`: **52 تست PASS**؛ هر دو امضای transistor short/open، JIT، duty/current boundary، reset-only lockout و edge-hardening contracts را پوشش می‌دهد. |
| Fault ownership | `host_test_fault.c`: **44/44 PASS**؛ بیت ۷ و مسیر set/clear بررسی شد. |
| UI | UI host suite در syntax gate **PASS**؛ technical fault قبل از overvoltage/BatLost و هم‌فازی سه LED و استقلال تنظیمات بوق بررسی می‌شود. |
| Protocol/NVM | ESP host suite: **93 تست PASS**؛ current map با 143 پارامتر، bulk chunk، import/export/reset و NVM contract بررسی شد. |
| Panel simulator | matrix اجرایی **PASS**: `/`، `/f.css`، `/t`، `/m`، `/lut`؛ آرایهٔ telemetry دارای 31 فیلد و `p` دارای 143 مقدار بود؛ ids `137..142` با clamp و write مستقل بررسی شدند. |
| Static consistency | `audit_consistency.py`: **438 invariant، 0 finding**. |
| Rules/RTL hygiene | `check_ai_rules.sh`: **ALL CHECKS PASSED**؛ `fix_rtl_comments.py --check`: **PASS**. |
| سایر Host testerها | Imbalance **3805/3805**، Changeover **114/114**، Protection **61/61**، Jitter **24/24**، McuPowerPath **49/49**، CalLut **114/114**، Measurement **286/286** — همه PASS. |

## رفتارهای مورد قبول / Acceptance checks

- شرط short/burned فقط با relay open + applied PWM `0‰` + JIT ثبت‌شده فعال
  می‌شود.
- شرط open/burned فقط با applied PWM `>200‰` + current `0mA` فعال می‌شود.
- بعد از latch، هر دو مسیر شارژ/PWM متوقف می‌مانند؛ پنل یا NVM قفل را آزاد
  نمی‌کند و reset/power-cycle لازم است.
- سه LED سناریوی ۷ یک phase مشترک دارند.
- `TECH_BEEP_PERIOD/LEN/COUNT/GAP` و cadence LED در ids `137..142` مستقل‌اند؛
  هیچ borrow از `q132..q135` وجود ندارد.
- import/export و factory reset از همان schema فعلی 143-id استفاده می‌کنند؛
  ارسال تغییرات فقط با global send انجام می‌شود و POST خودکار وجود ندارد.
- تست source-contract جدید، overflow گارد فلش، WaitIdle، reject شناسهٔ NVM
  خارج از byte، HAL failure، atomicity صف TX، نرخ تیک صفر، saturation مرزی و
  NULL APIها را نیز pin کرد؛ این موارد نیازمند تست روی برد نیستند اما جای
  validation واقعی سخت‌افزار را نمی‌گیرند.

## تست‌های اجرا نشده / Not run

| مورد | وضعیت | دلیل |
|---|---|---|
| DOM interaction suites | **SKIP** | `jsdom` در محیط نصب نیست؛ خود تست‌ها طبق قرارداد با exit code صفر skip می‌شوند. |
| ARM compile/link و اندازهٔ Flash/RAM | **NOT RUN** | `arm-none-eabi-gcc` و `.map` تولید CubeIDE موجود نیست. |
| تست رله/PWM/JIT/جریان/باتری واقعی | **NOT RUN** | برد و بار واقعی در محیط در دسترس نیست؛ Host test جایگزین نرم‌افزاری است. |

## نتیجه / Conclusion

تمام شواهد قابل اجرای Host، simulator و consistency برای سناریوی ۷ سبز هستند.
تنها محدودیت‌های واقعی، DOM اختیاری، toolchain ARM و validation فیزیکی هستند؛
این موارد در گزارش به‌عنوان محدودیت باقی مانده‌اند و به‌اشتباه PASS اعلام
نشده‌اند.

## پاس تکمیلی پنل — ۲۰۲۶-۱۰-۰۶ / Panel hardening follow-up

- زیر هر کادر عددی تنظیمات سناریو، `qrng` بازهٔ حداقل/حداکثر همان فیلد را
  به‌صورت visible چاپ می‌کند؛ `panel_preview.html` از `plink_panel.h` دوباره
  تولید شد و اسکریپت inline آن parse شد.
- در سناریوی ۴، بازهٔ Firmware برای `q69` همان `۰..۱۰۰۰۰ ms` است و مقدار کمتر
  از ۲۰ ذاتاً نامعتبر نیست. علت تفاوت با جدول این است که Firmware ماندهٔ شارژ را
  روی ۲٪ کف می‌گیرد: با دورهٔ ۱۰۰۰ ms، این کف `۲۰ ms` است و خروجی برابر
  `max(20, q69)` می‌شود؛ بنابراین `q69=5` پذیرفته می‌شود ولی جدول عمداً زیر
  `۲۰ ms` نمی‌رود. همین توضیح به‌صورت زنده در کارت و تست سناریو pin شده است.
- در سناریوی ۶، کنترل‌ها و متن چراغ قرمز (`q130/q131`) از بوق مستقل
  (`q128/q129/q134/q135`) جدا شدند؛ simulator نیز هر دو را با برچسب مستقل
  گزارش می‌کند و دیگر بوق را بخشی از تنظیم چراغ معرفی نمی‌کند.

### خروج واقعی ابزارها / Actual tool results

| ابزار | نتیجه |
|---|---|
| `python3 tools/audit_consistency.py` | **PASS — 438 invariant، 0 finding** |
| `bash tools/check_ai_rules.sh` | **PASS — ALL CHECKS PASSED** |
| `bash tools/check_firmware_syntax.sh` | **PASS — ESP 93، Charger 52 و همهٔ host suiteها سبز** |
| `python3 tools/fix_rtl_comments.py --check` | **PASS** |
| `git diff --check` | **PASS** |
| `node esp_link_panel/Tester/host_test_scenario_cards.js` | **SKIP با exit 0**؛ `jsdom` نصب نیست |

تست DOM سناریو شامل بازهٔ visible، توضیح `q69=5` و جداسازی چراغ/بوق است؛ به‌علت
نبود `jsdom` اجرای رفتاری آن در این محیط انجام نشد. این محدودیت و نبود تست واقعی
برد (رله، PWM، JIT، جریان و ترانزیستور) ادعای PASS سخت‌افزاری ایجاد نمی‌کند.
