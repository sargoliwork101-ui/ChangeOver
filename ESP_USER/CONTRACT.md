# ESP_USER contract

```contract json
{
  "version": 1,
  "project": "ESP_USER",
  "root": "ESP_USER",
  "connected_board": "STM32",
  "not_simultaneous_with": "ESP_Calibration",
  "http_routes": [
    "GET /",
    "GET /charts",
    "GET /diag",
    "GET /evt",
    "GET /evt/days",
    "GET /evt/dl",
    "GET /evt/fs",
    "GET /latch",
    "GET /t",
    "GET /wifi",
    "POST /clear",
    "POST /clock",
    "POST /evt/clr",
    "POST /evt/format",
    "POST /evt/file",
    "POST /evt/snap",
    "POST /latch/reset",
    "POST /login",
    "POST /logout",
    "POST /setwifi",
    "POST /time",
    "POST /wifi"
  ]
}
```

## Inputs and outputs

| Interface | Input | Output |
|---|---|---|
| UART, 921600 8N1 | STM32 frames, protocol v2 | ESP_USER commands, protocol v2 |
| Wi-Fi access point | Browser requests; defaults SSID `changeover_userpannel`, key `123456789` (settings can be changed through the admin Wi-Fi endpoint) | User panel over HTTP port 80 |
| LittleFS | Persisted files listed below | Updated settings, history and daily event files |

## Commands

| Source | Command | Input / output |
|---|---|---|
| STM32 → ESP_USER | `TLM_LIVE` (`0x10`) | Input: 128-byte telemetry payload |
| STM32 → ESP_USER | `HELLO_ACK` (`0x41`) | Input: board protocol/version, health and echoed uptime |
| STM32 → ESP_USER | `DIAG_DATA` (`0x32`) | Input: chunked diagnostics, charge totals, board time and events |
| STM32 → ESP_USER | `PARAM_REPORT` (`0x11`) | Input: parameter id `u8` + applied value `u32 LE` |
| STM32 → ESP_USER | `PARAM_NAK` (`0x15`) | Input: rejected reset-slot id and reason (ids 200–204) |
| STM32 → ESP_USER | `PARAMS_BULK` (`0x12`) | Input: parameter count followed by id `u8` + value `u32 LE` items |
| ESP_USER → STM32 | `HELLO` (`0x40`) | Output every 2 s: protocol version, ESP version, role, flags and uptime |
| ESP_USER → STM32 | `SET_PARAM` (`0x01`) | Output only for latch resets: slot id 200–204 `u8` + value 0 `u32 LE`; `n=5` resets ids 200–202, `n=6` id 203, `n=8` id 204 |
| ESP_USER → STM32 | `GET_PARAMS` (`0x02`) | Output: empty request for current parameters |
| ESP_USER → STM32 | `TIME_SYNC` (`0x30`) | Output: Unix time `u32 LE` |
| ESP_USER → STM32 | `DIAG_REQ` (`0x31`) | Output: empty request, or one-byte clear request when clearing diagnostics |
| ESP_USER → STM32 | `WIFI_CLIENT_STATE` (`0x33`) | Output: connected `1` / disconnected `0` |
| Browser ↔ ESP_USER | HTTP routes below | Request arguments and response data below |

UART frame: `AA 55`, version `2`, type, payload length `u16 LE`, payload, CRC-16/CCITT-FALSE `u16 LE`; maximum payload 512 bytes. `GET_PARAMS` reads parameter ids 0–170. `SET_PARAM` is used here only for reset-slot ids 200–204.

| HTTP route | Method | Input | Output |
|---|---|---|---|
| `/` | GET | — | Embedded panel HTML |
| `/t` | GET | — | JSON telemetry, link state and parameter values |
| `/charts` | GET | `range`: `1d`, `7d`, `1m`, or `3m` (default `1d`) | JSON arrays `pct`, `vin`, `vpack`, `time`, with `range` and `count` |
| `/diag` | GET | — | JSON diagnostic, power and charge history |
| `/time` | POST | Admin session; `ts`, optional `tz` | JSON result |
| `/login` | POST | `user`, `pass` | JSON result with session `token`, plus HttpOnly session cookie |
| `/logout` | POST | Session cookie or `X-Admin-Token` | JSON result; invalidates session and expires cookie |
| `/wifi` | GET / POST | GET public; POST admin session with `ssid`, `pass` | GET only escaped `ssid` and `passSet`, never password; POST action result |
| `/setwifi` | POST | Admin session; `ssid`, `pass` | JSON action result |
| `/clear` | POST | Admin session; optional `type`: `charge`, `charts`, `diag`, or all | JSON action result |
| `/latch` | GET | Public read-only | JSON reset-slot state |
| `/latch/reset` | POST | Admin session; exact `n`: `5` (error latch), `6` (dead battery), or `8` (weak battery) | Numeric JSON `n`; invalid values return HTTP 400, `e:"reset", allowed:[5,6,8]`, without queueing |
| `/clock` | POST | Public automatic sync with valid `sec`, optional `tz` (−50400..50400); requests without `sec` require admin (session probe) | JSON result and current seconds |
| `/evt` | GET | Optional `day` | Plain-text event-log tail |
| `/evt/days` | GET | — | Plain-text available days |
| `/evt/dl` | GET | Optional `day` (8 digits), `all=1` for every day | CSV download |
| `/evt/clr` | POST | Admin session | JSON clear result |
| `/evt/snap` | POST | Admin session | JSON log/filesystem snapshot |
| `/evt/format` | POST | Admin session | 200 `ok` only after remount, write/read probe and saved credential verification; 500 `fail`, `mount`, or `write`; 403 `admin` |
| `/evt/fs` | GET | Optional `offset` (0–65535) | JSON status; public log filenames only, admin all root files; `count`, `more`, `next` (-1 at end) |
| `/evt/file` | POST | Admin session; exact absolute root `name` | JSON `ok`; 400 invalid path, 403 unauthorized, 404 missing, 500 remove failed, 503 unmounted |

Admin requests accept either the session cookie or the same 64-character token in
`X-Admin-Token`. The panel retains that token only in this tab's `sessionStorage`;
it never stores the password. Authorized admin requests renew the 30-minute idle
deadline. A valid owner can reauthenticate without rotating the active token;
a second client without that token/cookie still receives 409. Public telemetry
does not expose tokens or renew this deadline.

Single-file deletion never reloads settings, logs out, restarts the ESP or sends
STM32 commands. It only unlinks the named root file; deleting a current day also
invalidates its log cache. Active RAM settings remain until restart/change and
ongoing recording may recreate deleted files. Deleting `/users.json` restores
the default admin account on the next reboot. This requires explicit UI confirmation.
The application's storage is flat (root files); directories and traversal paths
are not accepted. Listing never returns file contents or Wi-Fi passwords.

Wi-Fi updates (`/wifi`, `/setwifi`) and retained account saves stage encrypted data in
`/users.json.tmp` or `/wifi.json.tmp`, verify the closed file by reading it, then
replace the committed file with LittleFS rename. A failed update keeps previous
RAM credentials and the committed file, responds with `ok:0, e:"storage"` (500;
Wi-Fi uses 503 when storage is unmounted), and never schedules a Wi-Fi restart.
Successful save does not invalidate the admin session. An incomplete temporary
file is not a boot credential source. Crypto nonces use the full unsigned 32-bit
range, not Arduino's signed `String::toInt` range.

LittleFS files: `/wifi.json`, `/charts.bin`, `/charts.jrn`, `/time.json`, `/params.json`, `/charge_hist.json`, `/diag.json`, `/evt-YYYYMMDD.csv`.

Daily logs retain today and the preceding 29 local calendar days only after clock verification, subject to the unchanged 28-KiB log budget and 4-KiB day limit. A partial initial header/marker is repaired before ordinary rows. Byte pressure removes the minimum oldest-event prefix whose re-encoded suffix plus the incoming row fits; count pressure from RAM drain likewise retains at most 200 newest ordinary rows. The single day-start marker is preserved outside that count. Replacement is staged, decoded/verified and renamed atomically; failure preserves the old committed day and queued incoming row. Ordinary day-full writes still stop at 200.

The day files keep their `.csv` name for the existing day-list API, but a new day is stored as CRC-protected binary records (8-byte header `EVTD`, version 1, 18 fields, checkpoint 16; full records at least every 16th). Every text view - `/evt`, `/evt/dl` and the panel - is rebuilt from those records, so the CSV columns above are still the public form. The previous 22-column history is cleared by the owner before deployment. Format: `ESP_USER/simple_log_codec.h`.

## Run checks

```sh
bash ESP_USER/Tester/run_simple_tests.sh
```

Browser time synchronization is automatic on page load, resume/focus, reconnect and every minute; failed requests retry after five seconds. Only time sync is public; other mutations remain admin-only. Browser time is sent to the board and stale diagnostics cannot replace it while awaiting a matching board clock.

## Verified clock, battery recovery and missing-time incident

`/time.json` stores `ts`, `tz`, the matched `bw`/`bu` wall/STM32-uptime pair,
`rk` (known reset identity), `rc` (existing STM32 reset counter), `fw` and `beat`.
Disk time is unconfirmed after every boot. There is no eight-second fallback.
An existing DIAG chunk-zero reply must confirm the saved reset count and a
non-decreased uptime before the pair can reconstruct ESP-off time. HELLO alone
cannot confirm reset identity; once confirmed, HELLO/DIAG can advance the clock.
Changed reset counters or decreased uptime reject the old pair, including a
plausible retained board epoch belonging to the old uptime base. A currently
running, trusted ESP clock can re-anchor across a board reset. Phone time received
before the first reply does not invent an uptime; the next diagnostic pairs it.
Legacy pairs without reset identity require a fresh sync. No STM32 code, packet
length, message, telemetry field or byte offset is added or changed.

The real loop continues existing HELLO (2 s) and DIAG requests (10 s) indefinitely,
without a browser. A delayed valid reply corrects the clock immediately. While
waiting, no chart sample, dated log or ESP-derived fault/power/charge-history
record uses disk time. Live values and previously stored records remain visible;
aggregate counters may still be saved. `/t` reports `timeOk=0, bt=0, boardTime=0`;
`/diag` reports `timeOk=0, panelTime=0` while keeping the raw board response.
Unconfirmed disk time is never sent to STM32 or advanced by periodic saves. The
firmware-stamp writer preserves the committed disk timestamp while unconfirmed.
No historical sample is synthesized to fill the gap. Existing change masks can
coalesce while waiting and describe observed state on recovery, not every event
in the missing interval. Hourly/500-mV triggers and shared summaries are unchanged.

At 40,000 boot milliseconds without a trusted clock, retain one unresolved incident
in `/clock_gap.json` (`note=8, reason=clock_unavailable`). The fixed small record is
written to `.tmp`, read back and atomically renamed. Failed attempts retry at most
once per five seconds; a committed marker is not continually rewritten. RAM holds
it when storage is unavailable, with no claim of surviving power loss in that case.
A committed marker survives ESP resets. The current `/evt` view and all-days CSV
include a synthetic `epoch=0, note=8` warning while unresolved, not a dated sample;
explicit unrelated historical-day views remain unchanged. The panel renders only
the short Persian warning, not extra technical detail or a new card.

After clock recovery, append and read back one dated `note=8` row on the correct
day, then remove the pending marker. This row timestamps the recovery report,
not an invented loss start/end time. It may bypass the normal day cap like a
manual snapshot; normal capacity and retention constants are unchanged. A write
failure retains the marker and does not repeatedly fill the fallback RAM ring.
A failed marker removal retries without another append during that process;
a power cut between append and removal can repeat the report after reboot
(at-least-once reporting). Explicit log clear/full format clear pending warning
state too. Existing account, session and Wi-Fi preservation is unchanged.

Clock writes retain their five-minute/zone-change throttle and immediate newly
acquired-pair exception, using verified temporary replacement. A restored history
suppresses quiet boot rows, not real changes, snapshots or updates. Heartbeats use
wall time and the newest actual row, searching past empty files. The 540-wake,
97,258-board-second fixture still has exactly 28 rows across three dates.

Limits: neither total power-off time nor a missing initial clock can be inferred.
The existing STM32 reset counter is not a guaranteed unique boot nonce; if a reset
reuses the same persisted counter and its new uptime has overtaken the saved pair,
the existing protocol cannot always distinguish it from uninterrupted operation.
No changes to that firmware are made here. A simulator restart also restarts its
board model; persistence tests separately feed a continuing board. Host/simulator
checks are not a physical ESP flash/power-cycle or target-RAM validation.

## Immediate chart persistence (supersedes daily snapshot scheduling)

Fresh observations are considered on the existing 5-second chart tick only after
the boot-clock gate. Accept the first observation, otherwise elapsed wall time
>=3600 s OR absolute input/pack change >=500 mV from the last committed sample.
No independent percentage trigger, per-boot trigger or timer reset is used.
A failed write retains one pending observation and retries before accepting more;
it never advances the committed trigger baseline or invents a successful sample.
Stale/no-board data remain zero in the transient live ring, not in persisted ranges.

A shared accepted source feeds a 288-point/24-hour bounded daily ring; voltage
event bursts may reach capacity before a full day. The 28 six-hour UTC buckets,
30 daily UTC buckets and 90 daily UTC buckets hold sample-weighted averages of
that same source, including the in-progress bucket. These are neither additional
measurements nor time-weighted averages/minimum-maximum envelopes. Bucket sums
and counts persist so reboot cannot reset the average. Old summaries survive
expiration of source points. Persistent ring indices/counts and HTTP serialization
are 16-bit; public chart JSON shape remains unchanged.

`/charts.jrn` has CRC16-protected full/delta records: A6 C4, one-byte payload size,
one-byte delta flag, five unsigned/zig-zag varints (sequence, epoch, pct, vin,
vpack), little-endian CRC over size/flag/payload. Full frames restart the delta
chain at least every 16 records. Each append is closed and read-verified before
RAM publication. Torn tails are discarded to the verified prefix; replay stops
at the first invalid frame and never decodes a delta against a missing base.

The journal is bounded at 1024 bytes. Before exhaustion, version-2 CHTD snapshots
store sequence, four delta-varint rings and three active sum/count buckets, with
CRC16. Snapshot size is bounded at 9216 bytes. A second validation pass publishes
all ranges together. Only a complete staged/read-verified snapshot may atomically
replace `/charts.bin`; only then may the old journal be removed. Sequence numbers
skip already-compacted frames after a crash between rename and journal removal.
The temporary snapshot is never a boot source. Version-1 snapshots are not imported.

Flash-space failures retain committed files and the pending observation. The
current pending observation itself is volatile until a write succeeds; physically
unpowered intervals cannot be measured. Clearing charts/format resets rings,
active buckets, journal cursors and pending samples. Ordinary accepted samples
append rather than rewrite all history; temporary space for the snapshot is
required during compaction. Log retention and STM32 firmware are unchanged.

## Retained administrator credential

The existing valid encrypted administrator credential is loaded unchanged.
No password setter or `/setpass` route is compiled; direct requests return 404,
including authenticated requests. The settings page has no password-edit form;
the hidden login and its current-password input remain. Login, logout, lockout,
single-session ownership, idle renewal and Wi-Fi password editing are unchanged.
There is no delayed no-login recovery, recovery-marker write or boot-time reset.
A legacy `/boot_no_login.json` is inert and never consulted for authentication.
The telemetry `recovery` key remains a constant zero for response compatibility.
Format persists the current account and keeps the current session. Empty-storage
initial credentials and existing missing/corrupt-user-file fallback behavior are
unchanged; explicit deletion of the account file is still an admin-only operation.
There is no panel password reset/change workflow in this version.

## Shared 64 KiB storage policy (supersedes earlier retention budgets)
The physical envelope is min(reported partition capacity, 65536 bytes), not a
new flash partition or a RAM allocation. Committed payload ceilings are 28672
log bytes, 10240 chart bytes, 6144 charge/diagnostic bytes and 4096 settings bytes;
16384 bytes are budgeted for filesystem overhead and temporary/COW writes.
These ceilings are not guaranteed reservations: actual LittleFS allocation wins
and may reclaim logs before their payload ceiling is reached. Space admission
checks allocated bytes, rounded staging space and a spare erase block before
all snapshot writers and chart appends. Only oldest log days are auto-deleted;
credentials, Wi-Fi, clock and other settings are never reclaim candidates.
A writer protects its destination day, but a sole previous day can be reclaimed
before creating a new day. Reclaim is bounded to four days per call; a failed
reclaim/write returns failure and keeps dirty/pending state for retry.
Daily log files are bounded at 4096 bytes. Before overflow, an atomic verified
rewrite keeps the newest half of the day's rows, restarting a full codec chain.
Verification compares every decoded field against the original suffix before
rename. Old committed data survive short writes, readback or rename failures.
This is retention deletion, not a promise that every old event remains.
Parameters, charge and diagnostic snapshots now use the same staged byte-verified
rename path as clock and credentials. Dirty flags clear only on success.
History clear returns success only after an empty snapshot is committed; chart
clear advances a checkpoint past the old journal so a failed journal deletion
cannot resurrect data. New samples resume from a new chain after reboot.
Clear-all is per-class, not a cross-file atomic transaction; partial success
returns an error and may be retried. Unknown selectors return 400. Log clear
rescans remaining files and reports failed deletion rather than unconditional ok.
Successful physical format still clears RAM even if the following remount fails.
The 3600-second OR 500-mV chart trigger, capacities, verified clock recovery,
40-second warning, authentication policy and STM32 protocol are unchanged.
Normal 90-day virtual-NOR tests exercise allocation, eviction and continued new
writes; they do not imply 90 retained days. Fixed per-day files remain separate.
Read-only/corrupt storage or unreclaimable non-log occupancy can still prevent
progress; failure is visible and committed snapshots are not destroyed.
See STORAGE_64K.md for the measured workloads and test commands.

## User event-log schema and retention (2026-11-04)

New day files use the calibration codec's 18-field order: epoch, uptime_s,
change_bits, note, mains, fl, chg1, chg2, fault_mask, flags2, vlow_mv,
vhigh_mv, dead_mask, vin_mv, i1_ma, i2_ma, duty1_permille, duty2_permille.
No battery percent is persisted. Pack voltage for restored comparisons is the
sum of the two half-pack readings. Existing 22-field days must be cleared before
using this schema; no migration is provided. Charts keep their existing schema
and 90-day range. Logs retain at most 30 day files and 200 ordinary rows. The
existing 4-KiB day compaction and 28-KiB aggregate budget remain in force.
A note=7 day-start row is durable once per new current day, excluded from the
ordinary-row cap and preserved during day compaction. It never resets the
heartbeat/change baseline, including across reboot. Unresolved-clock warnings
use note=8 in both the synthetic response and the recovered dated row.
