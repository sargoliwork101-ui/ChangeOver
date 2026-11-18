# Delta history validation — ESP Calibration v1.138

Date: 2026-11-07. Baseline: `618e731` (v1.137).
Scope: ESP_Calibration daily history only; bench capture, ESP_USER and Firmware
sources are unchanged. Existing caps remain 200 event rows/day and 30 days.

## Before / after

| Gate | Before | After |
|---|---:|---:|
| ESP real-sketch host checks, ASan + UBSan | 533 PASS | 552 PASS |
| Panel label checks | 13 PASS | 13 PASS |
| Panel DOM/click checks, jsdom 26 | 270 PASS | 270 PASS |
| `check_ai_rules.sh` | PASS | PASS |
| `strip_fa_comments.py --check` | PASS | PASS |
| `check_project_contract.py` | PASS | PASS |
| `check_module_contract.py` | PASS | PASS |
| `check_firmware_syntax.sh` | PASS | PASS |
| `check_diag_fault_bits.py` | PASS | PASS |
| `audit_consistency.py` | PASS | PASS |
| `python3 -m unittest tools.test_check_project_contract` | PASS | PASS |

All nine runners exited 0 before and after. The first baseline attempt skipped
DOM because jsdom was absent; jsdom 26 was then installed and the **actual**
270-check baseline completed before production edits. A skip is not counted as
PASS. The added 19 host checks include aggregate exhaustive loops, not merely
19 example records. Existing raw-storage assertions now explicitly decode binary
files; filesystem stubs still store the actual bytes, not an artificial CSV view.
The HTTP CSV result is independently compared byte-for-byte with expected rows.

## New coverage

- All 18 fields, all-zero/all-u32-max rows, seeded random changes, backward time
  transitions and ESP uptime reset; exact integer/text reconstruction.
- Actual positive/negative 33-bit ZigZag differences across 0 and UINT32_MAX.
- Every byte truncation of a 64-record stream: exactly the completed prefix
  survives; partial file signatures are recognized separately from unknown ones.
- Every single-bit flip in the first record: detection, no invented values,
  recovery at a later complete checkpoint.
- CRC-valid malformed frames: payload length, unknown kind, bitmap bounds,
  overlong varints and surplus payload; overlong delta chains cannot wrap the
  checkpoint counter.
- Actual production append with short writes at every byte of both a full and
  delta frame; retry removes the incomplete suffix without duplicate rows.
  Interrupted header writes and all partial-header lengths also retry correctly.
- Unknown/corrupted format headers are not overwritten.
- 200 immediate production writes, row count, bounded table tail including a
  small byte budget, complete streamed HTTP CSV and last-row boot restoration.
- A damaged HTTP export explicitly warns; boot tail repair truncates only an
  invalid suffix, never inserts CSV newlines into binary data.
- Existing text days still append as real text. The pre-existing suite covers
  RAM retry, day changes, retention, budget, bench priority and panel behavior.

## Size and performance — synthetic host fixture, not hardware

The production codec was exercised on a representative **synthetic** 200-row day
(changing timestamp, uptime, battery halves and currents; fixed other state).

| Same public rows | Bytes |
|---|---:|
| CSV including current column legend | 14,514 |
| Stored binary including header, per-record CRC and checkpoints | 3,306 |
| Reduction | 77.2% |

One ASan/UBSan host run measured **1,426 microseconds total** for 200 compressed
appends versus **549 microseconds** for the matching legacy CSV append path.
Compression adds host CPU work (~2.6x in that run); these are in-memory filesystem
stubs and say **nothing conclusive about real flash/network latency**. Timings
are printed by the runner, not asserted (a noisy host must not fail correctness).
Saving on real histories may differ substantially, especially when many fields
change on every row. Full frames may be chosen when smaller than deltas.

No event waits for a batch: each append writes its complete CRC-protected record
and closes the file. A bounded last-state cache avoids replaying the day on each
successful append; restart/error/cache miss replays from flash. Decoding and CSV
streaming use fixed row/frame buffers rather than allocating an entire day.

## Reproduce and remaining checks

Run `bash ESP_Calibration/Tester/run_esp_tests.sh` with g++, node and jsdom 26.
The script uses `-Wall -Wextra -Werror`, AddressSanitizer and UndefinedBehaviorSanitizer.
Run the eight other commands listed above for the remaining repository gates.
The user manual is regenerated with `tools/make_user_manual.py` (python-docx).

**Not run:** native ESP8266 target build (Arduino target toolchain unavailable),
physical flash/power-cut tests, actual device timing and endurance measurements.
The sketch was compiled and executed on the host, not flashed to a board.
On-device validation should check a new day, restart, abrupt power loss during a
write, HTTP CSV download and timing with realistic histories before deployment.
