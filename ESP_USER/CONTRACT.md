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
| `/clock` | POST | Admin session; optional `sec`, `tz` | JSON result and current seconds |
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

LittleFS files: `/wifi.json`, `/charts.bin`, `/time.json`, `/params.json`, `/charge_hist.json`, `/diag.json`, `/evt-YYYYMMDD.csv`.

The day files keep their `.csv` name so old files and the day list stay readable, but a new day is stored as CRC-protected binary records (8-byte header `EVTD`, version 1, 22 fields, checkpoint 16; full records at least every 16th). Every text view - `/evt`, `/evt/dl` and the panel - is rebuilt from those records, so the CSV columns above are still the public form. Old text days are read and appended as text and are never rewritten. Format: `ESP_USER/simple_log_codec.h`.

## Run checks

```sh
bash ESP_USER/Tester/run_simple_tests.sh
```
