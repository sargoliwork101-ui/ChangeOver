# Calibration tests — current execution guide

This directory contains development-only host and browser tooling, not code
flashed onto the ESP. Scope is `ESP_Calibration`; it does not establish that
other projects' gates pass or that a physical board is safe.

## Run the complete Calibration runner

Run from the repository root with Bash, g++, Python 3 and Node.js available.
For DOM tests install **jsdom 26**. For real-browser layout tests install
`playwright-core` and `@sparticuz/chromium` (Linux), or set `CHROMIUM_PATH` to
an installed Chromium executable with `playwright-core` available.
For example, keep optional dependencies outside the repository:

```sh
npm install --prefix /tmp/changeover-cal-tools --no-audit --no-fund jsdom@26 playwright-core @sparticuz/chromium
NODE_PATH=/tmp/changeover-cal-tools/node_modules bash ESP_Calibration/Tester/run_esp_tests.sh
```

Missing Node, jsdom or browser tooling prints **SKIP**, not PASS. A zero exit
code alone does not establish full coverage: inspect the output for skips.
The sketch itself does not depend on Node/npm. Build output stays in the
ignored `Tester/build/` directory; do not commit dependencies or test binaries.

If the panel source changes, regenerate its standalone preview first:

```sh
python3 tools/stamp_panel.py
python3 tools/make_panel_preview.py
```

These two commands update generated/source artifacts and should not be run
merely to read documentation. `run_esp_tests.sh` itself does **not** regenerate
the preview. Several DOM tests read `panel_preview.html`, while Chromium reads
the HTML literal in `plink_panel.h`; stale generated HTML can test a different
page. Standalone generated HTML has no fake telemetry or automatic successes.
The separate preview server reads files at startup and must be restarted after
panel changes; it is never a substitute for real-board testing.

## Suites actually registered in run_esp_tests.sh

The following totals are the verified current runner snapshot; historical
counts below belong to prior runs and must not be added to these totals.

| Entry | Checks / coverage |
|---|---|
| `host_test_esp_link.cpp` | 619 real-sketch host checks, including protocol, HTTP, file management, compact day and bench codecs; ASan/UBSan, warnings as errors |
| `host_test_bench_codec_string.cpp` | Separate compile/runtime regression for core-style String overload candidates; all 1536 payload lengths, exact bytes, canaries and short reads |
| `host_test_panel_labels.js` | 13 history-label checks; no jsdom needed |
| `host_test_panel_click.js` | 287 real-panel DOM/click checks |
| `host_test_bench_repeat.js` | 46 capture/repeat, battery-only meter and restoration checks |
| `host_test_cal_point_edit.js` | 20 generated-point editing checks |
| `host_test_sample_rows.js` | 22 selection/order/focus/sample-row checks |
| `host_test_bench_storage.js` | 15 public CSV/precision/signature and storage-preflight checks |
| `host_test_cal_revision.js` | 47 sample/selection round-trip, filename and diagnostic checks |
| `host_test_restore_audit.js` | 32 numeric import, staging/equality, cancellation, restore and transaction checks |
| `host_test_backup_package.js` | 39 checks: ZIP structure/CRC with independent Python extraction, split restoration, full/partial/absent data, body timeout, snapshots and lock ownership |
| `host_test_bench_icons.js` | 28 icons, visible action labels, keyboard and accessibility checks |
| `host_test_transaction_audit.js` | 53 immutable-batch, newer-edit and ownership checks |
| `host_test_send_feedback.js` | 58 named-result, filters, read-only recheck and selective-retry checks |
| `host_test_panel_layout.cjs` | 288 real Chromium geometry/focus/contrast checks at 320, 375, 760, 1024 and 1440 px |

The `host_test_bench_codec.h`, `host_test_log_codec.h`,
`host_test_log_audit.h` and `host_test_file_delete.h` files are included by the
C++ host suite, not independent runners. The frozen input CSV is in
[fixtures/bench_capture_3step_2repeat.csv](fixtures/bench_capture_3step_2repeat.csv).

### What the key regressions prove

- Queue values are snapshotted before async sending. An older acknowledgment
  cannot consume a newer pending edit. Only matching reports consume unchanged
  requests; mismatch/unknown requests remain eligible for selective retry.
- Recheck makes a GET to `/t`, validates online/sequence conditions and preserves
  the prior result on failure. These are cached board reports; changed telemetry
  sequence is not per-parameter freshness or flash-persistence proof.
- A three-per-category fixture checks all four report groups. Actual retry
  sends only six unresolved original values, not three matching or three
  unrelated unsent values. After confirmation the retry action disappears.
- The String regression brings enum-addition overload candidates into scope
  before compiling the production bench codec. It reproduces the original
  ambiguous character-pointer expression and accepts the explicit buffer
  address. The stand-in omits the shared stub's extra implicit numeric
  conversions; this is not a full ESP8266 core/toolchain build.
- Bench corruption/read/write tests use the real codec and HTTP handlers with
  an in-memory filesystem and failure injection. Byte-exact CSV is tested;
  CSV precision is not substituted for full-precision sample JSON.
- Chromium checks screen width, local scrollports, focus, actual row positions,
  contrast and visible result actions. jsdom is not used as a pixel/layout
  engine. Installed browser tooling is required for this part to run.

## ZIP-package validation

The full Calibration runner passed after the package change: exit 0, zero
failures and no SKIP, including the separate String regression and all 288
Chromium layout checks. The new package suite independently opens the actual
ZIP bytes using Python's standard `zipfile` reader, checks every member CRC,
and restores the extracted settings/sample JSON through production importers.
The previous export assertions now verify independent archive/settings/sample
members instead of expecting an embedded audit-only LUT in settings JSON.

The HTML ceiling remains 491520 bytes. Eight long historical JavaScript comment
blocks were compacted to make room; Acorn token streams before/after that
comment-only compaction were identical. No CSS, markup controls or numerical
fit/build/transport/storage logic was removed for size. The Persian user guide
is [repository-only](../BACKUP_GUIDE.md), not inside the archive or device image.
An additional real-Chromium smoke run used the actual filename dialog and Blob
ZIP download with explicitly routed test responses; Python independently opened
the resulting downloaded ZIP and verified CRCs and `complete:true`. This was a
browser test, not a download from a physical ESP.
The read-only global project-contract checker still reports the pre-existing
ESP_USER HTTP-route mismatch; Calibration's 27 method/routes match its unchanged
contract. That other module was not changed.
Native ESP8266 compilation, physical ZIP download and physical-board tests were
not performed. A host/Chromium pass is not evidence for those tasks.

## Earlier documentation-sync validation

The full runner was executed against runtime baseline
`ad79f5197c8e602f73c208938f49c20399768fdf` during this documentation-only sync:
**exit 0, zero failures, no SKIP**. The then-existing suite counts were
confirmed by that run, including the separate 1536-length String regression.
The ZIP package suite was added later.
Dependencies were jsdom 26, playwright-core and @sparticuz/chromium, installed
outside the repository. Production code, tests and the generated preview were
not changed. Native ESP8266 compilation and physical-board tests were not run.

## Current references and historical evidence

| Document | Meaning |
|---|---|
| [Production guide](../README.md) / [contract](../CONTRACT.md) | Current usage, wire units, routes, storage and limitations |
| [Send feedback](SEND_FEEDBACK_VALIDATION.md) | Current send behavior followed by explicitly separated prior results |
| [Bench codec](BENCH_CODEC_VALIDATION.md) | Implemented format and frozen fixture measurements; no physical timing claim |
| [Bench review](BENCH_STORAGE_REVIEW.md) | Historical proposal, subsequently implemented; not a current request for approval |
| [Day codec](LOG_CODEC_VALIDATION.md) | Historical before/after results for the day-format change |
| [Layout](PANEL_LAYOUT_VALIDATION.md) | Historical reproduction/fix evidence; its 153-check run is not today's suite total |
| [Transactions](../AUDIT_TRANSACTIONS.md) / [send audit](../AUDIT_PANEL_SEND.md) | Scoped historical audits and owner decisions, not a full-product safety certificate |

Current validation must be reported separately from historical PASS results.
Actual ESP8266 3.1.2 target compilation, hardware flashing, power-cut behavior,
endurance, native device timing and mobile virtual-keyboard behavior have not
been newly validated by these host/browser runs.
No fixture or simulated fault is embedded in the production HTML.

Earlier execution notes are preserved in [HISTORY.md](HISTORY.md). They are not the current run guide.
