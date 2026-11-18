# Compact bench storage — implementation and validation

Date: 2026-11-01. Baseline: `7eab266`. Scope: ESP Calibration only.
Owner approved the proposed smaller ESP storage, retaining measurements,
editable sample files and readable CSV downloads.

## Result and deliberately bounded scope

`plink_bench_codec.h` is the production lossless envelope used by the actual
`/benchlog/add` and `/benchlog` handlers. It removes the on-device column legend
and repeated unchanged settings, NOT measurements. No extra numeric rounding,
integer conversion or signed/fractional loss is introduced. Every original
ASCII POST can be reconstructed exactly. Public CSV remains 53 measurement
columns; settings remain all 171 wire IDs. Raw fallback covers any otherwise
valid POST that is not a canonical complete settings snapshot.

Editable sample JSON remains the full-precision source for edited-table round
trips. Existing CSV formatting rounds raw means to one decimal; this change does
not pretend CSV can recover the unrounded fit samples. No fit algorithm, LUT
payload, repeat replacement identity, daily-history format/schema, Firmware or
ESP_USER source changed. This is not CSV session replay.

## Measured sizes (software fixtures, NOT device flash/timing)

| Fixture through production C++ HTTP/storage code | Previous CSV bytes including legend | New physical bytes | Result |
|---|---:|---:|---|
| Actual panel/HTTP three BOTH steps and two repeat attempts | 8540 | 3042 | 5498 bytes / 64.4% less for this fixture; downloaded CSV byte-identical |
| Synthetic 70 changing 171-setting snapshots | 69167 | 7450 | Byte-identical reconstruction through checkpoints and changing token widths |

The real-panel input is frozen at `fixtures/bench_capture_3step_2repeat.csv`:
4187 submitted bytes plus the existing 4353-byte download legend = 8540 CSV
bytes. It contains all five measurement attempts, run boundaries, all settings,
repeat annotations and timestamps. The production test posts each original
line, reads actual encoded stub-LittleFS bytes, downloads through the real
handler and compares **the whole CSV**, not only selected fields or a compressor
estimate. The fresh HTTP simulator fixture was also run through this path.
The simulator itself still holds CSV in memory; its CSV size is not advertised
as a compact-storage measurement.

The synthetic sweep cost about 0.15 seconds for all 70 appends plus download on
the host with sanitizers. This is not ESP8266 timing. Added persistent workspace
is 3107 bytes on the host (`State` + maximum frame); text decoding shares the
existing 2624-byte HTTP JSON buffer, since handlers are synchronous and the
serial drain does not modify it. No per-row heap allocation is used by the
codec; the existing bounded HTTP body String remains. Native layout/heap and
firmware flash size still need a device/toolchain check.

No universal compression claim: each POST has 15 bytes of integrity/framing
cost. Data dominated by short raw records or heavily changing settings can
save less or grow. Deltas larger than full settings fall back to full settings;
raw measurements are intentionally not compressed. The 100 KiB PHYSICAL cap
includes all overhead and is never exceeded to maintain a target row count.

## Internal format, version 1

All multi-byte integers are explicitly little-endian. No packed native structs.

- 12-byte header: `BNCH`, envelope version 1, CSV schema 3, settings count 171,
  reserved zero, checkpoint interval 16, three reserved zeros. Exact match.
- Each POST becomes one frame: `BF` (2 bytes), payload length u16, consecutive
  sequence u32 starting at 1, type u8, reconstructed text length u16, payload,
  reflected IEEE CRC32 u32 over the entire frame before the CRC (poly
  `0xedb88320`, initial/final XOR `0xffffffff`). Maximum payload 1536 bytes;
  maximum total frame 1551 bytes. All text must be printable ASCII/newlines and
  newline-terminated, as before.
- Type 0: exact raw POST text, including multi-line bodies and measurement,
  run/repeat/comment/note/timestamp strings. No field parsing or reformatting.
- Type 1: exact complete `# settings: ` line, 171 comma-separated lexical
  values, ending with newline. Each token is `-` or an optionally negative
  decimal integer of at most 11 characters. Signed limits, leading zeros and
  negative zero are preserved. Other settings syntax uses raw type 0.
- Type 2: 22-byte change mask, bit i denotes settings wire ID i. Unused high
  bits must be zero. In ascending ID order, each changed token is a u8 length
  followed by its exact bytes. Unchanged tokens come from the last validated
  settings snapshot. Decode must produce the declared length and valid complete
  settings syntax, consuming the entire payload. Missing dependencies fail.
- At the first settings line, after a raw body beginning `# run `, and after at
  most 15 intervening settings deltas, write a full settings checkpoint. Use a
  full settings frame whenever its payload is no larger than the candidate
  delta. No buffering of 16 events: EVERY POST is immediately written.
- Readers require exact sequence, bounds, CRC and schema. They do not skip
  corruption/resynchronize to a later checkpoint. Checkpoints bound settings
  dependencies; they are not permission to hide incomplete runs from the user.

## Failure, migration and capacity policy

- Filename stays `/benchlog.csv` for accounting/listing/deletion continuity,
  but new file contents are binary. Readable CSV is generated on download.
- Info identifies empty/compact/legacy/damaged files. Legacy preflight offers
  download-and-exit, not an impossible append; this path never touches hardware.
  Damaged-file preflight disables normal download/append, leaving clear/cancel.
- Known-header legacy CSV is still downloadable, unchanged. Append returns 409
  until the owner explicitly downloads/clears it. No automatic conversion,
  deletion, format or mixed binary/text append. Unknown/partial headers fail.
- Before every append, rescan the capped file from disk. No cached dependency
  state can survive reboot, clear, file-manager delete, format or prior failure.
  Verify full write counts; on partial write, attempt truncate to the old
  length. If rollback fails, preserve the damage and block later append/export.
- Reopen/validate size and frames after writing before HTTP 200. A failed
  verification read can leave a valid committed frame while returning 503;
  inspect before retry if exact audit-attempt accounting matters. This does
  not provide exactly-once network delivery (lost HTTP acknowledgment is also
  inherently uncertain without idempotency keys).
- Distinguish actual truncated file length from a short I/O read against the
  known complete length. Neither causes automatic tail deletion or a successful
  normal download. Explicit clear failures are reported, not called success.
- Validate ALL frames before sending download 200. Rewind and decode with exact
  expanded Content-Length. If a second-pass I/O failure occurs after headers,
  close early: browser fetch/blob must reject a short response. UART drain and
  yield happen between frames, on validation and export.
- Full-record suffix truncation cannot be distinguished from a legitimate
  complete prefix without an external committed manifest. A valid log is NOT
  a claim that a past session completed. CRC32 is corruption detection, not
  authentication. No new login/authentication scheme is introduced.
- Physical encoded bytes, not expanded CSV bytes, are charged to the existing
  cap/free-space reserve and history budget. Bench retains priority; oldest
  daily history is the only automatic space donor. Foreign files are not
  overwritten. The history-preemption host fixture was tightened from 35000
  to 36000 occupied bytes because removing the legend now leaves enough room
  in the old fixture; the test still verifies genuinely necessary preemption,
  not deletion that is no longer needed.
- The browser settings signature now advances only after successful append;
  an unsuccessful settings POST cannot suppress its retry. Old-file/damaged/
  readback failures have visible Persian messages. No new stale-link logic.

## Tests actually run

The full real-sketch host suite passes under AddressSanitizer and UBSan:
**619 checks** (34 new bench checks, previous 585 retained). The new tests include:

- Exact public POST/download round trips, 171 IDs, signs, missing vs zero,
  lexical limits/negative zero/leading zeros, decimals, timestamps and repeats.
- Every fixture byte truncation and single-bit corruption; deleted/duplicated
  full frames; malformed checksummed type/sequence/decoded lengths.
- Read failure at every stored-file offset, failed open/info read, and a read
  failure specifically AFTER successful download validation/response headers.
- Every partial existing-file frame write and initial header/body boundary,
  reboot/rescan/retry, failed rollback, failed post-write verification and clear.
- Full checkpoints, run boundaries, all-fields-changed/raw fallbacks, max POST
  size and invalid controls/NUL, physical cap, budget and insufficient space.
- Frozen real-panel/HTTP 3-step + 2-repeat fixture and fresh regenerated input.

The original seven public CSV/precision browser checks still pass. Seven added
checks verify failure-safe settings signature/retry, error reporting and legacy/
damaged-file preflight controls; suite now has **14** checks. Running it against baseline `7eab266` first fails at the
new signature assertion, reproducing that defect before the repair.
Other actual passing suites: **13 labels, 287 existing DOM, 42 repeat, 20
point-edit, 47 revision and 30 restore audit**. No DOM suite was skipped.
One cancellation check originally counted unrelated 300 ms telemetry polls as
"download fetches"; it now counts only bench-download fetches, preserving its
actual cancellation guarantee without a machine-speed race.

Live HTTP simulator integration: **15 assertions PASS**; scenario-5/6 simulator
self-tests PASS. All eight additional repository gates exit 0, including the
567-invariant cross-file audit (zero findings) and 10 project-contract unit tests.
`git diff --check` is clean. The early full ESP run intentionally revealed that
one old history-preemption fixture no longer needed deletion after removing the
legend; it was adjusted as explained above and rerun successfully.

**Not run:** native ESP8266 compile/link/flash-size report, physical-board
flash/heap/watchdog/latency/endurance tests, actual power-cut or Wi-Fi interrupted
transfer tests. Host stubs and simulated faults are not hardware certification.
Before physical deployment, verify firmware fits the chosen LittleFS layout,
free heap during the largest POST/download, and cut power during first header,
frame, rollback and close/flush. Confirm browser short-response rejection on the
actual ESP8266 web server. These remain hardware acceptance gates.
