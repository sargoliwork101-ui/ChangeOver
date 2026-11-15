# Bench storage review and post-finish repeat validation

**Current status:** the owner subsequently approved implementation. The compact
bench envelope is now implemented and tested; see `BENCH_CODEC_VALIDATION.md`
for the format, measured 8540 → 3042 byte fixture and remaining hardware gates.
The original proposal/review below is retained as historical investigation, not
a statement that implementation is still awaiting approval.

Date: 2026-11-01. Baseline: a9fb590. Scope: ESP Calibration only.

## Decision and measured current behavior

**No bench storage format or daily-history codec change is implemented here.**
The owner asked for a proposal before risking calibration correctness. A separate
approval and migration/test plan are required before implementing compression.
Post-finish recapture is implemented independently of that decision.

## Plain-language scope and example

Two different stores must not be confused:

| Store | Purpose | Proposed change |
|---|---|---|
| ESP bench log | Detailed audit of measurements, settings and every attempt | Replace repeated full settings images with exact setting deltas between complete checkpoints; keep all measurements. Replace the on-device text legend with a schema ID. |
| Downloadable editable sample JSON | Reopen full-precision samples, use flags, per-channel exclusions and scenario scope to rebuild the same table | Keep this working round trip; do not replace it with a smaller CSV that rounds raw means. |
| Daily history | Separate operational event log | No new change in this proposal. |

Example: when only channel-1 duty changes from 10% to 20%, save the changed
setting ID and exact new value rather than repeating all 171 settings. Still
save the complete new measurement row and any repeat-attempt identity. Start
with a full settings image and periodically write another checkpoint; include
framing/integrity checks. On CSV download expand the internal representation
back into a readable file and supply the column legend then.

The fixture below is approximately 8.5 KB total today, including approximately
4.3 KB of repeated-file explanatory header. This is not a promised compressed
size or a hardware result. Benefit depends on real sessions, noise and settings
changes; the cost is a more complicated reader/writer and mandatory corruption,
power-cut and memory tests. No source values may be discarded or rounded.
Approval is still required before implementing any such format change.

Measurements are reproducible with `node tools/test_panel_preview_bench.js`.
They are simulator fixture bytes, not a physical-board flash measurement. Fixture:
three BOTH-channel steps (0/10/20 percent), one earlier-step repeat before finish,
and another repeat after final confirmation; five submitted data rows total.

| Component | Observed bytes / behavior |
|---|---|
| Actual `ESP_BENCHLOG_HEADER` C string | 4353 bytes, once when creating the device file |
| Five CSV measurements | 226, 226, 241, 241, 227 bytes in this run; **53 columns**, not the stale 56-column source comment |
| Four complete settings snapshots | 702, 706, 706, 702 bytes; 171 settings each |
| Submitted CSV, including run/repeat comments, excluding device-added legend | 4187 bytes in this run |
| Corresponding device-style total | About 8540 bytes including the legend; arithmetic estimate for this fixture, not hardware validation |
| Three latest compact browser sample objects | 250 JSON bytes in this run, not stored in ESP LittleFS |
| Existing file limit | 102400 bytes (100 KiB), unchanged |

Browser timestamps, sequence numbers and timing fields make sizes vary slightly
between runs. Source settings snapshots are skipped only when all 171 values
match the preceding snapshot. Duty parameters 16/18 change between steps, so a
normal sweep can trigger a complete settings snapshot at **each step**. Scenario
preparation also deliberately resets the signature, and repeat attempts append
new records. The bulk of avoidable overhead is full settings snapshots plus a
large textual legend, not the compact calibration samples alone. This is separate
from the roughly 491 KB web-panel program stored in the application region.

The current CSV formatter rounds raw means to one decimal, while browser fit
samples keep unrounded raw means. Therefore a byte-exact CSV round-trip alone is
**not sufficient proof of exact calibration reconstruction**. Any future binary
capture intended to reconstruct calibration must preserve exact raw sums/counts
(or a precisely specified equivalent) and exact signed DMM input values, rather
than adding another rounding or assuming the current CSV has all fit precision.

## Recommended design, pending approval

1. Keep all current measurements and audit attempts; do not discard quality
   statistics, negative readings, decimal DMM values or notes to save space.
2. Put a versioned schema identifier in the file, not the 4353-byte legend. Expand
   the ordinary human-readable CSV legend on download.
3. Start each run/checkpoint with a complete settings image. Between checkpoints,
   encode only changed setting IDs and exact new values. Do the equivalent for
   measurement fields only after measuring the benefit on representative runs.
4. Reuse the daily engine's **principles and reviewed primitives** (bounded frames,
   explicit length, per-record CRC, change mask, checkpoints), not its 18-u32-field
   schema. Bench includes signed/fractional readings, timestamps and text. No
   silent integer conversion, rounding, or loss of source measurement precision.
5. Make each record independently framed; only advance encoder state after a
   complete successful write. Distinguish truncated tails from read errors.
   Corruption/missing dependencies must be reported; never silently fit a
   damaged or incomplete calibration run as if it were complete.
6. Retain CSV export and explicit attempt identity/replacement behavior. Decide
   old-file handling and capacity accounting before deployment. Do not promise
   a compression percentage before the complete candidate codec is measured.

## Mandatory future storage-change tests

Run the same frozen baseline/candidate fixtures through the **real implementation**,
not just a reference compressor:

- Full reconstruction of all settings and measurement fields; signed extremes,
  fractional DMM input, negative/zero values, missing versus zero, notes and time.
- Exact fit samples, generated counts/current tables, gains and final transmitted
  payloads before/after. Include nearly rounding-boundary raw means and repeated
  samples; reconstructing only the formatted CSV is not an adequate oracle.
- Every byte truncation, partial write, read error and relevant bit flip; restart
  after each. Verify a damaged run is not presented as a complete usable run.
- Repeat after finish, replacement rather than double weighting, calibration
  selection changes, failed append, retry and scenario boundaries.
- Checkpoint reconstruction and settings changes on every wire ID, including
  manual duty, enables and restoration between scenarios.
- Flash cap/full storage, header/version mismatch, export/API behavior, memory
  bounds and benchmark size on clean, noisy and worst-case workloads.
- Native ESP8266 build and real-board power-cut/write-cycle tests before claiming
  hardware reliability. Those tests are not performed by this agent.

## Tests actually run for this change

Before edits: 585 host / 13 labels / 287 DOM / 29 repeat / 20 point-edit PASS.
A new post-finish availability assertion first failed on the baseline, proving
the requested behavior was absent. The same expanded regression then passed on
the implementation. The new frozen 7-assertion storage contract was run against
both baseline panel HTML extracted from a9fb590 and the changed panel; both PASS.
It verifies existing 53-field CSV order/format, signed fractional values, missing
versus zero, all 171 settings, exact fit-sample precision and current settings
snapshot behavior. It does not claim an unimplemented binary codec is validated.

After implementation: 585 host / 13 labels / 287 existing DOM / 40 repeat /
20 point-edit / 7 storage-contract assertions PASS. The live HTTP integration
passes 15 assertions, including finished-session reopening; scenario-5/6 simulator
self-tests also PASS. All eight additional repository gates exit 0 and
`git diff --check` is clean. No DOM suite was skipped. Native ESP8266 build and
physical-board recapture/power-cut tests were not run by the agent.
