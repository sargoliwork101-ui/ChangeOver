# ESP-Link Agent Spec — ESP Side Implementation Handoff

> این سند برای ماموری است که سمت ESP را می‌نویسد (دستور کاربر ۲۰۲۶-۰۹-۲۲).
> سمت STM32 کامل و push شده است؛ فقط فعال‌سازی نهایی `MODULE_ESP` مانده (پایین را ببینید).
> متن فنی عمداً انگلیسی است تا هیچ ابهامی در پروتکل نماند.
>
> v1.2 (2026-09-23): manual test mode specified (ID 19, section 5.2) — the
> firmware side is the next STM32 task and NOT in the pushed build yet
> (see section 9). The panel can be built against this spec already.

---

## 1. What exists already (STM32 side — DONE)

- Binary command protocol + periodic telemetry over **USART1, 921600 8N1**.
- DMA transport in both directions on the STM32 (circular 256-byte RX ring,
  zero CPU per byte; DMA-drained TX ring) - user order 2026-09-23.
- 19 runtime parameters in the pushed build (current-chain calibration,
  voltage offsets, current-filter sizes, per-channel flyback efficiency,
  per-channel charger enable/cut, per-channel PWM duty ceiling and
  fixed-duty mode). v1.2 appends ID 19 (manual test mode, section 5.2)
  as the 20th parameter.
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

UART settings: **921600 baud, 8 data bits, no parity, 1 stop bit** (the
STM32 retunes its 115200 Cube default to 921600 at link init - user order
2026-09-23). 921600 runs reliably on both ESP8266 and ESP32 UARTs.

High-speed / zero-CPU transport on the STM32 side (user order
2026-09-23): reception is a 256-byte **circular DMA** ring (zero CPU per
byte, no reception interrupt at all) and transmission is **DMA from a
256-byte software ring** (non-blocking, whole-frame writes; a couple of
lightweight completion interrupts per frame, together ~20-40 IRQ/s at
the 100 ms telemetry cadence). A full 96-byte frame is ~1 ms of wire
time. The ESP side needs nothing special - it just sees a normal 921600
UART.

ESP -> STM burst budget: the STM drains its RX ring once per 100 ms
comm tick, so keep each burst under ~250 bytes (a handful of frames);
anything more can overflow the ring. Normal panel traffic is far below
this.

Timing: the STM32 sends one telemetry frame every **100 ms**
(`APP_CONFIG.comm_period_ms`). Replies to commands are queued
immediately and drain within ~1 ms.

## 3. Frame format (both directions)

```text
[0xAA][0x55][type:u8][len:u8][payload: len bytes][xor:u8]
```

- `xor` = XOR of `type`, `len`, and every payload byte (starting value 0x00).
- All multi-byte payload fields are **little-endian**.
- Max payload length = **112 bytes** (v1.2; was 96 - PARAMS_BULK grew with
  the 20th parameter; a 20-param bulk is 101 payload bytes). Longer `len` =
  invalid frame. The ESP parser must accept up to 112 regardless of STM
  firmware version.
- On checksum error or unknown type: the STM32 silently drops the frame and
  resynchronizes on the next `AA 55`. The ESP should do the same.

## 4. Message types

| Type | Direction | Name | Payload |
|---|---|---|---|
| 0x01 | ESP→STM | SET_PARAM | `[id:u8][value:u32 LE]` (5 bytes) |
| 0x02 | ESP→STM | GET_PARAMS | empty (len = 0) |
| 0x10 | STM→ESP | TLM_LIVE | 84 bytes, layout below |
| 0x11 | STM→ESP | PARAM_REPORT | `[id:u8][value:u32 LE]` — the **applied** value (sent after every accepted SET_PARAM) |
| 0x12 | STM→ESP | PARAMS_BULK | `[count:u8]` then `count` × `[id:u8][value:u32 LE]` (answer to GET_PARAMS; 20 params in v1.2 = 101 payload bytes) |

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

## 5. Parameter table (IDs 0..18 = protocol v1.1, ID 19 = v1.2 append — IDs are final, never renumbered)

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
| 19 | MANUAL_TEST_MODE | u32 | 0/1 | 0 | 0..1 | **v1.2, global manual test mode**: 1 = suspend the automatic charger completely and drive each channel directly at the ID 16/18 duty with every battery condition bypassed (only the hardware floor stays - see section 5.2) |

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
  Do NOT confuse it with manual test mode (section 5.2): fixed-duty is the
  auto-gated hold, manual mode is the gate-free bench mode.
- **No setpoints are exposed.** Charge scenario (14.4/14.3/14.6 V, float band,
  12.8 V reentry) is intentionally not adjustable from the ESP.
- **v1.2 dual use of IDs 16/18:** while ID 19 = 1 the ID 16/18 values act as
  the MANUAL duty command (applied immediately, no ramp); IDs 15/17 are
  ignored in that state. With ID 19 = 0 the v1.1 fixed-duty semantics of
  15..18 are unchanged.
- A latched FINAL_FAULT charger state is **never** released by CHGx_ENABLE
  or by manual mode; only a reboot clears it.

### 5.1 UI descriptions (show under each control - user order 2026-09-23)

Render the Persian line under (or beside) each control in the panel; the
English line is for the agent/maintainers.

| ID | English (label + hint) | توضیح فارسی برای نمایش زیر کنترل |
|---|---|---|
| 0 | Ch1 zero offset (ADC counts) - reading at zero current; subtracts from raw counts before conversion | آفست جریان صفر کانال ۱ (شمارش ADC)؛ مقداری که در جریان صفر خوانده می‌شود و قبل از تبدیل از counts کم می‌شود |
| 1 | Ch2 zero offset (ADC counts) - same for channel 2 | آفست جریان صفر کانال ۲؛ مانند کانال ۱ برای زنجیرهٔ دوم |
| 2 | Ch1 gain trim (permille) - final scale of the mA conversion; 1085 = bench value | ضریب گین تبدیل جریان کانال ۱ (پرمیل)؛ مقیاس نهایی تبدیل به mA، مقدار بنچ ۱۰۸۵ |
| 3 | Ch2 gain trim (permille) - final scale of the mA conversion | ضریب گین تبدیل جریان کانال ۲ (پرمیل) |
| 4 | VIN offset (mV, signed) - adder on the 24 V input reading after the divider | آفست کالیبراسیون ولتاژ ورودی ۲۴V بر حسب mV (علامت‌دار)؛ بعد از تبدیل مقسم جمع می‌شود |
| 5 | V24 offset (mV, signed) - adder on the 24 V battery pack reading | آفست کالیبراسیون ولتاژ پک ۲۴V بر حسب mV (علامت‌دار) |
| 6 | V12 offset (mV, signed) - adder on the 12 V battery reading (middle node) | آفست کالیبراسیون ولتاژ باتری ۱۲V (نود میانی) بر حسب mV (علامت‌دار) |
| 7 | Median window (1/3/5) - median-of-N on the raw current samples; 1 = off, 3 = default, 5 also kills double-spikes | اندازهٔ پنجرهٔ مدین روی نمونه‌های خام جریان؛ ۱ = خاموش، ۳ = پیش‌فرض، ۵ پالس‌های دوتایی را هم حذف می‌کند |
| 8 | Average window (1..10) - moving average over the last N current samples; 1 = off, 10 = default | پنجرهٔ میانگین متحرک روی آخرین N نمونهٔ جریان؛ ۱ = خاموش، ۱۰ = پیش‌فرض |
| 9 | Ch1 efficiency (permille) - only scales the current ESTIMATE, not the real charge | بازدهی کانال ۱ (پرمیل)؛ فقط روی تخمین جریان اثر دارد، نه شارژ واقعی |
| 10 | Ch2 efficiency (permille) - estimate scaling; 242 absorbs the ch2 sense over-read (do not set to ~700) | بازدهی کانال ۲ (پرمیل)؛ مقدار ۲۴۲ خطای over-read سنس کانال ۲ را جبران می‌کند (به ~۷۰۰ تغییرش ندهید) |
| 11 | Charger 1 on/off - 0 cuts the PWM immediately (battery keeps its charge), 1 resumes with a soft ramp | کلید قطع/وصل شارژر ۱؛ صفر فوراً PWM را قطع می‌کند و یک شارژ را با رمپ نرم ادامه می‌دهد |
| 12 | Charger 2 on/off - same for charger 2 | کلید قطع/وصل شارژر ۲ |
| 13 | Ch1 duty ceiling (permille) - hard cap on the PWM of charger 1 (ramp, regulation and fixed mode all respect it) | سقف duty ی PWM شارژر ۱ (پرمیل)؛ رمپ، تنظیم و مود فیکس همه به آن احترام می‌گذارند |
| 14 | Ch2 duty ceiling (permille) - hard cap on the PWM of charger 2 | سقف duty ی PWM شارژر ۲ (پرمیل) |
| 15 | Ch1 fixed-duty mode - hold the PWM at ID 16 instead of the regulation loop (switching still stops at the absorb voltage) | مود duty فیکس شارژر ۱؛ PWM روی مقدار شناسهٔ ۱۶ قفل می‌شود به‌جای حلقهٔ تنظیم (سوئیچینگ بالای ولتاژ ابزورب همچنان متوقف می‌شود) |
| 16 | Ch1 fixed duty value (permille) - the number to hold while ID 15 is on | مقدار duty فیکس شارژر ۱ (پرمیل)؛ عددی که در مود فیکس نگه داشته می‌شود |
| 17 | Ch2 fixed-duty mode - hold the PWM at ID 18 | مود duty فیکس شارژر ۲ |
| 18 | Ch2 fixed duty value (permille) | مقدار duty فیکس شارژر ۲ (پرمیل) |
| 19 | Manual test mode - master switch: the automatic charger stops completely and you set each channel's duty yourself; battery checks are off, the JIT/input/15 V hardware protections stay on, and the panel must keep the link alive | کلید مود تست دستی؛ با روشن‌شدنش شارژر خودکار کاملاً متوقف می‌شود و duty هر کانال را خودتان مستقیم می‌گذارید؛ شرط‌های باتری غیرفعال می‌شوند، محافظت‌های سخت‌افزاری JIT/ورودی/۱۵V باقی می‌مانند و پنل باید لینک را زنده نگه دارد |

### 5.2 Manual test mode contract (v1.2 — user order 2026-09-23)

`MANUAL_TEST_MODE` (ID 19) is a **global bench/test switch**. While it is 1
the automatic charger is fully suspended and the human at the panel owns the
PWM. Purpose: calibration and filter tuning - set a duty, watch the raw ->
shunt -> unfiltered -> filtered current chain, adjust offsets/gains/filter
sizes, with or without a battery in the loop.

**Bypassed while ID 19 = 1** (this is the point of the mode):

- the whole automatic state machine (BULK/ABSORB/FLOAT, JIT auto-retry,
  ramps) and the 15 s battery-settle wait;
- every battery condition: the battery-validity window, BAT_LOST detection
  and its alarm/buzzer, and the 14.4 V absorb stop. Running with no
  battery, an electronic load, or odd voltages is allowed.

**Commanded by the user:**

- per-channel duty = the existing ID 16 (ch1) / ID 18 (ch2) values, applied
  immediately (next control cycle, no soft ramp). 0 = channel off.
- IDs 15/17 (fixed-duty ON) are ignored while manual mode is on.

**Stays active - the non-negotiable hardware floor:**

1. **24 V input presence** (Vin >= 22 V). Below it the PWM is 0 and the
   channel state shows INPUT_WAIT; when input returns the commanded duty
   reapplies immediately.
2. **Hardware JIT over-current trip** (the LM393 transformer-saturation
   guard): a trip cuts that channel's PWM instantly. There is NO auto-retry
   in manual mode - re-send the duty value (any SET_PARAM of ID 16/18) to
   re-arm the channel. The 3rd trip still latches FINAL_FAULT (reboot-only).
3. **Hard overvoltage cutoff at 15.0 V per channel** (reuses the firmware
   constant `CHG_MAX_VALID_BATTERY_MV`): duty 0 while the channel voltage
   is >= 15000 mV, automatic resume below it. This protects the output
   stage when no battery clamps the voltage.
4. **Duty ceilings**: every applied duty is still clamped to
   min(compile-time 500 permille, runtime ceiling ID 13/14).
5. **Channel cut IDs 11/12** still force that channel's duty to 0.
6. **FINAL_FAULT** is never released by anything but a reboot.

**Link-loss dead-man (REQUIRED):** while ID 19 = 1 the STM32 expects at
least one valid frame every **3 s** (any SET_PARAM or GET_PARAMS counts).
The panel MUST send a keepalive (e.g. GET_PARAMS) every **1 s** while
manual mode is on, from every tab and in the background. On watchdog
expiry the STM32 sets both duties to 0, exits manual mode and returns to
the autonomous charger. Rationale: a crashed browser or a closed tab must
never leave a battery connected to an unregulated fixed duty. Note: a
throttled background browser tab stops the keepalive and the STM exits
manual mode within 3 s - that is by design; if nobody is watching the
bench, the drive stops.

**State reporting while manual:** priority FINAL_FAULT (7) > JIT_RETRY_WAIT
(5) > INPUT_WAIT (6) > MANUAL (9); with none of those active the state
field shows the new value **9 = MANUAL**. TLM flags bit 5 = manual mode
active. On exiting manual mode both channels restart the autonomous
charger from OFF (the normal 15 s settle applies again).

## 6. TLM_LIVE payload layout (84 bytes, little-endian)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | u16 | seq | Wraps at 65535; use for drop detection |
| 2 | u8 | flags | b0 snapshot valid, b1 input present, b2 meas data valid, b3 charger-1 ESP enable, b4 charger-2 ESP enable, b5 manual test mode active (v1.2), b6..b7 = 0 |
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
6 INPUT_WAIT, 7 FINAL_FAULT, 8 BAT_LOST, 9 MANUAL (v1.2, section 5.2)`.

Channel mapping: **channel 1 = Trans1 = upper battery (Vhigh)**,
**channel 2 = Trans2 = lower battery (Vlow)**.

Reference values from the 2026-09-22 bench point (use as sanity check for the
panel): raw1 ≈ 310 ↔ shunt1 ≈ 2720 µV ↔ ma1_unfiltered ≈ 287 ↔ iest1 ≈ 364;
raw2 ≈ 951 ↔ shunt2 ≈ 8346 µV ↔ ma2_unfiltered ≈ 897.

## 7. Recommended ESP behavior

1. Boot, open UART at 921600 8N1, start a 100 ms RX pump.
2. Wait for the first TLM_LIVE (proves the link; the STM32 powers the ESP
   enable line only when its comm task runs).
3. Send `GET_PARAMS`, parse `PARAMS_BULK`, show a live dashboard of all
   parameters (19 in the current firmware, 20 from v1.2).
4. UI controls (sliders/toggles) send `SET_PARAM` per change and wait for the
   matching `PARAM_REPORT`; display the **applied** value (it may differ from
   the requested value when clamped).
5. Keep the last known parameter set in ESP NVRAM/flash and re-apply it after
   an STM32 reboot (detect: PARAMS_BULK returns defaults, or telemetry seq
   restarts at 0).
6. Provide two prominent buttons: charger 1 ON/OFF and charger 2 ON/OFF
   (IDs 11/12). OFF is the safe direction: PWM stops immediately.
7. **Tab layout (user order 2026-09-23: manual mode is a SEPARATE tab,
   not part of the general settings):**
   - **Tab 1 - Status/telemetry:** the live dashboard (TLM chain, states,
     voltages, fault mask, ON/OFF buttons for IDs 11/12).
   - **Tab 2 - Settings/calibration:** every parameter control with its
     section 5.1 description under it: current-chain calibration (0..6),
     filters (7/8), efficiency (9/10), duty ceilings (13/14) and the
     v1.1 fixed-duty hold (15..18, regulation-off but still auto-gated).
   - **Tab 3 - Manual test (its own tab):** the ID 19 master switch behind
     a confirmation dialog ("the automatic charger and all battery
     protections stop - continue?"), per-channel duty slider + numeric
     field (0..min(500, ceiling), step 1 permille, writes ID 16/18 and
     shows the applied value from PARAM_REPORT), a big ALL-OFF button
     (both duties to 0), a keepalive indicator (link watched / dead-man
     countdown), and a live strip per channel: duty, ipri, iest, channel
     voltage, state (incl. MANUAL/JIT/INPUT_WAIT/15 V cutoff), plus a
     permanent warning banner that battery protections are bypassed.
8. Display telemetry continuously; the current-chain numbers (raw → shunt →
   unfiltered → filtered) exist exactly so the panel can show the chain
   step-by-step and help find the correct calibration numbers - this is
   exactly what the manual test tab is for.

## 8. Safety rules for the ESP implementation

- Never spam the link: a few SET_PARAM frames per second is plenty; the
  STM RX ring is 256 bytes (DMA-filled) and frames are checksummed, but
  there is no reason to flood.
- Always respect clamping (the STM32 enforces it anyway).
- Do not try to change charge voltages/setpoints — they are not in the
  protocol; requesting an unknown ID is silently ignored.
- Treat loss of telemetry > 1 s as "link down": show a warning, keep last
  values greyed out. (The charger continues autonomously — the STM32 never
  depends on the ESP.)
- ESP crash/reboot must leave CH_PD un-driven (input/high-Z) — the STM32 owns
  that line.
- **Manual test mode rules (v1.2):**
  - The keepalive (any frame, e.g. GET_PARAMS every 1 s) MUST keep running
    from every tab while ID 19 = 1 - the STM32 dead-man exits manual mode
    and zeroes both duties after 3 s of silence.
  - Enabling ID 19 requires a confirmation dialog; leaving the manual tab
    or closing the panel must NOT silently keep the mode on without the
    keepalive (the STM32 dead-man covers the crash case).
  - Show JIT trips, the 15 V cutoff and INPUT_WAIT prominently on the
    manual tab; a re-send of the duty re-arms after a JIT trip; FINAL_FAULT
    needs an STM32 reboot.
  - The panel may re-apply ID 19 + duties after an STM32 reboot (same
    policy as the other parameters), but only together with the keepalive.

## 9. Current activation state (STM32 side)

Everything of protocol v1.1 is implemented and pushed, but the STM32 build
keeps `MODULE_ESP = 0` in `Firmware/Config/Inc/modules_enable.h` because the
repo rule-check (`tools/check_ai_rules.sh`) currently requires it. Flipping
it to `1` (one line) activates TaskComm → EspLink_Init → ESP power +
protocol. That flip is intentionally left to the project owner.

**v1.2 firmware status (2026-09-23):** manual test mode (ID 19, section
5.2), the state-9/flags-b5 telemetry additions, the 3 s dead-man and the
payload limit 112 are SPECIFIED but NOT yet in the pushed firmware - they
are the next STM32 task. Until that lands: ID 19 is silently ignored,
PARAMS_BULK still carries 19 params at payload 96, and the parser still
rejects len > 96. The ESP side can be built and tested against v1.1 now;
the v1.2 additions are additive and will light up with the firmware
update.

## 10. Protocol version

v1.2 (2026-09-23, user order of the same day): added the global manual test
mode - new parameter ID 19 (append-only, IDs 0..18 unchanged and still
final), charger state 9 = MANUAL, TLM flags bit 5, the 3 s link-loss
dead-man with the 1 s panel keepalive, the manual-mode hardware floor
(input presence, JIT trip with manual re-arm, 15.0 V hard overvoltage
cutoff, duty ceilings, FINAL_FAULT latch), and the separate Manual Test
tab requirement. Max payload 96 -> 112 bytes (PARAMS_BULK = 101 payload
bytes with 20 params). The STM32 side of v1.2 is specified but not yet
implemented - see section 9.

v1.1 (2026-09-22, same day as v1 and BEFORE any ESP-side implementation
existed — the v1.1 IDs are the final ones). Changes vs v1: filter switches
merged into ONE size parameter per filter (median 1/3/5, average window
1..10; size 1 = bypass, no separate on/off), and six new duty params
(ceiling + fixed-mode enable/value per channel). 2026-09-23: link speed
raised to 921600 with DMA in both directions (no protocol change), and the
per-parameter UI descriptions (section 5.1) added. Parameter IDs and the
TLM layout are frozen from here on; future additions append new IDs / new
message types only, never renumber.
