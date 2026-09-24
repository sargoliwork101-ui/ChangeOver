# ESP-Link Agent Spec — ESP Side Implementation Handoff

> این سند برای ماموری است که سمت ESP را می‌نویسد (دستور کاربر ۲۰۲۶-۰۹-۲۲).
> سمت STM32 کامل و push شده است؛ فقط فعال‌سازی نهایی `MODULE_ESP` مانده (پایین را ببینید).
> متن فنی عمداً انگلیسی است تا هیچ ابهامی در پروتکل نماند.
>
> v1.3 (2026-09-24): CAL_REFERENCE command (type 0x03, section 5.4) + ETA
> conversion factors (ID 9/10 renamed CHG_ETA1/ETA2_PERMILLE, default 0 =
> identity; live `i_filtered x Vin x eta / (1000 x Vbat)` when set) — the
> STM32 side is IMPLEMENTED and pushed. With ETA = 0 nothing changes vs v1.2.
>
> v1.2 (2026-09-23): manual test mode (ID 19, section 5.2) — the STM32
> side is IMPLEMENTED and pushed (param 19, state 9, flags b5, dead-man,
> payload 112). Only the final `MODULE_ESP = 1` flip is left (section 9).

---

## 1. What exists already (STM32 side — DONE)

- Binary command protocol + periodic telemetry over **USART1, 921600 8N1**.
- DMA transport in both directions on the STM32 (circular 256-byte RX ring,
  zero CPU per byte; DMA-drained TX ring) - user order 2026-09-23.
- 20 runtime parameters (current-chain calibration, voltage offsets,
  current-filter sizes, per-channel ETA conversion factors (v1.3,
  CAL_REFERENCE-calibratable), per-channel
  charger enable/cut, per-channel PWM duty ceiling, fixed-duty mode, and
  the v1.2 global manual test mode - ID 19, section 5.2).
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
| 0x03 | ESP→STM | CAL_REFERENCE | `[target:u8][ref_mA:u32 LE]` (5 bytes) — one-shot calibration from a typed DMM reading; targets 0/1 = GAIN ch1/2, 2/3 = ETA ch1/2 (v1.3, section 5.4) |
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
| 2 | CUR1_GAIN_PERMILLE | u32 | permille | 1046 | 100..3000 | Bench gain trim, channel 1 |
| 3 | CUR2_GAIN_PERMILLE | u32 | permille | 1303 | 100..3000 | Same, channel 2 |
| 4 | VIN_OFFSET_MV | **i32** | mV | 0 | −2000..2000 | 24 V input voltage calibration |
| 5 | V24_OFFSET_MV | **i32** | mV | 0 | −2000..2000 | 24 V battery pack voltage calibration |
| 6 | V12_OFFSET_MV | **i32** | mV | 0 | −2000..2000 | 12 V (middle node) battery calibration |
| 7 | FILTER_MEDIAN_SIZE | u32 | samples | 3 | 1/3/5 | Median window on charge currents. Valid sizes 1, 3, 5; other values round DOWN to the next odd size. **1 = bypass** (no separate on/off switch exists). Filter state resets on change. |
| 8 | FILTER_AVERAGE_WINDOW | u32 | samples | 10 | 1..10 | Moving-average window on charge currents. **1 = bypass.** Filter state resets on change. |
| 9 | CHG_ETA1_PERMILLE | u32 | permille | 0 | 0..999 | **v1.3**: charger 1 conversion factor. 0 = identity (default - `iest = i_filtered`; with the battery-calibrated gains the reading already is the battery current). Non-zero: `iest = i_filtered x Vin x eta / (1000 x Vbat)` with LIVE voltages, so the battery-current reading stays true while the battery charges. Set with CAL_REFERENCE (section 5.4), not by hand |
| 10 | CHG_ETA2_PERMILLE | u32 | permille | 0 | 0..999 | Same, charger 2 |
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
- `Iest = I_filtered` by default (identity since 2026-09-24: the sense
  chain is battery-side per the user - the measured voltage IS the battery
  current). v1.3 makes the conversion EXPLICIT and optional: with ETA
  (ID 9/10) = 0 (compile default) the estimate stays the identity; a
  CAL_REFERENCE command (section 5.4) - or a manual ID 9/10 write - turns
  on the live `i_filtered x Vin x eta / (1000 x Vbat)` conversion so the
  reading tracks the battery voltage during a charge. The OFFSET/GAIN/
  FILTER parameters change the measured current the charger regulates on.
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
| 0 | Ch1 zero offset (ADC counts) - subtracted inside the mA formula: mA ≈ (raw − offset) × 0.8776 × gain/1000 | آفست جریان صفر کانال ۱ (شمارش ADC)؛ داخل فرمول mA کم می‌شود: mA ≈ (raw − آفست) × 0.8776 × گین/1000 |
| 1 | Ch2 zero offset (ADC counts) - same formula as channel 1 | آفست جریان صفر کانال ۲؛ همان فرمول کانال ۱ برای زنجیرهٔ دوم: mA ≈ (raw − آفست) × 0.8776 × گین/1000 |
| 2 | Ch1 gain trim (permille) - final scale of the mA conversion: mA ≈ (raw − offset) × 0.8776 × gain/1000; 1046 = bench value (bench 2026-09-24) | ضریب گین تبدیل جریان کانال ۱ (پرمیل)؛ فرمول: mA ≈ (raw − آفست) × 0.8776 × گین/۱۰۰۰ — مقدار بنچ ۱۰۴۶ (۲۰۲۶-۰۹-۲۴؛ پیش‌فرض ≈ ×۰٫۹۱۸۰ به‌ازای هر count) |
| 3 | Ch2 gain trim (permille) - same formula as channel 1 | ضریب گین تبدیل جریان کانال ۲ (پرمیل)؛ فرمول: mA ≈ (raw − آفست) × 0.8776 × گین/۱۰۰۰ |
| 4 | VIN offset (mV, signed) - adder in: Vin_mV ≈ counts × 9.007 + offset (divider 69.2k/6.8k) | آفست کالیبراسیون ولتاژ ورودی ۲۴V بر حسب mV (علامت‌دار)؛ فرمول: Vin ≈ counts × 9.007 + آفست (مقسم 69.2k/6.8k) |
| 5 | V24 offset (mV, signed) - adder in: V24_mV ≈ counts × 9.007 + offset | آفست کالیبراسیون ولتاژ پک ۲۴V بر حسب mV (علامت‌دار)؛ فرمول: V24 ≈ counts × 9.007 + آفست |
| 6 | V12 offset (mV, signed) - adder in: V12_mV ≈ counts × 4.859 + offset; Vhigh = V24 − V12 | آفست کالیبراسیون ولتاژ باتری ۱۲V (نود میانی) بر حسب mV (علامت‌دار)؛ فرمول: V12 ≈ counts × 4.859 + آفست و Vhigh = V24 − V12 |
| 7 | Median window (1/3/5) - first stage of the filter pipeline: average_W( median_N( mA_raw ) ); 1 = off, 3 = default, 5 also kills double-spikes | پنجرهٔ مدین (۱/۳/۵) - مرحلهٔ اول فیلتر: اول median(N) بعد average(W) روی mA خام؛ ۱ = خاموش، ۳ = پیش‌فرض، ۵ پالس‌های دوتایی را هم حذف می‌کند |
| 8 | Average window (1..10) - second stage of the filter pipeline: average_W( median_N( mA_raw ) ); 1 = off, 10 = default | پنجرهٔ میانگین (۱..۱۰) - مرحلهٔ دوم فیلتر: میانگین آخرین W نمونهٔ خروجی مدین؛ ۱ = خاموش، ۱۰ = پیش‌فرض |
| 9 | Ch1 conversion factor ETA1 (permille, v1.3) - 0 = identity (default: the reading already is the battery current); non-zero = `iest = i_filtered x Vin x eta / (1000 x Vbat)` with live voltages. Calibrate with the CAL_REFERENCE button (type the battery-side DMM mA), never by hand | ضریب تبدیل کانال ۱ (پرمیل، v1.3) — صفر = همانی (پیش‌فرض: عدد خودش جریان باتری است)؛ غیرصفر = iest = i_فیلترشده × Vin × η ÷ (۱۰۰۰ × Vbat) با ولتاژهای زنده. با دکمهٔ CAL_REFERENCE کالیبره کنید (عدد مولتی‌متر سمت باتری را بدهید)، نه دستی |
| 10 | Ch2 conversion factor ETA2 (permille, v1.3) - same as ID 9, charger 2 | ضریب تبدیل کانال ۲ (پرمیل، v1.3) — مانند ID 9، برای کانال ۲ |
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
never leave a battery connected to an unregulated fixed duty. Panel
behavior as shipped (2026-09-23): a throttled desktop background tab
still polls about once per second, so the keepalive continues; a fully
closed or frozen panel (e.g. phone screen off) actively sends ID 19 = 0
after its own 10 s no-browser guard, so an absent operator never leaves
an unregulated duty running. A periodic GET_PARAMS refresh (e.g. every
30 s) is allowed and expected so the parameter table stays truthful even
across an undetected STM32 reboot near the sequence wrap window.

**State reporting while manual:** priority FINAL_FAULT (7) > JIT_RETRY_WAIT
(5) > INPUT_WAIT (6) > MANUAL (9); with none of those active the state
field shows the new value **9 = MANUAL**. TLM flags bit 5 = manual mode
active. On exiting manual mode both channels restart the autonomous
charger from OFF (the normal 15 s settle applies again).

### 5.3 Conversion formulas - display them in the panel (user order 2026-09-23)

The panel must SHOW these formulas in its UI - under the calibration
controls on the Settings tab and next to each step of the live current
chain - with the live parameter values substituted. They are copied
verbatim from the firmware (one stage per schematic element); the TLM
fields of section 6 are exactly these formulas' outputs.

Current chain (per channel; offset = ID 0/1, gain = ID 2/3):

```text
shunt_uV = raw_counts x 3300/4095 x 11/10 x 1000/101   (= raw x 8.7756 uV)
           |_counts->pin mV_| |_R41/R42_| |_LM358 gain_|
           pure hardware value - BEFORE any offset/trim (TLM: shuntX_uv)

mA_unfiltered = max(raw_counts - offset, 0)
                x 3300/4095 x 11/10 x 1000/(101 x 10) x gain/1000
                (= (raw - offset) x 0.8776 x gain/1000 mA;
                   defaults offset=8; gain ch1=1046 -> x 0.9180, ch2=1303 -> x 1.1436 mA per count)
                (TLM: maX_unfiltered)

i_filtered_ma = average_W( median_N( mA_unfiltered ) )
                first median (N = ID 7, 1/3/5), then moving average
                (W = ID 8, 1..10) - this is what the charger decides on
                (TLM: iX_filtered_ma)
```

Output-current estimate (TLM: iestX_ma) - v1.3, two modes (ETA = ID 9/10):

```text
ETA = 0 (default):  iest_ma = i_filtered_ma
          (identity: with the battery-calibrated gains the filtered reading
           already is the battery current - user bench fact 2026-09-24)

ETA > 0:    iest_ma = i_filtered_ma x Vin_mv x eta / (1000 x Vbat_mv)
          with the LIVE Vin (TLM offset 60) and the channel battery voltage
          (ch1 = Vhigh = offset 76, ch2 = Vlow = offset 72), so the reading
          stays true while the battery voltage moves during a charge. eta
          comes from the CAL_REFERENCE command (section 5.4) - the panel
          must not hand-compute it.
```

Voltage chain (offsets = IDs 4/5/6, saturating add, never below 0 mV):

```text
Vin_mV = raw x 3300/4095 x 76000/6800 + VIN_OFFSET   (divider 69.2k/6.8k; = raw x 9.007)
V24_mV = raw x 3300/4095 x 76000/6800 + V24_OFFSET   (same 69.2k/6.8k divider)
V12_mV = raw x 3300/4095 x 41000/6800 + V12_OFFSET   (divider 34.2k/6.8k; = raw x 4.859)
Vlow_mV = V12_mV        Vhigh_mV = V24_mV - V12_mV (clamped at 0)
```

Fixed hardware constants (NOT parameters - never editable): 12-bit ADC,
3300 mV reference, full scale 4095; R41/R42 = 1 k / 10 k MCU-input divider
on the current nets; LM358 non-inverting gain 101; shunt 10 mOhm; voltage
dividers 69.2 k / 6.8 k (both 24 V nets) and 34.2 k / 6.8 k (12 V net).

### 5.4 Panel calibration command CAL_REFERENCE (v1.3 — user order 2026-09-24)

One command calibrates the whole current chain from the panel: the user
types the multimeter reading, the STM32 computes and applies the parameter
on its own live snapshot - the panel never needs the raw math. This is the
"send and receive every calibration number via the ESP" workflow.

Frame (ESP→STM, type 0x03, payload 5 bytes):

```text
AA 55 03 05 [target:u8] [ref_mA:u32 LE] [xor checksum]
```

| target | meaning | DMM placement | firmware computes | replies |
|---|---|---|---|---|
| 0 | GAIN ch1 | in series with whatever current the displayed reading should equal (battery side for a battery-reading gain; the channel 24 V input for a physical shunt-current gain) | `gain = gain x ref / i_filtered` (setter clamps 100..3000) | PARAM_REPORT ID 2, then PARAM_REPORT ID 9 = 0 |
| 1 | GAIN ch2 | same, charger 2 | same | PARAM_REPORT ID 3, then PARAM_REPORT ID 10 = 0 |
| 2 | ETA ch1 | in series with the BATTERY of charger 1 | `eta = ref x Vbat x 1000 / (i_filtered x Vin)` from the live snapshot (setter clamps 0..999) | PARAM_REPORT ID 9 |
| 3 | ETA ch2 | in series with the BATTERY of charger 2 | same | PARAM_REPORT ID 10 |

Why GAIN resets ETA: the old ETA absorbed the old gain's error, so after a
gain calibration it is stale by definition - the second report (ID 9/10 =
0) tells the panel to show identity again until the user reruns target
2/3. The recommended order is therefore: GAIN first (if at all), ETA last.

Rejection (NO reply frame at all - show a "رد شد: جریان/شرایط ناکافی"
message in the UI): snapshot invalid, ref outside 50..5000 mA, live
filtered current below 50 mA, or (ETA only) Vin < 10 V / Vbat < 5 V. Tell
the user to raise the channel current above 50 mA first (e.g. manual mode
at a small duty).

Verified example frames (hex):

```text
CAL ETA ch1, ref 425 mA:   AA 55 03 05 02 A9 01 00 00 AC
CAL GAIN ch1, ref 300 mA:  AA 55 03 05 00 2C 01 00 00 2B
```

Recommended UI: a "کالیبراسیون جریان" card with one mA number input and
four buttons (گین کانال۱ / گین کانال۲ / η کانال۱ / η کانال۲). After a
successful ETA calibration show the applied eta next to the button (it
arrives in the PARAM_REPORT). Include IDs 9 and 10 in the session cache
that is re-sent after a detected STM32 reset (all parameters are RAM-only
on the STM32).

Bench procedure (also in ESP_BENCH_MANUAL.md, گام ۵ب): with the channel
running above 50 mA (e.g. manual duty 15%):
1. DMM in series with the battery → type its mA → target 2 (or 3). iest now
   equals the DMM at this operating point AND keeps tracking the battery
   voltage as it rises during the charge (identity mode would drift by
   Vbat_cal/Vbat, about 10% across a full charge).
2. Optional, physical shunt-current gain: DMM in series with the channel
   input → target 0 (or 1) → then repeat step 1 (mandatory after a GAIN).

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
panel): raw1 ≈ 310 ↔ shunt1 ≈ 2720 µV ↔ ma1_unfiltered ≈ 287 ↔ iest1 ≈ 287 (identity since 2026-09-24; ≈364 was the retired conversion);
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
   exactly what the manual test tab is for. Render the section 5.3 formulas
   in the UI next to each chain step with the live parameter values
   substituted (user order 2026-09-23: the formulas must be visible in the
   panel appearance).

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
  - Enabling ID 19 requires a confirmation dialog. A background tab keeps
    the keepalive running; a closed/frozen panel must end the mode itself
    (as shipped: ID 19 = 0 after 10 s without a poll) - the STM32 dead-man
    covers the crash case.
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

**v1.2 firmware status (2026-09-23): IMPLEMENTED and pushed.** Manual
test mode (ID 19, section 5.2), charger state 9 = MANUAL, TLM flags bit
5, the 3 s link dead-man with manual JIT re-arm, the 15.0 V manual
overvoltage cutoff, the frozen battery-lost detection during manual, and
the payload limit 112 (PARAMS_BULK = 20 params / 101 payload bytes) are
all in the firmware. The ESP-side constraints that come with it are
documented in `Firmware/Modules/EspLink/README.md` - most importantly the
1 s keepalive while ID 19 = 1.

## 10. Protocol version

v1.3 (2026-09-24, user order of the same day): added the CAL_REFERENCE
command (type 0x03 - one-shot panel calibration of the current-chain GAIN
and the ETA factors from a typed multimeter reading; section 5.4) and
re-purposed parameters 9/10 as per-channel ETA conversion factors
(CHG_ETA1/ETA2_PERMILLE, default 0 = identity, range 0..999; renamed from
CHG_EFF_UP/DN_PERMILLE). With ETA = 0 (compile default) iest stays the
identity (2026-09-24 bench fact: the sense chain is battery-side); with
ETA > 0, iest = i_filtered x Vin x eta / (1000 x Vbat) with live voltages.
No TLM change, no ID renumbering; SET_PARAM/GET_PARAMS unchanged. The
STM32 side of v1.3 is implemented and pushed the same day.

v1.2 (2026-09-23, user order of the same day): added the global manual test
mode - new parameter ID 19 (append-only, IDs 0..18 unchanged and still
final), charger state 9 = MANUAL, TLM flags bit 5, the 3 s link-loss
dead-man with the 1 s panel keepalive, the manual-mode hardware floor
(input presence, JIT trip with manual re-arm, 15.0 V hard overvoltage
cutoff, duty ceilings, FINAL_FAULT latch), and the separate Manual Test
tab requirement. Also (same day, second order): the conversion formulas
(current chain, Iest, voltage chain - section 5.3) must be displayed in
the panel UI, and the section 5.1 rows now carry them. Max payload
96 -> 112 bytes (PARAMS_BULK = 101 payload bytes with 20 params). The
STM32 side of v1.2 is implemented and pushed the same day - see section 9.

v1.1 (2026-09-22, same day as v1 and BEFORE any ESP-side implementation
existed — the v1.1 IDs are the final ones). Changes vs v1: filter switches
merged into ONE size parameter per filter (median 1/3/5, average window
1..10; size 1 = bypass, no separate on/off), and six new duty params
(ceiling + fixed-mode enable/value per channel). 2026-09-23: link speed
raised to 921600 with DMA in both directions (no protocol change), and the
per-parameter UI descriptions (section 5.1) added. Parameter IDs and the
TLM layout are frozen from here on; future additions append new IDs / new
message types only, never renumber.
