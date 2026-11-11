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
| `/login` | POST | `user`, `pass` | JSON result and session cookie |
| `/logout` | POST | Session cookie | JSON result; expires session cookie |
| `/users` | GET | Admin session | JSON list of `{name, role}` |
| `/users` | POST | Admin session; `action=del` + `user`, or `action=add` + `user`, `pass`, optional `role` | JSON action result |
| `/adduser`, `/deluser` | POST | Admin session; add: `user`, `pass`, optional `role`; delete: `user` | JSON action result |
| `/setpass` | POST | `user`, `new`; optional `old`, `new2` | JSON action result |
| `/theme` | GET | — | JSON theme |
| `/settheme` | POST | Admin session; `theme` | JSON action result |
| `/wifi` | GET / POST | Admin session; POST: `ssid`, `pass` | JSON settings or action result |
| `/setwifi` | POST | Admin session; `ssid`, `pass` | JSON action result |
| `/clear` | POST | Admin session; optional `type`: `charge`, `charts`, `diag`, or all | JSON action result |
| `/latch` | GET | Admin session | JSON reset-slot state |
| `/latch/reset` | POST | Admin session; `n`: 5, 6, or 8 | JSON queued/result status |
| `/clock` | POST | `sec`, optional `tz` | JSON result and current seconds |
| `/evt` | GET | Optional `day` | Plain-text event-log tail |
| `/evt/days` | GET | — | Plain-text available days |
| `/evt/dl` | GET | Optional `day` (8 digits), `all=1` for every day | CSV download |
| `/evt/clr` | POST | Admin session | JSON clear result |
| `/evt/snap` | POST | — | JSON log/filesystem snapshot |
| `/evt/format` | POST | Admin session | Plain-text format result |
| `/evt/fs` | GET | — | JSON filesystem status |

LittleFS files: `/wifi.json`, `/charts.bin`, `/time.json`, `/params.json`, `/charge_hist.json`, `/diag.json`, `/evt-YYYYMMDD.csv`.

The day files keep their `.csv` name so old files and the day list stay readable, but a new day is stored as CRC-protected binary records (8-byte header `EVTD`, version 1, 22 fields, checkpoint 16; full records at least every 16th). Every text view - `/evt`, `/evt/dl` and the panel - is rebuilt from those records, so the CSV columns above are still the public form. Old text days are read and appended as text and are never rewritten. Format: `ESP_USER/simple_log_codec.h`.

## Run checks

```sh
bash ESP_USER/Tester/run_simple_tests.sh
```
