# گزارش نهایی Charger — فقط Trans2 (Channel 2) — شماتیک قطعی

**واقعیت شاخه/کامیت (push عادی):**
- `git rev-parse --abbrev-ref HEAD` → `arena/01a0aaca-changeover`
- `git rev-parse HEAD` → `e4611473a360d307d1e89e6978574b2fb896194c`
- `git log -1 --oneline --decorate` → `e461147 (grafted, HEAD -> arena/01a0aaca-changeover) Merge branch 'arena/01a0a923-changeover'`
- `git status --short --branch`:
```
## arena/01a0aaca-changeover
 M CubeIDE/Core/Src/main.c
 M CubeIDE/CubeIDE.ioc
 M CubeIDE/STM32CubeIDE/.cproject
 M CubeMX/CubeIDE.ioc
 M Firmware/Bsp/Inc/bsp_exti.h
 M Firmware/Bsp/Inc/bsp_pwm.h
 M Firmware/Bsp/Src/bsp_exti.c
 M Firmware/Bsp/Src/bsp_pwm.c
 M Firmware/Config/Inc/board_pins.h
 M Firmware/Config/Inc/modules_enable.h
 M Firmware/Modules/Changeover/README.md
 M Firmware/Modules/Changeover/changeover.c
 M Firmware/Modules/Changeover/changeover.h
 M Firmware/Modules/Charger/README.md
 M Firmware/Modules/Charger/charger.c
 M Firmware/Modules/Charger/charger.h
 M Firmware/Modules/Fault/README.md
 M Firmware/Modules/Ui/README.md
 M Firmware/Modules/Ui/UI_Board_Validation.xlsx
 M Firmware/Modules/Ui/host_test_ui.py
 M Firmware/Modules/Ui/ui_led.c
 M Firmware/Modules/Ui/ui_led.h
 M Firmware/Rtos/Src/rtos_app.c
 M Firmware/Rtos/Src/task_control.c
 M Firmware/Rtos/Src/task_ui.c
 M tools/check_ai_rules.sh
 M tools/check_firmware_syntax.sh
?? Charger_PreCoding_Report.md
?? DOC/
?? Firmware/Modules/Changeover/Changeover_Board_Validation.xlsx
?? Firmware/Modules/Charger/host_test_charger.py
?? Firmware/Modules/McuPowerPath/
?? Firmware/Modules/Measurement/Measurement_Board_Validation.xlsx
?? Firmware/Modules/Ui/Buzzer_Board_Validation.xlsx
?? Firmware/Modules/Ui/RTOS_Mapping_Report.md
?? McuPowerPath_Implementation_Report_2026-09-17.md
```
- `git diff --check` → clean

---

## 1) تست host — Host Test (43 تست در همین branch)

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

- اعلام 43 = خروجی 43 PASS
- فقط Trans2 فعال (`CHG_INSTALLED_CHANNEL_MASK 0x02` = PWM2 PA6, Current2 PA7, JIT2 PB6, OUT_12_Charger), Trans1 غیرفعال و هیچ `SetPwmBoth` فراخوانی نمی‌شود (فقط `SetPwmChannel CH2`)
- per-channel: duty/Bulk/Absorb/Float/Tail/Fault مستقل, V_BAT_HIGH=MID-V24 برای Trans2, V_BAT_LOW جداگانه
- relay NC: Open `PWM0→Relay true (NC باز)`, Close `Relay false (NC بسته)→settle 100ms→PWM`, Read فقط coil
- early JIT return حذف شد → retry نصف سپس 10% واقعاً اجرا
- battery missing جداگانه V_BAT_LOW/V_BAT_HIGH, CHG_TRANSFORMER_KNOWN 0 safe-off

## 2) تست syntax — Syntax

`bash tools/check_firmware_syntax.sh`:

```
HOST SYNTAX CHECK PASSED / بررسی syntax سمت Host موفق بود
```

`check_ai_rules` → `OK: ui.c / ui_led.c has non-linear formula broken into steps (range, offset, scaled)
  OK: ui.c / ui_led.c charging has non-linear steps (remainingPercent, periodPerPercent)
  OK: No linear one-line formula (all broken into steps)

ALL CHECKS PASSED`

## 3) تست‌های انجام‌نشدهٔ برد — Board NOT Done

- فقط Trans2 مونتاژ (`0x02`), Trans1 خاموش; پس از مونتاژ دوم mask `0x03`
- باتری 2×12V 4.5Ah/20HR 12AP-4.5 سری `GND/MID/+24V` — دیتاشیت ZICO نیست → 14400/13500 per-channel provisional, بدون equalization
- Trans2 فقط `V_BAT_HIGH` را کنترل (MID-V24) + `Current2` (PA7) + `JIT2` (PB6)
- اولین تست: بدون باتری, منبع محدودکننده, بار `150Ω 10W` (~100mA @12V), PWM `1%→5%→10%`
- اسکوپ: `50kHz` PWM2 PA6 gate, shunt primary, LM393 polarity (RISING موقت, اگر falling هر دو به FALLING), JIT→PWM0 latency, relay NC باز/بسته (continuity)
- `Charger_Init` (و `Jitter_Init` اگر استفاده) قبل اولین `Evaluate` — در `rtos_app` تضمین
- اتصال باتری تا تأیید شکل موج و رله ممنوع — تست host مجوز نیست

## 4) safe-off configs — هنوز unknown

- `CHG_TRANSFORMER_KNOWN=0` (Lp/NpNs/Ipeak/η) → `IsConfigValid false` → PWM safe-off — skeleton, نه پیاده‌سازی کامل
- `MODULE_CHARGER 0`/`MODULE_JITTER 0` تا safe path و تست برد
- `CHG_BALANCE_INDEPENDENT_PATH 0` → فقط monitor, active فقط پس از net/continuity
- `V_BAT` missing جداگانه, `SCALE_DEN 0`, `ABSBORB_* 0` → safe-off
- `PB5/Q1` هرگز در Charger/JIT, `PB11/Q17` تغییر نه

## جزئیات 10 بند Trans2

1. **Trans2 CH2**: PWM2 PA6, Current2 PA7, JIT2 PB6, OUT_12 Charger — `MASK 0x02`
2. **CH1 غیرفعال**: PWM1/Current1/JIT1 صفر, هیچ `SetPwmBoth` (فقط `SetPwmChannel CH2`)
3. **per-channel**: duty/Bulk/Absorb/Float/Tail/Fault مستقل هر کانال
4. **V_BAT**: `V_BAT_LOW=MID-GND`, `V_BAT_HIGH=V24-MID` جدا; Trans2 فقط HIGH را کنترل
5. **Init**: `Charger_Init` (+Jitter_Init) قبل `Evaluate` — در `func__App_Start`/`rtos_app`
6. **Relay NC**: `Open: PWM0→Relay true (NC باز)`, `Close: Relay false (NC بسته)→settle 100ms→PWM`, `Read` فقط coil, continuity board-test
7. **Early JIT**: `if (FAULT&JIT) return;` قبل `switch(FAULT)` حذف شد → retry نصف سپس 10% اجرا
8. **Battery missing**: `V_BAT_LOW` و `V_BAT_HIGH` جداگانه `<2000` چک
9. **CHG_TRANSFORMER_KNOWN 0**: safe-off تا کالیبراسیون η/ترانس
10. **First test**: فقط Trans2 + بار 150Ω + ورودی محدود + PWM 1% → اتصال باتری ممنوع تا تأیید

**PWM:** `72M/0/1439=50kHz 1440 0.0694%` — هر دو .ioc و main.c — `TripOffFromIsr` کوتاه بدون RTOS, `OnJitTrip` فقط volatile.
