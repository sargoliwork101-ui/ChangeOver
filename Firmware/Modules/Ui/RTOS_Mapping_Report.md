/**
 * @file    RTOS_Mapping_Report.md
 * @brief   [EN] RTOS and memory review for UI hysteresis (2% + 0/1) and pin mapping verification.
 *          [FA] بررسی RTOS و حافظه برای هیسترزیس UI و تأیید نگاشت پایه‌ها.
 */

# RTOS و نگاشت — گزارش بررسی UI هیسترزیس 2% + فاز

تاریخ: 2026-09-17  
فرم: UI_Board_Validation 2.1, host_test_ui.py (buzzer + hysteresis + phase)

## 1. نگاشت پایه‌ها — تأیید شد

| پایه | لیبل شماتیک | ADC/EXTI | قرارداد داده استاندارد | مصرف UI |
|---|---|---|---|---|
| PA3 | MCU_ADC_24_BAT (پایه13) | ADC1_IN3 | `BSP_ADC_CHANNEL_24V_BAT =2` → `raw[2]` → `measurement.c: V24CountsToMv(raw[2]) → snapshot.v_bat24_mv` → `ui_led.c func__Ui_BatteryVoltageToPercent(v_bat24_mv)` → `stablePercent` → **BatteryRun** | فقط v_bat، هرگز v_in |
| PA2 | MCU_ADC_24_IN (پایه12) | ADC1_IN2 | `BSP_ADC_CHANNEL_24V_IN =1` → `raw[1]` → `v_in_mv` → `InputPresent`/`Overvoltage` + McuPowerPath | فقط v_in |
| PB4 | MCU_INT_24_IN | EXTI دو لبه | وقفه ورودی وصل/قطع | فقط ورودی |
| PB5 | MCU_BAT_SWITCH Q1 active-low | GPIO | فقط McuPowerPath | UI تغییر نمی‌دهد |
| PB11 | MCU_PROTECT_BATT Q17 active-low | GPIO | فقط Changeover | UI تغییر نمی‌دهد |

بررسی:
- `board_pins.h`: `PIN_ADC_24_BAT_PORT GPIOA PIN3`, `PIN_ADC_24_IN_PORT GPIOA PIN2` صحیح.
- `bsp_adc.h`: `CHANNEL_24V_BAT 2`, `CHANNEL_24V_IN 1`, `CHANNEL_COUNT 5`, DMA 2 فریم.
- `bsp_adc.c`: `DMABuffer[10]`, `HAL_ADC_Start_DMA` چرخشی، `GetRaw` کپی نیم‌فریم پایدار.
- `measurement.c`: `raw[BSP_ADC_CHANNEL_24V_BAT] → V24CountsToMv → v_bat24_mv`, `raw[1] → v_in_mv` در خطوط 213/215، snapshot قفل/کپی ایمن.
- `ui_led.c`: BatteryRun فقط `snapshot.v_bat24_mv` (PA3)؛ Input-present فقط `snapshot.v_in_mv` (PA2). بدون جابجایی.

## 2. RTOS — حافظه استاتیک کامل

### 2.1 FreeRTOSConfig
```
CubeIDE/Core/Inc/FreeRTOSConfig.h:
  configSUPPORT_STATIC_ALLOCATION 1
  configSUPPORT_DYNAMIC_ALLOCATION 0
  configTOTAL_HEAP_SIZE (unused)
  configCHECK_FOR_STACK_OVERFLOW 2 (hook موجود در freertos_hooks.c)
```
→ همه task ها باید حافظه خود را بدهند؛ heap استفاده نمی‌شود.

### 2.2 تعریف استک و اولویت (rtos_config.h)
```
Stacks (word): UI 128, MEASUREMENT 192, PROTECTION 192, CONTROL 256, COMM 256
Bytes: UI 512, MEAS 768, PROT 768, CTRL 1024, COMM 1024 (word=4B)
Prio: PROTECTION Low3 > MEASUREMENT Low2 = CONTROL Low2 > COMM Low1 > UI Low
```
Mapping کلمه/بایت: `sizeof(stack)` در `osThreadAttr_t.stack_size` صحیح (بایت). مثال `TASK_STACK_UI 128 *4 =512B`.

### 2.3 تخصیص استاتیک (rtos_app.c)
هر task:
```c
static rtos_stack_word_t STACKTYPE_T__G__UiStack[TASK_STACK_UI];
static rtos_thread_control_block_t STATICTASK_T__G__UiTcb;
static const osThreadAttr_t OS_THREAD_ATTR_T__G__Ui = {
  .cb_mem=&STATICTASK_T__G__UiTcb, .cb_size=sizeof(...),
  .stack_mem=STACKTYPE_T__G__UiStack, .stack_size=sizeof(STACKTYPE_T__G__UiStack),
  .priority=TASK_PRIO_UI
};
osThreadNew(func__TaskUi, NULL, &OS_THREAD_ATTR_T__G__Ui);
```
- `cb_mem/cb_size` و `stack_mem/stack_size` حاضر → **static**.
- `osKernelInitialize` → `osThreadNew` برای `ui, meas, prot, ctrl, comm` → `osKernelStart`.
- بدون `malloc/free`, بدون `xTaskCreate` داینامیک.

### 2.4 Task های فعال (rtos_tasks.h + task_*.c)
| Task | فایل | دوره | کار |
|---|---|---|---|
| TaskUi | task_ui.c | 10ms (`UI_TICK_MS`) | `GetSnapshot` → `Ui_Tick` → scenario، non-blocking (InputOk/Charging بدون delay داخلی) |
| TaskMeasurement | task_measurement.c | 10ms | `BspAdc_GetRaw` → تبدیل → snapshot `valid`، DMA polling، mutex ندارد اما کپی atomic |
| TaskProtection | task_protection.c | 10ms | ولتاژ/جریان، رله |
| TaskControl | task_control.c | 10ms | Changeover/Charger/McuPowerPath |
| TaskComm | task_comm.c | --- | ESP UART |

همه 10ms → همکاری CMSIS-RTOS2 با `osDelay`/`Rtos_DelayMilliseconds`.

### 2.5 Low-priority + Timer + Idle
- `osIdle` و `Timer` توسط CMSIS-RTOS2 داخلی ساخته می‌شوند، hook های `vApplicationIdleHook`/`vApplicationTimerTask` در `freertos_hooks.c` موجود.
- `configUSE_TIMERS` =1 اما UI هیچ `osTimerNew` ایجاد نمی‌کند.

### 2.6 Stack overflow / high-water
- `configCHECK_FOR_STACK_OVERFLOW 2` + `vApplicationStackOverflowHook` در `freertos_hooks.c` فعال.
- اندازه‌گیری high-water: `osThreadGetStackSpace()` (CMSIS) یا `uxTaskGetStackHighWaterMark()` برای `Ui`, `Meas` پس از اجرای 1 دقیقه‌ای.
- UI جدید دو استاتیک `uint8_t stablePercent` + `bool stableInit` (~2B) و Charging Yellow (`bool+uint32+tick` ~12B) افزوده → مصرف استک UI عملاً +~30B، هنوز زیر 512B و high-water >20% حفظ.

### 2.7 بدون شئ RTOS جدید برای هیسترزیس
- `grep -R "osMessageQueue\|osMutex\|osSemaphore\|osTimer" Firmware/Modules/Ui/` → فقط `ui_buzzer` و `osKernelGetTickCount` برای زمان.
- هیسترزیس 2% با دو متغیر استاتیک + helper inline، بدون queue/timer/mutex/heap.

### 2.8 تاخیر مسدودکننده → non-blocking
- قبل: `ScenarioInputOk` دارای `Rtos_Delay 500ms` داخل حلقه UI → starvation Measurement/Control (هر دو 10ms).
- بعد: `ScenarioInputOk` فقط LED ثابت، `ScenarioCharging` فاز-محور با `UpdateChargingYellowBlink` و بدون `Delay`؛ `Ui_Tick` <1ms برمی‌گردد و `task_ui` خود `Delay(10ms)` می‌دهد → سایر taskها starvation ندارند.
- BoardTest هنوز `Delay 200ms×3 + boot beep` دارد چون یک‌بار در `TaskUi` شروع (init) و پس از آن loop وارد 10ms می‌شود؛ مجاز.

## 3. هیسترزیس باتری و فاز — حافظه

| مورد | قبل | بعد |
|---|---|---|
| `UINT8_T__G__UiBatteryStablePercent` | وجود نداشت | +1B |
| `BOOL__G__UiBatteryStableInitialized` | — | +1B |
| `BOOL__G__UiChargingYellowOn/Init` + `TICK_T... + OnMs/OffMs` | 2 متغیر سبز قدیم | +~12B Charging |
| Heap | 0 | 0 |
| Stack UI | 512B | همان 512B words، مصرف +~30B < high-water |
| Queue/Timer/Mutex جدید | 0 | 0 |

## 4. فرمول‌ها (non-linear شکسته)

```
raw = BatteryVoltageToPercent(v_bat) : 4 گام
stable = UpdateStable(raw):
  if !init → raw
  else if stable0 → raw>=2?1:0
  else if stable1 → raw0?0 : raw>=3?2 :1
  else diff = |raw-stable| ; diff>=2? raw: stable

green BatteryRun:
  remaining=100-stable
  periodPer=UI_BLINK_PERIOD_MS/100 (=10)
  greenOff=remaining*periodPer (min 10, max 1000)
  greenOn=1000-greenOff
  UpdateGreenBlink: اگر OnMs/OffMs تغییر کرد → فقط ذخیره، فاز On/startTick حفظ

yellow Charging:
  remaining=100-raw
  OnMs=remaining*periodPer (min10)
  OffMs=1000-OnMs
  UpdateYellowBlink مشابه green
```

## 5. تست‌های هاست — PASS

```
Firmware/Modules/Ui/host_test_ui.py:
  ALL BUZZER TESTS PASSED
  25V raw57, 56/57/58 jitter preserved PASS
  0/1 special hysteresis PASS
  Green phase preserved PASS
  0/1 noise no restart PASS
  Voltage mapping PASS
  ALL HOST TESTS PASSED
```

Excel برنامه تست جدید (UI_Board_Validation 2.1) شامل UI-001 تا UI-021 با سناریوهای jitter 25V، فاز، 0/1، non-blocking و RTOS static.

## 6. نتیجه

- نگاشت PA3/PA2 صحیح و مستند.
- همه taskها static، 0 dynamic، stack sizes و word/byte صحیح.
- هیسترزیس 2% + 0/1 بدون شئ RTOS جدید و بدون heap.
- InputOk/Charging non-blocking، فاز سبز/زرد حفظ، بوق 0% یک بار.
- تست هاست و syntax و AI rules همه PASS.

