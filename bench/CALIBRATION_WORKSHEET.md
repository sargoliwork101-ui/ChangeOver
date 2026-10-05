/**
 * @file    bench/CALIBRATION_WORKSHEET.md
 * @brief   [EN] Bench worksheet: exactly what to record so every scale can be rebuilt from scratch.
 *          [FA] برگهٔ کار بنچ: دقیقاً چه چیزی ثبت شود تا هر مقیاس از پایه بازساخته شود.
 */

# Calibration worksheet / برگهٔ کالیبراسیون

---

## 0. Before anything / قبل از هر کاری

> **[EN] FLASH BOTH SIDES. The frame changed twice: it is now 104 bytes (five
> raw-count fields) and carries a CRC-16 plus a version byte. An
> out-of-step flash no longer fails silently - the panel now says "the firmware
> and panel versions do not match, reflash both" - but the two still will not
> talk until you do.**
>
> **[FA] هر دو طرف را فلش کنید. قاب دو بار عوض شد: حالا ۱۰۴ بایت است و CRC-16
> به‌اضافهٔ بایت نسخه دارد. فلش ناهماهنگ دیگر بی‌صدا شکست نمی‌خورد — پنل صریح
> می‌گوید «نسخهٔ فرم‌ور و پنل یکی نیست» — ولی تا فلش نکنید با هم حرف نمی‌زنند.**

- [ ] **Clean + Rebuild** (not incremental - a stale `bsp.o` gives a hybrid:
      new LUT with the old divider) / **Clean + Rebuild کامل**، نه افزایشی
- [ ] Flash **STM32** / فلش STM32
- [ ] Flash **ESP** / فلش ESP
- [ ] Panel shows live data / پنل داده زنده نشان می‌دهد

---

## 0b. WHAT IS ALREADY DONE / آنچه قبلاً انجام شده

[EN] These came out of your 2026-09-29 readings and are already in the
firmware, so do NOT redo them - they are here so you know what the board now
believes:

| item | value | how it was established |
|---|---|---|
| VDDA | 3.300 V | you measured it; the reference is fine |
| 24 V dividers (both) | 68000 / 6800 | connector vs ADC pin: 10.9968 and 11.0139, and 68K/6.8K is exactly 11.0000 |
| 12 V divider | 34398 / 6800 | 14.88 V / 2.456 V = 6.0586 |
| V12 bench compensation | **OFF** | it was subtracting 150 mV + I x 0.47 ohm, which pushed the real terminal 0.4 V above target |
| both current LUTs | refitted | they are power tables and the voltage they divide by moved |

[FA] این‌ها از خوانش‌های ۲۰۲۹-۰۹-۲۹ شما درآمدند و در فرم‌ور هستند؛ **دوباره
انجامشان ندهید**. اینجا آمده‌اند تا بدانید برد حالا چه باوری دارد.

---

## 1. ADC reference - do this first / مرجع ADC، اول این

[EN] One DMM reading fixes all three voltage channels at once, because they all
share this. / [FA] یک اندازه‌گیری، هر سه کانال ولتاژ را با هم درست می‌کند، چون
همه این را مشترک دارند.

| Read from firmware | DMM probe on | Value |
|---|---|---|
| `MeasVddaMv` | — | ________ |
| `MeasVrefintRawCounts` | — | ________ |
| — | **VDDA / the 3.3 V rail** | ________ mV |

---

## 2. Voltages - 3 points each, NOT 1 / ولتاژها: سه نقطه، نه یکی

> **[EN] Why three: with a single point a gain error and an offset error look
> identical, so one point can be "corrected" in a way that is wrong everywhere
> else. That is exactly how this board ended up with a fabricated divider. Vary
> the supply and take three spread-out readings per channel.**
>
> **[FA] چرا سه‌تا: با یک نقطه، خطای ضربی و خطای جمعی عین هم به نظر می‌رسند، پس
> «اصلاحی» که روی آن یک نقطه بنشیند همه‌جای دیگر غلط است. دقیقاً همین‌طور شد که
> این برد یک مقسم ساختگی گرفت. تغذیه را تغییر دهید و سه نقطهٔ پراکنده بگیرید.**

### 2a. Input 24 V - DMM on the input terminal / ورودی

| # | `MeasVinRawCounts` | `MeasInputVoltageMv` | DMM (mV) |
|---|---|---|---|
| low | ________ | ________ | ________ |
| mid | ________ | ________ | ________ |
| high | ________ | ________ | ________ |

### 2b. Pack 24 V - **DMM from pack+ to GND** / پک

*[EN] The most important one: its divider was just corrected. /
[FA] مهم‌ترین: مقسمش تازه اصلاح شده.*

| # | `MeasV24RawCounts` | `MeasBattery24Mv` | DMM (mV) |
|---|---|---|---|
| low | ________ | ________ | ________ |
| mid | ________ | ________ | ________ |
| high | ________ | ________ | ________ |

### 2c. Mid node 12 V - DMM from the middle point to GND / نقطهٔ میانی

| # | `MeasV12RawCounts` | `MeasBattery12Mv` | DMM (mV) |
|---|---|---|---|
| low | ________ | ________ | ________ |
| mid | ________ | ________ | ________ |
| high | ________ | ________ | ________ |

> **[EN] Take 2a/2b/2c with NO charging current flowing**, so the wire-drop term
> is out of the picture. / **[FA] این سه را بدون جریان شارژ بگیرید** تا افت سیم
> وارد حساب نشود.

---

## 3. Currents - the duty sweep / جریان‌ها: سوییپ

[EN] Same procedure as the BAT1 / BAT2 runs. The CSV now records its own raw
counts, so the file alone is enough. / [FA] همان روش «فقط باتری ۱»/«فقط باتری ۲». حالا خود CSV
شمارش خام را ثبت می‌کند، پس فایل به‌تنهایی کافی است.

- [ ] **Manual mode ON**, one channel at a time / مود دستی، هر بار یک کانال
- [ ] Sweep the duty in steps, settle at each step / دیوتی را پله‌پله بالا ببرید
- [ ] At every step enter in the panel's DMM form:
      **battery current (mA)** and **battery voltage (mV)** /
      در هر پله در فرم مولتی‌متر پنل: **جریان باتری** و **ولتاژ باتری**
- [ ] **BAT1 — فقط باتری ۱** (upper / channel 1) → download the CSV
- [ ] **BAT2 — فقط باتری ۲** (lower / channel 2) → download the CSV
- [ ] Send me **both CSV files**

---

## 3b. The one measurement still open / تنها اندازه‌گیری باز

[EN] If your battery sits at the end of a cable rather than right at the
connector, a drop term may be legitimate - but it has to be MEASURED, not
fitted. The old one was fitted to a single run and was subtracting 203 mV at
zero current, where an I x R term must be zero.

  1. set a known charge current (say 400 mA)
  2. DMM on **CON2 pin 2** (the connector) -> ______ V
  3. DMM on the **battery post itself**     -> ______ V
  4. the current at that moment             -> ______ mA

[FA] اگر باتری سر یک کابل است نه روی خود کانکتور، جملهٔ افت می‌تواند درست باشد —
ولی باید **اندازه گرفته شود**، نه برازش. قبلی روی یک اجرا برازش شده بود و در
جریان صفر ۲۰۳ میلی‌ولت کم می‌کرد، جایی که I×R باید صفر باشد.

---

## 4. Send me / برایم بفرستید

1. Section 1 - the three VDDA numbers / سه عدد مرجع
2. Sections 2a, 2b, 2c - the table above / جدول ولتاژها
3. Section 3 - **both CSV files** / هر دو فایل CSV
4. Section 3b - the cable-drop measurement, if the battery is not at the
   connector / اندازه‌گیری افت کابل، اگر باتری روی کانکتور نیست
5. Anything that looked wrong / هرچه عجیب به نظر رسید

[EN] From that I rebuild, from first principles: the ADC reference, all three
voltage scales, and both current LUTs. / [FA] از روی این‌ها مرجع ADC، هر سه
مقیاس ولتاژ و هر دو جدول جریان را از پایه می‌سازم.

---

## WHERE to put the probes / پراب را کجا بگذارم

[EN] Read off the schematic, so these are physical parts, not net names.
[FA] از شماتیک خوانده شده - پس این‌ها قطعهٔ فیزیکی‌اند، نه اسم نت.

### Ground reference / مرجع زمین
[EN] Black probe stays on **CON2 pin 1** (battery minus) for everything below.
That is the same ground the dividers measure against, so using any other
ground point adds an error that is not in the firmware.
[FA] پراب مشکی برای همهٔ اندازه‌گیری‌های زیر روی **پین ۱ کانکتور CON2** (منفی
باتری) بماند. همان زمینی است که مقسم‌ها نسبت به آن می‌سنجند؛ هر زمین دیگری خطایی
اضافه می‌کند که در فرم‌ور نیست.

### The three voltages / سه ولتاژ

| Firmware value | Red probe on | What it is |
|---|---|---|
| `MeasVinRawCounts` / `MeasInputVoltageMv` | **CON1 pin 2** (`24V_IN_CON`) | the 24 V supply coming in / ورودی ۲۴ ولت |
| `MeasV24RawCounts` / `MeasBattery24Mv` | **CON2 pin 3** (`24V_BAT_CON`) | top of the battery string / سر مثبت پک |
| `MeasV12RawCounts` / `MeasBattery12Mv` | **CON2 pin 2** (`12V_BAT_CON`) | the tap between the two batteries / وسط دو باتری |

```
CON2  (the connector marked BATT / کانکتور BATT)
   pin 3  ---- pack +      <- 24 V measurement
              [ battery 1 ]
   pin 2  ---- middle tap  <- 12 V measurement
              [ battery 2 ]
   pin 1  ---- pack -      <- BLACK PROBE HERE / پراب مشکی اینجا
```

### The ADC reference / مرجع ADC

| Firmware value | Red probe on |
|---|---|
| `MeasVddaMv`, `MeasVrefintRawCounts` | **UP3 output** (the `AMS1117-3.3` regulator, the pin that is NOT input or ground) - or equivalently **MCU pin 9 (VDDA)** on the LQFP48 |

[EN] Power chain for orientation: 24 V in -> UP1 (LM2576-12) -> 12 V ->
UP2 (78M05) -> 5 V -> **UP3 (AMS1117-3.3)** -> 3.3 V. UP3's output IS the ADC
reference, which is why one reading there fixes all three voltage channels.
[FA] زنجیرهٔ تغذیه: ۲۴ ولت ورودی به UP1 و بعد UP2 و بعد **UP3**. خروجی UP3
همان مرجع ADC است - برای همین یک اندازه‌گیری آنجا هر سه کانال را درست می‌کند.

### The currents / جریان‌ها
[EN] Do NOT probe the shunt. Put the DMM **in series with the battery** and read
the charge current the normal way; that is the number the panel form wants.
[FA] روی شانت پراب نگذارید. مولتی‌متر را **سری با باتری** ببندید و جریان شارژ را
عادی بخوانید؛ همان عددی است که فرم پنل می‌خواهد.

---

## Safety while you work / ایمنی حین کار

- [EN] The board now reads ~1.2 % HIGH, so the over-voltage cut trips EARLY -
  the safe direction. Do not "fix" that by hand before section 1. /
  [FA] برد الان ~۱٫۲٪ زیاد می‌خواند، پس قطع اضافه‌ولتاژ زودتر می‌زند - جهت امن.
  قبل از بخش ۱ دستی «درستش» نکنید.
- [EN] Hard limits still active: 950 mA fault, 15.0 V ceiling, OV cut at
  14850 mV, duty ceiling 500 permille. /
  [FA] حدهای سخت فعال‌اند: خطای ۹۵۰ میلی‌آمپر، سقف ۱۵٫۰ ولت، قطع روی
  ۱۴۸۵۰ میلی‌ولت، سقف دیوتی ۵۰۰ پرمیل.
- [EN] Do not change panel params 0..3 (off/gain) during the sweep - they define
  the current LUT's axis and would invalidate the run. /
  [FA] حین سوییپ پارامترهای ۰ تا ۳ پنل را عوض نکنید - محور جدول جریان‌اند و کل
  اجرا را باطل می‌کنند.
