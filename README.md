/**
 * @file    README.md
 * @brief   [EN] ChangeOver project front page: overview and file connections.
 *          [FA] صفحه اول پروژه ChangeOver: معرفی و درخت اتصال فایل‌ها.
 */

# ChangeOver

تغییر مسیر تغذیه ۲۴ ولت DC: ورودی یا باتری. MCU: `STM32F103C8T6`.

**مرحله فعلی:** فقط LED و بازر. ADC، PWM، UART، رله خاموش.

شماتیک: `Circuit/ChangeOver(24V_DC).pdf`

قوانین دستیار: `Firmware/AI_CONTEXT.md`

## پوشه‌ها

| پوشه | چیست |
|---|---|
| `Circuit/` | شماتیک |
| `Firmware/` | کد محصول (App, Bsp, Config, Modules, Rtos) |
| `CubeMX/` | فایل `.ioc` |
| `CubeIDE/` | پروژه STM32CubeIDE بعد از Generate |

`main.c` فقط کلاک، HAL، `MX_*_Init`، بعد `App_Start()`. منطق محصول در `Firmware/`.

کلید ماژول‌ها: `Firmware/Config/Inc/modules_enable.h` — الان فقط `MODULE_UI = 1`.

## اجرا الان (منطق جدید ولتاژ-مبنا)

```text
CubeIDE/Core/.../main.c
  func__App_Start()                  Firmware/App/Src/app.c
    func__Ui_Init()                  Firmware/Modules/Ui/ui_led.c
    func__Rtos_Start()               Firmware/Rtos/Src/rtos_app.c
      func__TaskUi                   Firmware/Rtos/Src/task_ui.c
        func__Ui_BoardTest_Start()   ui_led.c + ui_buzzer.c // تست LED و بوق کوتاه قبلی
        در هر نوبت بر اساس متغیرهای تست ولتاژ (volatile) یک سیکل اجرا می‌شود:
          UINT32_T__G__InputVoltageMv   // تست دستی: ولتاژ ورودی mV، آستانه و هیسترزیس از ui_led.h
          UINT32_T__G__BatteryVoltageMv // تست دستی: 21V=0%، 28V=100%، قابل تغییر Live Expressions
          func__Ui_BatteryVoltageToPercent() // نگاشت ولتاژ به درصد
          |
          if (Input > UI_INPUT_OVERVOLTAGE_THRESHOLD_MV) {
            → InputOverVoltage // سبز ثابت، زرد خاموش، قرمز 50%، بوق 1s هر 10s
          } else if (input state: connected/disconnected with hysteresis) {
            if (battery <100%) → func__Ui_ScenarioCharging_Tick() // سبز ثابت، زرد: 0% روشن، 100% خاموش
            else               → func__Ui_ScenarioInputOk()        // سبز ثابت، بقیه خاموش
          } else {
            → func__Ui_ScenarioBatteryRun_Tick() // سبز چشمک، زرد خاموش، بوق‌های درصدی جدید BatteryRun
          }
          func__BspGpio_Write()  Firmware/Bsp/Src/bsp_gpio.c
          PIN_*                  Firmware/Config/Inc/board_pins.h
          APP_CONFIG             Firmware/Config/Src/app_config.c // منبع تنظیمات زمان اجرا؛ پیش‌فرض‌ها از هدرهای UI
```

### سناریوهای فعلی (لیست درخواستی)

1. **InputOk**: ورودی ۲۴ ولت وصل، `V_in >=21V` و باتری `>=28V` → سبز ثابت، زرد و قرمز خاموش. ثابت‌ها: `UI_INPUT_CONNECTED_THRESHOLD_MV = 21000u` و `UI_BAT_V_MAX_MV = 28000u` در `Firmware/Modules/Ui/ui_led.h`.
2. **BatteryRun**: ورودی قطع با هیسترزیس (`V_in <=20V` قطع، `V_in >=21V` وصل) → سبز چشمک (روشن متناسب با درصد باتری)، زرد خاموش؛ زیر ۴۰٪ یک بوق هر ۶۰ ثانیه، زیر ۲۰٪ دو بوق هر ۶۰ ثانیه، زیر ۱۰٪ سه بوق هر ۲۰ ثانیه و زیر ۱٪ یک بوق ممتد ۱۰ ثانیه‌ای و سپس خاموشی همه خروجی‌ها.
3. **Charging**: ورودی وصل و باتری `<28V` → سبز ثابت، زرد متناسب با مانده تا فول چشمک می‌زند: ۰٪ (21V) بیشترین روشنایی و ۱۰۰٪ (28V) خاموش.
4. **InputOverVoltage**: `V_in >28V` → سبز دائم، زرد خاموش، قرمز با دوره `1000ms` و دیوتی `50%` چشمک می‌زند و بوق یک‌ثانیه‌ای هر `10s` اجرا می‌شود. خطا در `V_in <=27V` پاک می‌شود و بازهٔ 27V تا 28V وضعیت خطا را حفظ می‌کند.

ثابت‌های هیسترزیس و نمایش خطا در `Firmware/Modules/Ui/ui_led.h` هستند: `UI_INPUT_CONNECTED_THRESHOLD_MV`، `UI_INPUT_DISCONNECTED_THRESHOLD_MV`، `UI_INPUT_HYSTERESIS_MV`، `UI_INPUT_OVERVOLTAGE_THRESHOLD_MV`، `UI_INPUT_OVERVOLTAGE_CLEAR_THRESHOLD_MV` و `UI_INPUT_OVERVOLTAGE_HYSTERESIS_MV`. مقادیر قابل تنظیم رفتار UI از ثابت‌های هدر و `APP_CONFIG` خوانده می‌شوند. متغیرهای تست با پیشوند تایپ کامل مانند `UINT32_T__G__` تعریف شده‌اند.

سرویس مستقل بوق فقط یک API دارد: `int32_t func__Ui_Buzzer_Tick(periodMs, dutyPercent, beepCount, gapMs)`. سناریوهای `BoardTest`، `BatteryRun` و خطای `InputOverVoltage` این API را صدا می‌زنند؛ `InputOk` و `Charging` آن را خاموش می‌کنند.

محدودیت‌های ایمنی با ثابت‌های `ui_buzzer.h` تعیین می‌شوند: دوره غیرصفر کمتر از `UI_BUZZER_MIN_PERIOD_MS=1000ms` و گپ کمتر از `UI_BUZZER_MIN_GAP_MS=100ms` برای بیش از یک بوق نامعتبر است و بوق روی LOW می‌ماند. `beepCount=0`، مانند `dutyPercent=0`، خاموشی معتبر است. نتیجه `0` خاموشی معتبر، نتیجه `-1` خطای تنظیمات و نتیجه مثبت زمان مراجعه بعدی RTOS است؛ این زمان ۱۰٪ کوچک‌ترین بخش مثبت الگو است.

یک caller صریح RTOS می‌تواند نتیجه مثبت را به `vTaskDelay(pdMS_TO_TICKS(nextCheckMs))` بدهد. تسک عمومی UI زمان‌بندی LED را اجرا می‌کند و سناریوهای BatteryRun و InputOverVoltage سرویس بوق را به‌صورت خودکار صدا می‌زنند.

تسک‌های measurement / protection / control / comm فایل دارند؛ با فلگ صفر ساخته نمی‌شوند.

## درخت اتصال کل پروژه

```text
ChangeOver
├── README.md                          ← همین صفحه
├── Circuit/
│   └── ChangeOver(24V_DC).pdf
├── CubeMX/
│   └── CubeIDE.ioc                    ← کپی تنظیمات مکعب (هم‌نام پروژه، بعد از Generate کپی شود)
├── CubeIDE/                           ← HAL، main.c، FreeRTOS مکعب
│   └── Core/Src/main.c ──#include──► Firmware/App/Inc/app.h
├── tools/
│   ├── check_ai_rules.sh              ← اجرای خودکار قوانین AI_CONTEXT.md (چک هدر، README، فلگ‌ها، .ioc)
│   └── (host tests در Modules/Ui)
└── Firmware/
    ├── AI_CONTEXT.md
    ├── App/
    │   app.c ──► ui_led.h
    │         ──► rtos_app.h
    ├── Config/
    │   board_pins.h
    │   app_config.c / app_config.h
    │   app_types.h
    │   modules_enable.h
    │   rtos_config.h
    ├── Bsp/
    │   bsp_gpio.c ◄── Ui ، EspLink
    │   bsp_adc.c  ◄── Measurement (اسکلت)
    │   bsp_pwm.c  ◄── Charger (اسکلت)
    │   bsp_uart.c ◄── EspLink (اسکلت)
    │   bsp_exti.c ◄── Jitter (اسکلت)
    ├── Rtos/
    │   rtos_app.c ──► task_ui.c          (MODULE_UI)
    │              ──► task_measurement.c (فلگ ۰)
    │              ──► task_protection.c  (فلگ ۰)
    │              ──► task_control.c     (فلگ ۰)
    │              ──► task_comm.c        (فلگ ۰)
    │   freertos_hooks.c
    └── Modules/
        Ui          ◄── فعال (+ host_test_ui.py)
        Measurement     اسکلت
        Protection      اسکلت
        Changeover      اسکلت
        Charger         اسکلت
        Jitter          اسکلت
        Fault           اسکلت
        EspLink         اسکلت (درخت اتصال تکمیل شد)
```

برگهٔ هر ماژول (توابع، پایه‌ها، درخت همان ماژول): `Firmware/Modules/<نام>/README.md`

## تاریخچه این صفحه

| تاریخ | تغییر |
|---|---|
| 2026-09-15 | اولین ساخت کامل UI: چهار سناریوی InputOk، Charging، BatteryRun و InputOverVoltage، هیسترزیس ورودی و چهار بازه بوق BatteryRun. |
| 2026-09-15 | اتصال API جدید بوق به BoardTest و BatteryRun برای حفظ بوق کوتاه و بوق هوشمند قبلی؛ خاموشی بوق در InputOk و Charging |
| 2026-09-15 | افزودن محدودیت‌های قابل تنظیم بوق: حداقل دوره ۱۰۰۰ms، حداقل گپ ۱۰۰ms برای چند بوق، کد خطای `-1` و زمان مراجعه RTOS برابر ۱۰٪ کوچک‌ترین بخش الگو |
| 2026-09-15 | ساده‌سازی بوق به یک سرویس مستقل با ورودی‌های دوره، دیوتی، تعداد بوق و گپ؛ حذف بوق خودکار از سناریوهای LED و به‌روزرسانی تست فرمول بوق |
| 2026-09-14 | بازنویسی UI طبق درخواست جدید: حذف BatteryLow، زرد در دشارژ خاموش، بوق هوشمند با تابع جدا `Ui_BuzzerBeep()` (اگر <50% هر درصد ثانیه، 40%→40s، اگر <20% طول 2 برابر)، سناریوی شارژ جدید با زرد چشمک‌زن (0% زرد ثابت روشن=21V، 100% خاموش=28V، ON=(100-درصد)*دوره)، ورودی از bool به ولتاژ (آستانه 20V)، باتری 0%=21V و 100%=28V با `Ui_BatteryVoltageToPercent()`، پارامترها بالای فایل/تابع، نام‌گذاری U32_G_ گلوبال و u32_ داخلی |
| 2026-09-14 | اجرای AI: فیکس EspLink README (اضافه شدن «درخت اتصال» اجباری)؛ اسکریپت `tools/check_ai_rules.sh` برای اجرای خودکار قوانین AI_CONTEXT (هدر دوزبانه، قالب ۷ بخشی، جدایی CubeIDE/CubeMX، فلگ ماژول‌ها، .ioc بدون ADC/PWM/UART)؛ تست هاست UI `host_test_ui.py`؛ همه چک‌ها پاس شد |
| 2026-09-14 | سناریوهای UI به سبک خطی یک‌سیکلی (InputOk/BatteryRun/BatteryLow)؛ حذف تسک مرده defaultTask از main.c و هر دو .ioc؛ رفع Init تکراری؛ اصلاح نام `CubeIDE.ioc` در مستندات |
| 2026-09-14 | اسکلت‌های Bsp ADC/UART با تایپ ناقص (opaque) بدون فعال‌کردن درایور کامپایل می‌شوند؛ همه فایل‌های Firmware در Build هستند |
| 2026-09-14 | سناریوهای UI مبتنی بر وضعیت با `Ui_Indicate` (ورودی/درصد باتری)؛ حذف Scenario1/2 از درخت اجرا |
| 2026-09-14 | اصلاح Build پروژه CubeIDE: لینک نسبی Firmware، مسیرهای Include، Exclude اسکلت ADC/UART |
| 2026-09-14 | صفحه اول + درخت کل پروژه؛ پوشه CubeMX و CubeIDE |
