# Panel behaviour tests / تست‌های رفتاری پنل

## EN

Everything else that checks the panel reads it as **text** — `host_test_charger.py`
greps `plink_panel.h`, `audit_consistency.py` cross-checks its tables against the
firmware. That is the right tool for "does this id exist", "does this window match
the firmware", "is this number copied correctly". It is the wrong tool for anything
that only exists while the page is *running*.

This directory holds the tests that load the real generated page into a DOM, click
the real controls and assert on what happens.

| file | what it proves |
|---|---|
| `host_test_panel_click.js` | the click-to-edit operating table: a cell opens an editor, the editor carries the raw value, Enter commits, Escape does not, a telemetry re-render does not delete the field being typed into, a clamped value is visible, and the read-only derived cell is inert |
| `host_test_scenario_cards.js` | the six scenario cards (v1.43/v1.44): each card explains what triggers it and is split into numbered sections, and every derived line is the number the board will really use — enter/exit thresholds, red/green/yellow blink milliseconds, the beep window against its period, the mV/percent translation on the 74/75 ladder, the per-band blocks (every variable of a band in one place) with each shared value having exactly one writable field and read-only echoes elsewhere, and the factory default printed under every field, and the clamp warnings. Since v1.51 it also covers the per-card LED/buzzer simulator: the widgets exist on all six cards, the old board-driven dots are gone, the engine runs with no board data at all, and the beep/blink helpers replay the firmware window maths and the v1.52 global send queue (nothing is posted until the button is pressed, undo restores the board value, one press posts each staged edit once, and the post-handshake report names every value the board clamped) - and the v1.56 cross-field rules the panel took over from the MCU (gap floor, window fit, band ordering, critical count shrink, min-off/min-on, low-battery clear, 100 mV ladder span, full exit under full entry), the v1.87 LUT readback audit, and the v1.88 calibration-first independent battery update/readback guards, the v1.90 commit/reset/flush gates and fresh post-reset readback, the complete backup/import layers for applied settings, pending edits, raw calibration samples and LUT readback, and the accessible icon-only backup controls - 363 checks. Values are typed into the real inputs, so a derived line that stops following the user is a failure |

### Why this exists

The click-to-edit table (v1.29, user order) has failure modes that no text check
can see:

- The table re-renders from telemetry roughly once a second. A naive implementation
  deletes the input the user is halfway through typing into. The markup is identical
  either way.
- An Escape key that saves instead of cancelling is worse than no Escape at all —
  the user believes they backed out of changing a safety limit.
- If the board clamps a value and the panel keeps showing what was typed, a ceiling
  is believed-set and is not set. This repo has already shipped exactly that class
  of bug (the OV cutoff default, where the documentation said 15000 and the board
  booted 14850).

Each of those is one line of JavaScript away at all times, and each one passes every
grep.

### Running

`jsdom` is **not** a dependency of this project. The firmware build must never need
node, so the test skips with exit code 0 when jsdom is absent:

```sh
npm install --no-save jsdom
node esp_link_panel/Tester/host_test_panel_click.js
node esp_link_panel/Tester/host_test_scenario_cards.js
```

`node_modules/` is git-ignored. The test reads `esp_link_panel/panel_preview.html`,
so run `python3 tools/make_panel_preview.py` first if you have edited
`plink_panel.h` (`audit_consistency.py` fails if that file is stale, so a normal
full run already covers it).

### Mutation coverage

Verified by deliberately breaking the panel and confirming the test fails — all
caught:

| mutation | caught by |
|---|---|
| re-render no longer blocked while editing | this test |
| Escape saves instead of cancelling | this test |
| input prefilled with the formatted string instead of the raw value | this test |
| clamp feedback removed | this test |
| the read-only duty cell becomes clickable | this test |
| a limit silently stops being drawn | `audit_consistency.py` |
| an `EVB` window drifts wider than the firmware's | `audit_consistency.py` |
| a limit loses its `EVB` row | `audit_consistency.py` |

---

## FA / فارسی

هر چیز دیگری که پنل را بررسی می‌کند، آن را به‌صورت **متن** می‌خواند —
`host_test_charger.py` داخل `plink_panel.h` را grep می‌کند و
`audit_consistency.py` جدول‌هایش را با فرم‌ور مقابله می‌کند. این ابزار برای
«آیا این شناسه هست»، «آیا این پنجره با فرم‌ور می‌خواند» و «آیا این عدد درست کپی
شده» ابزار درستی است. برای هر چیزی که فقط هنگام **اجرای** صفحه وجود دارد، ابزار
غلطی است.

این پوشه تست‌هایی را نگه می‌دارد که صفحهٔ واقعیِ تولیدشده را در یک DOM بار
می‌کنند، روی کنترل‌های واقعی کلیک می‌کنند و روی آنچه رخ می‌دهد ادعا می‌گذارند.

| فایل | چه چیزی را اثبات می‌کند |
|---|---|
| `host_test_scenario_cards.js` | شش کارت سناریو (۱٫۴۳/۱٫۴۴): هر کارت شرط ورودش را توضیح می‌دهد و بخش‌های شماره‌دار دارد، و هر خط محاسبه‌شده همان عددی است که برد به‌کار می‌برد — آستانهٔ ورود/خروج، میلی‌ثانیهٔ چشمک قرمز/سبز/زرد، پنجرهٔ بوق در برابر دوره‌اش، تبدیل mV↔درصد روی نردبان ۷۴/۷۵، بلوک هر باند (همهٔ متغیرهای یک باند یک‌جا) و یک‌جا-نوشتنیِ مقادیر مشترک با بازتاب فقط-خواندنی در جاهای دیگر، و چاپ پیش‌فرض کارخانه زیر هر فیلد، و هشدارهای گیره. مقادیر در ورودی‌های واقعی تایپ می‌شوند، پس خطی که دیگر کاربر را دنبال نکند یعنی تست رد شده |
| `host_test_panel_click.js` | جدول عملکردِ کلیک-و-ویرایش: خانه ویرایشگر باز می‌کند، ویرایشگر مقدار خام را دارد، Enter ثبت می‌کند، Esc نمی‌کند، بازرسمِ تلمتری فیلدِ در حال تایپ را پاک نمی‌کند، مقدار گیره‌خورده دیده می‌شود، و خانهٔ فقط-خواندنیِ مشتق بی‌اثر است |

### چرا وجود دارد

جدول کلیک-و-ویرایش (v1.29، دستور کاربر) خرابی‌هایی دارد که هیچ چک متنی
نمی‌بیندشان:

- جدول تقریباً هر ثانیه از تلمتری بازرسم می‌شود. پیاده‌سازی ساده‌انگارانه،
  ورودی‌ای را که کاربر وسط تایپ کردنش است پاک می‌کند. مارک‌آپ در هر دو حالت
  یکسان است.
- کلید Esc که به‌جای لغو ذخیره کند، از نبودِ Esc بدتر است — کاربر باور می‌کند از
  تغییر یک حد ایمنی منصرف شده است.
- اگر برد مقداری را گیره بزند و پنل همان چیزی را که تایپ شده نشان بدهد، یک سقف
  «تنظیم‌شده پنداشته می‌شود» ولی تنظیم نشده است. همین مخزن دقیقاً از همین دسته
  باگ منتشر کرده است (پیش‌فرض قطع OV، جایی که مستندات ۱۵۰۰۰ می‌گفت و برد ۱۴۸۵۰
  بوت می‌کرد).

هر کدام از این‌ها همیشه به فاصلهٔ یک خط جاوااسکریپت‌اند و هر کدام از هر grep سالم
رد می‌شوند.

### اجرا

`jsdom` وابستگی این پروژه **نیست**. بیلد فرم‌ور هرگز نباید به node نیاز داشته
باشد، پس نبودن jsdom یعنی خروج با کد ۰ و پیام SKIP:

```sh
npm install --no-save jsdom
node esp_link_panel/Tester/host_test_panel_click.js
```

پوشهٔ `node_modules/` در gitignore است. تست فایل
`esp_link_panel/panel_preview.html` را می‌خواند، پس اگر `plink_panel.h` را ویرایش
کرده‌اید اول `python3 tools/make_panel_preview.py` را اجرا کنید (ممیزی اگر آن فایل
کهنه باشد خطا می‌دهد، پس اجرای کامل عادی خودش این را پوشش می‌دهد).

### پوشش موتیشن

با خراب‌کردن عمدی پنل و تأیید شکست تست راستی‌آزمایی شد — همه گرفته شدند: جدول
بالا در بخش انگلیسی فهرست کامل را دارد.
