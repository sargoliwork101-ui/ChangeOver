## Grouped restore/send report
Current feedback coverage: 58 checks. Only matching readback consumes an
unchanged pending edit; mismatch-only batches retain retry. Recheck is read-only,
requires online telemetry and a different sequence, refreshes delayed results,
and preserves failed reads and newer edits. Retry excludes confirmed entries,
unrelated or newer edits and cancelled entries; both actions honor capture locks.
The full calibration runner passes, including 53 transaction and 32 restore
checks. Chromium: 288 checks at five widths, including a full-sized report,
internal scrolling with visible footer actions, isolated no-wrap numeric groups,
and count-based filters with accurate totals and accessible pressed states. No physical-board or flash-persistence proof.
Plain-language help distinguishes cached equality from persistence, absent feedback
from failure, and retry from sending unrelated pending edits. A 3-per-category
fixture exercises real selective retry (six writes) and leaves three unsent edits.
The HTML including NUL stays below the existing 491520-byte budget; it was not
raised. Older counts below describe earlier runs.

## Text-field color regression
The Chromium suite checks resolved foreground/background contrast (at least
4.5:1) for the filename dialog, manual duty list, capture note and visible
calibration controls at five widths. Filename focus, editing and extension
preservation are covered. Current run: 208 checks, zero failures. Production
changes are limited to text-field colors; download behavior is unchanged.

## Stable sample-row editing
`node ESP_Calibration/Tester/host_test_sample_rows.js` runs 22 real-panel DOM
checks for whole-row and checkbox clicks, retained row/table nodes and focus,
unchanged sample order, select-all, source deletion (including zero/last),
updated event indices and bench-repeat references, local persistence, stale
payload/code invalidation, missing-zero rejection and all three edit guards.
The full runner includes it; missing jsdom is SKIP, not PASS.
The Chromium layout suite also clicks populated sample rows at five widths,
checking actual scroll/row coordinates, node identity, order and deletion.
Current dedicated run: 178 checks, zero failures. No hardware tests implied.

## Named send feedback

`node ESP_Calibration/Tester/host_test_send_feedback.js` checks real-panel named
progress and per-variable confirmed/failed/missing/different-readback outcomes,
new pending edits and escaped labels: 30 checks, zero failures. Also registered
in the full runner. Layout coverage now includes the populated result dialog
at all five widths (158 browser checks). [Evidence and limits](SEND_FEEDBACK_VALIDATION.md).

## Current send-batch integrity audit

The transaction suite now has **53 checks**: a110168 baseline has 12 failures;
current HTML has none. It covers immutable validated batch values, edits while
POST/readback is pending, retention/re-send of new values, and both bounds for
all EVB chart/chip parameters. Recurring polling is stopped in the test harness;
controlled POST replies advance synthetic telemetry. Production polling is
unchanged. See [send audit](../AUDIT_PANEL_SEND.md) for limits and owner decisions.

## Current panel layout regression

`node ESP_Calibration/Tester/host_test_panel_layout.cjs` runs real Chromium
width/focus checks (also called by `run_esp_tests.sh`). Optional tooling:
`npm install --no-save --package-lock=false playwright-core @sparticuz/chromium`;
or install `playwright-core` and set `CHROMIUM_PATH` to a local Chromium binary.
Missing tooling is explicitly SKIP, not PASS. Final dedicated run: 153 checks,
zero failures. Details, baseline evidence and limits: [layout validation](PANEL_LAYOUT_VALIDATION.md).

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

## Lossless day codec (v1.153)

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

### Editable-export, filename and diagnostic regression coverage

`host_test_cal_revision.js` is part of the standard ESP runner and exercises
the real panel (explicit SKIP if jsdom is absent). It checks automatic sample
selection recalculation; exact JSON-v2 edited-table round trips; legacy v1;
malformed view rejection without mutation; full-precision signed source data;
settings backup view restoration; regenerated code; editable Persian filenames
for all export types; cancellation, invalid names, queued dialogs, Enter/Escape,
remote filename precedence, failed reads and canceled automatic backup writes.
It also checks suspect-pair/channel isolation, visible reasons, incomplete and
duplicate samples, failed-step attention, reduced motion, and real readback
mismatch attention versus a merely pending proposal. Bench repeat regressions
verify that failed-write attention clears immediately on successful recapture.

The new regression was first run against baseline 680b1ab and failed on the
missing automatic recalculation, then passed on the updated implementation.
The baseline full suite was 585 host / 13 labels / 287 existing DOM / 40 repeat /
20 point-edit / 7 storage-contract assertions PASS, with no DOM suites skipped.
Storage compression remains a separate unapproved proposal; the 53-field CSV,
171 settings and raw fit-sample precision are unchanged.

Final validation for this change: 585 host / 13 label / 287 existing DOM /
42 bench-repeat / 20 point-edit / 7 frozen storage / 47 new calibration-revision
assertions PASS. The 15-assertion live HTTP simulator suite and scenario-5/6
simulator self-tests also PASS. All eight additional repository gates exit 0;
`git diff --check` is clean. No DOM suite was skipped. Panel HTML is 484792 bytes
including NUL, under the unchanged 491520-byte ceiling. Native ESP8266 build and
physical-board tests were not performed by the agent.


### Restore audit regression coverage

`host_test_restore_audit.js` is included in `run_esp_tests.sh` (missing jsdom is
an explicit SKIP). Its 30 assertions cover strict numeric imports, real zero,
malformed containers/samples/IDs and derived id 70, queue/UI reconciliation,
settings cancellation, removal of automatic pending replay and dead LUT restore,
explicit audit-only LUT exports, real three-choice sample dialogs and Escape,
replace/append object identity, stale capture-row cleanup, preflight locks,
post-confirm clear locks, same-file reselection, restore failure collection and
a complete two-scenario capture that must stop after a failed inter-scenario
restore while preserving the first committed sample.

The new test was run against frozen 1159cd5 preview HTML and failed on the first
numeric-import assertion; it passes on the repaired panel. The existing 47-check
revision suite now explicitly requires settings-backup import to regenerate the
visible edited tables, not only restore the hidden cut array. Its legacy-import
fixture chooses replacement explicitly instead of relying on destructive Cancel.
The cross-file consistency gate now enforces removal of automatic queue replay
and its obsolete stored data rather than requiring a branch inside deleted code;
explicit imports remain restricted to the existing non-derived `XIDS` set.

Validation actually run after this audit: 585 host / 13 labels / 287 existing DOM /
42 repeat / 20 point-edit / 7 frozen storage / 47 revision / 30 restore-audit
assertions PASS; 15 live HTTP simulator integration assertions and scenario-5/6
self-tests PASS. All eight repository gates exit 0 (consistency 567 invariants,
zero findings; project-contract unit tests 10 PASS). `git diff --check` is clean.
The first syntax gate caught an obsolete audit assertion requiring the deleted
replay branch; the assertion was updated as described above and the gate rerun
successfully. No DOM suite was skipped. No native ESP8266 build, physical-board
restore or power-cut test was run. Firmware and ESP_USER are unchanged; the
bench storage proposal remains unimplemented and requires separate approval.


### Compact bench envelope tests

`host_test_bench_codec.h` is compiled into the actual-sketch ASan/UBSan suite.
It uses real production `EspBenchCodec` and HTTP handlers with stubbed LittleFS,
not a reference-only compressor. Coverage includes all fixture bit flips and
byte truncations; missing/duplicate records; short reads at every offset;
short writes at every frame byte and initial header/body boundary; failed
rollback, clear and readback; interrupted known-length download; legacy files;
171-token deltas/checkpoints/raw fallback; cap/free-space/physical accounting;
and a frozen real-panel 3-step + 2-repeat input fixture. See
`BENCH_CODEC_VALIDATION.md` for format, exact totals and caveats.

The first seven `host_test_bench_storage.js` assertions remain the frozen public
CSV/precision contract. Seven additional checks require retry-safe browser
settings signatures, visible old-file/integrity errors and non-destructive
legacy/damaged-file capture preflight. The simulator can
regenerate the integration input with
`BENCH_CODEC_FIXTURE=/tmp/bench-fixture.csv node tools/test_panel_preview_bench.js`;
set the same environment variable when running the real-sketch host binary to
check that freshly captured input through the production storage code. The
simulator itself still keeps CSV in memory; its size is not a device codec result.
A cancellation regression now counts only bench-download fetches, not unrelated
background telemetry polls, avoiding a false failure on slower test machines.


### Battery-only meter entry (input-current collection removed)

The trace audit found `wIi`/`ii` only in the capture form's validation, visible
bench row, CSV serialization and preview/test helpers, not calibration samples,
fit math, table generation or LUT packing. Source comparisons against 660ff3e
confirm calpush/calnorm/calfit/calbuild/lpack are unchanged. The form, required
input-current validation, visible input-current column and preview-only input
meter calculation are removed. CSV retains its legacy field as missing (`-`)
in new captures; old values are not rewritten or converted to zero. The compact
codec/schema and frozen old-file fixture are unchanged.

Before: 619 host / 13 labels / 287 DOM / 42 repeat / 20 point-edit / 14 storage /
47 revision / 30 restore assertions PASS. A new real-form assertion failed on
the old page before the source change. After: 619 host / 13 labels / 287 DOM /
46 repeat / 20 point-edit / 15 storage / 47 revision / 30 restore assertions
PASS. New checks cover missing field/table column; active-battery readings still
mandatory; battery-only save with the 53 CSV columns aligned and missing input
slot; nine visible table cells with status/repeat in the correct position; and
input-current independence of full-precision normalized calibration samples.
Draft restoration now explicitly tests the remaining battery current and note.
Existing repeat/restore workflows submit battery readings only, across both
single-channel scenarios. The actual HTTP simulator exercises both channels,
repeat and post-finish recapture with no input-current entry; 17 assertions PASS,
including every new attempt's empty legacy CSV slot. Its helper no longer writes
to a deleted DOM input. Early test edits had duplicate local names; those syntax
errors were fixed before the final successful runs. No DOM suite was skipped.
Firmware/ESP_USER are unchanged. Native ESP/hardware tests are not run.

Final repository checks for this removal: all eight gates PASS, cross-file audit
567 invariants / zero findings, project-contract unit tests 10 PASS and
`git diff --check` clean. Live preview serves build `14e0767` on port 3000.


### Compact bench icon presentation

`host_test_bench_icons.js` runs with the standard ESP runner; absent jsdom is an
explicit SKIP, not PASS. Its 24 checks cover nine compact routine actions,
Persian accessible names/native titles, keyboard focus/Enter/Space import, 44px
icon targets, a touch-accessible collapsible icon legend, closed static help,
always-expanded warnings/results, readable destructive/board-action labels,
original disabled locks and calculate/download/copy/repeat bindings. The new
suite was run against frozen da6b730 HTML and failed on missing compact icons,
then passed on the updated panel. Native details open/close and capture-form
cleanup also pass. Existing filename confirmations and actual HTTP capture/
repeat workflows still pass unchanged. Toolbar sizing is scoped so it does not
inflate every measurement table row. Simulator-only demo captions are shortened
with full Persian accessible names; its longer introduction is also collapsible.

Final runs: 619 real-sketch host / 13 label / 287 existing DOM / 46 repeat /
20 point-edit / 15 storage / 47 revision / 30 restore / 24 icon assertions PASS;
17 live HTTP simulator assertions and scenario-5/6 self-tests PASS. All eight
repository gates exit 0, with 567 cross-file invariants / zero findings and
10 project-contract unit tests PASS. No DOM suite was skipped. `git diff --check`
is clean. Existing calpush/calnorm/calfit/calbuild/lpack/wrow cores are unchanged;
there are no storage, math, transport, Firmware or ESP_USER changes. Embedded
HTML is 490178 bytes including NUL, within the 491520-byte transfer ceiling.
Documentation and the generated user manual are updated. These are DOM/host/
simulator checks, not pixel/screenshot validation or native ESP/hardware tests.

### Outline icons and simulation-free generated panel

The real bench now uses shared local outline SVG paths (20px artwork, 44px
routine touch targets); no icon font, CDN or framework is added. The icon suite
now has 28 checks, including consistent stroke/viewBox, explicit SVG dimensions,
decorative accessibility and line icons in the active capture form. Original
write/delete labels, locks, confirmations and action bindings remain intact.

`make_panel_preview.py` no longer contains or injects an offline simulator.
The generated HTML is a layout copy with the real font; it does not manufacture
telemetry or successful command replies. For live behavior use an ESP or the
separate Node test server. Normal Node previews load no demo toolbar or automatic
sample fixtures; `/preview-bench.js` returns 404 unless explicitly started with
`PREVIEW_BENCH_DEMO=1`. This flag is only for test tooling, never ESP firmware.
The HTTP regression opts in for its existing capture tests, and separately
checks the default page, disabled endpoint, empty sample list and generated HTML.

Validation: 619 host / 13 labels / 287 existing DOM / 46 repeat / 20 point-edit /
15 storage / 47 revision / 30 restore / 28 icon checks PASS, with no DOM skips.
22 HTTP integration checks and scenario-5/6 self-tests PASS. Rules, host syntax,
fault-bit mapping, project and module contract checks PASS; project-contract
unit tests 10 PASS; consistency audit 564 invariants / zero findings. Removed
three generator-simulation invariants because that simulator no longer exists;
external-server telemetry coverage remains. `git diff --check` is clean.
Embedded HTML including NUL is 490503 bytes (limit 491520). The calculation and
serialization cores calpush/calnorm/calfit/calbuild/lpack/wrow are unchanged.
Firmware and ESP_USER are untouched. No native ESP build, hardware test or
pixel/screenshot validation was performed. Removing preview-only simulation
saves no ESP flash: it was never embedded in the board's HTML.

### Hardware transaction ownership audit

`host_test_transaction_audit.js` loads the real generated panel and controls
only the async hardware/network boundaries. It is included in the ESP runner;
missing jsdom is explicit SKIP. The 38 assertions cover capture preflight/run,
global sends, voltage confirmation gaps, proposed LUT application, reset button
and handler ownership, duplicate reset POSTs, rejection without consuming pending
state, failure/cancel cleanup, emergency channel-off/manual exit and the existing
authorized automatic post-commit reset. Frozen da0fba4 HTML yields 25 failures;
updated HTML yields zero. `PANEL_TX_BASELINE` selects frozen HTML for reproduction.

Final runs: 619 host / 13 labels / 287 existing DOM / 46 repeat / 20 point-edit /
15 storage / 47 revision / 30 restore / 28 icons / 38 transaction checks PASS;
no final DOM SKIPs (jsdom 26 installed). Initial baseline run before installation
explicitly skipped DOM tests; that is not counted as a pass. 22 HTTP bench tests
and scenario-5/6 self-tests PASS. Rules and host syntax gates PASS; diagnostics
fault-bit and project-contract checks PASS; project-contract unit tests 10 PASS.
Module strict checker exits 0 but currently enforces zero declared modules.
Consistency audit: 564 invariants, zero findings. `git diff --check` clean.
Unchanged cores: calpush/calnorm/calfit/calbuild/lpack/wrow/wrestore/wchk.
HTML including NUL: 490987 / 491520 bytes, stamp f7cdb8d. Firmware/ESP_USER have
no edits. No native ESP8266 build, physical-board or power-cut test was performed.
This is a per-page guard, not server-wide exclusion of multiple clients.
Details and remaining owner decisions: `../AUDIT_TRANSACTIONS.md`.
