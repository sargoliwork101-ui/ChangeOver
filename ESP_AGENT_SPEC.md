# ESP-Link Agent Spec — ESP Side Implementation Handoff

> این سند برای ماموری است که سمت ESP را می‌نویسد (دستور کاربر ۲۰۲۶-۰۹-۲۲).
> سمت STM32 کامل و push شده است؛ فقط فعال‌سازی نهایی `MODULE_ESP` مانده (پایین را ببینید).
> متن فنی عمداً انگلیسی است تا هیچ ابهامی در پروتکل نماند.

---

## 1. What exists already (STM32 side — DONE)

- Binary command protocol + periodic telemetry over **USART1, 115200 8N1**.
- Interrupt-driven RX on the STM32 with a 128-byte ring (the STM32 never loses
  bytes at this baud).
- 19 runtime parameters the ESP can read and write (current-chain
  calibration, voltage offsets, current-filter sizes, per-channel flyback
  efficiency, per-channel charger enable/cut, per-channel PWM duty ceiling
  and fixed-duty mode).
- Charger module cut/reconnect commands (param IDs 11/12).
- All parameter values are **clamped** by the STM32; the reply always returns
  the **actually applied** value.
- Parameters are **RAM-only on the STM32**: after an STM32 reset everything
  returns to the compiled defaults. The ESP must re-apply its tuned set
  (recommended: on link-up and whenever a PARAMS_BULK shows defaults).

## 2. Wiring / physical

| STM32 pin | Label | Connect to ESP | Notes |
|---|---|---|---|
| PA9 | `USART1_TX` | ESP **RX** | 3.3 V logic |
| PA10 | `USART1_RX` | ESP **TX** | 3.3 V logic |
| PA8 | `MCU_ESP_CHPD` | ESP enable (CH_PD/EN per schematic) | Driven by the STM32, goes HIGH when the link task starts. **Do not drive from the ESP side.** |
| — | GND | GND | Common ground required |
| — | 3V3 | ESP supply | Per board schematic |

UART settings: **115200 baud, 8 data bits, no parity, 1 stop bit**.

Timing: the STM32 sends one telemetry frame every **100 ms**
(`APP_CONFIG.comm_period_ms`). Replies to commands are sent immediately.

## 3. Frame format (both directions)

```text
[0xAA][0x55][type:u8][len:u8][payload: len bytes][xor:u8]
```

- `xor` = XOR of `type`, `len`, and every payload byte (starting value 0x00).
- All multi-byte payload fields are **little-endian**.
- Max payload length = **96 bytes** (longer `len` = invalid frame).
- On checksum error or unknown type: the STM32 silently drops the frame and
  resynchronizes on the next `AA 55`. The ESP should do the same.

## 4. Message types

| Type | Direction | Name | Payload |
|---|---|---|---|
| 0x01 | ESP→STM | SET_PARAM | `[id:u8][value:u32 LE]` (5 bytes) |
| 0x02 | ESP→STM | GET_PARAMS | empty (len = 0) |
| 0x10 | STM→ESP | TLM_LIVE | 84 bytes, layout below |
| 0x11 | STM→ESP | PARAM_REPORT | `[id:u8][value:u32 LE]` — the **applied** value (sent after every accepted SET_PARAM) |
| 0x12 | STM→ESP | PARAMS_BULK | `[count:u8]` then `count` × `[id:u8][value:u32 LE]` (answer to GET_PARAMS) |

Verified example frames (hex):

```text
SET_PARAM CUR1_GAIN_PERMILLE = 1200:
AA 55 01 05 02 B0 04 00 00 B2
                      └id=2  └value=1200 LE   checksum=B2

SET_PARAM CHG1_ENABLE = 0 (cut charger 1):
AA 55 01 05 0C 00 00 00 00 08

GET_PARAMS:
AA 55 02 00 02

PARAM_REPORT reply for id=2, applied=1200:
AA 55 11 05 02 B0 04 00 00 A2
```

## 5. Parameter table (protocol v1.1 — IDs renumbered BEFORE any ESP-side implementation existed; treat these IDs as final)

| ID | Name | Type | Unit | Default | Range | What it changes |
|---|---|---|---|---|---|---|
| 0 | CUR1_OFFSET_COUNTS | u32 | ADC counts | 8 | 0..255 | Zero-current offset, current channel 1 (Trans1, upper battery) |
| 1 | CUR2_OFFSET_COUNTS | u32 | ADC counts | 8 | 0..255 | Same, channel 2 (Trans2, lower battery) |
| 2 | CUR1_GAIN_PERMILLE | u32 | permille | 1085 | 100..3000 | Bench gain trim, channel 1 |
| 3 | CUR2_GAIN_PERMILLE | u32 | permille | 1085 | 100..3000 | Same, channel 2 |
| 4 | VIN_OFFSET_MV | **i32** | mV | 0 | −2000..2000 | 24 V input voltage calibration |
| 5 | V24_OFFSET_MV | **i32** | mV | 0 | −2000..2000 | 24 V battery pack voltage calibration |
| 6 | V12_OFFSET_MV | **i32** | mV | 0 | −2000..2000 | 12 V (middle node) battery calibration |
| 7 | FILTER_MEDIAN_SIZE | u32 | samples | 3 | 1/3/5 | Median window on charge currents. Valid sizes 1, 3, 5; other values round DOWN to the next odd size. **1 = bypass** (no separate on/off switch exists). Filter state resets on change. |
| 8 | FILTER_AVERAGE_WINDOW | u32 | samples | 10 | 1..10 | Moving-average window on charge currents. **1 = bypass.** Filter state resets on change. |
| 9 | CHG_EFF_UP_PERMILLE | u32 | permille | 758 | 100..999 | Charger 1 flyback efficiency for the current estimate |
| 10 | CHG_EFF_DN_PERMILLE | u32 | permille | 242 | 100..999 | Charger 2 efficiency (242 is NOT physical — it absorbs the ch2 sense over-read; do not "fix" it to ~700) |
| 11 | CHG1_ENABLE | u32 | 0/1 | 1 | 0..1 | 0 = cut charger module 1 (PWM off, state OFF); 1 = reconnect (soft BULK restart from 1% duty) |
| 12 | CHG2_ENABLE | u32 | 0/1 | 1 | 0..1 | Same for charger module 2 |
| 13 | CHG1_DUTY_CEILING | u32 | permille | 500 | 0..500 | PWM duty cap, charger 1. EVERY applied duty (ramp, regulation, fixed mode) is clamped to min(compile max, this ceiling). |
| 14 | CHG2_DUTY_CEILING | u32 | permille | 500 | 0..500 | Same, charger 2 |
| 15 | CHG1_DUTY_FIXED_ON | u32 | 0/1 | 0 | 0..1 | Fixed-duty mode, charger 1: hold the PWM at ID 16's value instead of the regulation loop |
| 16 | CHG1_DUTY_FIXED_VAL | u32 | permille | 0 | 0..500 | Fixed duty value for charger 1 (effective only while ID 15 = 1; also respects the ID 13 ceiling on apply) |
| 17 | CHG2_DUTY_FIXED_ON | u32 | 0/1 | 0 | 0..1 | Fixed-duty mode, charger 2 |
| 18 | CHG2_DUTY_FIXED_VAL | u32 | permille | 0 | 0..500 | Fixed duty value for charger 2 (respects the ID 14 ceiling) |

Notes:
- Signed values (4..6) travel as two's-complement u32 on the wire.
- `Iest = Ipri × Vin × eta / Vbat` — the ETA parameters only change the
  **estimate**, never the actual charging behavior. The OFFSET/GAIN/FILTER
  parameters change the measured current that the charger regulates on.
- **Fixed-duty mode safety wrapper** (identical to the proven compile-time
  bench-test mode): switching STOPS when the battery reaches the absorb
  voltage (no overcharge with regulation off), the hardware JIT
  over-current comparator stays armed, and the input/battery/ESP-cut gates
  remain active. State shows BULK while held. The state machine is NOT
  bypassed — fixed mode sits inside the normal regulation path.
- **No setpoints are exposed.** Charge scenario (14.4/14.3/14.6 V, float band,
  12.8 V reentry) is intentionally not adjustable from the ESP.
- A latched FINAL_FAULT charger state is **never** released by CHGx_ENABLE;
  only a reboot clears it.

## 6. TLM_LIVE payload layout (84 bytes, little-endian)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | u16 | seq | Wraps at 65535; use for drop detection |
| 2 | u8 | flags | b0 snapshot valid, b1 input present, b2 meas data valid, b3 charger-1 ESP enable, b4 charger-2 ESP enable, b5..b7 = 0 |
| 3 | u8 | reserved | 0 |
| 4 | u32 | raw1_counts | Raw ADC counts, current ch1, **unfiltered** (mid-ON synchronized sample) |
| 8 | u32 | shunt1_uv | Pure-hardware shunt voltage ch1, µV (no offset/trim) — LM358 output in mV = this × 101 / 1000 |
| 12 | u32 | ma1_unfiltered | Converted mA ch1 before any filter |
| 16 | u32 | i1_filtered_ma | Filtered primary current ch1 (what the charger decides on) |
| 20 | u32 | iest1_ma | Estimated battery current ch1 (`ChargerIest1Ma`) |
| 24 | u32 | duty1_permille | PWM duty ch1 (‰) |
| 28 | u32 | state1 | Charger state ch1 (see below) |
| 32 | u32 | raw2_counts | Raw ADC counts, current ch2, unfiltered |
| 36 | u32 | shunt2_uv | Pure-hardware shunt voltage ch2, µV |
| 40 | u32 | ma2_unfiltered | Converted mA ch2 before any filter |
| 44 | u32 | i2_filtered_ma | Filtered primary current ch2 |
| 48 | u32 | iest2_ma | Estimated battery current ch2 (`ChargerIest2Ma`) |
| 52 | u32 | duty2_permille | PWM duty ch2 (‰) |
| 56 | u32 | state2 | Charger state ch2 |
| 60 | u32 | v_in_mv | 24 V input voltage, mV |
| 64 | u32 | v_bat24_mv | 24 V battery pack, mV |
| 68 | u32 | v_bat12_mv | 12 V (middle node) battery, mV |
| 72 | u32 | v_bat_low_mv | Lower battery = V12, mV |
| 76 | u32 | v_bat_high_mv | Upper battery = V24 − V12, mV |
| 80 | u32 | fault_mask | Bit 6 = FAULT_CHARGER_BAT_LOST; other bits reserved |

Charger states: `0 OFF, 1 BULK, 2 ABSORB, 3 FLOAT, 4 BRINGUP, 5 JIT_RETRY_WAIT,
6 INPUT_WAIT, 7 FINAL_FAULT, 8 BAT_LOST`.

Channel mapping: **channel 1 = Trans1 = upper battery (Vhigh)**,
**channel 2 = Trans2 = lower battery (Vlow)**.

Reference values from the 2026-09-22 bench point (use as sanity check for the
panel): raw1 ≈ 310 ↔ shunt1 ≈ 2720 µV ↔ ma1_unfiltered ≈ 287 ↔ iest1 ≈ 364;
raw2 ≈ 951 ↔ shunt2 ≈ 8346 µV ↔ ma2_unfiltered ≈ 897.

## 7. Recommended ESP behavior

1. Boot, open UART at 115200 8N1, start a 100 ms RX pump.
2. Wait for the first TLM_LIVE (proves the link; the STM32 powers the ESP
   enable line only when its comm task runs).
3. Send `GET_PARAMS`, parse `PARAMS_BULK`, show a live dashboard of all 19
   parameters.
4. UI controls (sliders/toggles) send `SET_PARAM` per change and wait for the
   matching `PARAM_REPORT`; display the **applied** value (it may differ from
   the requested value when clamped).
5. Keep the last known parameter set in ESP NVRAM/flash and re-apply it after
   an STM32 reboot (detect: PARAMS_BULK returns defaults, or telemetry seq
   restarts at 0).
6. Provide two prominent buttons: charger 1 ON/OFF and charger 2 ON/OFF
   (IDs 11/12). OFF is the safe direction: PWM stops immediately. Offer
   the duty ceiling sliders (IDs 13/14) and the fixed-duty toggles+fields
   (IDs 15..18) in a clearly marked "manual/test" section: fixed mode
   holds the PWM at one number with the regulation loop off (bounded by
   the ceiling and the absorb-voltage stop).
7. Display telemetry continuously; the current-chain numbers (raw → shunt →
   unfiltered → filtered) exist exactly so the panel can show the chain
   step-by-step and help find the correct calibration numbers.

## 8. Safety rules for the ESP implementation

- Never spam the link: at most a few SET_PARAM frames per second is plenty;
  the STM RX ring is 128 bytes.
- Always respect clamping (the STM32 enforces it anyway).
- Do not try to change charge voltages/setpoints — they are not in the
  protocol; requesting an unknown ID is silently ignored.
- Treat loss of telemetry > 1 s as "link down": show a warning, keep last
  values greyed out. (The charger continues autonomously — the STM32 never
  depends on the ESP.)
- ESP crash/reboot must leave CH_PD un-driven (input/high-Z) — the STM32 owns
  that line.

## 9. Current activation state (STM32 side)

Everything is implemented and pushed, but the STM32 build keeps
`MODULE_ESP = 0` in `Firmware/Config/Inc/modules_enable.h` because the repo
rule-check (`tools/check_ai_rules.sh`) currently requires it. Flipping it to
`1` (one line) activates TaskComm → EspLink_Init → ESP power + protocol.
That flip is intentionally left to the project owner.

## 10. Protocol version

v1.1 (2026-09-22, same day as v1 and BEFORE any ESP-side implementation
existed — the v1.1 IDs are the final ones). Changes vs v1: filter switches
merged into ONE size parameter per filter (median 1/3/5, average window
1..10; size 1 = bypass, no separate on/off), and six new duty params
(ceiling + fixed-mode enable/value per channel). Parameter IDs and the TLM
layout are frozen from here on; future additions append new IDs / new
message types only, never renumber.
