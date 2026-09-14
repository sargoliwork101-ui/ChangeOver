/**
 * @file    README.md
 * @brief   [EN] UI module sheet: LEDs and buzzer.
 *          [FA] برگه ماژول UI: ال‌ای‌دی و بازر.
 */

# ماژول UI

## وضعیت

فعال. `MODULE_UI = 1`. تنها ماژولی که الان اجرا می‌شود.

نمایش وضعیت با `Ui_Indicate(input_present, battery_percent)`، تابع غیرمسدودکننده که تسک هر ۱۰ms صدا می‌زند:

| حالت (enum) | شرط | رفتار |
|---|---|---|
| `UI_INPUT_OK` | ورودی وصل (`input_present = true`) | سبز ثابت، بقیه خاموش |
| `UI_BATTERY_RUN` | ورودی قطع، باتری بالاتر از `ui_low_battery_percent` | سبز چشمک؛ دوره `ui_blink_period_ms`=۱۰۰۰ms، سهم روشن برابر درصد باتری (فول ۹۹٪ روشن، ۲۱٪ → ۲۱٪ روشن) |
| `UI_BATTERY_LOW` | باتری ≤ `ui_low_battery_percent` (۲۰٪) | زرد ۵۰۰ روشن/۵۰۰ خاموش + بوق `ui_warn_beep_ms`=۲۵۰ms در ابتدای هر `ui_warn_beep_period_ms`=۳۰ ثانیه |

حالت چهارم (زیر ۱۰٪) هنوز تصمیم‌گیری نشده و فعلاً همان `UI_BATTERY_LOW` ادامه دارد.

تست بدون ADC: دو متغیر `volatile` در `task_ui.c` (`ui_test_input_present`، `ui_test_battery_percent`) که در دیباگر از Live Expressions زنده عوض می‌شوند.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | سناریوهای مبتنی بر وضعیت با `Ui_Indicate` و enum سه حالته؛ حذف `Ui_Scenario1/2` و `UI_FLAG`؛ متغیرهای تست دستی در تسک؛ تست زمان‌بندی روی هاست پاس شد |
| 2026-09-14 | درخت اتصال UI کامل شد (از main تا پایه) |
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه یکدست شد |
| 2026-09 | کامنت خط‌به‌خط انگلیسی روی `Ui_Scenario1` / `Ui_Scenario2`؛ سناریو ۲ بدون متغیر اضافه |
| 2026-09 | `Ui_BoardTest` یک‌بار، بعد `UI_FLAG` یکی از دو سناریو |
| 2026-09 | الگوها داخل `ui.c`؛ تسک فقط انتخاب می‌کند |

## فایل‌ها

| فایل | نقش |
|---|---|
| `ui.h` / `ui.c` | API، enum حالت‌ها و ماشین نمایش غیرمسدودکننده |
| `../../Rtos/Src/task_ui.c` | تسک؛ متغیرهای تست دستی؛ صدا زدن تناوبی `Ui_Indicate` |
| `../../Bsp/Src/bsp_gpio.c` | نوشتن پایه |
| `../../Config/Inc/board_pins.h` | شماره پایه |
| `../../Config/Src/app_config.c` | زمان‌ها و آستانه‌ها (`ui_*`) |

## توابع

| نام | کار |
|---|---|
| `Ui_Init` | همه خروجی UI خاموش؛ حالت اولیه `UI_INPUT_OK` و تایمرهای نرم صفر |
| `Ui_BoardTest` | یک‌بار قرمز، زرد، سبز، بوق؛ برمی‌گردد (مسدودکننده، فقط شروع) |
| `Ui_Indicate` | یک گام نمایش؛ ورودی: فلگ ورودی و درصد باتری؛ حالت را انتخاب و لبه‌های چشمک/بوق را با شمارنده می‌سازد |
| `TaskUi` | Init، تست برد، بعد حلقه تناوبی `ui_task_period_ms`؛ برنمی‌گردد |
| `green` (static) | PB10 |
| `red` (static) | PB0 |
| `yellow` (static) | PB1 |
| `buzzer` (static) | PA4 |
| `all_off` (static) | هر چهار تا Low |

متغیرهای داخلی `static`: `s_state` (حالت فعلی)، `s_phase_ms` (زمان درون دوره چشمک)، `s_warn_ms` (زمان درون دوره بوق هشدار). با عوض‌شدن حالت هر دو صفر می‌شوند تا الگو از لبه اولش شروع شود.

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی |
|---|---|---|---|
| PB0 | `MCU_R_LED` | LED قرمز Q4 | روشن (این مرحله استفاده نمی‌شود) |
| PB1 | `MCU_Y_LED` | LED زرد Q5 | روشن |
| PB10 | `MCU_G_LED` | LED سبز Q6 | روشن |
| PA4 | `MCU_BUZZER` | بازر Q7 | صدا |

همه خروجی، Push-Pull، بعد Reset باید Low باشند.

## پیش‌فرض امن

`Ui_Init` هر چهار پایه را Low می‌کند. قبل از `App_Start` هم CubeMX Level = Low. در `Ui_Indicate` شاخهٔ `default` حالت ناشناخته را با همه خروجی خاموش مدیریت می‌کند.

## درخت اتصال

صدا زده می‌شود از:

```text
main.c → App_Start() → app.c
  Ui_Init()
  Rtos_Start() → rtos_app.c → TaskUi → task_ui.c
    Ui_BoardTest()  (یک‌بار)
    حلقه هر ui_task_period_ms:
      Ui_Indicate(ui_test_input_present, ui_test_battery_percent)
```

این ماژول صدا می‌زند:

```text
ui.c
  bsp_gpio.h / bsp_gpio.c     BspGpio_Write
  board_pins.h                PIN_LED_* ، PIN_BUZZER_*
  app_config.h / app_config.c APP_CONFIG.ui_*
  FreeRTOS.h / task.h         vTaskDelay (فقط Ui_BoardTest)
```

به ADC، PWM، UART وصل نیست. درصد باتری و فلگ ورودی فعلاً دستی‌اند؛ بعداً از ماژول Measurement می‌آیند.
