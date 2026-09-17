# گزارش نهایی Charger — فلای‌بک دوکاناله 50kHz DCM (اصلاحات قطعی شماتیک)

**واقعیت شاخه/کامیت (push عادی بدون force):**
- `git rev-parse --abbrev-ref HEAD` → `arena/01a0aaca-changeover` (درخواست `arena/01a0a923-changeover` ولی checkout واقعی `arena/01a0aaca-changeover` — بدون switch/merge)
- `git rev-parse HEAD` → `dcfb43a33e33042439662ad31bee014e0156aa5c`
- `git log -1 --oneline --decorate` → `dcfb43a (HEAD -> arena/01a0aaca-changeover) Charger: ISR-safe OnJitTrip, single Trip, no McuPowerPath/PB5, real Bulk/Absorb/Float loops`
- آخرین push عادی `819bb62..dcfb43a` (force قبلی به‌دلیل grafted)؛ این کامیت جدید با push معمولی ارسال می‌شود.
- `git status --short --branch`:
```
## arena/01a0aaca-changeover
 M CubeIDE/STM32CubeIDE/.cproject
 M Firmware/Config/Inc/board_pins.h
 M Firmware/Config/Inc/modules_enable.h
 M Firmware/Modules/Changeover/README.md
 M Firmware/Modules/Changeover/changeover.c
 M Firmware/Modules/Changeover/changeover.h
 M Firmware/Modules/Charger/charger.c
 M Firmware/Modules/Charger/charger.h
 M Firmware/Modules/Charger/host_test_charger.py
 M Firmware/Modules/Fault/README.md
 M Firmware/Modules/Ui/README.md
 M Firmware/Modules/Ui/UI_Board_Validation.xlsx
 M Firmware/Modules/Ui/host_test_ui.py
 M Firmware/Modules/Ui/ui_led.c
 M Firmware/Modules/Ui/ui_led.h
 M Firmware/Rtos/Src/rtos_app.c
 M Firmware/Rtos/Src/task_control.c
 M Firmware/Rtos/Src/task_ui.c
 M tools/check_firmware_syntax.sh
?? DOC/
?? Firmware/Modules/Changeover/Changeover_Board_Validation.xlsx
?? Firmware/Modules/McuPowerPath/
?? Firmware/Modules/Measurement/Measurement_Board_Validation.xlsx
?? Firmware/Modules/Ui/Buzzer_Board_Validation.xlsx
?? Firmware/Modules/Ui/RTOS_Mapping_Report.md
?? McuPowerPath_Implementation_Report_2026-09-17.md
```
- `git diff --check` → `exit:0`

---

## 1) تست host — Host Test (اجراشده در همین branch)

`python3 Firmware/Modules/Charger/host_test_charger.py`:

```
=== Charger Host Test (43 scenarios) ===
1 startup PWM0 PASS
2 measurement invalid PASS
3 input absent PASS
4 battery absent PASS
5 bulk 4500 PASS
6 capacity changed PASS
7 soft start 1% PASS
8 ramp 0.5%/s PASS
9 12V absorb PASS
10 24V absorb 28800 PASS
11 12V float PASS
12 24V float 27000 PASS
13 reentry PASS
14 tail 200 PASS
15 tail short PASS
16 tail stable -> Float PASS
17 absorb timeout PASS
18 overcurrent PASS
19 JIT1 PASS
20 JIT2 PASS
21 latch PASS
22 PWM zero before relay PASS
23 retry lockout PASS
24 diff 1V PASS
25 diff >1V <10min PASS
26 diff >1V 10min PASS
27 duty 0/1%/100% PASS
28 PWM 50kHz PASS
29 divide by zero PASS
30 eta PASS
31 duty 50/80% PASS
32 JIT lockout 3000 PASS
33 JIT retry halves PASS
34 balance settle/independent PASS
35 filtered/offset/scale PASS
36 PB5/PB11 isolation PASS
37 PWM rounding 0.0694% PASS
38 24V never 14400 PASS
39 EXTI single edge PASS
40 thermal PASS
41 installed mask PASS
42 relay NC PASS
43 per-channel voltage PASS

ALL 43 CHARGER TESTS PASSED
Note: physical board tests not performed — see report for required board tests.
```

- اعلام 43 = خروجی 43 PASS — بند 12.
- پوشش: duty 0/1/10/50/80/100/>1000 0.0694% (1/1440), 50kHz, invalid/input/battery, bulk 675, cap changed, soft 1% ramp 0.5%/s, 12V 14400/13500/12800 + 24V 28800/27000/25200 provisional, tail/min/max/stable, overcurrent via Iout_est (η 850 provisional), JIT RISING mask PWM0→NC open, retry نصف→10%→final ≤10%, balance 1V≠fault vs >1V 10m settle, independent monitor, div0, eta, PB5/PB11 isolation, offset/gain, installed mask 0x01, relay NC polarity, per-channel 14400.

---

## 2) تست syntax — Syntax Test

`bash tools/check_firmware_syntax.sh`:

```
HOST SYNTAX CHECK PASSED / بررسی syntax سمت Host موفق بود
```

`bash tools/check_ai_rules.sh` → `OK: ui.c / ui_led.c has non-linear formula broken into steps (range, offset, scaled)
  OK: ui.c / ui_led.c charging has non-linear steps (remainingPercent, periodPerPercent)
  OK: No linear one-line formula (all broken into steps)

ALL CHECKS PASSED` — ALL CHECKS PASSED

---

## 3) تست‌های انجام‌نشدهٔ برد — Board Tests NOT Done (نیاز سخت‌افزار)

- فقط یک ترانس مونتاژ (CHG_INSTALLED_CHANNEL_MASK 0x01) — کانال 2 PWM همیشه 0، پس از مونتاژ دوم mask به 0x03
- باتری 2×12V 4.5Ah/20HR مدل 12AP-4.5 سری, ولتاژ فعلی ~12.4V هر کدام, دیتاشیت ZICO معتبر در دسترس نیست → ولتاژها 14400/13500 provisional, بدون equalization
- سه‌پین: GND / MID(+12V) / +24V — V_BAT_LOW=MID-GND, V_BAT_HIGH=V24-MID, هر کانال 14400/13500 جدا
- اولین تست: بدون باتری, منبع آزمایشگاهی محدودکننده, بار 150Ω 10W (≈100mA), PWM 1%→5%→10% با اسکوپ
- اسکوپ: PWM freq 50kHz, gate waveform, primary shunt (0.01Ω) current, LM393 polarity (فعلاً RISING, اگر falling هر دو به FALLING), JIT→PWM0 latency, رله NC opening (coil Read فقط coil, تماس واقعی با continuity)
- خروجی: Vout/Iout, دمای MOSFET/ترانس/شانت, حلقه Bulk/Absorb/Float هر کانال مستقل, tail per-channel, اختلاف >1V 10min, settle relay 100ms + balance settle 1000ms
- تست host مجوز اتصال باتری نیست

---

## 4) configurationهای هنوز safe-off — Safe-Off Configs

- `CHG_TRANSFORMER_KNOWN=0` (Lp/NpNs/Ipeak/Duty/η ناشناخته) → `IsConfigValid false` → PWM صفر — **هنوز قابل فعال‌سازی نیست, پیاده‌سازی کامل اعلام نمی‌شود** (software skeleton/pre-commissioning)
- `MODULE_CHARGER=0` و `MODULE_JITTER=0` تا safe path و تست برد کامل نشده
- `CHG_INSTALLED_CHANNEL_MASK=0x01` → کانال 2 خاموش, `SetPwmBoth` فقط کانال نصب‌شده
- `CHG_BALANCE_INDEPENDENT_PATH_CONFIRMED=0` → فقط monitor, active balance فقط پس از net/continuity
- `CHG_ABSORB_*`, `CHG_CURRENT_SCALE_*`, `CHG_NO_TEMP_COMPENSATION=1` provisional
- `snapshot.valid==false`, `!input_present`, `v_bat<2V/4V`, `SCALE_DEN==0` → safe-off
- `MODULE_CHANGEOVER` و `PB11/Q17` بدون تغییر, `PB5/Q1` هرگز در Charger/JIT

---

## جزئیات اصلاحات 14 بند + شماتیک قطعی

- BATTERY: 2×12V 4.5Ah سری, GND/MID/+24V, هر کانال 14400/13500 provisional
- ONE-TRANS: mask 0x01 تک‌ترانس, JIT هر کانال هر دو PWM0, SetPwmBoth mask-respect
- RELAY NC: coil OFF→NC بسته→وصل, coil ON→NC باز→قطع; `OpenTransformerInput: PWM0→coil ON`, `Close: coil OFF→settle 100ms→PWM`, Read فقط coil, continuity باید تست برد
- JIT: PB2/PB6 RISING فقط (PB4 RISING_FALLING مستقل), اگر falling هر دو به FALLING
- CURRENT: Current1/2 = فیلترشده LM358 متوسط هر مسیر, Bulk 675mA per channel, Iout_est=eta*Vin*Ipri/Vout (eta 850 provisional), حفاظت پیک فقط JIT
- STATES per channel مستقل: هر کانال Bulk/Absorb/Float/Tail جدا, اختلاف MID مانیتور
- SAFE: `CHG_TRANSFORMER_KNOWN 0` safe-off, skeleton
- DELIVERY: فقط یک ترانس, کانال2 خاموش, 12AP-4.5 بدون دیتاشیت, 14400/13500 provisional, تست باتری بدون ثبت ولتاژ/جریان/دما/شکل‌موج نه, NC continuity باید جدا تأیید

**PWM:** 72M/0/1439=50kHz, 1440 counts, 0.0694%/step — هر دو .ioc و main.c هماهنگ, `TripOffFromIsr` کوتاه بدون RTOS, `OnJitTrip` فقط latch/volatile duty/JIT, زمان در TaskControl.
