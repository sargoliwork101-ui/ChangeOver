/**
 * @file    README.md
 * @brief   [EN] UI module sheet: LEDs and buzzer, voltage-based scenarios.
 *          [FA] برگه ماژول UI: ال‌ای‌دی و بازر، سناریوهای مبتنی بر ولتاژ.
 */

# ماژول UI

## وضعیت

فعال. `MODULE_UI = 1`. تنها ماژولی که الان اجرا می‌شود.

### سناریوها (لیست درخواستی کاربر)

| # | نام تابع | شرط انتخاب در `task_ui.c` | رفتار یک سیکل | پارامترهای قابل تنظیم (`ui_config.h`) |
|---|---|---|---|---|
| 1 | `func_Ui_ScenarioInputOk()` | `V_in >= 20V` (`UI_INPUT_THRESHOLD_MV=20000`) و باتری فول (100% = 28V) | سبز ثابت روشن، قرمز/زرد/بازر خاموش؛ سیکل `UI_INPUT_OK_POLL_MS=500ms` | `UI_INPUT_OK_POLL_MS`, `UI_INPUT_THRESHOLD_MV` |
| 2 | `func_Ui_ScenarioBatteryRun(V_bat)` | `V_in < 20V` (ورودی قطع) | **دشارژ**: سبز چشمک با ON = درصد*10ms (100%→990/10، 50%→500/500، 0%→0/1000). **زرد خاموش**. بوق هوشمند از طریق تابع جدید `func_Ui_BuzzerPatternMs(period=0, onTime=beepDur, repeat=1, gap=0)` | `UI_BLINK_PERIOD_MS=1000`, `UI_GREEN_MIN_OFF_MS=10`, `UI_BAT_V_MIN_MV=21000`, `UI_BAT_V_MAX_MV=28000`, `UI_BEEP_BASE_MS=250`, `UI_BEEP_START_PCT=50`, `UI_BEEP_DOUBLE_THRESH_PCT=20` |
| 3 | `func_Ui_ScenarioCharging(V_bat)` | `V_in >= 20V` و باتری <100% (در حال شارژ) | **شارژ**: سبز ثابت روشن (ورودی وصل). زرد نشانگر مانده تا فول: 0% (21V) زرد ثابت روشن، 100% (28V) زرد خاموش، بینشان ON=(100-درصد)*دوره (مثلاً 42%→580/420). | `UI_CHARGING_BLINK_PERIOD_MS=1000`, `UI_CHARGING_YELLOW_MIN_OFF_MS=10`, `UI_BAT_V_MIN_MV`, `UI_BAT_V_MAX_MV` |

#### منطق بوق هوشمند (حالت 2) - با تابع جدید

- تابع بازر جدا جدید: `func_Ui_BuzzerPatternMs(periodMs, onTimeMs, repeatCount, gapMs)` و `func_Ui_BuzzerPatternPercent(periodMs, onTimeMs, repeatCount, gapPercent)`
- ورودی: `periodMs` دوره تناوب تکرار بوق، `onTimeMs` زمان روشن بودن بوق، `repeatCount` تکرار داخل روشن، `gapMs` یا `gapPercent` گپ روشن بودن (اگر تکرار=۱ نادیده)
- مثال: `onTime=1000ms, repeat=2, gap=20%` => `ON400 OFF200 ON400`
- اگر باتری <50%: هر `درصد` ثانیه یک بوق (40%→هر 40 ثانیه) با `period=0, onTime=beepDur, repeat=1, gap=0` از تابع جدید استفاده می‌شود
- اگر باتری <20%: طول بوق 2 برابر (`UI_BEEP_BASE_MS=250` → 500ms)
- پیاده‌سازی: شمارنده استاتیک `UINT32_T_G_BeepCnt` هر سیکل BatteryRun (~1 ثانیه) زیاد می‌شود؛ وقتی به `درصد` رسید بوق با تابع جدید و ریست

#### نگاشت ولتاژ به درصد

- باتری صفر درصد صفر ولت نیست: `UI_BAT_V_MIN_MV=21000` (21V) = 0%
- فول شارژ: `UI_BAT_V_MAX_MV=28000` (28V) = 100%
- فرمول: `pct = (Vbat - Vmin)*100 / (Vmax-Vmin)` محدود 0..100
- تابع `Ui_BatteryVoltageToPercent(Vbat_mV)` همین کار را می‌کند.
- ورودی: `V_in < 20000mV` یعنی ورودی نداریم، `>=20000` داریم.

#### نام‌گذاری متغیر (درخواستی کاربر - قانون جدید)

- اول تایپ کامل بعد نام: `uint32_t`, `uint8_t`, `bool`
- گلوبال: تایپ بزرگ با `G_`: `UINT32_T_G_InputVoltageMv`, `UINT32_T_G_BatteryVoltageMv`, `UINT32_T_G_BeepCnt`
- داخلی: تایپ کوچک: `uint32_t_inputVoltageMv`, `uint8_t_batteryPercent`, `uint32_t_onMs`, `uint32_t_offMs`, `bool_inputPresent`
- تابع خودمان: پیشوند `func_` مثل `func_Ui_Init`, `func_Ui_BuzzerPatternMs`, `func_TaskUi`
- استاتیک سطح فایل هم گلوبال: `UINT32_T_G_BeepCnt`

تست بدون ADC: دو متغیر `volatile` در `task_ui.c` (`UINT32_T_G_InputVoltageMv`، `UINT32_T_G_BatteryVoltageMv`) که در دیباگر Live Expressions زنده عوض می‌شوند. تغییر سناریو حداکثر بعد از پایان سیکل جاری (0.5 تا 1 ثانیه) اعمال می‌شود.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-15 | ساده‌سازی بازر: توابع اضافی حذف، فقط ۲ تابع ماند `func_Ui_BuzzerPatternMs/Percent` با ورودی دوره تناوب، زمان روشن، تکرار داخل روشن، گپ ms/درصد؛ اگر تکرار=۱ گپ نادیده؛ داخل سناریوهای LED (BatteryRun) از تابع جدید استفاده می‌شود؛ ثابت‌های اضافی MAX_* حذف، فقط DEFAULT_GAP_PERCENT ماند |
| 2026-09-15 | بازر سناریو جدا با توابع مختلف: `func_Ui_BuzzerPatternOnceMs/Percent`, `PeriodicMs/Percent`, `RepeatMs/Percent` با ورودی دوره تناوب، زمان روشن، تکرار داخل روشن، گپ ms یا درصد؛ اگر تکرار=۱ گپ نادیده؛ مثال ۱۰۰۰ms، تکرار۲، گپ۲۰٪ => ON400 OFF200 ON400؛ مدیریت حافظه (بدون malloc) به AI اضافه شد |
| 2026-09-14 | ساده‌سازی LED طبق قانون جدید AI: هر تابع یک کار، خطی روشن/تاخیر/خاموش؛ `green/red/yellow/buzzer/all_off` یک خط `BspGpio_Write`، `Ui_BuzzerBeep` ساده، سناریوها ۵ گام خطی؛ قانون سادگی به `AI_CONTEXT.md` اضافه شد |
| 2026-09-14 | تمیزکاری: همه مین/ماکس و تایم‌ها به فایل واحد `ui_config.h` منتقل شد تا تکراری در ۲-۳ فایل نباشد؛ `ui.c`، `task_ui.c`، `app_config.c` و `host_test_ui.py` فقط همین را می‌خوانند (single source) |
| 2026-09-14 | بازنویسی کامل طبق درخواست جدید: حذف سناریوی سوم (BatteryLow)، زرد در دشارژ خاموش، بوق هوشمند با تابع جدا `Ui_BuzzerBeep()` (اگر <50% هر درصد ثانیه یک بوق، 40%→40s، اگر <20% طول بوق 2 برابر)، سناریوی شارژ جدید با زرد چشمک‌زن (0% زرد ثابت روشن، 100% خاموش، ON=(100-درصد)*دوره)، ورودی از bool به ولتاژ (آستانه 20V)، باتری 0%=21V و 100%=28V با تابع `Ui_BatteryVoltageToPercent()`، نام‌گذاری با پیشوند تایپ (U32_G_ گلوبال، u32_ داخلی) |
| 2026-09-14 | اجرای AI: اسکریپت چک قوانین `tools/check_ai_rules.sh` + تست هاست `host_test_ui.py`؛ پاس شد |
| 2026-09-14 | بازنویسی به سبک سناریویی خطی: هر سناریو یک سیکل اجرا و برمی‌گردد (`Ui_ScenarioInputOk/BatteryRun/BatteryLow`) |
| 2026-09-14 | سناریوهای مبتنی بر وضعیت با `Ui_Indicate` و enum سه حالته؛ حذف `Ui_Scenario1/2` و `UI_FLAG` |
| 2026-09-14 | درخت اتصال UI کامل شد |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه یکدست شد |
| 2026-09 | کامنت خط‌به‌خط انگلیسی روی `Ui_Scenario1` / `Ui_Scenario2` |
| 2026-09 | `Ui_BoardTest` یک‌بار، بعد `UI_FLAG` یکی از دو سناریو |
| 2026-09 | الگوها داخل `ui.c`؛ تسک فقط انتخاب می‌کند |

## فایل‌ها

| فایل | نقش |
|---|---|
| `ui_config.h` | **فایل واحد مین/ماکس و تایم‌ها** — همه آستانه‌های قابل تنظیم اینجاست (Vmin=21V, Vmax=28V, Vth=20V, blink, beep) تا تکراری در ۲-۳ فایل نباشد؛ `ui.c`، `task_ui.c`، `app_config.c` و `host_test_ui.py` همین را می‌خوانند |
| `ui.h` / `ui.c` | API و 3 تابع سناریو + تابع جدا بازر + تبدیل ولتاژ به درصد؛ دیگر ثابت تکراری ندارد، فقط `#include ui_config.h` |
| `../../Rtos/Src/task_ui.c` | تسک؛ متغیرهای تست ولتاژ `U32_G_InputVoltageMv` / `U32_G_BatteryVoltageMv` (volatile)؛ انتخاب سناریو بر اساس ولتاژ؛ دیگر ثابت تکراری ندارد، فقط `ui_config.h` را include می‌کند |
| `../../Bsp/Src/bsp_gpio.c` | نوشتن پایه |
| `../../Config/Inc/board_pins.h` | شماره پایه |
| `../../Config/Inc/app_config.h` / `Src/app_config.c` | مقادیر پیش‌فرض از `ui_config.h` می‌آیند (single source) تا تکراری نباشد: `ui_input_threshold_mv`, `ui_bat_v_min/max`, `charging`, `beep` |
| `host_test_ui.py` | تست هاست: آستانه‌ها را از `ui_config.h` می‌خواند (parse #define) تا تکراری نباشد؛ نگاشت 21V=0% 28V=100% |

`host_test_ui.py` به بیلد ARM نمی‌رود، فقط روی هاست اجرا می‌شود.

## توابع

| نام | کار |
|---|---|
| `func_Ui_Init` | همه خروجی UI خاموش؛ فقط یک‌بار از `App_Init` قبل از زمان‌بند |
| `func_Ui_BoardTest` | یک‌بار قرمز، زرد، سبز، بوق با تابع جدید `func_Ui_BuzzerPatternMs(0, BOOT_BEEP_MS, 1, 0)`؛ برمی‌گردد |
| `func_Ui_BuzzerPatternMs` | **سناریو جدا بازر**: ورودی دوره تناوب `periodMs`، زمان روشن `onTimeMs` کل شامل گپ‌ها، تکرار داخل روشن `repeatCount` ۱..۱۰، گپ `gapMs`؛ اگر تکرار=۱ گپ نادیده؛ مثال ۱۰۰۰ms، تکرار۲، گپ۲۰۰ => ON400 OFF200 ON400؛ اگر period=0 یا <=onTime فقط یک‌بار |
| `func_Ui_BuzzerPatternPercent` | سناریو جدا بازر با گپ درصدی: `periodMs`، `onTimeMs`، `repeatCount`، `gapPercent` درصد از کل زمان روشن؛ اگر تکرار=۱ گپ بکار نمی‌رود؛ مثال ۱۰۰۰ms، تکرار۲، ۲۰٪ => گپ۲۰۰، هر بوق ۴۰۰ |
| `func_Ui_BatteryVoltageToPercent` | تبدیل ولتاژ باتری (mV) به درصد 0..100 با `UI_BAT_V_MIN_MV=21000` و `UI_BAT_V_MAX_MV=28000`؛ فرمول `(V-Vmin)*100/(Vmax-Vmin)` |
| `func_Ui_ScenarioInputOk` | یک سیکل سبز ثابت، بقیه خاموش؛ پارامتر `UI_INPUT_OK_POLL_MS` |
| `func_Ui_ScenarioBatteryRun` | ورودی: `uint32_t_batteryMv`؛ یک سیکل چشمک سبز ON=درصد*10ms؛ **زرد خاموش**؛ بوق هوشمند با تابع جدید: اگر `pct<50` هر درصد ثانیه یک بوق با `func_Ui_BuzzerPatternMs(0, beepDur, 1, 0)`، اگر `<20` طول 2 برابر؛ شمارنده `UINT32_T_G_BeepCnt` |
| `func_Ui_ScenarioCharging` | ورودی: `uint32_t_batteryMv`؛ سبز ثابت روشن؛ زرد: 0% ثابت روشن، 100% خاموش، بینشان ON=(100-درصد)*دوره |
| `func_TaskUi` | تست برد، بعد حلقه: `uint32_t_inputVoltageMv = UINT32_T_G_InputVoltageMv`، `uint32_t_batteryVoltageMv = UINT32_T_G_BatteryVoltageMv`، `uint8_t_batteryPercent = func_Ui_BatteryVoltageToPercent(...)`، `bool_inputPresent = (uint32_t_inputVoltageMv >= UI_INPUT_THRESHOLD_MV_TASK)`؛ اگر ورودی وصل و باتری <100% → `Charging`، اگر فول → `InputOk`، اگر قطع → `BatteryRun` |
| `func_green` (static) | PB10، پارامتر `bool_on` (bool) |
| `func_red` (static) | PB0 |
| `func_yellow` (static) | PB1 |
| `func_buzzer` (static) | PA4، سطح پایین |
| `func_all_off` (static) | هر چهار تا Low |
| `func_calc_beep_on` (static) | محاسبه زمان روشن هر بوق داخل زمان کل روشن با تکرار و گپ |

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی |
|---|---|---|---|
| PB0 | `MCU_R_LED` | LED قرمز Q4 | روشن (این مرحله استفاده نمی‌شود) |
| PB1 | `MCU_Y_LED` | LED زرد Q5 | روشن — در شارژ: 0% ثابت روشن، 100% خاموش |
| PB10 | `MCU_G_LED` | LED سبز Q6 | روشن — در InputOk ثابت، در BatteryRun چشمک |
| PA4 | `MCU_BUZZER` | بازر Q7 | صدا — تابع جدا `Ui_BuzzerBeep()` |

همه خروجی، Push-Pull، بعد Reset باید Low باشند.

## پیش‌فرض امن

`func_Ui_Init` هر چهار پایه را Low می‌کند. هر سناریو خروجی‌های نامرتبط را خاموش می‌کند. در BatteryRun زرد خاموش می‌ماند. در Charging قرمز و بازر خاموش (بازر فقط از BatteryRun با تابع جدید صدا زده می‌شود).

## درخت اتصال

صدا زده می‌شود از:

```text
main.c → func_App_Start() → app.c
  func_App_Init() → func_Ui_Init()        (یک‌بار، قبل از زمان‌بند)
  func_Rtos_Start() → rtos_app.c → func_TaskUi → task_ui.c
    func_Ui_BoardTest()              (یک‌بار) -> func_Ui_BuzzerPatternMs(0, BOOT_BEEP_MS, 1, 0)
    حلقه ولتاژ-مبنا:
      uint32_t_inputVoltageMv = UINT32_T_G_InputVoltageMv (volatile, Live Expressions)
      uint32_t_batteryVoltageMv = UINT32_T_G_BatteryVoltageMv
      uint8_t_batteryPercent = func_Ui_BatteryVoltageToPercent(uint32_t_batteryVoltageMv) // 21V=0% 28V=100%
      bool_inputPresent = (uint32_t_inputVoltageMv >= 20000)
      if (bool_inputPresent)
        if (uint8_t_batteryPercent < 100)  func_Ui_ScenarioCharging(uint32_t_batteryVoltageMv) // زرد: 0% ثابت روشن، 100% خاموش
        else                               func_Ui_ScenarioInputOk() // سبز ثابت
      else
        func_Ui_ScenarioBatteryRun(uint32_t_batteryVoltageMv) // سبز چشمک + بوق هوشمند با تابع جدید
          -> func_Ui_BuzzerPatternMs(0, beepDur, 1, 0) // تابع جدا بازر با دوره ۰، تکرار ۱، گپ نادیده
          -> func_Ui_BuzzerPatternPercent(periodMs, onTimeMs, repeatCount, gapPercent) // سناریو جدا با گپ درصدی
```

این ماژول صدا می‌زند:

```text
ui.c
  bsp_gpio.h / bsp_gpio.c     func_BspGpio_Write → PIN_LED_*, PIN_BUZZER_*
  board_pins.h                PIN_LED_G/Y/R, PIN_BUZZER
  app_config.h / app_config.c APP_CONFIG.ui_* (همچنین پارامترهای محلی بالای ui.c)
  FreeRTOS.h / task.h         vTaskDelay
  func_Ui_BatteryVoltageToPercent  تبدیل 21V=0% 28V=100%
  func_Ui_BuzzerPatternMs/Percent  تابع جدا بازر با دوره تناوب، زمان روشن، تکرار، گپ
```

به ADC، PWM، UART وصل نیست. ولتاژها فعلاً دستی‌اند (UINT32_T_G_... volatile)؛ بعداً از Measurement می‌آیند. مدیریت حافظه: بدون malloc، فقط استاتیک/استک.
