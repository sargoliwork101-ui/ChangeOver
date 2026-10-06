# گزارش کیفیت کد نهایی — سناریوی ۷ — ۲۰۲۶-۱۰-۰۶
# Final code-quality report — scenario 7 — 2026-10-06

## دامنه / Scope

این پاس کیفیت نام‌گذاری، جداسازی ماژول‌ها، قراردادهای کامنت، اندازه و نوع
پارامترها، مسیرهای lockout و هم‌خوانی simulator/panel را بررسی می‌کند. منطق
اصلی برای refactor گسترده شکسته نشد؛ اصلاحات سناریوی ۷ در همان مالکیت‌های
موجود Charger/Fault/UI/EspLink نگه داشته شده‌اند.

This pass covers naming, module separation, comment contracts, parameter types,
lockout paths and panel/simulator mirroring. No broad refactor of the core
control logic was introduced; scenario 7 remains owned by Charger/Fault/UI/
EspLink as documented.

## نکات کیفیت مهم / Quality points

- `FAULT_CHARGER_TECHNICAL` مالکیت مرکزی Fault را دارد و Charger فقط detector و
  lockout policy را اجرا می‌کند؛ UI آن را با اولویت روشن و مستقل مصرف می‌کند.
- detectorها به‌جای threshold مبهم، از دو شرط صریح short/open استفاده می‌کنند:
  relay/PWM/JIT برای short و duty/current برای open. مقدار applied duty و
  current همان کانال خوانده می‌شود.
- lockout مسیرهای خروجی را در سمت MCU نگه می‌دارد و API آزادسازی فقط در init/reset
  است؛ این از آزادسازی تصادفی با پارامتر، NVM، retry یا app state جلوگیری می‌کند.
- LEDهای سناریوی ۷ با phase مشترک محاسبه می‌شوند و buzzer از ids `137..140`
  استفاده می‌کند؛ cadence LED در `141..142` است. q-maskهای سناریوهای ۵/۶ در
  این مسیر مصرف نمی‌شوند.
- پروتکل از map دستی قدیمی جدا شده است: `0..142`، chunk حداکثر ۱۰۲ آیتم،
  payload حداکثر ۵۱۲ بایت، NVM v12 با ۱۴۴ slot. `ESP_AGENT_SPEC.md` نیز current
  authoritative overlay دارد و بخش‌های تاریخی را از قرارداد جاری جدا می‌کند.
- قوانین کیفیت repository حفظ شده‌اند: نام‌گذاری کامل typeها، پیشوند
  `func__`، function brief/param docs، کامنت دوزبانه و علامت جهت برای فرمول‌های
  داخل متن RTL، بدون malloc/free و بدون `HAL_Delay` در مسیر RTOS.
- مسیرهای Flash/NVM/UART با guards صریح برای overflow، status failure،
  نیمه‌کپی TX، شناسهٔ wire و NULL خروجی بازبینی شدند؛ time conversion نیز
  برای ورودی صفر و tick-frequency صفر رفتار امن دارد.

## دروازه‌ها / Gates

| گیت | نتیجهٔ واقعی |
|---|---|
| `bash tools/check_ai_rules.sh` | **ALL CHECKS PASSED** |
| `python3 tools/fix_rtl_comments.py --check` | **PASS** |
| `bash tools/check_firmware_syntax.sh` | **PASS**؛ ESP 93، Charger 52، Imbalance 3805، Changeover 114، Fault 44، Protection 61، Jitter 24، McuPowerPath 49، CalLut 114، Measurement 286 |
| `python3 tools/audit_consistency.py` | **PASS — 438 invariant، 0 finding** |
| `git diff --check` | **PASS** |
| panel simulator matrix | **PASS**؛ current HTML/protocol/NVM map، endpoints و ids `137..142` بررسی شدند |

## محدودیت / Limitations

- `arm-none-eabi-gcc` در محیط موجود نیست؛ بنابراین این گزارش ادعای build/link
  واقعی ARM یا اندازهٔ نهایی Flash/RAM ندارد.
- دو suite رفتاری DOM (`host_test_panel_click.js` و
  `host_test_scenario_cards.js`) به‌دلیل نبود `jsdom` با قرارداد پروژه SKIP
  شدند؛ parser و تست‌های متنی/host همچنان اجرا شدند.
- هیچ تست فیزیکی روی رله، PWM، JIT، سنسور جریان یا باتری انجام نشده است.
- چند تابع اصلی فرم‌ور طولانی هستند، اما شکستن آن‌ها در این پاس به‌دلیل ریسک
  تغییر رفتار انجام نشد؛ این مورد کیفیت ساختاریِ باز است، نه failure در gate.

## نتیجه / Conclusion

کد و اسناد سناریوی ۷ از نظر gateهای repository، قرارداد current پروتکل و NVM،
تفکیک ownership و mirror پنل/simulator با موفقیت عبور کردند. مواردی که نیازمند
ابزار ARM، jsdom یا برد واقعی هستند صریحاً به‌عنوان محدودیت باقی مانده‌اند.
