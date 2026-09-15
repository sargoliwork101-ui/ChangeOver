/**
 * @file    README.md
 * @brief   [EN] UI module sheet: LED scenarios remain separate from the single periodic buzzer service.
 *          [FA] برگه ماژول UI: سناریوهای LED از سرویس یگانه بوق دوره‌ای جدا هستند.
 */

# ماژول UI

## وضعیت

فعال. `MODULE_UI = 1`. LED و بوق در دو فایل مستقل هستند. بوق دیگر از سناریوهای LED، BatteryRun یا تست LED به‌صورت خودکار صدا زده نمی‌شود؛ هر فراخواننده باید صریحاً چهار ورودی الگوی بوق را به سرویس بوق بدهد.

API کاربردی بوق فقط یک تابع است:

```c
func__Ui_Buzzer_Tick(periodMs, dutyPercent, beepCount, gapMs)
```

این تابع غیرمسدودکننده است و باید هر `UI_TICK_MS` از یک تسک صدا زده شود.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-15 | ساده‌سازی کامل بوق: حذف APIهای Start/Tick میلی‌ثانیه‌ای و درصدی، حذف Stop و توابع داخلی اضافی؛ باقی ماندن یک تابع عمومی با ورودی‌های دوره، دیوتی، تعداد بوق و گپ. |
| 2026-09-15 | بوق از منطق خودکار BatteryRun و تست LED جدا شد؛ LED فقط مالک LEDها است و بوق مالک پایه PA4 است. |
| 2026-09-15 | تعریف محاسبه جدید: پنجره دیوتی برابر `period*duty/100` است، گپ‌ها داخل این پنجره قرار می‌گیرند و زمان باقی‌مانده تا دوره بعدی خاموش است. |
| 2026-09-15 | اصلاح مستندات با ساختار واقعی دو بخش LED و BUZZER و استفاده از `APP_CONFIG` برای مقدارهای قابل تنظیم LED. |
| 2026-09-14 | اجرای AI: اسکریپت چک قوانین `tools/check_ai_rules.sh` + تست هاست UI؛ پاس شد. |

## فایل‌ها

| فایل | نقش |
|---|---|
| `ui_led.h` / `ui_led.c` | منطق LED: نگاشت ولتاژ، سناریوهای InputOk/Charging/BatteryRun و تست LED. این فایل‌ها بوق را شروع نمی‌کنند. |
| `ui_buzzer.h` / `ui_buzzer.c` | سرویس یگانه بوق: محاسبه پنجره دیوتی، تقسیم آن بین پالس‌ها و گپ‌ها، و نوشتن PA4. |
| `../../Rtos/Src/task_ui.c` | تسک UI؛ محل مناسب برای فراخوانی دوره‌ای سرویس بوق، فقط وقتی یک سناریو صریحاً بوق خواسته باشد. |
| `../../Bsp/Src/bsp_gpio.c` | نوشتن سطح GPIO از طریق `func__BspGpio_Write`. |
| `../../Config/Inc/board_pins.h` | تعریف `PIN_BUZZER_PORT` و `PIN_BUZZER_PIN`؛ PA4 طبق شماتیک. |
| `../../Config/Inc/app_config.h` / `../../Config/Src/app_config.c` | تنظیمات عمومی پروژه؛ ثابت‌های قدیمی بوق برای سازگاری نگه داشته شده‌اند، اما سرویس جدید ورودی‌های الگوی خود را مستقیم می‌گیرد. |
| `host_test_ui.py` | تست هاست فرمول دوره، دیوتی، تعداد پالس و گپ. |

## توابع

| نام | کار | فایل |
|---|---:|---|
| `func__Ui_Buzzer_Tick` | تنها API کاربردی بوق؛ یک الگوی دوره‌ای را با چهار ورودی اجرا می‌کند و هر بار فقط یک نمونه زمانی را پردازش می‌کند | `ui_buzzer.c` |
| `func__Ui_Init` | خاموش کردن LEDها؛ بوق در مالکیت سرویس مستقل خودش است | `ui_led.c` |
| `func__Ui_BoardTest_Start` | تست یک‌باره قرمز، زرد و سبز؛ بوق به‌صورت ضمنی اجرا نمی‌شود | `ui_led.c` |
| `func__Ui_ScenarioInputOk` | سبز ثابت و قرمز/زرد خاموش | `ui_led.c` |
| `func__Ui_ScenarioCharging_Tick` | سبز ثابت و زرد متناسب با درصد شارژ | `ui_led.c` |
| `func__Ui_ScenarioBatteryRun_Tick` | سبز چشمک‌زن و زرد خاموش؛ بوق مستقل است | `ui_led.c` |
| `func__Ui_Tick` | انتخاب سناریوی LED بر اساس ولتاژ ورودی و باتری | `ui_led.c` |

فرمول سرویس بوق:

```text
dutyWindowMs = periodMs × dutyPercent / 100
totalGapMs   = gapMs × (beepCount - 1)
availableOnMs = dutyWindowMs - totalGapMs
beepOnMs     = availableOnMs / beepCount
periodTailMs = periodMs - dutyWindowMs
```

مثال:

```text
period = 10000ms
نسبت دیوتی = 10٪
تعداد بوق = 2
گپ = 100ms

پنجره دیوتی = 1000ms
هر بوق = 450ms

450ms روشن
100ms خاموش
450ms روشن
9000ms خاموش
```

اگر `dutyPercent` یا `beepCount` صفر باشد، بوق خاموش می‌شود. اگر گپ‌ها تمام پنجره دیوتی را مصرف کنند، الگو نامعتبر است و بوق خاموش می‌ماند.

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی |
|---|---|---|---|
| PB0 | `MCU_R_LED` | LED قرمز | روشن |
| PB1 | `MCU_Y_LED` | LED زرد | روشن |
| PB10 | `MCU_G_LED` | LED سبز | روشن |
| PA4 | `MCU_BUZZER` | بازر | فعال شدن بوق طبق شماتیک |

قطبیت تا اندازه‌گیری روی برد، شماتیکی است. سرویس بوق فقط PA4 را HIGH/LOW می‌کند و فرکانس صوتی PWM تولید نمی‌کند؛ بنابراین این منطق برای Active Buzzer مناسب است.

## پیش‌فرض امن

بعد از Reset و `func__Ui_Init`، LEDها خاموش هستند. سرویس بوق تا زمانی که با ورودی معتبر صدا زده نشود، PA4 را خاموش نگه می‌دارد. ورودی صفر برای `dutyPercent` یا `beepCount` نیز حالت خاموش امن است.

## درخت اتصال

```text
Firmware/Rtos/Src/task_ui.c
  └── در صورت درخواست صریح یک سناریو
      └── func__Ui_Buzzer_Tick(periodMs, dutyPercent, beepCount, gapMs)
          ├── xTaskGetTickCount()       زمان نمونه فعلی RTOS
          └── func__BspGpio_Write()     Firmware/Bsp/Src/bsp_gpio.c
              └── PIN_BUZZER_PORT/PIN_BUZZER_PIN  = PA4

LED path (independent):
CubeIDE/Core/Src/main.c
  → func__App_Start() → func__App_Init() → func__Ui_Init()
  → func__Rtos_Start() → func__TaskUi() → func__Ui_Tick()
```
