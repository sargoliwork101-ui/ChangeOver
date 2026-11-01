# STM32 contract

```contract json
{
  "version": 1,
  "project": "STM32",
  "roots": ["Firmware", "CubeIDE", "CubeMX", "Circuit", "Programmer"],
  "connected_boards": ["ESP_USER", "ESP_Calibration"],
  "connection_rule": "Connect one ESP board at a time; ESP_USER and ESP_Calibration are not connected simultaneously"
}
```

## Inputs and outputs

| Interface | Input | Output |
|---|---|---|
| UART1, PA10 RX / PA9 TX, 921600 8N1 | Protocol-v2 frames from one connected ESP board | Protocol-v2 replies and telemetry |
| ESP enable, PA8 `MCU_ESP_CHPD` | — | ESP board enable signal |
| ADC1, 6-value frame | Ordered as current 1 (PA1/IN1), 24 V input (PA7/IN7), 24 V battery (PA2/IN2), 12 V battery (PA5/IN5), current 2 (PA3/IN3), internal VREFINT | — |
| Digital inputs | 24 V present (PB4, active high), jitter 1 (PB2, active-low trip), jitter 2 (PB6, active-low trip) | — |
| PWM outputs | — | Charger 1 (PA0/TIM2_CH1), charger 2 (PA6/TIM3_CH1), duty 0–1000 permille |
| Digital outputs | — | Buzzer (PA4), red LED (PB0), yellow LED (PB1), green LED (PB10), battery switch (PB5, active low), relay (PB7), battery protection (PB11, active high) |
| Non-volatile flash | Parameter and calibration-table write commands | Persisted parameter values and current-calibration tables after restart |

## Commands

| Direction | Command | Input / output |
|---|---|---|
| Either ESP → STM32 | `HELLO` (`0x40`) | 8 bytes: protocol version, ESP firmware version, role (`1` user / `2` calibration), flags, uptime `u32 LE`; answered with `HELLO_ACK` |
| STM32 → either ESP | `HELLO_ACK` (`0x41`) | 24 bytes: protocol/board versions, module flags, uptime, accepted-frame count, CRC/version error counts, parameter count, telemetry period, echoed ESP uptime |
| Either ESP → STM32 | `SET_PARAM` (`0x01`) | 5 bytes: id `u8` + value `u32 LE`; normal user-parameter ids are 0–170; reset-slot ids 200–204 are also accepted for the documented latch-reset actions (write zero); accepted applied value is replied as `PARAM_REPORT`, rejected/unknown id as `PARAM_NAK` |
| STM32 → ESP | `PARAM_REPORT` (`0x11`) | Applied id `u8` + value `u32 LE` |
| STM32 → ESP | `PARAM_NAK` (`0x15`) | Rejected id `u8` + reason `u8` |
| Either ESP → STM32 | `GET_PARAMS` (`0x02`) | Empty payload; requests current parameter values |
| STM32 → ESP | `PARAMS_BULK` (`0x12`) | Count `u8` + id `u8` / value `u32 LE` records; up to 102 records per frame, split as required |
| STM32 → either ESP | `TLM_LIVE` (`0x10`) | Every 100 ms; 128 bytes: sequence `u16`, flags `u8`, flags2 `u8`, then 31 `u32 LE` fields in this order: ch1 raw counts, shunt µV, unfiltered mA, filtered mA, estimated mA, duty permille, state; ch2 same seven fields; input mV, pack-24 mV, node-12 mV, low-pack mV, high-pack mV, fault mask; raw input/24-pack/12-node/VREFINT counts, VDDA mV; imbalance mV, imbalance event count, latched cycle count; dead-channel mask, ch1 dead-charge seconds, ch2 dead-charge seconds |
| Either ESP → STM32 | `TIME_SYNC` (`0x30`) | Unix seconds `u32 LE` |
| ESP_USER → STM32 | `DIAG_REQ` (`0x31`) | Empty payload requests diagnostics; one byte `1` clears diagnostic history and requests diagnostics |
| STM32 → ESP_USER | `DIAG_DATA` (`0x32`) | Chunk 0: charge/uptime/time summary; chunks 1–2: error records; chunks 3–4: power-event records |
| Either ESP → STM32 | `WIFI_CLIENT_STATE` (`0x33`) | One byte: connected `1`, disconnected `0` |
| ESP_Calibration → STM32 | `LUT_BEGIN` (`0x04`) | Two channel point counts `u8` |
| ESP_Calibration → STM32 | `LUT_CHUNK` (`0x05`) | Channel `u8`, first index `u8`, count `u8`, then count `(counts:u32 LE, battery_mA:u32 LE)` pairs |
| ESP_Calibration → STM32 | `LUT_COMMIT` (`0x06`) | Staged-table CRC32 `u32 LE` |
| ESP_Calibration → STM32 | `LUT_RESET` (`0x07`) | Four bytes `RST!`; accepted only after successful commit and NVM completion |
| ESP_Calibration → STM32 | `LUT_READ` (`0x08`) | Empty payload; requests active calibration-table points |
| STM32 → ESP_Calibration | `LUT_ACK` (`0x13`) | 8 bytes: stage, status, channel point counts, CRC32 `u32 LE` |
| STM32 → ESP_Calibration | `LUT_DATA` (`0x14`) | Channel, first index, count, then count `(counts:u32 LE, battery_mA:u32 LE)` pairs |
| Either direction | UART frame | `AA 55`, version `2`, type, payload length `u16 LE`, payload, CRC-16/CCITT-FALSE `u16 LE`; payload maximum 512 bytes |
