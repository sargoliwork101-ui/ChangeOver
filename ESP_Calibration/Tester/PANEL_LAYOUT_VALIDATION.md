# Calibration panel: editor stability and screen-width layout

## Scope

Requested fix: clicking editable numbers must not jump the page; information must
fit screen width with no horizontal scrolling. Only Calibration panel UI,
its generated preview, tests and documentation changed. STM32, ESP_USER,
calibration mathematics, board writes, file formats and storage are unchanged.

## Reproduced causes

- Editors were absolutely positioned below the clicked label in document
  coordinates. Near the viewport bottom, the focused input was outside the
  visible area. Chromium scrolled to expose it; cancelling left the new position.
  Before: a 320 px chart case moved from scrollY 326 to 352; the popup extended
  to y=753.6 in a 720 px viewport. Duty editors had the same placement/focus issue.
- At 320 px, the status page could measure 364 px wide. Fixed-width scenario
  tables required 560/680 px local horizontal scrolling; populated capture and
  sample tables measured 674/623 px inside a 276 px scrollport.
- Closing an unchanged chart editor rebuilt its SVG during blur. A subsequent
  click could target a label that was already detached, so it did not open the
  requested next editor.

## Fix

- One shared fixed-position placement routine uses the visible viewport,
  measures the popup before focus, flips above low anchors and clamps its
  position. Focus uses preventScroll; there is no scroll-position restoration
  or compensation. Popup prose wraps; narrow-screen inputs use 16 px text.
- Unchanged/cancelled chart edits no longer replace the SVG on blur. Changed
  values retain the existing pending/send workflow.
- Grid children may shrink; headers/actions wrap. At widths <=800 px, report
  tables become vertical records with the original column names above each
  cell. Dynamic table insertions receive the same labels, batching each table
  once per mutation callback. No columns/data/controls are deleted or hidden.
  Above that breakpoint, capture/sample cells wrap within fixed table layout.
- Only explanatory comments adjacent to the editor/chip code were shortened
  to preserve the ESP8266 HTML literal budget; historical reports were not edited.

## Automated checks

Run from the repository root:

```sh
npm install --no-save --package-lock=false jsdom@26 playwright-core @sparticuz/chromium
node ESP_Calibration/Tester/host_test_panel_layout.cjs
bash tools/check_firmware_syntax.sh
bash tools/check_ai_rules.sh
python3 tools/check_project_contract.py
python3 tools/check_module_contract.py --strict
python3 tools/check_diag_fault_bits.py
git diff --check
```

The browser test starts/stops its own simulator on an ephemeral port, loads the
actual firmware HTML and embedded font, and uses 320/375/760/1024/1440 px widths.
It checks status, both duty editors, settings subpages, all eight scenarios,
chart/chip editors near the viewport edge, direct label-to-label clicking,
populated capture/sample tables, the confirmation dialog and capture form.
Recurring telemetry timers are frozen to avoid DOM replacement between locating a
label and a mouse event. Explicit telemetry draws while an editor is open verify
that its focus survives. Existing overlapping chart annotations are not changed;
the test uses an actually exposed chart label, not a forced click through another
label. It checks document width AND local report scrollports; screenshots alone are
not the assertion. It does not synthesize writes to real hardware.

- Final dedicated browser run: **153 checks, 0 failures**.
- The identical browser suite on frozen pre-fix HTML completed 153 checks with 68 failures, including the
  concrete width/scroll failures above. Reproduction against frozen old source:
  `PANEL_LAYOUT_SOURCE=/path/to/old/plink_panel.h node ESP_Calibration/Tester/host_test_panel_layout.cjs`.
  Old markup may also terminate the test when a jumped/covered label cannot be
  clicked; that is not a PASS.
- Full host syntax gate PASS, including Calibration's 619 host checks, existing
  DOM suites and 38 transaction-audit checks. Browser suite is now also invoked
  by the Calibration runner. An absent optional browser dependency prints SKIP,
  not PASS; installed tooling was used for the final runs.
- Rules, project contract, diagnostics fault-bit check and diff whitespace PASS.
  Strict module checker exits 0, but currently enforces no declared modules.
- HTML literal including NUL: **491143 / 491520 bytes**; build **26c1685**.

## Limits

This is real headless Chromium geometry/focus testing, not jsdom layout emulation.
Native ESP8266 compilation, flashing, physical-board operation, Safari/Firefox
and mobile virtual-keyboard behavior were not tested. The simulator is not
proof of a hardware outcome. Existing explicit section-jump/navigation actions
and refresh-position restoration retain their original behavior.
