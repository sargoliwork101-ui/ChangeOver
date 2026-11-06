# Panel behaviour tests / تست‌های رفتاری پنل

## EN

Everything else that checks the panel reads it as **text** — `host_test_charger.py`
greps `plink_panel.h`, `audit_consistency.py` cross-checks its tables against the
firmware. That is the right tool for "does this id exist", "does this window match
the firmware", "is this number copied correctly". It is the wrong tool for anything
that only exists while the page is *running*.

This directory holds the two tests that run the panel's own code: one loads the
real generated page into a DOM and clicks the real controls, and one calls the page's
own label function with no DOM at all. (The scenario-card simulator test that used to
live here was deleted with the simulator on 2026-11-01, `AI_AGENT_RULES.md:30-40`.)

| file | what it proves |
|---|---|
| `host_test_panel_click.js` | the click-to-edit operating table: a cell opens an editor, the editor carries the raw value, Enter commits, Escape does not, a telemetry re-render does not delete the field being typed into, a clamped value is visible, and the read-only derived cell is inert |
| `host_test_panel_labels.js` | the short English tag a history row gets (v1.123): the function `evtRowText` is lifted out of `plink_panel.h` and called with an update row, a danger row and a legacy row. It fails if the card goes back to saying "the panel came on" for a row that is really a firmware update or a return from the battery cut. No jsdom, so it always runs - 8 checks |

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
node ESP_Calibration/Tester/host_test_panel_labels.js   # no jsdom needed
npm install --no-save jsdom
node ESP_Calibration/Tester/host_test_panel_click.js
```

The normal way is `bash ESP_Calibration/Tester/run_esp_tests.sh`, which runs the
host tests, then the label checks (always, they need no DOM) and then the click
tests (skipped with exit code 0 when jsdom is absent). Current counts:
**510** host checks, **13** label checks, **259** click checks.

`node_modules/` is git-ignored. The test reads `ESP_Calibration/panel_preview.html`,
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
| the history card goes back to "the panel came on" for an update row | `host_test_panel_labels.js` |
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

این پوشه دو تستی را نگه می‌دارد که خودِ کد پنل را اجرا می‌کنند: یکی صفحهٔ
واقعیِ تولیدشده را در DOM بار می‌کند و روی کنترل‌های واقعی کلیک می‌کند، و یکی
تابع برچسبِ خود پنل را بدون DOM صدا می‌زند. (تست شبیه‌ساز کارت‌ها که قبلاً اینجا
بود، ۲۰۲۶-۱۱-۰۱ همراه شبیه‌ساز حذف شد — `AI_AGENT_RULES.md:30-40`.)

| فایل | چه چیزی را اثبات می‌کند |
|---|---|
| `host_test_panel_labels.js` | جملهٔ فارسیِ هر ردیف تاریخچه (v1.118): تابع `evtRowText` از `plink_panel.h` بیرون کشیده و با سه ردیف صدا زده می‌شود — ردیف بروزرسانی، ردیف خطر و ردیف قدیمی. اگر کارت دوباره برای ردیفِ بروزرسانی یا بازگشت از قطع باتری بنویسد «پنل روشن شد»، تست رد می‌شود. jsdom نمی‌خواهد، پس همیشه اجرا می‌شود — ۸ بررسی |
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
node ESP_Calibration/Tester/host_test_panel_labels.js   # jsdom نمی‌خواهد
npm install --no-save jsdom
node ESP_Calibration/Tester/host_test_panel_click.js
```

راه معمول `bash ESP_Calibration/Tester/run_esp_tests.sh` است: تست‌های هاست، بعد
چک برچسب‌ها (همیشه، DOM نمی‌خواهد) و بعد تست کلیک (بدون jsdom با کد ۰ SKIP
می‌شود). شمارش فعلی: **۴۹۷** بررسی هاست، **۱۳** چک برچسب، **۲۵۹** چک کلیک.

پوشهٔ `node_modules/` در gitignore است. تست فایل
`ESP_Calibration/panel_preview.html` را می‌خواند، پس اگر `plink_panel.h` را ویرایش
کرده‌اید اول `python3 tools/make_panel_preview.py` را اجرا کنید (ممیزی اگر آن فایل
کهنه باشد خطا می‌دهد، پس اجرای کامل عادی خودش این را پوشش می‌دهد).

### پوشش موتیشن

با خراب‌کردن عمدی پنل و تأیید شکست تست راستی‌آزمایی شد — همه گرفته شدند: جدول
بالا در بخش انگلیسی فهرست کامل را دارد.
