/**
 * @file    README.md
 * @brief   [EN] UI module sheet: LEDs and buzzer, split into LED and BUZZER, with APP_CONFIG as the runtime configuration source.
 *          [FA] برگه ماژول UI: LED و بازر، جداشده در دو بخش، با APP_CONFIG به‌عنوان منبع تنظیمات زمان اجرا.
 */

# ماژول UI

## وضعیت

فعال. `MODULE_UI = 1`. تنها ماژولی که الان اجرا می‌شود. RTOS ساده و خوانا با `vTaskDelay` (میکرو قفل نمی‌شود). کد UI در دو بخش مستقل LED و BUZZER در همین پوشه قرار دارد.

مقادیر قابل تنظیم رفتار UI از `APP_CONFIG` خوانده می‌شوند. مقدارهای پیش‌فرض این شیء `const` در `app_config.c` از ثابت‌های `ui_led.h` و `ui_buzzer.h` ساخته می‌شوند؛ بنابراین منطق اجرا مستقیماً مقدارهای هدر را نمی‌خواند و فقط یک منبع تنظیمات دارد.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-15 | اصلاح شمارنده بوق هوشمند: سیکل کامل‌شده ابتدا شمرده می‌شود؛ بنابراین بوق ۴۰٪ در سیکل‌های ۴۰، ۸۰ و بوق ۰٪ با کف یک سیکل اجرا می‌شود. تست رگرسیون به `host_test_ui.py` اضافه شد. |
| 2026-09-15 | یکسان‌سازی تنظیمات: منطق LED مقادیر قابل تنظیم را از `APP_CONFIG` می‌خواند؛ ثابت‌های هدر فقط پیش‌فرض ساخت `APP_CONFIG` یا ثابت‌های الگوریتم هستند. |
| 2026-09-15 | مستندات با ساختار واقعی دو فایل LED و BUZZER هماهنگ شد؛ wrapper تک‌فایلی قبلی دیگر بخشی از UI نیست. |
| 2026-09-15 | دو بخش شدن UI به LED و BUZZER: `ui_led.h/c` (سناریوهای LED) و `ui_buzzer.h/c` (الگوهای بازر) در همین پوشه، هر تابع با `/* ==================== */` جدا. |
| 2026-09-15 | بالای هر تابع جداکننده مدل درخواستی: `/* ==================== Name ==================== */` در هدر و فایل پیاده‌سازی. |
| 2026-09-15 | RTOS ساده و خوانا + فرمول غیرخطی: نگاشت ولتاژ در چهار گام و زمان‌بندی چشمک با متغیرهای معنادار. |
| 2026-09-14 | اجرای AI: اسکریپت چک قوانین `tools/check_ai_rules.sh` + تست هاست `host_test_ui.py`؛ پاس شد. |

## فایل‌ها

| فایل | نقش |
|---|---|
| `ui_led.h` / `ui_led.c` | بخش LED: ثابت‌های پیش‌فرض LED، نگاشت ولتاژ باتری، سناریوهای InputOk/Charging/BatteryRun و انتخاب سناریو. منطق زمان‌بندی مقادیر قابل تنظیم را از `APP_CONFIG` می‌خواند. |
| `ui_buzzer.h` / `ui_buzzer.c` | بخش BUZZER: الگوهای بوق میلی‌ثانیه‌ای و درصدی، ماشین حالت غیرمسدودکننده، و ثابت‌های الگوریتم بازر. |
| `../../Config/Inc/app_config.h` | تعریف `app_config_t` و اعلان `APP_CONFIG`. |
| `../../Config/Src/app_config.c` | مقداردهی `const APP_CONFIG` با پیش‌فرض‌های هدرهای UI. |
| `../../Rtos/Src/task_ui.c` | تسک UI؛ ورودی‌های تست `UINT32_T__G__InputVoltageMv` و `UINT32_T__G__BatteryVoltageMv` را می‌خواند و `func__Ui_Tick` را اجرا می‌کند. |
| `../../Bsp/Src/bsp_gpio.c` | نوشتن سطح GPIO برای LED و بازر. |
| `../../Config/Inc/board_pins.h` | شماره و قطبیت شماتیکی پایه‌های LED و بازر. |
| `host_test_ui.py` | تست هاست سناریوها، اتصال پیش‌فرض‌های `APP_CONFIG` و زمان دقیق اولین بوق هوشمند. |

ساختار تک‌فایلی قبلی در نسخه فعلی وجود ندارد و نباید به‌عنوان wrapper یا منبع ثابت‌های UI استفاده شود. ثابت‌های پیش‌فرض LED در `ui_led.h` و ثابت‌های پیش‌فرض بازر در `ui_buzzer.h` هستند؛ منطق زمان اجرا از `APP_CONFIG` می‌خواند.

## توابع

| نام | کار | فایل |
|---|---:|---|
| `func__Ui_BatteryVoltageToPercent` | چهار گام غیرخطی: `voltageRangeMv`، `voltageOffsetMv`، `scaledOffset` و `batteryPercent`؛ محدوده از `APP_CONFIG.ui_bat_v_min_mv/max_mv` | `ui_led.c` |
| `func__Ui_Init` | خاموش کردن همه خروجی‌ها و صفر کردن شمارنده بوق | `ui_led.c` |
| `func__Ui_BoardTest_Start` | تست یک‌باره قرمز، زرد، سبز و بوق با `APP_CONFIG.ui_selftest_led_ms` و `APP_CONFIG.ui_boot_beep_ms` | `ui_led.c` |
| `func__Ui_BoardTest_Tick` | API سازگاری؛ چون تست در `Start` کامل می‌شود، `false` برمی‌گرداند | `ui_led.c` |
| `func__Ui_ScenarioInputOk` | سبز ثابت، سایر خروجی‌ها خاموش و بازبینی طبق `APP_CONFIG.ui_input_ok_poll_ms` | `ui_led.c` |
| `func__Ui_ScenarioCharging_Tick` | سبز ثابت؛ زرد بر اساس `remainingPercent` و `APP_CONFIG.ui_charging_blink_period_ms` چشمک می‌زند | `ui_led.c` |
| `func__Ui_ScenarioBatteryRun_Tick` | سبز بر اساس درصد چشمک می‌زند؛ زرد خاموش است؛ بوق در سیکل دقیق درصد باتری شروع می‌شود و زیر آستانه مدت آن دو برابر می‌شود | `ui_led.c` |
| `func__Ui_Tick` | انتخاب سناریو بر اساس `APP_CONFIG.ui_input_threshold_mv` و درصد باتری | `ui_led.c` |
| `func__green/red/yellow/all_off` (static) | درایور سطح پایین LED و حالت امن خاموش | `ui_led.c` |
| `func__Ui_BuzzerPatternMs_Start` | شروع الگوی بازر با دوره، زمان روشن، تعداد تکرار و گپ میلی‌ثانیه‌ای | `ui_buzzer.c` |
| `func__Ui_BuzzerPatternMs_Tick` | اجرای یک گام ماشین حالت بازر | `ui_buzzer.c` |
| `func__Ui_BuzzerPatternPercent_Start` | تبدیل گپ درصدی به میلی‌ثانیه و شروع الگو | `ui_buzzer.c` |
| `func__Ui_BuzzerPatternPercent_Tick` | اجرای تیک الگوی درصدی | `ui_buzzer.c` |
| `func__Ui_BuzzerPattern_Stop` | توقف فوری بازر و رفتن به حالت بیکار | `ui_buzzer.c` |
| `func__buzzer/calc_beep_on/buzzer_start_internal` (static) | درایور GPIO، محاسبه زمان پالس و آماده‌سازی ماشین حالت بازر | `ui_buzzer.c` |

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی |
|---|---|---|---|
| PB0 | `MCU_R_LED` | LED قرمز | روشن |
| PB1 | `MCU_Y_LED` | LED زرد | روشن؛ در شارژ ۰٪ ثابت روشن و ۱۰۰٪ خاموش |
| PB10 | `MCU_G_LED` | LED سبز | روشن؛ در InputOk ثابت و در BatteryRun چشمک |
| PA4 | `MCU_BUZZER` | بازر | فعال شدن بازر طبق شماتیک |

همه خروجی‌ها Push-Pull هستند و بعد از Reset طبق شماتیک Low در نظر گرفته می‌شوند. قطبیت تا اندازه‌گیری روی برد، شماتیکی است.

## پیش‌فرض امن

`func__Ui_Init` همه پایه‌های LED و بازر را Low می‌کند و شمارنده بوق را صفر می‌کند. هر سناریو خروجی‌های نامرتبط را خاموش می‌کند؛ در BatteryRun زرد خاموش و در Charging قرمز خاموش است. اگر باتری ۰٪ باشد، زرد در شارژ ثابت روشن می‌ماند؛ اگر ۱۰۰٪ باشد، زرد خاموش است.

## درخت اتصال

صدا زده می‌شود از:

```text
CubeIDE/Core/Src/main.c
  → func__App_Start()                  Firmware/App/Src/app.c
    → func__App_Init()
      → func__Ui_Init()                ui_led.c
    → func__Rtos_Start()               Firmware/Rtos/Src/rtos_app.c
      → func__TaskUi()                 Firmware/Rtos/Src/task_ui.c
        → func__Ui_BoardTest_Start()  ui_led.c + ui_buzzer.c
        → loop: func__Ui_Tick(inputMv, batteryMv)
          → func__Ui_ScenarioCharging_Tick()       ui_led.c
          → func__Ui_ScenarioInputOk()              ui_led.c
          → func__Ui_ScenarioBatteryRun_Tick()      ui_led.c
            → func__Ui_BuzzerPatternMs_Start()      ui_buzzer.c
```

این ماژول صدا می‌زند:

```text
ui_led.c
  app_config.h / app_config.c  APP_CONFIG: تنظیمات const زمان اجرا
  ui_led.h                    پیش‌فرض‌های ولتاژ، درصد و زمان‌های LED
  ui_buzzer.h                 func__Ui_BuzzerPatternMs_Start/Tick
  bsp_gpio.h / bsp_gpio.c     func__BspGpio_Write → PIN_LED_*, PIN_BUZZER_*
  board_pins.h                PIN_LED_G/Y/R, PIN_BUZZER
  FreeRTOS.h / task.h         vTaskDelay

ui_buzzer.c
  ui_buzzer.h                 ثابت‌های الگوی بازر و UI_BEEP_MIN_INTERVAL_CYCLES
  bsp_gpio.h / bsp_gpio.c     func__BspGpio_Write → PIN_BUZZER_*
  board_pins.h                PIN_BUZZER
  FreeRTOS.h / task.h         xTaskGetTickCount
```
