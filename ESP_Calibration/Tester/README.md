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
**533** host checks, **13** label checks, **270** click checks.

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
| `host_test_panel_labels.js` | جملهٔ فارسیِ هر ردیف تاریخچه (v1.118): تابع `evtRowText` از `plink_panel.h` بیرون کشیده و با سه ردیف صدا زده می‌شود — ردیف بروزرسانی، ردیف خطر و ردیف قدیمی. اگر کارت دوباره برای ردیفِ بروزرسانی یا بازگشت از قطع باتری بنویسد «پنل روشن شد»، تست رد می‌شود. jsdom نمی‌خواهد، پس همیشه اجرا می‌شود — ۱۳ بررسی |
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
می‌شود). شمارش فعلی: **۵۳۳** بررسی هاست، **۱۳** چک برچسب، **۲۷۰** چک کلیک.

پوشهٔ `node_modules/` در gitignore است. تست فایل
`ESP_Calibration/panel_preview.html` را می‌خواند، پس اگر `plink_panel.h` را ویرایش
کرده‌اید اول `python3 tools/make_panel_preview.py` را اجرا کنید (ممیزی اگر آن فایل
کهنه باشد خطا می‌دهد، پس اجرای کامل عادی خودش این را پوشش می‌دهد).

### پوشش موتیشن

با خراب‌کردن عمدی پنل و تأیید شکست تست راستی‌آزمایی شد — همه گرفته شدند: جدول
بالا در بخش انگلیسی فهرست کامل را دارد.

## Lossless day codec (v1.138)

`host_test_log_codec.h` is included by the real-sketch host suite. It exercises
all 18 integer fields, full/delta checkpoints, u32 and signed-delta extremes,
every truncation boundary, single-bit corruption, CRC-valid malformed packets,
partial-write retries, file-header recovery, exact public tail/CSV/boot outputs
and unchanged legacy text append. The stub has explicit short-write injection;
it never disguises binary files as text. See [validation results](LOG_CODEC_VALIDATION.md).

## Audit regressions (v1.139)

`host_test_log_audit.h` adds read-failure injection at every header/body byte,
non-destructive repair/retry, complete day recount and boot-state validation,
FIFO ordering after a one-shot open failure, and shared file/RAM newest-row
budgets with day filtering across midnight, missing-file and offline paths.
Baseline: 552 host / 13 label / 270 DOM PASS. After: 570 / 13 / 270 PASS.
See [audit results](../AUDIT_v139.md).

## Per-file deletion (v1.140)

`host_test_file_delete.h` tests exact owned/foreign removal, bad/empty/NUL paths,
missing/unavailable/failed removal, wrapped RAM FIFO preservation by day,
current-day cache invalidation and recreation, share/clock deletion, exact
JSON filename escaping and access to all 60 fixture files through pagination.
DOM tests cover each real delete button, escaped unusual names, cancellation,
exact POST selection, confirmation warnings, truthful failures and file paging.
Before: 570 host / 13 labels / 270 DOM PASS. After: 585 / 13 / 280 PASS.
All nine repository runners exit 0; preview-server self-tests also pass.
Native ESP8266 target build and physical flash tests were not run.

### Per-step bench repeat regressions

`host_test_bench_repeat.js` runs the real generated panel workflow with jsdom,
mocking only hardware and flash boundaries. It covers current/earlier/cross-
scenario repeats, interrupted drafts, sample identity/replacement, final review,
commit locks, fit invalidation, append-only CSV annotations, early End, link
loss, preparation/write failures, and two-column live current display. It is
part of `run_esp_tests.sh`; missing jsdom is explicitly SKIP, never PASS.

Validation for this change: 585 host + 13 labels + 287 existing DOM + 29 bench
repeat assertions PASS (jsdom installed, no skipped DOM suites). All eight
additional repository gates exit 0 and `git diff --check` is clean. Panel HTML
remains below its existing 491520-byte ceiling. Native ESP8266 compilation and
physical-board capture/repeat were not run by the agent.

### Generated-point editing and live simulator integration

`host_test_cal_point_edit.js` covers generated merged-point removal, per-channel
isolation, multiple removals, restoration, source preservation, recalculation,
mandatory-zero/minimum guards, fit/code/payload consistency, transaction locks,
suspect-point repair, invalid identities and stale-payload cancellation.
It is part of `run_esp_tests.sh` (explicit SKIP when jsdom is missing).

`node tools/test_panel_preview_bench.js` starts an isolated HTTP simulator on a
free port and drives the real panel with jsdom. It checks the three synthetic
fixtures, manual-mode capture, accumulating statistics, signed DMM helper,
earlier-step repeat/resume, final review, real CSV contents/size, malformed
append rejection and file clear. Server and browser are cleaned up on exit.
`node tools/panel_preview_server.js --self-test` retains scenario-5/6 checks.
These are software simulations, not a target build or physical-board validation.

Validation for generated-point editing: 585 host, 13 labels, 287 existing DOM,
29 repeat and 20 point-edit assertions PASS; 14 real-HTTP preview integration
assertions and preview scenario-5/6 self-tests PASS. No DOM suites skipped.
Point edits also clear stale generated code, requiring regeneration before copy
or download. The preview landing invariant now derives the calibration tab from
its bench form instead of enforcing the formerly requested chargers landing.

### Finished-session repeat and frozen storage baseline

Finished rows now remain repeatable in the same open page. The repeat suite has
40 assertions covering cancellation, live-link/storage/fixed-duty preflight,
confirmation races, duplicate-start prevention, preservation of original rows,
single-sample replacement and restoring the newly snapshotted board settings.
Live simulator integration has 15 passing assertions including reopening a
finished row through HTTP. `host_test_bench_storage.js` freezes the actual
53-field CSV format, signed fractional readings, unrounded fit samples and all
171 settings. Its 7 assertions pass against both the baseline and changed panel.
It is included in the standard ESP runner (missing jsdom explicitly SKIPs).
The storage investigation and proposed future compression safety gates are in
`BENCH_STORAGE_REVIEW.md`; no storage format or codec change is made here.
