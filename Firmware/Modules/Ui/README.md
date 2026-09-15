/**
 * @file    README.md
 * @brief   [EN] UI module sheet: LEDs and buzzer, split into LED and BUZZER per user request, RTOS simple readable, non-linear formulas.
 *          [FA] برگه ماژول UI: LED و بازر، دو بخش LED و BUZZER، RTOS ساده خوانا، فرمول غیرخطی.
 */

# ماژول UI

## وضعیت

فعال. `MODULE_UI = 1`. تنها ماژولی که الان اجرا می‌شود. RTOS ساده و خوانا با `vTaskDelay` (میکرو قفل نمی‌شود). دو بخش شده: LED و BUZZER در همین پوشه.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-15 | دو بخش شدن UI به LED و BUZZER: `ui_led.h/c` (LED scenarios) و `ui_buzzer.h/c` (buzzer patterns) در همین پوشه، هر تابع با `/* ==================== */` جدا، `ui.h` فقط ثابت‌ها + include دو بخش، `ui.c` wrapper برای سازگاری |
| 2026-09-15 | بالای هر تابع جدا کننده مدل درخواستی: `/* ==================== Blink / Poll timings ==================== */` هم در h و هم c، برای هر تابع |
| 2026-09-15 | RTOS ساده و خوانا + فرمول غیرخطی: `BatteryVoltageToPercent` 4 گام (voltageRangeMv, voltageOffsetMv, scaledOffset, batteryPercent)، چشمک با remainingPercent, periodPerPercent, greenOnMs/offMs, yellowOnMs/offMs |
| 2026-09-15 | جداسازی توابع با علامت مشخص و بازر انتهای فایل: همه فایل‌ها `/* ==================== */`، بازر بعد LED، `ui_config.h` حذف و همه ثابت‌ها در `ui.h` (single source) |
| 2026-09-14 | اجرای AI: اسکریپت چک قوانین `tools/check_ai_rules.sh` + تست هاست `host_test_ui.py`؛ پاس شد |

## فایل‌ها

| فایل | نقش |
|---|---|
| `ui.h` | **Single source** همه ثابت‌های UI: `UI_BAT_V_MIN_MV=21000`, `UI_BAT_V_MAX_MV=28000`, `UI_INPUT_THRESHOLD_MV=20000`, `UI_BLINK_PERIOD_MS=1000`, `UI_BEEP_BASE_MS=250`, `UI_TICK_MS=10`؛ شامل `ui_led.h` و `ui_buzzer.h` |
| `ui_led.h` / `ui_led.c` | **LED بخش**: `BatteryVoltageToPercent` 4 گام غیرخطی، `green/red/yellow/all_off`، `ScenarioInputOk`, `Charging_Tick`, `BatteryRun_Tick`, `Tick`, `Init`, `BoardTest`؛ هر تابع با `/* ==================== */` جدا، RTOS ساده `vTaskDelay` |
| `ui_buzzer.h` / `ui_buzzer.c` | **BUZZER بخش**: `buzzer`, `calc_beep_on` غیرخطی (repeatMinusOne, totalGapMs, denominator), `buzzer_start_internal`, `BuzzerPatternMs/Percent`, `Stop`؛ هر تابع با `/* ==================== */` جدا، `/* ==================== Buzzer / Beep ==================== */` دارد |
| `ui.c` | Wrapper برای سازگاری قدیم: خالی، پیاده‌سازی‌ها در `ui_led.c` و `ui_buzzer.c`؛ شامل `/* ==================== Buzzer / Beep ==================== */` برای چک قدیم |
| `../../Rtos/Src/task_ui.c` | تسک ساده RTOS: `for(;;){ func__Ui_Tick(); vTaskDelay(UI_TICK_MS); }`؛ متغیرهای تست `UINT32_T__G__InputVoltageMv` / `BatteryVoltageMv` volatile |
| `../../Bsp/Src/bsp_gpio.c` | نوشتن پایه |
| `../../Config/Inc/board_pins.h` | شماره پایه |
| `host_test_ui.py` | تست هاست: آستانه‌ها را از `ui.h` می‌خواند (single source) |

`ui_config.h` حذف شد. همه ثابت‌ها در `ui.h` هستند. دو بخش LED و BUZZER هر دو در همین پوشه.

## توابع

| نام | کار | فایل |
|---|---:|---|
| `func__Ui_BatteryVoltageToPercent` | 4 گام غیرخطی: `voltageRangeMv = Vmax-Vmin`, `voltageOffsetMv = Vbat-Vmin`, `scaledOffset = offset*100`, `batteryPercent = scaled/range` | `ui_led.c` |
| `func__Ui_Init` | همه خروجی خاموش، شمارنده بوق صفر | `ui_led.c` |
| `func__Ui_BoardTest_Start` | قرمز/زرد/سبز هر کدام `UI_SELFTEST_LED_MS` با `vTaskDelay` ساده، بوق `UI_BOOT_BEEP_MS` | `ui_led.c` |
| `func__Ui_BoardTest_Tick` | برای سازگاری false | `ui_led.c` |
| `func__Ui_ScenarioInputOk` | سبز ثابت، بقیه خاموش، `vTaskDelay(UI_INPUT_OK_POLL_MS)` ساده RTOS | `ui_led.c` |
| `func__Ui_ScenarioCharging_Tick` | سبز ثابت، زرد: `remainingPercent = 100-pct`, `periodPerPercent = period/100`, `yellowOnMs = remaining*periodPer`, `yellowOffMs = period-yellowOn` | `ui_led.c` |
| `func__Ui_ScenarioBatteryRun_Tick` | سبز چشمک: `remainingPercent`, `periodPerPercent`, `greenOffMs = remaining*periodPer`, `greenOnMs = period-greenOff`؛ بوق هوشمند هر `pct` ثانیه | `ui_led.c` |
| `func__Ui_Tick` | انتخاب سناریو بر اساس ولتاژ | `ui_led.c` |
| `func__green/red/yellow/all_off` (static) | سطح پایین LED | `ui_led.c` |
| `func__Ui_BuzzerPatternMs_Start` | ورودی `periodMs`, `onTimeMs`, `repeatCount`, `gapMs`؛ فرمول غیرخطی `totalGapMs`, `pulseOnMs` | `ui_buzzer.c` |
| `func__Ui_BuzzerPatternMs_Tick` | تیکه بازر | `ui_buzzer.c` |
| `func__Ui_BuzzerPatternPercent_Start` | گپ درصدی: `onTimeTimesPercent = onTime*percent`, `gapMs = onTimeTimesPercent/100` غیرخطی | `ui_buzzer.c` |
| `func__Ui_BuzzerPatternPercent_Tick` | تیکه درصدی | `ui_buzzer.c` |
| `func__Ui_BuzzerPattern_Stop` | خاموش | `ui_buzzer.c` |
| `func__buzzer/calc_beep_on/buzzer_start_internal` (static) | سطح پایین بازر، `calc_beep_on` غیرخطی: `repeatMinusOne`, `totalGapMs`, `denominator` | `ui_buzzer.c` |

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
  func__App_Init() → func__Ui_Init() (ui_led.c)
  func__Rtos_Start() → rtos_app.c → func__TaskUi → task_ui.c
    func__Ui_BoardTest_Start() // ui_led.c با vTaskDelay ساده
    loop: func__Ui_Tick(inputMv, batteryMv) // ui_led.c: ولتاژ → درصد غیرخطی 4 گام
      → func__Ui_ScenarioCharging_Tick() // ui_led.c: remainingPercent, periodPerPercent
      → func__Ui_ScenarioInputOk() // ui_led.c
      → func__Ui_ScenarioBatteryRun_Tick() // ui_led.c: greenOnMs/offMs + beepIntervalCycles
        → func__Ui_BuzzerPatternMs_Start() // ui_buzzer.c: totalOnClamped, gapClamped, non-linear
```

این ماژول صدا می‌زند:

```text
ui_led.c
  ui.h                        UI_BAT_V_MIN_MV, UI_TICK_MS, etc (single source)
  ui_buzzer.h                 func__Ui_BuzzerPatternMs_Start
  bsp_gpio.h / bsp_gpio.c     func__BspGpio_Write → PIN_LED_*, PIN_BUZZER_*
  board_pins.h                PIN_LED_G/Y/R, PIN_BUZZER
  FreeRTOS.h / task.h         vTaskDelay (RTOS ساده، میکرو قفل نمی‌شود)
  func__Ui_BatteryVoltageToPercent // 4 گام غیرخطی

ui_buzzer.c
  ui.h                        UI_BUZZER_DEFAULT_GAP_PERCENT etc
  bsp_gpio.h / bsp_gpio.c     func__BspGpio_Write → PIN_BUZZER_*
  board_pins.h                PIN_BUZZER
  FreeRTOS.h / task.h         xTaskGetTickCount
  func__calc_beep_on          // non-linear: repeatMinusOne, totalGapMs
```
