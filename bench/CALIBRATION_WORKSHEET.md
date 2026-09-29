/**
 * @file    bench/CALIBRATION_WORKSHEET.md
 * @brief   [EN] Bench worksheet: exactly what to record so every scale can be rebuilt from scratch.
 *          [FA] برگهٔ کار بنچ: دقیقاً چه چیزی ثبت شود تا هر مقیاس از پایه بازساخته شود.
 */

# Calibration worksheet / برگهٔ کالیبراسیون

---

## 0. Before anything / قبل از هر کاری

> **[EN] FLASH BOTH SIDES. The STM32 telemetry frame grew from 84 to 104 bytes
> (five new raw-count columns). The ESP drops any frame whose length does not
> match, and there is NO version handshake - so if you flash only one side the
> panel simply goes blank with no error at all.**
>
> **[FA] هر دو طرف را فلش کنید. فریم تله‌متری از ۸۴ به ۱۰۴ بایت رفت. ESP هر
> فریمی را که طولش نخواند بی‌صدا دور می‌ریزد و هیچ هندشیک نسخه‌ای هم وجود ندارد
> - پس اگر فقط یک طرف را فلش کنید، پنل بدون هیچ خطایی خالی می‌ماند.**

- [ ] **Clean + Rebuild** (not incremental - a stale `bsp.o` gives a hybrid:
      new LUT with the old divider) / **Clean + Rebuild کامل**، نه افزایشی
- [ ] Flash **STM32** / فلش STM32
- [ ] Flash **ESP** / فلش ESP
- [ ] Panel shows live data / پنل داده زنده نشان می‌دهد

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

[EN] Same procedure as the SOLO1 / SOLO2 runs. The CSV now records its own raw
counts, so the file alone is enough. / [FA] همان روش SOLO1/SOLO2. حالا خود CSV
شمارش خام را ثبت می‌کند، پس فایل به‌تنهایی کافی است.

- [ ] **Manual mode ON**, one channel at a time / مود دستی، هر بار یک کانال
- [ ] Sweep the duty in steps, settle at each step / دیوتی را پله‌پله بالا ببرید
- [ ] At every step enter in the panel's DMM form:
      **battery current (mA)** and **battery voltage (mV)** /
      در هر پله در فرم مولتی‌متر پنل: **جریان باتری** و **ولتاژ باتری**
- [ ] **SOLO1** (upper / channel 1) → download the CSV
- [ ] **SOLO2** (lower / channel 2) → download the CSV
- [ ] Send me **both CSV files**

---

## 4. Send me / برایم بفرستید

1. Section 1 - the three VDDA numbers / سه عدد مرجع
2. Sections 2a, 2b, 2c - the table above / جدول ولتاژها
3. Section 3 - **both CSV files** / هر دو فایل CSV
4. Anything that looked wrong / هرچه عجیب به نظر رسید

[EN] From that I rebuild, from first principles: the ADC reference, all three
voltage scales, and both current LUTs. / [FA] از روی این‌ها مرجع ADC، هر سه
مقیاس ولتاژ و هر دو جدول جریان را از پایه می‌سازم.

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
