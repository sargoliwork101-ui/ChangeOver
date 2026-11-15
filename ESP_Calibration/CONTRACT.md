# ESP_Calibration contract

```contract json
{
  "version": 1,
  "project": "ESP_Calibration",
  "root": "ESP_Calibration",
  "connected_board": "STM32",
  "not_simultaneous_with": "ESP_USER",
  "http_routes": ["GET /", "GET /benchlog", "GET /evt", "GET /evt/days", "GET /evt/dl", "GET /evt/fs", "GET /evt/pct", "GET /f.css", "GET /latch", "GET /lut", "GET /m", "GET /ping", "GET /t", "POST /benchlog/add", "POST /benchlog/clear", "POST /clock", "POST /evt/clean", "POST /evt/clr", "POST /evt/format", "POST /evt/pct", "POST /evt/snap", "POST /latch/reset", "POST /lut", "POST /lut/read", "POST /lut/reset", "POST /m", "POST /s"]
}
```

## Inputs and outputs

| Interface | Input | Output |
|---|---|---|
| UART, 921600 8N1 | STM32 protocol-v2 frames | Calibration commands and values in protocol-v2 frames |
| Wi-Fi AP `ChangeOver-ESP` (`192.168.4.1`; current source default key `123456789`) | Browser HTTP requests | Calibration panel on port 80 |
| LittleFS | `/benchlog.csv`, `/evt-clock.txt`, `/evt-YYYYMMDD.csv`, `/logpct.txt` | Compact bench envelope (CSV at download), time anchor, event-history files and the history share chosen in the panel (v1.134) |

## Commands

| Direction | Command | Input / output |
|---|---|---|
| STM32 → ESP_Calibration | `TLM_LIVE` (`0x10`) | 128 bytes: sequence, flags, 31 little-endian measurement/status fields |
| STM32 → ESP_Calibration | `HELLO_ACK` (`0x41`) | 24-byte board/protocol/link-health response |
| STM32 → ESP_Calibration | `PARAM_REPORT` (`0x11`) | Parameter id `u8` + applied value `u32 LE` |
| STM32 → ESP_Calibration | `PARAM_NAK` (`0x15`) | Rejected parameter id `u8` + reason `u8` |
| STM32 → ESP_Calibration | `PARAMS_BULK` (`0x12`) | Count `u8` followed by id `u8` + value `u32 LE` items; may span frames |
| STM32 → ESP_Calibration | `LUT_ACK` (`0x13`) | Stage, status, channel point counts and CRC32 |
| STM32 → ESP_Calibration | `LUT_DATA` (`0x14`) | Channel, first point, count, then `(counts:u32 LE, battery_mA:u32 LE)` pairs |
| ESP_Calibration → STM32 | `HELLO` (`0x40`) | Every 2 s: protocol version, firmware version, role `2`, flags and uptime |
| ESP_Calibration → STM32 | `SET_PARAM` (`0x01`) | Normal parameter writes: id `u8` 0–170 + value `u32 LE`; latch-reset actions also use ids 200–204 with value 0 (n=5 → 200–202, n=6 → 203, n=8 → 204); board returns applied value or NAK |
| ESP_Calibration → STM32 | `GET_PARAMS` (`0x02`) | Empty request for current parameter values |
| ESP_Calibration → STM32 | `LUT_BEGIN` (`0x04`) | Point counts for channels 1 and 2 (`u8`, `u8`) |
| ESP_Calibration → STM32 | `LUT_CHUNK` (`0x05`) | Channel `u8`, first index `u8`, count `u8`, then count pairs `(counts:u32 LE, battery_mA:u32 LE)` |
| ESP_Calibration → STM32 | `LUT_COMMIT` (`0x06`) | CRC32 `u32 LE` of staged table contents |
| ESP_Calibration → STM32 | `LUT_RESET` (`0x07`) | Four bytes `RST!`; sent only after successful commit and point-by-point readback |
| ESP_Calibration → STM32 | `LUT_READ` (`0x08`) | Empty request for active table points |
| ESP_Calibration → STM32 | `WIFI_CLIENT_STATE` (`0x33`) | One byte: connected `1`, disconnected `0` |
| Either direction | UART frame | `AA 55`, version `2`, type, length `u16 LE`, payload, CRC-16/CCITT-FALSE `u16 LE`; payload maximum 512 bytes |
| Browser → ESP_Calibration | `POST /s?id=&v=` | Parameter id 0–170 and integer value; board-side range clamp; JSON `{ok}` |
| Browser → ESP_Calibration | `POST /m` / `GET /m` | POST resets the sample window and queues `GET_PARAMS`; GET returns JSON `n`, `s[31]`, `lo[31]`, `hi[31]`, `la[31]`, `or`, `seq`, `fl` |
| Browser → ESP_Calibration | `POST /lut` | Plain CSV: `n1,n2`, then `n1` pairs `(counts,mA)`, `n2` pairs `(counts,mA)`, final CRC32; 0–24 points per channel |
| Browser → ESP_Calibration | `GET /lut` | JSON transfer status and, when ready, readback point pairs `r1`, `r2` |
| Browser → ESP_Calibration | `POST /lut/read` | Empty body; starts active-table readback; JSON pending result |
| Browser → ESP_Calibration | `POST /lut/reset` | Empty body; JSON result; refused unless commit CRC and point-by-point readback match |
| Browser → ESP_Calibration | `POST /latch/reset?n=` | `n=5` queues zero for ids 200–202; `n=6` id 203; `n=8` id 204; JSON result |
| Browser → ESP_Calibration | `GET /latch` | JSON last slot echo, counts, NAK and queue status |
| Browser → ESP_Calibration | `GET /benchlog?i=1` | JSON filesystem status, physical encoded size, maximum size and format (empty/compact/legacy/damaged); 503 on read/open failure |
| Browser → ESP_Calibration | `GET /benchlog` | Validated expanded CSV download; header-only CSV when empty; 409/503 for damage/read failure |
| Browser → ESP_Calibration | `POST /benchlog/add` | Printable ASCII CSV body, newline-terminated, maximum 1536 bytes; JSON result; HTTP 507 at 102400-byte physical cap; 409 for legacy/damaged append, 503 on storage failure |
| Browser → ESP_Calibration | `POST /benchlog/clear` | Empty body; JSON result |
| Browser → ESP_Calibration | `POST /clock?sec=&tz=` | Unix seconds and timezone offset; JSON result and current seconds |
| Browser → ESP_Calibration | `GET /t` | JSON live telemetry, parameter values and link/handshake status |
| Browser → ESP_Calibration | `GET /ping` | Plain-text ESP/filesystem/log status |
| Browser → ESP_Calibration | `GET /` / `GET /f.css` | Panel HTML / font CSS |
| Browser ↔ ESP_Calibration | `/evt`, `/evt/days`, `/evt/dl`, `/evt/clr`, `/evt/snap`, `/evt/format`, `/evt/fs`, `/evt/clean`, `/evt/pct` | Event-log text/CSV, day list, filesystem JSON and log-management results; `day` selects a date for `/evt` and `/evt/dl`; `p` sets the history share (40..100 percent) on `/evt/pct` |

`POST /evt/clean?name=/exact-file` deletes exactly one root file, including files
owned by this firmware; no `name` retains the existing foreign-file bulk cleanup.
An empty/invalid `name` never falls back to bulk deletion. Returns JSON `ok` and
`removed`, with HTTP 200 / 400 / 404 / 503 for success / invalid path / absent file /
unavailable or failed removal. `GET /evt/fs?offset=N` pages the bounded file list
and returns `offset` plus `more`; names are escaped losslessly for exact selection.
Active log/clock files can be recreated by later operation. Deleting `/logpct.txt`
restores the default share (100%). Deleting a day removes its matching RAM rows
only and invalidates its cached state; unrelated pending rows remain intact.

There is no application login/session check in this firmware: HTTP routes, including destructive operations such as `/evt/format`, `/evt/clr`, `/evt/clean`, `/benchlog/clear`, `/lut`, and `/lut/reset`, are reachable by any station that can join the AP. Current AP key is hard-coded as `123456789`. These are verified current behaviors, not recommendations; changing authentication or credentials requires owner approval and device-side changes.

## Bench per-step repeat (current panel)

Each measured/current row has a repeat action while a capture session is open.
A single capture loop serializes channel/duty changes and measurement commits;
row actions request a switch rather than starting a second hardware task.
Cross-scenario switching and final review use the existing zero-duty/restore
path. Interrupted DMM drafts and unrelated committed samples are retained.
Final review keeps repeat available until explicit completion; early End still
stops the session. A successful recapture replaces its session-owned sample by
object identity and invalidates computed fits. Failed CSV writes do not replace
that sample. The 53-measurement-column CSV schema is unchanged: append-only repeat attempts
are marked with a `# repeat` comment; prior CSV rows are never rewritten. The
daily 18-field schema, Firmware and ESP_USER are unchanged.

The live row updater now writes only its two board-current columns; its old
five-value voltage/current layout no longer matched the table and could
overwrite the DMM display cells while waiting for input. The capture submit
button is not subject to the Start button's running-session pointer lock.

## Generated-point editing

Calculate renders each selected battery's generated counts/current table. Row
removal excludes all selected source samples sharing that rounded raw count
from that channel's fit and table only. Source sample objects, selection flags,
CSV and the other battery are unchanged. Multiple exclusions accumulate across
recalculation; Restore clears that channel's exclusions. Duty-zero groups and
the minimum two distinct points are protected. Edits are blocked during capture
or calibration writes. Replacing source samples or reloading the page resets in-memory exclusions; sample-selection changes preserve exclusions and recalculate after the first calculation.
Editing invalidates any pending transmission proposal, preventing stale payload
application. Both generated code and newly prepared LUT payloads use the edits.
No automatic outlier deletion or table thinning is introduced. Existing exact-
count MAX merging, monotonicity checks, robust fit warnings and 24-point board
capacity remain distinct rules; a warning does not establish a measurement error.

Preview-only `tools/panel_preview_bench.js` provides labeled synthetic fixtures
and DMM helpers only with explicit `PREVIEW_BENCH_DEMO=1`, never embedded into ESP HTML.
The default preview has no demo controls or automatic sample loading. Generated
`panel_preview.html` contains no fetch shim or simulated replies; offline use is
layout-only. The separate Node test server remains a simulation, not a real ESP. Existing browser samples are not
overwritten automatically. The simulator honors manual channel/duty settings
with stable demo measurements, accumulates real ticker-frame `/m` statistics and
stores/downloads actual submitted CSV text in RAM, cleared on restart.

## Reopening finished bench rows

Row repeat remains available after final confirmation, early End or an error,
while the same in-memory table remains open. An explicit per-recapture
confirmation and new link/fixed-duty/filesystem checks precede any command.
A preflight lock prevents overlapping confirmation/start actions. Link, parameter
availability, fixed-duty mode and calibration-busy state are rechecked after
confirmation, then current restore parameters are snapshotted. Original row
definitions are retained; changing the sweep controls does not change the row
being repeated. After the chosen row is committed the panel returns to review,
not to the beginning of the sweep. Prior sample replacement, CSV repeat audit,
fit invalidation and safe restore apply unchanged. Reload/session-file recovery
is not implemented. Daily-history codec is unchanged; current bench storage is specified below.

## Edited sample files, download naming and diagnostics

Sample JSON v2 adds `view: {cuts: [counts1, counts2], scenarios: [...]}` to the
original full-precision samples and their use flags. Export does not round or
remove source readings. Import validates scope, cut types/range/references and
mandatory zero, restores the view and recalculates locally; v1 remains readable
with no cuts. Invalid metadata is rejected without changing the current session.
Settings backups include the same view as `benchView`; their import remains
local staging only. Import and selection mutations are blocked during capture
or calibration writes. Changed selections automatically update computed results
and invalidate old code/pending payloads. Code downloads rebuild current code.

All six file-export paths share a queued in-panel editable filename confirmation,
including automatic pre-apply backup and both bench download buttons. Cancel
performs no download; blank/path/control names are rejected, required extensions
are retained. Remote files are fetched to browser blobs after confirmation so
server Content-Disposition cannot override the chosen name. Failed reads do not
produce an empty file. Canceling the pre-apply backup also prevents board writes.
Internal browser persistence and automatic ESP logging are not user file exports
and do not open filename dialogs. The public bench CSV/schema remain unchanged; current internal bench storage is specified below.

Slow 2.4-second background pulses identify incomplete/out-of-range samples,
failed bench rows, suspect monotonic pairs, high residual candidates, duplicate
duty samples, preparation failures, invalid calculated parameters and actual
post-commit readback mismatches. Both members of an inconsistent pair are only
marked as suspects; before-commit differences are not mislabeled as failures.
Visible reasons and static reduced-motion presentation accompany color. Masks
and markers never change source values or automatically delete a measurement.

To retain the existing HTML transfer ceiling, non-executable old bench/calibration
source comments were compacted. No CSS/markup was removed for this size cleanup.
Token comparisons of calfit/calbuild/lpack and the frozen storage tester confirm
their numerical/storage cores are unchanged. Firmware and ESP_USER are untouched.


## Restore audit and deliberately removed dead paths

The automatic browser pending-queue replay is removed, including its writer and
obsolete localStorage key. It ran before the `const PN` initialization and caught
its own temporal-dead-zone failure, so it never replayed a nonempty valid queue.
Unsent edits now live only in the current page. Explicit JSON backup export/import
still retains/stages them; raw calibration sample persistence is unchanged.

Imported `lut` had only a dead `LUT_BACKUP` assignment and no viewer or apply
consumer. That restore branch/validator/storage is removed. Exported LUT snapshots
remain available as explicitly `lutRole: 'audit-only'` evidence; imports explain
that they are not restored. LUT-only imports report no supported restore rather
than success. Usable calibration tables are regenerated from full-precision samples.

Settings import requires finite JSON numbers, validates container/sample structure
before mutation, ignores malformed pending IDs and reconciles included board-equal
values against existing pending edits. Unrelated edits stay intact. Sample/view
restoration now calls the same local calculation path as sample-file import, not
only checklist rendering. Replacing a dataset drops stale capture rows and their
object references; append retains existing capture links. No file imports replay
a historical capture session or write to the board.

Sample import has three explicit choices: replace, append, cancel. Cancel/Escape
are non-destructive (the old two-choice cancel-as-replace behavior is removed).
File inputs reset on selection so retries of the same file work. Imports, sample
mutations and log clear respect capture preflight as well as active transactions;
clear operations recheck ownership after their asynchronous confirmation.

Bench hardware restoration remains mandatory. Every command is attempted; failed
acknowledgments are collected and reported, not silently treated as success.
A failed inter-scenario restore aborts before starting the next scenario; final
best-effort restoration still runs and already committed measurements remain.
This does not add telemetry freshness logic or change board firmware/transport,
fit/build mathematics, bench CSV or daily-history codec.


## Compact bench storage (approved implementation)

`plink_bench_codec.h` defines a lossless internal bench envelope, version 1 / CSV
schema 3. The existing path `/benchlog.csv` is kept for physical accounting,
file listing/deletion and deployment simplicity, but NEW files are not text CSV.
The static column legend is generated on download. Known-header legacy text
files are download-only; append returns 409 until explicit clear. Preflight offers legacy download-and-exit before any board
commands; damaged files cannot continue or offer normal download. There is no
automatic format/migration/erase. Empty files can start the new format.

Every ASCII POST is one immediately written frame, bounded by the existing
1536-byte body limit. Raw measurement/comment/repeat bodies retain every byte.
Canonical 171-token `# settings: ` lines use full snapshots or a 171-bit change
mask and exact lexical replacement values, never integer/float conversions.
A full snapshot is forced at run boundaries, at most every 16 settings frames,
and whenever a delta would be larger. The daily 18-field codec is not reused or
modified. Editable sample JSON and fit/LUT payload math remain unchanged.

Length, sequence and CRC32 protect each frame. The capped existing file is
strictly rescanned before append and after writing; writer dependencies are not
cached across requests. Full/short write return values are checked; failure
attempts to truncate back to the previous confirmed length. If rollback fails,
damage remains visible and blocks later use. A genuine truncated tail differs
from a short/read-error against an otherwise complete file; neither is silently
repaired or fitted. A failed verification read may leave a complete committed
frame on disk while returning failure; no exactly-once network guarantee is made.

Download validates the entire file before 200, then decodes with a known expanded
Content-Length. A second-pass read failure closes the connection early so the
browser cannot accept a successful partial blob. Complete-record suffix loss is
not detectable without an external committed manifest; valid complete prefixes
are valid logs, not proof that a past session is complete. No session replay is
introduced. Header/CRC checks are corruption detection, not authentication.

The 100 KiB cap, free-space checks, daily budget deduction and history preemption
use PHYSICAL encoded bytes, including envelope overhead. HTTP 507 retains full,
size and freed fields. UART draining/yield happens between frames. Workspace is
bounded (about 3.1 KiB added static state/frame on host plus the existing shared
JSON buffer); append rescans cost O(file size), bounded by the cap. ESP heap,
flash-cycle/power-cut reliability and device timing still require board tests.

The browser settings signature advances only after a successful append, so a
failed settings write is retried rather than silently skipped. Full format and
actual tests/measurements: `Tester/BENCH_CODEC_VALIDATION.md`.


## Battery-only DMM capture

Capture now collects only each active battery's signed DMM current; notes stay
optional. The unused input-current field, required validation and visible table
column are removed, including the preview meter helper. Each row has two board
current cells and two battery-DMM cells before status/repeat; draft restoration,
zero/negative readings, arbitrary repeat and both-channel capture remain intact.

Trace audit found the former `ii` input was only read by form validation, the
visible bench row and the CSV serializer. `calpush`/`calnorm` retain only battery
readings and raw source values; fit, table building, guards and LUT packing never
consume it. The serializer still supports historical callers that explicitly
supply `ii`, but normal new captures never create it. The legacy CSV field
`dmm_i_in_ma` therefore becomes `-` (missing), preserving all 53 positions and
not substituting zero. Existing compact frames, archived data and daily history
need no format/schema migration. Firmware and ESP_USER remain unchanged.


## Compact bench presentation

Routine actions use shared local outline SVG paths (20px, currentColor, no icon
font or CDN), with Persian aria-labels/native titles,
44px minimum icon targets and visible keyboard focus. A collapsed native-details
icon legend provides touch discovery; long static instructions and keyboard help
also use native details. Live readings, warnings, checklist, samples and results
remain outside collapsed help. Board-write/reset/delete operations keep short
visible labels; existing IDs, handlers, disabled/CAL_BUSY/run-lock classes and
confirmation paths are retained. The sample-import label has keyboard button
semantics (Tab, Enter/Space, with default scrolling prevented). Capture repeat
and code/download actions remain separate. Table row height is not globally
increased by the toolbar's touch sizing. No transport, math or storage change.
The optional test-only demo toolbar is not loaded in normal previews.


## Transaction ownership audit

The capture preflight (`W.opening`) and active loop (`W.run`) exclude public
parameter batches, voltage calibration, table proposal/apply/read and reset.
Global parameter sends and voltage confirmations acquire `CAL_BUSY` before their
first asynchronous boundary and release it on all completion/cancel/error paths.
Public reset acquires that same lock before POST; its finally block also clears
`LREAD_IN_PROGRESS`. Automatic post-commit reset runs under the already-owned
commit transaction via the internal `lreset` helper. Existing commit eligibility,
CRC and exact readback checks remain. UI disabled state matches handler guards;
rejected proposals/pending parameter edits are not consumed. Emergency channel-off
and manual-mode exit commands remain usable. These are per-page ownership rules,
not cross-client HTTP authentication or a server-wide transaction mutex.
Evidence and owner decisions: `AUDIT_TRANSACTIONS.md`.
