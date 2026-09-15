/**
 * @file    README.md
 * @brief   [EN] UI module sheet: LEDs and buzzer, voltage-based scenarios, RTOS simple readable, non-linear formulas.
 *          [FA] برگه ماژول UI: LED و بازر، سناریوهای ولتاژی، RTOS ساده خوانا، فرمول غیرخطی.
 */

# ماژول UI

## وضعیت

فعال. `MODULE_UI = 1`. تنها ماژولی که الان اجرا می‌شود. RTOS ساده و خوانا با `vTaskDelay` (میکرو قفل نمی‌شود).

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-15 | RTOS ساده و خوانا + فرمول غیرخطی: `BatteryVoltageToPercent` به 4 گام (voltageRangeMv, voltageOffsetMv, scaledOffset, batteryPercent)، چشمک زرد/سبز با remainingPercent, periodPerPercent, greenOnMs/offMs, yellowOnMs/offMs، بازر با totalGapMs, pulseOnMs؛ `vTaskDelay` در سناریوها مجاز (RTOS)، کد کمتر و خواناتر، `check_ai_rules.sh` [17] اضافه شد |
| 2026-09-15 | جداسازی توابع با علامت مشخص و بازر انتهای فایل: همه فایل‌ها `/* ==================== */`، بازر بعد LED، `ui_config.h` حذف و همه ثابت‌ها در `ui.h` (single source) |
| 2026-09-15 | نام‌گذاری معنادار و پیشوند ثابت: `UI_BAT_V_MIN_MV` در `ui.h`، متغیرها `uint32_t__batteryVoltageMv`, `UINT32_T__G__UiBatteryRunBeepCycleCnt`, `UINT32_T__G__BuzzerTotalOnMs`, `uint32_t__greenOnMs/offMs`، تابع `func__` با `__` |
| 2026-09-14 | اجرای AI: اسکریپت چک قوانین `tools/check_ai_rules.sh` + تست هاست `host_test_ui.py`؛ پاس شد |
| 2026-09-14 | سناریوهای ولتاژی: InputOk (سبز ثابت)، BatteryRun (سبز چشمک + بوق هوشمند)، Charging (سبز ثابت + زرد متغیر) |

## فایل‌ها

| فایل | نقش |
|---|---|
| `ui.h` | **Single source** همه ثابت‌های UI: `UI_BAT_V_MIN_MV=21000`, `UI_BAT_V_MAX_MV=28000`, `UI_INPUT_THRESHOLD_MV=20000`, `UI_BLINK_PERIOD_MS=1000`, `UI_BEEP_BASE_MS=250`, `UI_TICK_MS=10` و API ها |
| `ui.c` | 3 سناریو + تبدیل ولتاژ به درصد (غیرخطی 4 گام) + بازر (غیرخطی)؛ LED اول، بازر آخر با `/* ==================== Buzzer / Beep ==================== */`؛ ساده و خوانا با `vTaskDelay` |
| `../../Rtos/Src/task_ui.c` | تسک ساده RTOS: `for(;;){ func__Ui_Tick(); vTaskDelay(UI_TICK_MS); }`؛ متغیرهای تست `UINT32_T__G__InputVoltageMv` / `BatteryVoltageMv` volatile |
| `../../Bsp/Src/bsp_gpio.c` | نوشتن پایه |
| `../../Config/Inc/board_pins.h` | شماره پایه |
| `host_test_ui.py` | تست هاست: آستانه‌ها را از `ui.h` می‌خواند (single source) |

`ui_config.h` حذف شد. همه ثابت‌ها در `ui.h` هستند.

## توابع

| نام | کار |
|---|---|
| `func__Ui_Init` | همه خروجی خاموش، شمارنده بوق صفر |
| `func__Ui_BoardTest_Start` | قرمز/زرد/سبز هر کدام `UI_SELFTEST_LED_MS` با `vTaskDelay` ساده، بوق `UI_BOOT_BEEP_MS` |
| `func__Ui_BoardTest_Tick` | برای سازگاری، false برمی‌گرداند (تست در Start انجام شد) |
| `func__Ui_BatteryVoltageToPercent` | ورودی `uint32_t__batteryMv` mV؛ 4 گام غیرخطی: `voltageRangeMv = Vmax-Vmin`, `voltageOffsetMv = Vbat-Vmin`, `scaledOffset = offset*100`, `batteryPercent = scaled/range` |
| `func__Ui_ScenarioInputOk` | سبز ثابت، بقیه خاموش، `vTaskDelay(UI_INPUT_OK_POLL_MS)` ساده RTOS |
| `func__Ui_ScenarioCharging_Tick` | ورودی `uint32_t__batteryMv`؛ سبز ثابت، زرد: `remainingPercent = 100-pct`, `periodPerPercent = period/100`, `yellowOnMs = remaining*periodPer`, `yellowOffMs = period-yellowOn`؛ 0% زرد ثابت روشن، 100% خاموش |
| `func__Ui_ScenarioBatteryRun_Tick` | ورودی `uint32_t__batteryMv`؛ سبز چشمک: `remainingPercent = 100-pct`, `periodPerPercent = period/100`, `greenOffMs = remaining*periodPer`, `greenOnMs = period-greenOff`؛ بوق هوشمند: هر `pct` ثانیه با `beepIntervalCycles`, `<20%` طول 2 برابر |
| `func__Ui_Tick` | ورودی `uint32_t__inputVoltageMv`, `uint32_t__batteryVoltageMv`؛ انتخاب سناریو بر اساس ولتاژ |
| `func__Ui_BuzzerPatternMs_Start` | ورودی `uint32_t__periodMs`, `uint32_t__onTimeMs`, `uint8_t__repeatCount`, `uint32_t__gapMs`؛ شروع الگوی بازر با فرمول غیرخطی |
| `func__Ui_BuzzerPatternMs_Tick` | تیکه بازر، true تا تمام |
| `func__Ui_BuzzerPatternPercent_Start` | گپ درصدی: `onTimeTimesPercent = onTime*percent`, `gapMs = onTimeTimesPercent/100` |
| `func__Ui_BuzzerPattern_Stop` | خاموش |
| `func__green/red/yellow/all_off/buzzer/calc_beep_on` (static) | سطح پایین، `calc_beep_on` غیرخطی: `repeatMinusOne`, `totalGapMs`, `denominator`, `pulseOnMs` |

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی |
|---|---|---|---|
| PB0 | `MCU_R_LED` | LED قرمز | روشن |
| PB1 | `MCU_Y_LED` | LED زرد | روشن - شارژ: 0% ثابت روشن، 100% خاموش |
| PB10 | `MCU_G_LED` | LED سبز | روشن - InputOk ثابت، BatteryRun چشمک `greenOnMs/offMs` |
| PA4 | `MCU_BUZZER` | بازر | صدا - `BuzzerTotalOnMs`, `BuzzerGapMs` |

همه خروجی Push-Pull، بعد Reset Low.

## پیش‌فرض امن

`func__Ui_Init` همه پایه‌ها Low. هر سناریو خروجی نامرتبط خاموش. BatteryRun زرد خاموش. Charging قرمز خاموش.

## درخت اتصال

صدا زده می‌شود از:

```text
main.c → func__App_Start() → app.c
  func__App_Init() → func__Ui_Init()
  func__Rtos_Start() → rtos_app.c → func__TaskUi → task_ui.c
    func__Ui_BoardTest_Start() // با vTaskDelay ساده
    loop: func__Ui_Tick(inputMv, batteryMv) // ولتاژ → درصد غیرخطی 4 گام
      → func__Ui_ScenarioCharging_Tick() // remainingPercent, periodPerPercent
      → func__Ui_ScenarioInputOk() // vTaskDelay ساده
      → func__Ui_ScenarioBatteryRun_Tick() // greenOnMs/offMs + beepIntervalCycles
        → func__Ui_BuzzerPatternMs_Start() // totalOnClamped, gapClamped
```

این ماژول صدا می‌زند:

```text
ui.c
  bsp_gpio.h / bsp_gpio.c     func__BspGpio_Write → PIN_LED_*, PIN_BUZZER_*
  board_pins.h                PIN_LED_G/Y/R, PIN_BUZZER
  ui.h                        UI_BAT_V_MIN_MV, UI_TICK_MS, etc (single source)
  FreeRTOS.h / task.h         vTaskDelay (RTOS ساده، میکرو قفل نمی‌شود)
  func__Ui_BatteryVoltageToPercent // 4 گام غیرخطی
  func__Ui_BuzzerPatternMs_Start // فرمول غیرخطی
```
