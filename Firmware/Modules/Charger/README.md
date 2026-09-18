/**
 * @file    README.md
 * @brief   [EN] Charger for 2-channel flyback, DCM, 50kHz, safe state machine.
 *          [FA] شارژر فلای‌بک دوکاناله، DCM، 50kHz، ماشین حالت ایمن.
 */

# ماژول Charger

## وضعیت

`MODULE_CHARGER = 0` (پیش‌فرض safe). Backend PWM (`.ioc` TIM2/TIM3) فعال اما خروجی‌ها در startup صفر و متوقف. `MODULE_JITTER = 0` تا ISR تریپ آماده شود؛ `MODULE_PROTECTION = 0` باقی می‌ماند. تمام آستانه‌ها provisional تا تأیید دیتاشیت باتری و پارامترهای ترانس (`Lp`, `Np/Ns`, `Ipeak_max`, `η`). تا وقتی `func__Charger_IsConfigValid()==false` یا `snapshot.valid==false` یا `input/battery` نامعتبر، PWM صفر و `safe-off` می‌ماند. `PB5/Q1` (Power-Path) و `PB11/Q17` (Changeover) در مالکیت Charger نیست و هرگز تغییر نمی‌کند؛ فقط `PWM1/PA0`, `PWM2/PA6`, `JIT1/PB2`, `JIT2/PB6`, `PB7 Relay` (ثانویه) کنترل می‌شود. رله فقط پس از صفر PWM باز می‌شود.

PWM 50kHz: `72MHz/(0+1)/(1439+1)=50000Hz`, رزولوشن 1440 (0.0694%/step), 1%≈14-15 count. دلیل تغییر از `Period 999` فرکانس مدار است، نه رزولوشن (999 نیز 1000 سطح داشت). تبدیل duty با rounding و جلوگیری overflow تست شده: `0`, `10 permille`, `1000 permille`, overflow clamp, `100%→1439` با `+500` rounding.

DCM: فرمول CCM `Iout=Ipri×Np/Ns×(1-D)/D` استفاده نمی‌شود. تخمین `Iout_est=η·Vin·Ipri_avg/Vout` فقط با `Vin/Vout≠0`, `Ipri_avg` واقعی (نه پیک), `η` کالیبراسیون و فقط برای کنترل/تخمین، نه حفاظت پیک. نسبت دور برای `Ipeak`, اشباع هسته, `demag` زمان, مرز DCM/CCM و تنش MOSFET/دیود همچنان لازم و تا تأیید باز است.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-17 | پیاده‌سازی ایمن 50kHz (PSC0 ARR1439 در main.c و هر دو `.ioc`), DCM, ماشین حالت 11 حالت (IDLE/PRECHECK/PRECHARGE/SOFTSTART/BULK/ABSORB/FLOAT/REST/FAULT/SUSPEND/BALANCE), `IsConfigValid` provisional, `DutyPermilleToCounts` با rounding, `RawToIpri` با offset/gain, `EstimateIoutDcm` با چک صفر, `OnJitTrip` + `BspPwm_TripOffFromIsr` ISR-safe, `30` تست هاست، مستندات provisional و لیست تست برد. |
| 2026-09-14 | درخت اتصال فایل‌ها |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها |
| 2026-09 | اسکلت `Charger_Init/Evaluate` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `charger.h` / `charger.c` | سیاست duty, 11 حالت, hysteresis, tail, fault latch, retry 3, balance |
| `host_test_charger.py` | تست هاست 30 سناریو (startup, invalid, bulk 675mA, duty 0/1%/100%, 50kHz, DCM, tail, JIT, retry, balance 1V/10min) |
| `../../Bsp/Inc/bsp_pwm.h` / `../../Bsp/Src/bsp_pwm.c` | `SetDutyPermille`, `StopAll`, `TripOffFromIsr` ISR-safe, `GetFrequencyHz` |
| `../../Bsp/Src/bsp_exti.c` | JIT PB2/PB6 → `TripOffFromIsr` + `OnJitTrip` بدون RTOS, جلوگیری EXTI storm |
| `../../Rtos/Src/task_control.c` | تسک مشترک `Charger_Evaluate(&snap,state)` روی snapshot معتبر |
| `../../Config/Inc/app_config.h` | `pwm_max_duty_permille` سراسری 0 نباید Charger را همیشه صفر کند — حد مستقل در `charger.h` |
| `../../Config/Inc/modules_enable.h` | `MODULE_CHARGER 0` تا safe init, `JITTER 0` تا ISR آماده |

## توابع

| نام | کار | فایل |
|---|---:|---|
| `func__Charger_Init` | PWM 0%, state IDLE, fault clear, relay safe | `charger.c` |
| `func__Charger_Evaluate` | DCM state machine از `snapshot` و `app_state`؛ safe-off اگر invalid/input/battery/config نامعتبر | `charger.c` |
| `func__Charger_IsConfigValid` | بررسی `transformer known`, `capacity`, `bulk` با uint64, ولتاژها, زمان‌های Absorb, scale | `charger.c` |
| `func__Charger_DutyPermilleToCounts` | پرمیل 0..1000 → 0..1439 با rounding `+500`, clamp, `100%→1439` | `charger.c` |
| `func__Charger_RawToIpriMa` | `max(0,(raw-offset)*scale)` متوسط اولیه، نه پیک | `charger.c` |
| `func__Charger_EstimateIoutDcm` | `η·Vin·Ipri_avg/Vout` با چک صفر و η کالیبراسیون | `charger.c` |
| `func__Charger_OnJitTrip` | latch JIT fault, `TripOffFromIsr`, FAULT | `charger.c` |
| `func__Charger_GetState` / `GetFault` | برای تست | `charger.c` |
| `func__BspPwm_TripOffFromIsr` | ISR-safe: صفر CCR, قطع CCxE, بدون HAL/RTOS | `bsp_pwm.c` |
| `func__BspPwm_GetFrequencyHz` | `72M/(PSC+1)/(ARR+1)` → 50000 | `bsp_pwm.c` |

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی | مالک |
|---|---|---|---:|---|
| PA0 | `MCU_PWM1` | TIM2_CH1 شارژر 1 | duty | Charger |
| PA6 | `MCU_PWM2` | TIM3_CH1 شارژر 2 | duty | Charger |
| PA1 | `ADC_CURRENT1` | شانت 0.01Ω LM358 کانال1 | — | Measurement→Charger |
| PA7 | `ADC_CURRENT2` | شانت 0.01Ω LM358 کانال2 | — | Measurement→Charger |
| PB2 | `MCU_JITTER1` | LM393 تریپ پیک کانال1 EXTI | High تریپ | Charger ISR |
| PB6 | `MCU_JITTER2` | LM393 تریپ پیک کانال2 EXTI | High تریپ | Charger ISR |
| PB7 | `MCU_PROTECT_CHARGER` | رله ثانویه | High وصل (ActiveHigh 1) | Charger (فقط پس از PWM0) |
| PB5 | `MCU_BAT_SWITCH` Q1 | مسیر تغذیه MCU | Low وصل (ActiveLow) | **Power-Path** — Charger ممنوع |
| PB11 | `MCU_PROTECT_BATT` Q17 | Changeover | Low وصل | **Changeover** — Charger ممنوع |

قطبیت از `board_pins.h`. Charger هرگز `PB5` یا `PB11` را تغییر نمی‌دهد.

## پیش‌فرض امن

بعد از `Init` هر دو کانال 0%, رله safe (low), state `IDLE`, fault `NONE`. بدون `snapshot.valid` یا `input_present` یا `battery present` یا `IsConfigValid`، PWM صفر می‌ماند. JIT/overcurrent بحرانی latch و PWM فوری صفر via `TripOffFromIsr` (بدون `osDelay`/queue/mutex/malloc/logging/blocking), بدون auto-retry تا cooldown/شرایط مجاز؛ پس از 3 تلاش lockout و نیاز به manual/ESP reset. Absorption زمان‌دار (min/max) و tail `200mA` با پایداری `5min`؛ بدون زمان‌های تأییدشده safe-off. Float `12V 13500` / `24V 27000` provisional و reentry `12800/25200` تا دیتاشیت. بدون سنسور دما ادعای temp-comp نمی‌شود؛ قطع سنسور → safe.

## درخت اتصال

```text
CubeIDE/Core/Src/main.c → MX_TIM2/3_Init PSC0 ARR1439 (50kHz)
CubeIDE.ioc / CubeMX.ioc → TIM2/3 PSC0 Period1439
rtos_app.c → TaskControl → task_control.c
  func__Measurement_GetSnapshot(&snap) + func__Fault_Get() + app_state
    → func__Charger_Evaluate(&snap, state)
      ├─ !valid / !IsConfigValid / !input / !battery → SafeOff PWM0 + relay
      ├─ JIT1/2 EXTI (PB2/PB6) → bsp_exti.c HAL_GPIO_EXTI_Callback → BspPwm_TripOffFromIsr + Charger_OnJitTrip latch FAULT
      ├─ SoftStart 1% (10 permille) ramp 0.5%/s (5 permille/s) + current limit
      ├─ Bulk 0.15C 675mA (uint64) → Absorb (12V 14400 / 24V 28800) → Tail 200mA + 5min stable + Min/Max → Float (13500/27000) → Reentry (12800/25200) → Bulk
      ├─ Bulk 30min work / 5min rest only Bulk
      ├─ Overcurrent → duty-10 or FAULT
      ├─ Balance diff >1V for 10min → BALANCE fault
      └─ Fault retry 3 then lockout
      └── bsp_pwm.c → TIM2/3 CCR (DutyPermilleToCounts) + TripOffFromIsr (ISR-safe)
```

نیازمند تست برد (با اسکوپ/آمپرمتر/ترمومتر): فرکانس واقعی 50kHz, duty/گیت, rise/fall/ringing, Ipeak/Iavg/Iout, η, دما, اشباع, LM393/EXTI storm, زمان trip تا PWM0, تحمل رله, CV, Bulk/Absorption, سری 12V, قطع/وصل ورودی, اتصال معکوس/کوتاه, offset/gain ADC.
