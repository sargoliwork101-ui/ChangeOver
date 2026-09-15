/**
 * @file    README.md
 * @brief   [EN] UI module sheet: LED scenarios remain separate from the single periodic buzzer service.
 *          [FA] برگه ماژول UI: سناریوهای LED از سرویس یگانه بوق دوره‌ای جدا هستند.
 */

# ماژول UI

## وضعیت

فعال. `MODULE_UI = 1`. LED و بوق در دو فایل مستقل هستند. سرویس بوق همچنان فقط یک API دارد؛ سناریوهای قبلی `BoardTest` و `BatteryRun` آن را صریحاً صدا می‌زنند و سناریوهای `InputOk` و `Charging` آن را خاموش می‌کنند.

API کاربردی بوق فقط یک تابع است:

```c
int32_t func__Ui_Buzzer_Tick(periodMs, dutyPercent, beepCount, gapMs)
```

این تابع غیرمسدودکننده است. در الگوی معتبر، مقدار بازگشتی زمان پیشنهادی مراجعه بعدی بر حسب میلی‌ثانیه است؛ این زمان برابر ۱۰٪ کوچک‌ترین بخش مثبت الگو است. مقدار `0` خاموشی معتبر و مقدار `-1` تنظیمات نامعتبر را نشان می‌دهد. فراخواننده صریح می‌تواند مقدار مثبت را به `vTaskDelay` بدهد و نیازی نیست برای دوره‌های بزرگ، تسک را با فاصله ثابت و کوتاه بیدار کند.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-15 | ساده‌سازی کامل بوق: حذف APIهای Start/Tick میلی‌ثانیه‌ای و درصدی، حذف Stop و توابع داخلی اضافی؛ باقی ماندن یک تابع عمومی با ورودی‌های دوره، دیوتی، تعداد بوق و گپ. |
| 2026-09-15 | بوق از منطق خودکار BatteryRun و تست LED جدا شد؛ LED فقط مالک LEDها است و بوق مالک پایه PA4 است. |
| 2026-09-15 | تعریف محاسبه جدید: پنجره دیوتی برابر `period*duty/100` است، گپ‌ها داخل این پنجره قرار می‌گیرند و زمان باقی‌مانده تا دوره بعدی خاموش است. |
| 2026-09-15 | اتصال دوباره سرویس جدید به رفتار قبلی سناریوها: بوق کوتاه BoardTest و بوق هوشمند BatteryRun؛ InputOk و Charging بوق را خاموش می‌کنند. |
| 2026-09-15 | افزودن محدودیت‌های ایمنی با ثابت‌های حداقل دوره ۱۰۰۰ms و حداقل گپ ۱۰۰ms؛ صفر برای خاموشی معتبر و منفی یک برای خطا؛ بازگرداندن زمان مراجعه پیشنهادی ۱۰٪ برای RTOS. |
| 2026-09-15 | اصلاح مستندات با ساختار واقعی دو بخش LED و BUZZER و استفاده از `APP_CONFIG` برای مقدارهای قابل تنظیم LED. |
| 2026-09-14 | اجرای AI: اسکریپت چک قوانین `tools/check_ai_rules.sh` + تست هاست UI؛ پاس شد. |

## فایل‌ها

| فایل | نقش |
|---|---|
| `ui_led.h` / `ui_led.c` | منطق LED: نگاشت ولتاژ، سناریوهای InputOk/Charging/BatteryRun و تست LED؛ فقط نقاط صریح سناریو برای شروع یا خاموش کردن سرویس بوق را فراخوانی می‌کند. |
| `ui_buzzer.h` / `ui_buzzer.c` | سرویس یگانه بوق: محاسبه پنجره دیوتی، تقسیم آن بین پالس‌ها و گپ‌ها، اعتبارسنجی محدودیت‌های ایمنی، محاسبه مراجعه بعدی و نوشتن PA4. |
| `../../Rtos/Src/task_ui.c` | تسک UI؛ محل مناسب برای فراخوانی دوره‌ای سرویس بوق، فقط وقتی یک سناریو صریحاً بوق خواسته باشد. |
| `../../Bsp/Src/bsp_gpio.c` | نوشتن سطح GPIO از طریق `func__BspGpio_Write`. |
| `../../Config/Inc/board_pins.h` | تعریف `PIN_BUZZER_PORT` و `PIN_BUZZER_PIN`؛ PA4 طبق شماتیک. |
| `../../Config/Inc/app_config.h` / `../../Config/Src/app_config.c` | تنظیمات عمومی پروژه؛ ثابت‌های قدیمی بوق برای سازگاری نگه داشته شده‌اند، اما سرویس جدید ورودی‌های الگوی خود را مستقیم می‌گیرد. |
| `host_test_ui.py` | تست هاست فرمول دوره، دیوتی، تعداد پالس و گپ. |

## توابع

| نام | کار | فایل |
|---|---:|---|
| `func__Ui_Buzzer_Tick` | تنها API کاربردی بوق؛ یک الگوی دوره‌ای را با چهار ورودی اجرا می‌کند، GPIO را به‌روزرسانی می‌کند و زمان مراجعه بعدی یا کد وضعیت را برمی‌گرداند | `ui_buzzer.c` |
| `func__Ui_Init` | خاموش کردن LEDها؛ بوق در مالکیت سرویس مستقل خودش است | `ui_led.c` |
| `func__Ui_BoardTest_Start` | تست یک‌باره قرمز، زرد و سبز، سپس بوق کوتاه قبلی با API جدید | `ui_led.c` |
| `func__Ui_ScenarioInputOk` | سبز ثابت، قرمز/زرد خاموش و بوق خاموش | `ui_led.c` |
| `func__Ui_ScenarioCharging_Tick` | سبز ثابت و زرد متناسب با درصد شارژ؛ بوق خاموش | `ui_led.c` |
| `func__Ui_ScenarioBatteryRun_Tick` | سبز چشمک‌زن، زرد خاموش و بوق هوشمند قبلی با API جدید | `ui_led.c` |
| `func__Ui_Tick` | انتخاب سناریوی LED بر اساس ولتاژ ورودی و باتری | `ui_led.c` |

## محدودیت‌های ایمنی و کد بازگشتی

محدودیت‌ها در `ui_buzzer.h` با ثابت تعریف شده‌اند:

| ثابت | مقدار | رفتار |
|---|---:|---|
| `UI_BUZZER_MIN_PERIOD_MS` | `1000ms` | دوره غیرصفر کمتر از این مقدار نامعتبر است و بوق خاموش می‌شود. |
| `UI_BUZZER_MIN_GAP_MS` | `100ms` | برای `beepCount > 1`، گپ کمتر از این مقدار نامعتبر است و بوق خاموش می‌شود. |
| `UI_BUZZER_CHECK_PERCENT` | `10%` | درصد کوچک‌ترین بخش مثبت الگو برای زمان مراجعه بعدی RTOS. |
| `UI_BUZZER_MIN_CHECK_MS` | `1ms` | حداقل زمان بازگشتی برای جلوگیری از مراجعه صفرمیلی‌ثانیه‌ای. |

کدهای بازگشتی:

- `UI_BUZZER_OFF_RESULT` برابر صفر: خاموشی معتبر، مانند `dutyPercent == 0` یا `beepCount == 0`؛ این خطا نیست.
- `UI_BUZZER_INVALID_RESULT` برابر منفی یک: تنظیمات نامعتبر؛ GPIO روی `LOW` قرار می‌گیرد.
- مقدار مثبت: زمان پیشنهادی مراجعه بعدی بر حسب میلی‌ثانیه.

برای یک بوق، گپ بین بوق‌های مجاور وجود ندارد؛ بنابراین مقدار `gapMs` نادیده گرفته می‌شود و محدودیت ۱۰۰ms فقط از `beepCount > 1` اعمال می‌شود.

نمونه استفاده در یک caller صریح RTOS:

```c
int32_t int32_t__nextCheckMs;

int32_t__nextCheckMs = func__Ui_Buzzer_Tick(periodMs, dutyPercent, beepCount, gapMs);
if (int32_t__nextCheckMs > 0)
{
    vTaskDelay(pdMS_TO_TICKS((uint32_t)int32_t__nextCheckMs));
}
else if (int32_t__nextCheckMs == UI_BUZZER_INVALID_RESULT)
{
    /* [EN] Keep the buzzer disabled and handle the invalid request.
       [FA] بوق خاموش است؛ درخواست نامعتبر را مدیریت کن. */
}
```

فرمول سرویس بوق:

```text
dutyWindowMs = periodMs × dutyPercent / 100
effectiveGapMs = (beepCount > 1) ? gapMs : 0
totalGapMs   = effectiveGapMs × (beepCount - 1)
availableOnMs = dutyWindowMs - totalGapMs
beepOnMs     = availableOnMs / beepCount
periodTailMs = periodMs - dutyWindowMs
nextCheckMs  = max(1ms, 10% × smallest_positive(beepOnMs, effectiveGapMs, periodTailMs))
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

اگر `periodMs` صفر، `dutyPercent` صفر یا `beepCount` صفر باشد، بوق خاموش می‌شود و نتیجه صفر است. اگر دوره غیرصفر کمتر از ۱۰۰۰ms باشد، برای چند بوق گپ کمتر از ۱۰۰ms باشد، دیوتی خارج از محدوده باشد، یا گپ‌ها تمام پنجره دیوتی را مصرف کنند، الگو نامعتبر است؛ بوق خاموش می‌ماند و نتیجه منفی یک است.

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی |
|---|---|---|---|
| PB0 | `MCU_R_LED` | LED قرمز | روشن |
| PB1 | `MCU_Y_LED` | LED زرد | روشن |
| PB10 | `MCU_G_LED` | LED سبز | روشن |
| PA4 | `MCU_BUZZER` | بازر | فعال شدن بوق طبق شماتیک |

قطبیت تا اندازه‌گیری روی برد، شماتیکی است. سرویس بوق فقط PA4 را HIGH/LOW می‌کند و فرکانس صوتی PWM تولید نمی‌کند؛ بنابراین این منطق برای Active Buzzer مناسب است.

## پیش‌فرض امن

بعد از Reset و `func__Ui_Init`، LEDها و بوق خاموش هستند. در شروع `func__Ui_BoardTest_Start`، رفتار قبلی تست بوق حفظ می‌شود. در BatteryRun، وقتی درصد باتری زیر `UI_BEEP_START_PCT` باشد، بعد از تعداد سیکل قبلی یک بوق اجرا می‌شود؛ اگر درصد زیر `UI_BEEP_DOUBLE_THRESH_PCT` باشد، مدت آن دو برابر می‌شود. در `InputOk` و `Charging`، سرویس بوق با ورودی خاموشی معتبر متوقف می‌شود. ورودی صفر برای `dutyPercent` یا `beepCount` نیز حالت خاموش امن است.

## درخت اتصال

```text
Firmware/Rtos/Src/task_ui.c
  → func__TaskUi() → func__Ui_Tick()
      ├── func__Ui_ScenarioBatteryRun_Tick()
      │   └── func__Ui_Buzzer_Tick(...)  // بوق هوشمند قبلی با API جدید
      ├── func__Ui_ScenarioInputOk()/Charging_Tick()
      │   └── func__Ui_Buzzer_Tick(0, 0, 0, 0)  // خاموشی امن
      └── func__Ui_BoardTest_Start()
          └── func__Ui_Buzzer_Tick(...)  // بوق تست قبلی
              ├── xTaskGetTickCount()       زمان نمونه فعلی RTOS
              ├── return nextCheckMs         ۱۰٪ کوچک‌ترین بخش مثبت الگو
              └── func__BspGpio_Write()     Firmware/Bsp/Src/bsp_gpio.c
                  └── PIN_BUZZER_PORT/PIN_BUZZER_PIN  = PA4

LED and BUZZER remain separate:
CubeIDE/Core/Src/main.c
  → func__App_Start() → func__App_Init() → func__Ui_Init()
  → func__Rtos_Start() → func__TaskUi() → explicit scenario calls
```
