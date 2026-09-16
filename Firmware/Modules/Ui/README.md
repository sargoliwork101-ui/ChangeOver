/**
 * @file    README.md
 * @brief   [EN] UI module sheet: LED scenarios remain separate from the single periodic buzzer service.
 *          [FA] برگه ماژول UI: سناریوهای LED از سرویس یگانه بوق دوره‌ای جدا هستند.
 */

# ماژول UI

## وضعیت

فعال. `MODULE_UI = 1`. LED و بوق در دو فایل مستقل هستند. سرویس بوق همچنان فقط یک API دارد؛ سناریوهای `BoardTest`، `BatteryRun` و خطای `InputOverVoltage` آن را صریحاً صدا می‌زنند و سناریوهای `InputOk` و `Charging` آن را خاموش می‌کنند.

API کاربردی بوق فقط یک تابع است:

```c
int32_t func__Ui_Buzzer_Tick(periodMs, dutyPercent, beepCount, gapMs)
```

این تابع غیرمسدودکننده است. در الگوی معتبر، مقدار بازگشتی زمان پیشنهادی مراجعه بعدی بر حسب میلی‌ثانیه است؛ این زمان برابر ۱۰٪ کوچک‌ترین بخش مثبت الگو است. مقدار `0` خاموشی معتبر و مقدار `-1` تنظیمات نامعتبر را نشان می‌دهد. فراخوانندهٔ صریح می‌تواند مقدار مثبت را به `func__Rtos_DelayMilliseconds` بدهد و نیازی نیست برای دوره‌های بزرگ، تسک را با فاصلهٔ ثابت و کوتاه بیدار کند.

## سناریوهای توافق‌شده UI

> این بخش مشخصات کامل سناریوها است. همه ولتاژها در منطق Firmware بر حسب میلی‌ولت هستند و نام ثابت هر عدد کنار همان سناریو آمده است.

### قواعد مشترک تشخیص ورودی و هیسترزیس

- ورودی نامی سیستم ۲۴ ولت است؛ آستانه‌های نرم‌افزاری برای جلوگیری از سوئیچ اشتباه استفاده می‌شوند.
- `Input >= UI_INPUT_CONNECTED_THRESHOLD_MV` یعنی ورودی وصل است؛ مقدار فعلی `21000u` یعنی ۲۱ ولت.
- `Input <= UI_INPUT_DISCONNECTED_THRESHOLD_MV` یعنی ورودی قطع است؛ مقدار فعلی `20000u` یعنی ۲۰ ولت.
- بین ۲۰ و ۲۱ ولت، وضعیت قبلی حفظ می‌شود و سیستم بین حالت‌ها سوئیچ نمی‌کند.
- مقدار هیسترزیس تشخیص اتصال: `UI_INPUT_HYSTERESIS_MV = 1000u`.
- در مسیر production، سیگنال منطقی `snapshot.input_present` مرجع اتصال ورودی است؛ ولتاژ `snapshot.v_in_mv` برای خطای اضافه‌ولتاژ استفاده می‌شود.
- ثابت‌های قدیمی اتصال و هیسترزیس در `ui_led.h` برای مرجع سناریو نگه داشته شده‌اند؛ تصمیم runtime از snapshot معتبر می‌آید.

### سناریو ۱: InputOk

- شرط: ورودی وصل باشد و ولتاژ باتری به حد شارژ کامل رسیده باشد.
- مثال توافق‌شده:
  ```text
  Input = 24V ، Battery = 28V
  → سبز دائم، زرد و قرمز خاموش
  ```
- چراغ سبز دائم روشن است.
- چراغ زرد خاموش است.
- چراغ قرمز خاموش است.
- بوق خاموش است.
- محدودهٔ کامل‌بودن باتری از `UI_BAT_V_MAX_MV = 28000u` تعیین می‌شود.
- آستانهٔ اتصال ورودی از `UI_INPUT_CONNECTED_THRESHOLD_MV = 21000u` تعیین می‌شود.
- اگر ورودی وصل باشد ولی باتری هنوز به ۲۸ ولت نرسیده باشد، سناریوی Charging اجرا می‌شود.

### سناریو ۲: BatteryRun

- شرط: ورودی در وضعیت قطع باشد؛ یعنی ورودی به `UI_INPUT_DISCONNECTED_THRESHOLD_MV` رسیده باشد.
- مثال توافق‌شده:
  ```text
  Input = 0V یا 19V
  → BatteryRun
  ```
- در بازهٔ ۲۰ تا ۲۱ ولت، وضعیت قبلی ورودی حفظ می‌شود و هیسترزیس اعمال می‌شود.
- چراغ سبز چشمک‌زن است.
- چراغ زرد خاموش است.
- چراغ قرمز خاموش است.
- دیوتی روشن‌بودن چراغ سبز بر اساس درصد باتری تعیین می‌شود؛ درصد باتری از ولتاژ باتری محاسبه می‌شود.
- محدودهٔ تبدیل درصد باتری:
  - `UI_BAT_V_MIN_MV = 21000u` → صفر درصد
  - `UI_BAT_V_MAX_MV = 28000u` → صد درصد
- هرچه ولتاژ باتری از ۲۸ ولت به ۲۱ ولت نزدیک‌تر شود، درصد باتری کمتر و دیوتی روشن‌بودن سبز به صفر نزدیک‌تر می‌شود.
- ثابت‌های زمان چشمک:
  - `UI_BLINK_PERIOD_MS`
  - `UI_GREEN_MIN_OFF_MS`

#### جدول بوق BatteryRun

اولویت از بحرانی‌ترین وضعیت به کم‌خطرترین وضعیت است:

| محدوده درصد باتری | رفتار بوق | دوره تکرار | دیوتی تقریبی | مدت تقریبی هر بوق | تعداد بوق | ثابت‌های مربوط |
|---|---|---:|---:|---:|---:|---|
| `Battery < 1%` | یک بوق ممتد؛ بعد از پایان بوق چراغ‌ها و بوق خاموش می‌مانند و تا وقتی زیر ۱٪ است تکرار نمی‌شود | یک‌بار | `100%` | `10000ms` | ۱ | `UI_BATTERY_RUN_BEEP_CRITICAL_PERCENT`، `UI_BATTERY_RUN_BEEP_CRITICAL_PERIOD_MS`، `UI_BATTERY_RUN_BEEP_CRITICAL_DUTY_PERCENT`، `UI_BATTERY_RUN_BEEP_CRITICAL_DURATION_MS`، `UI_BATTERY_RUN_BEEP_CRITICAL_COUNT` |
| `1% <= Battery < 10%` | سه بوق | `20000ms` | `31%` | `2000ms` | ۳ | `UI_BATTERY_RUN_BEEP_TRIPLE_PERCENT`، `UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS`، `UI_BATTERY_RUN_BEEP_TRIPLE_DUTY_PERCENT`، `UI_BATTERY_RUN_BEEP_TRIPLE_DURATION_MS`، `UI_BATTERY_RUN_BEEP_TRIPLE_COUNT` |
| `10% <= Battery < 20%` | دو بوق | `60000ms` | `4%` | حدود `1150ms` | ۲ | `UI_BATTERY_RUN_BEEP_DOUBLE_PERCENT`، `UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS`، `UI_BATTERY_RUN_BEEP_DOUBLE_DUTY_PERCENT`، `UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS`، `UI_BATTERY_RUN_BEEP_DOUBLE_COUNT` |
| `20% <= Battery < 40%` | یک بوق | `60000ms` | `2%` | حدود `1200ms` | ۱ | `UI_BATTERY_RUN_BEEP_START_PERCENT`، `UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS`، `UI_BATTERY_RUN_BEEP_STANDARD_DUTY_PERCENT`، `UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS`، `UI_BATTERY_RUN_BEEP_STANDARD_COUNT` |
| `Battery >= 40%` | بوق خاموش | — | — | — | — | `UI_BATTERY_RUN_BEEP_START_PERCENT` |

- گپ بین بوق‌های چندگانه `UI_BATTERY_RUN_BEEP_GAP_MS = 100u` میلی‌ثانیه است.
- زمان بوق‌های استاندارد تقریبی است؛ دیوتی صحیح درصدی عمداً برای سادگی استفاده می‌شود.
- در حالت زیر ۱٪، چراغ سبز، زرد و قرمز همگی خاموش می‌شوند.
- اعداد درصدی، duty و زمانی جدول در `Firmware/Modules/Ui/ui_led.h` تعریف شده‌اند.

### سناریو ۳: Charging

- شرط: ورودی وصل باشد و باتری هنوز به ولتاژ کامل نرسیده باشد.
- مثال توافق‌شده:
  ```text
  Input = 24V ، Battery = 25V
  → سبز روشن، زرد چشمک‌زن، قرمز خاموش
  ```
- چراغ سبز دائم روشن است.
- چراغ زرد چشمک‌زن است.
- چراغ قرمز خاموش است.
- بوق خاموش است.
- دیوتی چشمک چراغ زرد بر اساس درصد باقی‌مانده تا شارژ کامل تعیین می‌شود.
- هرچه باتری به ۱۰۰٪ نزدیک‌تر شود، دیوتی روشن‌بودن چراغ زرد کمتر می‌شود.
- محدودهٔ درصد شارژ همان محدودهٔ `UI_BAT_V_MIN_MV = 21000u` تا `UI_BAT_V_MAX_MV = 28000u` است.
- ثابت‌های زمان و محدودیت چشمک زرد:
  - `UI_CHARGING_BLINK_PERIOD_MS`
  - `UI_CHARGING_YELLOW_MIN_OFF_MS`

### سناریو ۴: InputOverVoltage

- منبع خطا فقط **ولتاژ ورودی** است، نه ولتاژ باتری.
- شرط فعال‌شدن: `Input > UI_INPUT_OVERVOLTAGE_THRESHOLD_MV`؛ مقدار فعلی بیشتر از `28000mV` یعنی بیشتر از ۲۸ ولت.
- هیسترزیس خطا: `UI_INPUT_OVERVOLTAGE_HYSTERESIS_MV = 1000u`.
- شرط پاک‌شدن: `Input <= UI_INPUT_OVERVOLTAGE_CLEAR_THRESHOLD_MV`؛ مقدار فعلی `27000mV` یعنی ۲۷ ولت یا کمتر.
- در بازهٔ ۲۷ تا ۲۸ ولت، وضعیت خطا حفظ می‌شود.
- چراغ سبز دائم روشن است.
- چراغ زرد خاموش است.
- چراغ قرمز هر ۱ ثانیه با دیوتی ۵۰٪ چشمک می‌زند.
- بوق هر ۱۰ ثانیه، یک بوق یک‌ثانیه‌ای می‌زند.
- ثابت‌های نمایش خطا:
  - `UI_INPUT_OVERVOLTAGE_THRESHOLD_MV`
  - `UI_INPUT_OVERVOLTAGE_HYSTERESIS_MV`
  - `UI_INPUT_OVERVOLTAGE_CLEAR_THRESHOLD_MV`
  - `UI_INPUT_OVERVOLTAGE_LED_PERIOD_MS = 1000u`
  - `UI_INPUT_OVERVOLTAGE_LED_DUTY_PERCENT = 50u`
  - `UI_INPUT_OVERVOLTAGE_BEEP_PERIOD_MS = 10000u`
  - `UI_INPUT_OVERVOLTAGE_BEEP_DURATION_MS = 1000u`
  - `UI_INPUT_OVERVOLTAGE_BEEP_DUTY_PERCENT`
  - `UI_INPUT_OVERVOLTAGE_BEEP_COUNT = 1u`
  - `UI_INPUT_OVERVOLTAGE_BEEP_GAP_MS = 0u`

### خلاصه انتخاب سناریوها

```text
InputOverVoltage فعال
    → نمایش خطای اضافه‌ولتاژ و اجرای بوق خطا

در غیر این صورت، اگر ورودی قطع باشد
    → BatteryRun

در غیر این صورت، اگر ورودی وصل و باتری کمتر از 28V باشد
    → Charging

در غیر این صورت، اگر ورودی وصل و باتری حداقل 28V باشد
    → InputOk
```

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-16 | افزودن `UI_Board_Validation.xlsx` برای تست عملی روی برد، ثبت نتیجه و ایراد و تأیید نهایی سناریوهای LED/BUZZER. |
| 2026-09-15 | اولین ساخت کامل سناریوهای UI: InputOk، Charging، BatteryRun با چهار بازه بوق، InputOverVoltage، هیسترزیس ورودی و مستندات کامل ثابت‌ها. |
| 2026-09-15 | ساده‌سازی کامل بوق: حذف APIهای Start/Tick میلی‌ثانیه‌ای و درصدی، حذف Stop و توابع داخلی اضافی؛ باقی ماندن یک تابع عمومی با ورودی‌های دوره، دیوتی، تعداد بوق و گپ. |
| 2026-09-15 | بوق از منطق خودکار BatteryRun و تست LED جدا شد؛ LED فقط مالک LEDها است و بوق فقط سیگنال منطقی `BSP_GPIO_BUZZER` را مصرف می‌کند. |
| 2026-09-15 | تعریف محاسبه جدید: پنجره دیوتی برابر `period*duty/100` است، گپ‌ها داخل این پنجره قرار می‌گیرند و زمان باقی‌مانده تا دوره بعدی خاموش است. |
| 2026-09-15 | اتصال دوباره سرویس جدید به رفتار قبلی سناریوها: بوق کوتاه BoardTest و بوق هوشمند BatteryRun؛ InputOk و Charging بوق را خاموش می‌کنند. |
| 2026-09-15 | افزودن محدودیت‌های ایمنی با ثابت‌های حداقل دوره ۱۰۰۰ms و حداقل گپ ۱۰۰ms؛ صفر برای خاموشی معتبر و منفی یک برای خطا؛ بازگرداندن زمان مراجعه پیشنهادی ۱۰٪ برای RTOS. |
| 2026-09-15 | اصلاح مستندات با ساختار واقعی دو بخش LED و BUZZER و استفاده از `APP_CONFIG` برای مقدارهای قابل تنظیم LED. |
| 2026-09-14 | اجرای AI: اسکریپت چک قوانین `tools/check_ai_rules.sh` + تست هاست UI؛ پاس شد. |

## فایل‌ها

| فایل | نقش |
|---|---|
| `ui_led.h` / `ui_led.c` | منطق LED: نگاشت ولتاژ، هیسترزیس ورودی، خطای InputOverVoltage، سناریوهای InputOk/Charging/BatteryRun و تست LED؛ فقط نقاط صریح سناریو برای شروع یا خاموش کردن سرویس بوق را فراخوانی می‌کند. |
| `ui_buzzer.h` / `ui_buzzer.c` | سرویس یگانه بوق: محاسبه پنجره دیوتی، تقسیم آن بین پالس‌ها و گپ‌ها، اعتبارسنجی محدودیت‌های ایمنی، محاسبه مراجعه بعدی و نوشتن سیگنال منطقی `BSP_GPIO_BUZZER`. |
| `../../Rtos/Src/task_ui.c` | تسک UI؛ یک snapshot از Measurement را می‌خواند و فقط در صورت `valid=true` با `func__Ui_Tick` سناریوی مناسب را اجرا می‌کند. |
| `../../Bsp/Src/bsp_gpio.c` | نوشتن سطح GPIO از طریق `func__BspGpio_Write`. |
| `../../Bsp/Inc/bsp_gpio.h` | سیگنال‌های منطقی `BSP_GPIO_LED_GREEN/RED/YELLOW` و `BSP_GPIO_BUZZER`؛ نگاشت پایه در BSP پنهان است. |
| `../../Config/Inc/app_config.h` / `../../Config/Src/app_config.c` | تنظیمات عمومی زمان‌بندی LED و نگاشت ولتاژ؛ ثابت‌های BatteryRun و بوق‌های سناریویی در هدرهای UI تعریف شده‌اند. |
| `host_test_ui.py` | تست هاست فرمول دوره، دیوتی، تعداد پالس و گپ. |
| `UI_Board_Validation.xlsx` | برگهٔ ثبت تست عملی روی برد: برنامهٔ تست، مرجع سناریوها، ثبت ایراد و تأیید نهایی. |

## برگه تست و اعتبارسنجی روی برد

فایل `UI_Board_Validation.xlsx` برای ولیدیشن عملی همین رفتار فعلی UI روی برد است. این فایل پنج برگهٔ کاری دارد: راهنما، برنامهٔ تست، مراجع سناریو، ثبت ایراد و تأیید نهایی؛ برگهٔ فهرست‌های داخلی Excel برای dropdownها مخفی است.

- دامنهٔ این فرم فقط رفتار LED و BUZZER است؛ ADC/Measurement فعال است، اما نتیجهٔ ADC در این فرم ثبت نمی‌شود.
- UI یک snapshot منسجم را با `func__Measurement_GetSnapshot` می‌گیرد و فقط وقتی `snapshot.valid=true` باشد تصمیم می‌گیرد.
- ولتاژ ورودی و باتری به‌ترتیب از `snapshot.v_in_mv` و `snapshot.v_bat24_mv` بر حسب mV می‌آیند؛ مقدار دستی باتری در مسیر production وجود ندارد.
- `BOOL__G__UiBatteryAlarmIssued` فقط توسط UI نوشته می‌شود؛ در snapshot نامعتبر false و هنگام آلارم معتبر باتری کم true است.
- برای هر ردیف، مقدار واقعی مشاهده‌شده، وضعیت، نام تست‌کننده و تاریخ را ثبت کن. تفاوت با انتظار را با شناسهٔ `BUG-xxx` در برگهٔ «ثبت ایراد» هم بنویس.
- دوره و مدت بوق‌ها برای سریع‌شدن تست تغییر نکند؛ بوق‌های ۲۰ و ۶۰ ثانیه‌ای با دورهٔ واقعی سناریو تست شوند.
- قطبیت پایه‌های `PA4`، `PB0`، `PB1` و `PB10` تا زمان اندازه‌گیری روی برد شماتیکی است؛ مقدار واقعی باید در تست پایه‌ها ثبت شود.
- برگهٔ «تأیید نهایی» فقط پس از قبول تست‌ها و بسته‌شدن ایرادهای باز تکمیل شود.

## توابع

| نام | کار | فایل |
|---|---:|---|
| `func__Ui_Buzzer_Tick` | تنها API کاربردی بوق؛ یک الگوی دوره‌ای را با چهار ورودی اجرا می‌کند، GPIO را به‌روزرسانی می‌کند و زمان مراجعه بعدی یا کد وضعیت را برمی‌گرداند | `ui_buzzer.c` |
| `func__Ui_Init` | خاموش کردن LEDها؛ بوق در مالکیت سرویس مستقل خودش است | `ui_led.c` |
| `func__Ui_BoardTest_Start` | تست یک‌باره قرمز، زرد و سبز، سپس بوق کوتاه قبلی با API جدید | `ui_led.c` |
| `func__Ui_ScenarioInputOk` | سبز ثابت، قرمز/زرد خاموش و بوق خاموش | `ui_led.c` |
| `func__Ui_ScenarioCharging_Tick` | سبز ثابت و زرد متناسب با درصد شارژ؛ بوق خاموش | `ui_led.c` |
| `func__Ui_ScenarioBatteryRun_Tick` | سبز چشمک‌زن، زرد خاموش و بوق‌های جدید بر اساس بازه‌های زیر ۴۰٪، ۲۰٪، ۱۰٪ و ۱٪ | `ui_led.c` |
| `func__Ui_ScenarioInputOverVoltage_Tick` | خطای ورودی: سبز ثابت، زرد خاموش، قرمز ۵۰٪ و یک بوق یک‌ثانیه‌ای هر ۱۰ ثانیه | `ui_led.c` |
| `func__Ui_Tick` | بررسی اعتبار snapshot، به‌روزرسانی وضعیت ورودی/اضافه‌ولتاژ و انتخاب سناریوی LED بر اساس ورودی و باتری | `ui_led.c` |

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
    func__Rtos_DelayMilliseconds((uint32_t)int32_t__nextCheckMs);
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

جدول بالا مرجع فیزیکی برد فعلی است؛ منطق UI پایهٔ فیزیکی را نمی‌شناسد و فقط سیگنال‌های `BSP_GPIO_*` را مصرف می‌کند. قطبیت تا اندازه‌گیری روی برد، شماتیکی است. سرویس بوق سیگنال منطقی `BSP_GPIO_BUZZER` را HIGH/LOW می‌کند و فرکانس صوتی PWM تولید نمی‌کند؛ بنابراین این منطق برای Active Buzzer مناسب است.

## پیش‌فرض امن

بعد از Reset و `func__Ui_Init`، LEDها و بوق خاموش هستند. در شروع `func__Ui_BoardTest_Start`، رفتار تست بوق حفظ می‌شود. در BatteryRun، بوق‌ها طبق جدول سناریو اجرا می‌شوند: زیر ۴۰٪ یک بوق، زیر ۲۰٪ دو بوق، زیر ۱۰٪ سه بوق و زیر ۱٪ یک بوق ممتد ده‌ثانیه‌ای؛ بعد از بوق بحرانی زیر ۱٪، همه خروجی‌ها خاموش می‌مانند تا باتری از این محدوده خارج شود. در `InputOk` و `Charging`، سرویس بوق با ورودی خاموشی معتبر متوقف می‌شود. ورودی صفر برای `dutyPercent` یا `beepCount` نیز حالت خاموش امن است.

## درخت اتصال

```text
Firmware/Rtos/Src/task_ui.c
  → func__TaskUi() → func__Ui_Tick()
      ├── InputOverVoltage (ورودی >28V، پاک‌سازی <=27V)
      │   ├── red: period=1000ms, duty=50%
      │   └── func__Ui_Buzzer_Tick(10000, 10, 1, 0)  // بوق 1s هر 10s
      ├── func__Ui_ScenarioBatteryRun_Tick()
      │   └── func__Ui_Buzzer_Tick(...)  // بوق‌های درصدی جدید BatteryRun
      ├── func__Ui_ScenarioInputOk()/Charging_Tick()
      │   └── func__Ui_Buzzer_Tick(0, 0, 0, 0)  // خاموشی امن
      └── func__Ui_BoardTest_Start()
          └── func__Ui_Buzzer_Tick(...)  // بوق تست قبلی
              ├── osKernelGetTickCount()   زمان نمونه فعلی CMSIS-RTOS2
              ├── return nextCheckMs         ۱۰٪ کوچک‌ترین بخش مثبت الگو
              └── func__BspGpio_Write()     Firmware/Bsp/Src/bsp_gpio.c
                  └── BSP_GPIO_BUZZER              = نگاشت پایه در bsp_gpio.c

LED and BUZZER remain separate:
CubeIDE/Core/Src/main.c
  → func__App_Start() → func__App_Init()
  → func__Rtos_Start() → func__TaskUi() → func__Ui_Init() → explicit scenario calls
```
