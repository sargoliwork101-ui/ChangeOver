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
    "POST /setpass",
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
| `/setpass` | POST | Admin session; `old`, `new`, optional `new2` | JSON action result for admin password |
| `/wifi` | GET / POST | GET public; POST admin session with `ssid`, `pass` | GET only escaped `ssid` and `passSet`, never password; POST action result |
| `/setwifi` | POST | Admin session; `ssid`, `pass` | JSON action result |
| `/clear` | POST | Admin session; optional `type`: `charge`, `charts`, `diag`, or all | JSON action result |
| `/latch` | GET | Public read-only | JSON reset-slot state |
| `/latch/reset` | POST | Admin session; `n`: 5, 6, or 8 | JSON queued/result status |
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

Credential updates (`/setpass`, `/wifi`, `/setwifi`) stage encrypted data in
`/users.json.tmp` or `/wifi.json.tmp`, verify the closed file by reading it, then
replace the committed file with LittleFS rename. A failed update keeps previous
RAM credentials and the committed file, responds with `ok:0, e:"storage"` (500;
Wi-Fi uses 503 when storage is unmounted), and never schedules a Wi-Fi restart.
Successful save does not invalidate the admin session. An incomplete temporary
file is not a boot credential source. Crypto nonces use the full unsigned 32-bit
range, not Arduino's signed `String::toInt` range.

LittleFS files: `/wifi.json`, `/charts.bin`, `/charts.jrn`, `/time.json`, `/params.json`, `/charge_hist.json`, `/diag.json`, `/evt-YYYYMMDD.csv`.

The day files keep their `.csv` name so old files and the day list stay readable, but a new day is stored as CRC-protected binary records (8-byte header `EVTD`, version 1, 22 fields, checkpoint 16; full records at least every 16th). Every text view - `/evt`, `/evt/dl` and the panel - is rebuilt from those records, so the CSV columns above are still the public form. Old text days are read and appended as text and are never rewritten. Format: `ESP_USER/simple_log_codec.h`.

## Run checks

```sh
bash ESP_USER/Tester/run_simple_tests.sh
```

Browser time synchronization is automatic on page load, resume/focus, reconnect and every minute; failed requests retry after five seconds. Only time sync is public; other mutations remain admin-only. Browser time is sent to the board and stale diagnostics cannot replace it while awaiting a matching board clock.

## Battery clock and logging retention

`/time.json` stores `ts` (current Unix seconds), `tz`, a matched pair `bw` (wall
seconds) / `bu` (STM32 uptime seconds), `fw`, and `beat` (last accepted row time).
The pair is separate from `ts`: delayed or silent-session saves cannot double-count
elapsed time. The next HELLO_ACK or DIAG_DATA uptime recovers ESP-off intervals;
a decreased board uptime re-anchors without rewinding or inventing missing time.
Phone sync before the first board reply retains/advances the saved pair; the live
reply binds the authoritative phone clock to the actual board uptime. Newly acquired
or reset pairs persist immediately. Ordinary saves retain the five-minute threshold
and timezone-change exception. All clock writers, including the firmware stamp,
use a verified temporary file and atomic LittleFS rename, never truncate the
committed clock. No incomplete `.tmp` file is loaded at boot.

Initial logging without history retains one boot row. A restored history suppresses
plain repeated boot rows, not actual state changes, manual snapshots or firmware
updates. Heartbeats use wall seconds restored from the clock and the newest actual
log row (searching past empty day files). The eight-second boot-clock gate retains
pending events/updates and prevents stale-day creation until recovery or timeout.
Day cap, retention and storage budgets are unchanged. The main loop saves clock
state with its existing 30-second maintenance pass (writes remain throttled);
board epoch adoption occurs only on receipt, not repeatedly from stale RAM.

Recovery requires an initially synchronized wall clock and a still-counting STM32.
Events during physical ESP power-off cannot be individually observed by ESP;
state differences are recorded when fresh telemetry returns. Complete board power
loss needs external time synchronization; no new STM32/RTC behavior is introduced.

## Immediate chart persistence (supersedes daily snapshot scheduling)

Fresh observations are considered on the existing 5-second chart tick only after
the boot-clock gate. Accept the first observation, otherwise elapsed wall time
>=300 s OR absolute input/pack change >=500 mV from the last committed sample.
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
