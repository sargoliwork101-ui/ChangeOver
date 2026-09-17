/**
 * @file    RTOS_Mapping_Report.md
 * @brief   [EN] RTOS and memory review for UI hysteresis (BatteryRun 2%+0/1, Charging 5%, Full 100/95) and pin mapping.
 *          [FA] بررسی RTOS و حافظه برای هیسترزیس UI و نگاشت پایه‌ها.
 */

# RTOS و نگاشت — گزارش بررسی UI 2.2

تاریخ: 2026-09-17 — فرم UI_Board_Validation 2.2, host_test_ui.py (BatteryRun2%+Charging5%+Full100/95)

## 1. نگاشت پایه‌ها — تأیید شد (بدون تغییر)

| پایه | لیبل | ADC/EXTI | قرارداد داده | مصرف UI |
|---|---|---|---|---|
| PA3 | MCU_ADC_24_BAT پایه13 | ADC1_IN3 | `BSP_ADC_CHANNEL_24V_BAT 2` → `raw[2]` → `measurement.c: V24CountsToMv(raw[2])→v_bat24_mv` → `BatteryRun/Charging` | فقط v_bat |
| PA2 | MCU_ADC_24_IN پایه12 | ADC1_IN2 | `BSP_ADC_CHANNEL_24V_IN 1` → `raw[1]` → `v_in_mv` → Input/Overvoltage | فقط v_in |
| PB4 | MCU_INT_24_IN | EXTI دو لبه | وقفه حضور/قطع ورودی | فقط ورودی |
| PB5 | MCU_BAT_SWITCH Q1 active-low | GPIO | فقط McuPowerPath | UI دست نمی‌زند |
| PB11 | MCU_PROTECT_BATT Q17 active-low | GPIO | فقط Changeover | UI دست نمی‌زند |

- `board_pins.h`: `PIN_ADC_24_BAT PA3`, `PIN_ADC_24_IN PA2` صحیح.
- `bsp_adc.h`: `CHANNEL_24V_BAT 2`, `_24V_IN 1`, `COUNT 5`, DMA 2-frame.
- `bsp_measurement.c` GetRaw کپی نیم‌فریم پایدار.
- `measurement.c:213 raw[1]→v_in`, `215 raw[2]→v_bat24` snapshot قفل/کپی ایمن.
- `ui_led.c`: BatteryRun/Charging فقط `v_bat24 PA3`، Input فقط `v_in PA2`.

## 2. RTOS — حافظه استاتیک کامل

### FreeRTOSConfig
```
CubeIDE/Core/Inc/FreeRTOSConfig.h:
  SUPPORT_STATIC 1
  SUPPORT_DYNAMIC 0
  CHECK_FOR_STACK_OVERFLOW 2 (hook in freertos_hooks.c)
```

### تعریف استک و اولویت (rtos_config.h)
```
Stacks word: UI 128 (512B), MEASUREMENT 192 (768B), PROTECTION 192, CONTROL 256 (1024B), COMM 256
Prio: PROTECTION Low3 > MEASUREMENT Low2 = CONTROL Low2 > COMM Low1 > UI Low
Mapping word→byte: sizeof(stack) bytes in osThreadAttr_t (مثال UI 128*4=512)
```

### تخصیص استاتیک (rtos_app.c)
```c
static rtos_stack_word_t STACKTYPE_T__G__UiStack[TASK_STACK_UI];
static rtos_thread_control_block_t STATICTASK_T__G__UiTcb;
static const osThreadAttr_t OS_THREAD_ATTR_T__G__Ui = {
  .cb_mem=&STATICTASK_T__G__UiTcb, .cb_size=sizeof(...),
  .stack_mem=STACKTYPE_T__G__UiStack, .stack_size=sizeof(STACKTYPE_T__G__UiStack),
  .priority=TASK_PRIO_UI
};
osThreadNew(func__TaskUi,NULL,&OS_THREAD_ATTR_T__G__Ui);
```
همه `cb_mem/cb_size` + `stack_mem/stack_size` حاضر → **static**. بدون malloc/free, بدون xTaskCreate داینامیک.

### Taskها
| Task | فایل | دوره | کار |
|---|---|---|---|
| TaskUi | task_ui.c | 10ms `UI_TICK_MS` | `GetSnapshot→Ui_Tick` non-blocking (InputOk/Charging بدون delay) |
| TaskMeasurement | task_measurement.c | 10ms | `BspAdc_GetRaw→convert→snapshot valid` DMA polling |
| TaskProtection | task_protection.c | 10ms | رله/حفاظت |
| TaskControl | task_control.c | 10ms | Changeover/Charger/McuPowerPath |
| TaskComm | task_comm.c | — | ESP UART |

همه 10ms با `osDelay/Rtos_DelayMilliseconds` همکاری.

### Stack overflow / high-water
- `configCHECK_FOR_STACK_OVERFLOW 2` + `vApplicationStackOverflowHook` فعال.
- اندازه‌گیری: `osThreadGetStackSpace()` یا `uxTaskGetStackHighWaterMark()` پس از 60s اجرا.
- UI جدید: `stablePercent BatteryRun` 2B + `chargingStable 2B` + `fullActive 1B` + yellow `bool+tick+OnMs/OffMs` ~16B → `+~30B` روی 512B، high-water >20% حفظ (UI 128w, باقی‌مانده ~90w تخمین).
- Measurement/Control بدون تغییر.

### بدون شئ RTOS جدید برای hysteresis
`grep -R "osMessageQueue|osMutex|osSemaphore|osTimer" Firmware/Modules/Ui/` → فقط `osKernelGetTickCount` برای زمان. هیسترزیس با متغیرهای استاتیک (`BatteryRun stable`, `Charging stable`, `FullActive`)، بدون queue/timer/mutex/heap.

### تاخیر مسدودکننده رفع شد
- قبل: `ScenarioInputOk` delay 500ms + `Charging` delay 1000ms → starvation Measurement/Control (10ms).
- بعد: هر دو فاز-محور بدون delay داخل سناریو؛ `Ui_Tick <1ms`، `task_ui Delay(10ms)` → سایر taskها starvation ندارند. BoardTest فقط `200ms×3` یک‌بار در init.

## 3. هیسترزیس و فاز — حافظه

| مورد | قبل 2.1 | بعد 2.2 |
|---|---|---|
| `BatteryRun stable` (2%+0/1) | `stablePercent` 1B + init 1B | همان |
| `Charging stable` (5%) | — | `chargingStable 1B + init 1B` + helper `|raw-stable|>=5` |
| `Full hysteresis` (100/95) | ساده `<100` | `fullActive bool 1B` + hysteresis 100/95 |
| `Yellow blink` | `YellowOn/Init/tick/OnMs/OffMs` ~12B | همان + stable 5% |
| `Green blink` | `GreenOn/Init/tick/OnMs/OffMs` ~12B | همان |
| Heap | 0 | 0 |
| Stack UI | 512B | همان words، مصرف +~30B < high-water |
| Queue/Timer/Mutex جدید | 0 | 0 |

## 4. فرمول‌ها (non-linear شکسته)

```
BatteryRun raw = Percent(v_bat) 4 گام
  stable BatteryRun:
    !init→raw
    stable0: raw>=2?1:0
    stable1: raw0?0 : raw>=3?2 :1
    else diff=|raw-stable| def hysteresis RUN 2 → raw:stable
  green: remaining=100-stable, per=10, Off=remaining*per(min10) On=1000-Off, preserve phase
  buzzer bands with stable (<1,<10,<20,<40)

Charging raw = Percent(v_bat)
  stable Charging: !init→raw else diff=|raw-stable| >=5?raw:stable (53..61 keep when stable57)
  yellow: remaining=100-stable5, per=10, On=remaining*per(min10) Off=1000-On preserve phase

Full hysteresis (raw):
  fullActive false: raw>=100 → true (InputOk)
  fullActive true: raw<95 → false (Charging) else hold
```

## 5. تست‌های هاست — PASS

```
host_test_ui.py:
  ALL BUZZER PASSED
  BatteryRun 2% 56/57/58 jitter preserved PASS (57+56/58 keep, 55/59 change)
  0/1 special PASS (0→raw>=2→1, 1→0/2)
  Green phase preserved PASS
  0/1 noise no restart PASS
  Charging 5% 53..61 keep PASS, outside 5 change PASS
  Charging yellow phase preserved PASS
  Full 100/95 hysteresis PASS (99 Charging, 100 InputOk, 99/95 stay, 94 Charging)
  Voltage mapping PASS
  ALL HOST TESTS PASSED
```

Excel برنامه تست 2.2: UI-001 تا UI-028 (BatteryRun jitter, Charging 5% 53..61, Full 100/95, phase, non-blocking, RTOS static).

## 6. نتیجه

- نگاشت PA3/PA2 صحیح.
- همه taskها static, DYNAMIC 0, word/byte صحیح.
- هیسترزیس BatteryRun 2%+0/1، Charging 5%، Full 100/95 بدون شئ RTOS جدید و بدون heap.
- هر دو چشمک preserve phase، InputOk/Charging non-blocking، 0% یک 10s، 1% مستقل.
- Changeover/McuPowerPath/PB5/PB11/State.xlsx بدون تغییر.

