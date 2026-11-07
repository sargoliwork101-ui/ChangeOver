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
| LittleFS | `/benchlog.csv`, `/evt-clock.txt`, `/evt-YYYYMMDD.csv`, `/logpct.txt` | Bench CSV, time anchor, event-history files and the history share chosen in the panel (v1.134) |

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
| Browser → ESP_Calibration | `GET /benchlog?i=1` | JSON filesystem status, current size and maximum size |
| Browser → ESP_Calibration | `GET /benchlog` | CSV download; header-only CSV when empty |
| Browser → ESP_Calibration | `POST /benchlog/add` | Printable ASCII CSV body, newline-terminated, maximum 1536 bytes; JSON result; HTTP 507 at 102400-byte cap |
| Browser → ESP_Calibration | `POST /benchlog/clear` | Empty body; JSON result |
| Browser → ESP_Calibration | `POST /clock?sec=&tz=` | Unix seconds and timezone offset; JSON result and current seconds |
| Browser → ESP_Calibration | `GET /t` | JSON live telemetry, parameter values and link/handshake status |
| Browser → ESP_Calibration | `GET /ping` | Plain-text ESP/filesystem/log status |
| Browser → ESP_Calibration | `GET /` / `GET /f.css` | Panel HTML / font CSS |
| Browser ↔ ESP_Calibration | `/evt`, `/evt/days`, `/evt/dl`, `/evt/clr`, `/evt/snap`, `/evt/format`, `/evt/fs`, `/evt/clean`, `/evt/pct` | Event-log text/CSV, day list, filesystem JSON and log-management results; `day` selects a date for `/evt` and `/evt/dl`; `p` sets the history share (40..100 percent) on `/evt/pct` |

There is no application login/session check in this firmware: HTTP routes, including destructive operations such as `/evt/format`, `/evt/clr`, `/evt/clean`, `/benchlog/clear`, `/lut`, and `/lut/reset`, are reachable by any station that can join the AP. Current AP key is hard-coded as `123456789`. These are verified current behaviors, not recommendations; changing authentication or credentials requires owner approval and device-side changes.
