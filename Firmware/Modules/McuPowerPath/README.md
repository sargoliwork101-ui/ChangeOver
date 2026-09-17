/**
 * @file    README.md
 * @brief   [EN] McuPowerPath module sheet: Q1 (PB5) self-supply with hysteresis 22000/21500, independent from Changeover Q17 (PB11).
 *          [FA] برگه ماژول McuPowerPath: تغذیهٔ MCU با Q1 (PB5) با هیسترزیس 22000/21500، مستقل از Changeover Q17 (PB11).
 */

# ماژول McuPowerPath

## وضعیت

پیاده‌سازی مستقل مسیر تغذیهٔ MCU با ترانزیستور Q1 روی PB5 (BSP_GPIO_BATTERY_SWITCH) فعال-Low؛ PB5 Low = باتری وصل، High = قطع. `MODULE_MCU_POWER_PATH=1` است و بدون تسک اختصاصی، فقط از تسک کنترل حدود 10ms اجرا می‌شود. PB11/Q17 کاملاً در اختیار Changeover می‌ماند و هیچ منطق Q1 به `changeover.c` یا `protection.c` اضافه نشده؛ `MODULE_PROTECTION=0` باقی می‌ماند.

**هیسترزیس ورودی DC (فقط v_in، نه v_bat):**

- `MCU_POWER_INPUT_QUALIFY_MV = 22000u` : ورودی برای احراز 5 ثانیه باید `>=22000mV` بماند تا `PB5 High` (قطع) شود.
- `MCU_POWER_INPUT_RECONNECT_MV = 21500u` : اگر Q1 قبلاً قطع شده و `v_in < 21500mV` شود، باتری دوباره وصل (`PB5 Low`) و تایمر لغو می‌شود.
- `MCU_POWER_INPUT_HYSTERESIS_MV = 500u` : ناحیه مرده `21500..21999mV` وضعیت فعلی Q1 را حفظ می‌کند (21.9V به‌تنهایی reconnect نمی‌کند).
- `MCU_POWER_INPUT_STABLE_MS = 5000u` : زمان پیوسته لازم برای قطع پس از احراز.

تغییر ولتاژ باتری هنگام ورودی معتبر تصمیم Q1 را عوض نمی‌کند. ورودی از طریق `BSP_GPIO_INPUT_24V_PRESENT` (PB4) با وقفه دو لبه: rising = ورودی وصل، falling = ورودی قطع. در وقفه PB4، اگر PB4 قطع را نشان داد فوراً PB5 Low و تایمر pending لغو می‌شود، سپس رویداد `BSP_EXTI_INPUT_DETECT` ثبت می‌گردد. فقط GPIO و پرچم volatile در ISR، بدون صف، mutex، تایمر RTOS، تاخیر یا ADC. تبدیل زمان فقط با `rtos_time.h` و بدون فرض `tick=1ms`.

منطق (خلاصه هیسترزیس):
- بوت بدون ورودی DC: `PB5 Low` از `board_pins.h` و `MX_GPIO_Init` + `func__McuPowerPath_Init()` وصل می‌ماند.
- بوت با ورودی `>=22000` پیوسته 5 ثانیه → `PB5 High` (قطع)؛ اگر `21500..21999` → تایمر لغو، Q1 حفظ؛ اگر `<21500` → `PB5 Low` (وصل).
- وصل ورودی `>=22000` → شروع احراز 5 ثانیه؛ افت به `21.9V` قبل از 5 ثانیه → لغو تایمر، وصل بماند (عدم قطع).
- پس از قطع (High): `21.9V` → همچنان High (وضعیت حفظ)؛ `<21500` (مثلاً 21.4V) → فوراً Low (وصل) و لغو تایمر؛ بازگشت به `>=22V` → دوباره 5 ثانیه qualification.
- قطع ناگهانی با falling PB4 → ISR فوراً Low بدون نیاز به ADC، لغو تایمر، MCU reset نمی‌شود.
- `v_bat` تاثیری ندارد وقتی `v_in` معتبر است.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-17 | افزودن هیسترزیس `QUALIFY 22000u / RECONNECT 21500u / HYSTERESIS 500u` به `mcu_power_path.h` و بازنویسی `func__McuPowerPath_Run` برای سه باند (`>=22000` احراز 5s، `21500..21999` حفظ، `<21500` وصل)، تکمیل مستندات و فایل‌های Validation/Board_Validation، به‌روزرسانی README و گزارش. |
| 2026-09-17 | ایجاد ماژول مستقل `McuPowerPath` با `mcu_power_path.h/.c`، نگاشت PB5، `PIN_SAFE_BAT_SWITCH_HIGH=0u`، latch Low در `MX_GPIO_Init`، هوک ISR در `bsp_exti.c` با اولویت PB5 Low قبل از `BSP_EXTI_INPUT_DETECT`، `MODULE_MCU_POWER_PATH=1`، `func__McuPowerPath_Run` در `task_control` حدود 10ms، 5000ms با `rtos_time.h` و بدون آستانه‌های Changeover. |
| 2026-09-17 | بازرسی فایل‌های فعلی و استخراج نگاشت سخت‌افزار و شماتیک (Q1=PB5 active-low، Q17=PB11). |

## فایل‌ها

| فایل | نقش |
|---|---|
| `mcu_power_path.h` / `mcu_power_path.c` | مالک انحصاری Q1/PB5 (BSP_GPIO_BATTERY_SWITCH) با `Init/Run/OnInputIrq`، ثابت‌های `MCU_POWER_INPUT_STABLE_MS=5000u`، `QUALIFY 22000u`، `RECONNECT 21500u`، `HYSTERESIS 500u`، فقط `rtos_time.h` و `measurement.h` (snapshot v_in)؛ ISR امن فقط GPIO و پرچم |
| `McuPowerPath_Validation.xlsx` / `McuPowerPath_Board_Validation.xlsx` | برگه اعتبارسنجی منطق و برد: نگاشت PB5/PB4/PB11، startup، ورودی 22V با 5s، هیسترزیس 21.9/21.5/<21.5، falling PB4 در ISR، بی‌اثری v_bat، regression Changeover |
| `../../Config/Inc/board_pins.h` | `PIN_SAFE_BAT_SWITCH_HIGH=0u` (Low امن برای Q1) و `PIN_BAT_SWITCH_ACTIVE_HIGH=0u`؛ PB11 مستقل و High امن |
| `../../Bsp/Src/bsp_gpio.c` | منطق وارونگی active-low برای PB5، مقداردهی امن از `PIN_SAFE_*` |
| `CubeIDE/Core/Src/main.c` | `MX_GPIO_Init()` latch `PB5 RESET Low` قبل از `HAL_GPIO_Init` و `PB11 SET High` مستقل |
| `../../Bsp/Src/bsp_exti.c` | نگاشت PB4 هر دو لبه به `BSP_EXTI_INPUT_DETECT` + هوک `func__McuPowerPath_OnInputIrq()` با ترتیب PB5 Low → لغو تایمر → ثبت رویداد |
| `../../Config/Inc/modules_enable.h` | `MODULE_MCU_POWER_PATH 1` (PROTECTION همچنان 0) |
| `../../Rtos/Src/task_control.c` | صدای دوره‌ای `func__McuPowerPath_Run()` در تسک کنترل حدود 10ms (بدون تسک جدید) |
| `../../Rtos/Src/rtos_app.c` | شرط ساخت تسک کنترل شامل `MODULE_MCU_POWER_PATH` |
| `../../../CubeIDE/STM32CubeIDE/.cproject` | افزودن `Firmware/Modules/McuPowerPath` به include های Debug/Release |

`changeover.c/.h`، آستانه‌های 21000/20800/21200 و تایمر 3000ms، و منطق PB11 به‌هیچ‌وجه تغییر نکرده؛ PB5 در Changeover ممنوع است؛ `DOC/State.xlsx` بدون تغییر.

## توابع

| نام | کار |
|---|---|
| `func__McuPowerPath_Init` | تایمر غیرفعال، `BatteryConnected=true`، `BSP_GPIO_BATTERY_SWITCH=true` (Low وصل)؛ قبل از تسک‌ها صدا زده شود |
| `func__McuPowerPath_Run` | هر ~10ms از تسک کنترل با هیسترزیس v_in: <br>• `v_in >=22000` و باتری وصل → تایمر 5s شروع/ادامه، پس از 5s `BSP_GPIO_BATTERY_SWITCH=false` (High قطع)<br>• `21500 <= v_in <22000` → لغو تایمر، حفظ Q1 (21.9V به‌تنهایی reconnect نمی‌کند)<br>• `v_in <21500` → `BSP_GPIO_BATTERY_SWITCH=true` (Low وصل) و لغو تایمر<br>• `snapshot` نامعتبر → لغو تایمر، حفظ Q1 (زمان نامعتبر شمرده نشود)<br>• اگر Q1 قبلاً قطع و `>=22000` → قطع بماند، تایمر نچرخد<br>زمان با `osKernelGetTickCount` + `func__Rtos_TicksToMilliseconds` |
| `func__McuPowerPath_OnInputIrq` | امن در ISR: `Read PB4`؛ اگر `input_present==false` → فوراً `Write PB5 Low`، `BatteryConnected=true`، `TimerActive=false`؛ اگر `true` کاری نمی‌کند و `Run` احراز را آغاز می‌کند؛ فقط GPIO و پرچم volatile |
| `HAL_GPIO_EXTI_Callback` (PB4) | `#if MODULE_MCU_POWER_PATH` هوک `OnInputIrq` سپس `func__BspExti_OnIrq(BSP_EXTI_INPUT_DETECT)` با ترتیب Low قبل از رویداد |

## پایه‌ها

این ماژول فقط PB5 را می‌راند؛ PB4 فقط ورودی تشخیص، PB11 فقط Q17.

| پایه منطقی | پایه فیزیکی | لیبل | نقش | HIGH یعنی (شماتیک) |
|---|---|---|---|---|
| `BSP_GPIO_BATTERY_SWITCH` | PB5 / پایه 41 | `MCU_BAT_SWITCH` / Q1 | مالک McuPowerPath؛ قطع/وصل مسیر باتری MCU | Low وصل (فعال Low)، High قطع امن پس از 5 ثانیه (هیسترزیس 500mV) |
| `BSP_GPIO_INPUT_24V_PRESENT` | PB4 | `MCU_INT_24_IN` | ورودی تشخیص Presence با EXTI دو لبه (BspExti) | High = ورودی حاضر (>=22V واجد)، Low = قطع (ISR فوراً PB5 Low) |
| `BSP_GPIO_PROTECT_BATTERY` | PB11 | `MCU_LOW_BAT` / `MCU_PROTECT_BATT` | فقط Changeover Q17، High امن | **ممنوع: McuPowerPath تغییر نمی‌دهد** |
| `BSP_GPIO_RELAY` | PB7 | `MCU_PROTECT_CHARGER` | فقط Charger/Relay | **ممنوع** |

پلاریتی و نگاشت از `board_pins.h` (active-low برای PB5/PB11)؛ ماژول `board_pins.h` یا HAL را include نمی‌کند؛ آستانه‌ها فقط برای `v_in`، نه `v_bat24`.

## پیش‌فرض امن

- راه‌اندازی: `PIN_SAFE_BAT_SWITCH_HIGH=0u` و در `MX_GPIO_Init` لچ `PB5 RESET Low` قبل از `HAL_GPIO_Init`؛ `func__BspGpio_Init` سپس `McuPowerPath_Init` دوباره Low امن را تضمین می‌کنند تا MCU بدون ورودی از باتری بوت شود.
- `PB11` همیشه `PIN_SAFE_PROTECT_BATT_HIGH=1u` و `SET High` مستقل قبل از خروجی؛ هیچ منطقی PB5 و PB11 را به‌هم پیوند نمی‌دهد.
- `Run` هیسترزیس: <br>• `v_in >=22000` → تایمر 5 ثانیه؛ `snapshot` نامعتبر یا `21500..21999` → لغو تایمر و حفظ Q1 (زمان نامعتبر/ناحیه مرده شمرده نشود).<br>• `v_in <21500` → وصل Low و لغو تایمر (حتی اگر قبلاً قطع بود).<br>• پس از قطع (High)، `21.9V` (21900) → همچنان High؛ تا `<21500` نرسد reconnect نشود.
- زمان‌گیری فقط با `osKernelGetTickCount()` و `func__Rtos_TicksToMilliseconds()` و بدون فرض `tick=1ms`.
- ISR `OnInputIrq` فقط `BspGpio_Write` و پرچم volatile؛ بدون `TakeEvent`، صف، mutex، تایمر، تاخیر یا ADC؛ ترتیب: اول PB5 Low سپس ثبت رویداد.
- اگر ورودی از ابتدا حاضر باشد، بوت با باتری سپس احراز 5 ثانیه `>=22000` بدون نیاز به لبه rising گمشده.
- `v_bat24` هیچ‌گاه تصمیم Q1 را در حضور `v_in` معتبر تغییر نمی‌دهد (تست hysteresis).

## درخت اتصال

صدا زده می‌شود از:

```text
CubeIDE/Core/Src/main.c → MX_GPIO_Init
  HAL_GPIO_WritePin(GPIOB, PB11, SET)   // Q17 safe High
  HAL_GPIO_WritePin(GPIOB, PB5, RESET)  // Q1 Low → battery on before output mode
  HAL_GPIO_Init(...)                    // switch to output
  func__BspGpio_Init() → safe level from PIN_SAFE_* (PB5 Low, PB11 High)
```

```text
stm32f1xx_it.c → EXTI4_IRQHandler → HAL_GPIO_EXTI_IRQHandler(PB4) → HAL_GPIO_EXTI_Callback(PIN_INT_24_IN_PIN)
  bsp_exti.c
    #if MODULE_MCU_POWER_PATH
      func__McuPowerPath_OnInputIrq() // if PB4 Low → BSP_GPIO_BATTERY_SWITCH true (Low), cancel timer
    #endif
    func__BspExti_OnIrq(BSP_EXTI_INPUT_DETECT) // latch for other modules

rtos_app.c → func__Rtos_Start → osThreadNew(TaskControl) // when MODULE_MCU_POWER_PATH or Changeover/Charger/Jitter
  task_control.c → func__TaskControl
    func__McuPowerPath_Init() // before loop: battery connected
    for(;;)
      measurement_snapshot_t snap; (void)func__Measurement_GetSnapshot(&snap); // valid, v_in_mv
      func__McuPowerPath_Run() // hysteresis v_in: >=22000 5s→High; 21500..21999 preserve+cancel; <21500→Low
      func__Changeover_Evaluate(&snap, faults) // PB11 only, thresholds 21000/20800/21200, 3000ms unchanged
      func__Rtos_DelayMilliseconds(APP_CONFIG.control_period_ms) // ~10ms
```

این ماژول صدا می‌زند:

```text
mcu_power_path.c
  ├── bsp_gpio.h → func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true/false) // true=Low on, false=High off (owned)
  │               → func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT) // PB4 level in ISR
  ├── measurement.h → func__Measurement_GetSnapshot(&snap) // valid, v_in_mv (thresholds QUALIFY/RECONNECT for v_in)
  ├── rtos_time.h → func__Rtos_TicksToMilliseconds(elapsed_ticks) + osKernelGetTickCount()
  └── cmsis_os2.h → osKernelGetTickCount()
```
