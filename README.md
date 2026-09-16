/**
 * @file    README.md
 * @brief   [EN] ChangeOver project front page: overview and file connections.
 *          [FA] صفحه اول پروژه ChangeOver: معرفی و درخت اتصال فایل‌ها.
 */

# ChangeOver

تغییر مسیر تغذیه ۲۴ ولت DC: ورودی یا باتری. MCU: `STM32F103C8T6`.

**مرحله فعلی:** LED و بازر و اندازه‌گیری (ADC+DMA، ۵ کانال، خودمختار). PWM، UART، رله خاموش.

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

کلید ماژول‌ها: `Firmware/Config/Inc/modules_enable.h` — الان `MODULE_UI = 1` و `MODULE_MEASUREMENT = 1`.

## اجرا الان (منطق جدید ولتاژ-مبنا)

```text
CubeIDE/Core/.../main.c
  App_Start()                    Firmware/App/Src/app.c
    Ui_Init()                    Firmware/Modules/Ui/ui.c  // پارامترها بالای همین فایل: Vmin=21V, Vmax=28V, Vth=20V
    Rtos_Start()                 Firmware/Rtos/Src/rtos_app.c
      TaskUi                     Firmware/Rtos/Src/task_ui.c
        Ui_BoardTest()           یک‌بار تست سیم‌کشی
        در هر نوبت بر اساس متغیرهای تست ولتاژ (volatile) یک سیکل اجرا می‌شود:
          U32_G_InputVoltageMv   // تست دستی: ولتاژ ورودی mV، آستانه 20V
          U32_G_BatteryVoltageMv // تست دستی: 21V=0% 28V=100%، قابل تغییر Live Expressions
          Ui_BatteryVoltageToPercent() // نگاشت ولتاژ به درصد
          |
          if (V_in >= 20V) {
            if (battery <100%) → Ui_ScenarioCharging()  // سبز ثابت، زرد: 0% ثابت روشن، 100% خاموش، ON=(100-pct)*دوره
            else               → Ui_ScenarioInputOk()   // سبز ثابت، بقیه خاموش
          } else {
            → Ui_ScenarioBatteryRun()  // سبز چشمک ON=درصد*10ms، زرد خاموش، بوق هوشمند هر درصد ثانیه (40%→40s) و <20% طول 2 برابر
              -> Ui_BuzzerBeep()       // تابع جدا بازر، در هر سناریو قابل صدا زدن
          }
          BspGpio_Write()        Firmware/Bsp/Src/bsp_gpio.c
          PIN_*                  Firmware/Config/Inc/board_pins.h
          APP_CONFIG             Firmware/Config/Src/app_config.c // مقادیر پیش‌فرض جدید: ui_input_threshold_mv, ui_bat_v_min/max, beep, charging

  ADC1 + DMA1 (سخت‌افزار، بدون CPU)   CubeMX/CubeIDE.ioc: 5 کانال (PA1/2/3/5/7)، scan+continuous، 12MHz (بیشترین مقدار قانونی با PCLK2=72MHz)
    بافر چرخشی 10 نصف‌واژه            Firmware/Bsp/Src/bsp_adc.c — سخت‌افزار مدام پر می‌کند
      TaskMeasurement                  Firmware/Rtos/Src/task_measurement.c
        هر MEASUREMENT_PERIOD_MS (10ms، بالای measurement.h):
          func__Measurement_Run()     Firmware/Modules/Measurement/measurement.c
            خام → mV / mA (توابع تبدیل گام‌به‌گام) + input_present از PB4
            → گلوبال‌های مشترک UINT32_T__G__Meas* / BOOL__G__Meas* (هر تسک می‌خواند؛ در Live Expressions دیده می‌شود)
            → measurement_snapshot_t (کپی یکجا با GetSnapshot)
```

### سناریوهای فعلی (لیست درخواستی)

1. **InputOk**: `V_in >=20V` و باتری فول (28V) → سبز ثابت
2. **BatteryRun**: `V_in <20V` → سبز چشمک (روشن=درصد باتری)، زرد خاموش، بوق هوشمند: اگر <50% هر درصد ثانیه یک بوق (40%→40s)، اگر <20% طول بوق 2 برابر (250→500ms) — تابع `Ui_BuzzerBeep()` جدا
3. **Charging**: `V_in >=20V` و باتری <100% → سبز ثابت، زرد چشمک مانده تا فول: 0% (21V) زرد ثابت روشن، 100% (28V) خاموش، ON=(100-درصد)*دوره

پارامترها و تایم‌ها بالای هر فایل/تابع (`ui_led.h`، `ui_buzzer.h` و `task_ui.c`) تعریف شده‌اند تا بدون عوض کردن کل برنامه تغییر کنند. نام متغیرها با پیشوند تایپ: `U32_G_` گلوبال (حروف بزرگ)، `u32_` داخلی (حروف کوچک).

تسک‌های protection / control / comm فایل دارند؛ با فلگ صفر ساخته نمی‌شوند (measurement فعال است، ببینید بالا).

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
    │   app.c ──► ui.h
    │         ──► rtos_app.h
    ├── Config/
    │   board_pins.h
    │   app_config.c / app_config.h
    │   app_types.h
    │   modules_enable.h
    │   rtos_config.h
    ├── Bsp/
    │   bsp_gpio.c ◄── Ui ، EspLink
    │   bsp_adc.c  ◄── Measurement (فعال: ADC1+DMA چرخشی، بدون CPU)
    │   bsp_pwm.c  ◄── Charger (اسکلت)
    │   bsp_uart.c ◄── EspLink (اسکلت)
    │   bsp_exti.c ◄── Jitter (اسکلت)
    ├── Rtos/
    │   rtos_app.c ──► task_ui.c          (MODULE_UI)
    │              ──► task_measurement.c (MODULE_MEASUREMENT، فعال)
    │              ──► task_protection.c  (فلگ ۰)
    │              ──► task_control.c     (فلگ ۰)
    │              ──► task_comm.c        (فلگ ۰)
    │   freertos_hooks.c
    └── Modules/
        Ui          ◄── فعال (+ host_test_ui.py)
        Measurement     فعال (تبدیل ADC → mV/mA + snapshot)
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
| 2026-09-16 | اصلاح محدود ADC/Measurement: کلاک ADC روی 12MHz (PCLK2/6، بیشترین مقدار قانونی F103 با PCLK2=72MHz)، همسان‌سازی `.ioc`ها، افزودن HAL ADC/ADCEx به Build، کالیبراسیون، فریم پایدار DMA، ضرایب صحیح تقسیم ولتاژ و snapshot اتمیک؛ ماژول‌های دیگر تغییر نکردند |
| 2026-09-15 | چک کامل UI با `AI_CONTEXT.md` و اصلاحات: braces MISRA در `ui_buzzer.c`، بازر در `ui_led.c` فقط از طریق API ماژول بازر (جداسازی کامل)، شارژ بازر را صریح خاموش می‌کند، نام `BUZZER_STATE_T__G__State`، پاک‌سازی `task_ui.c`؛ مقادیر measurement به سبک قانون `BOOL__G__` اصلاح شد؛ مستندات قدیمی `ui.h`/`ui.c` (حذف‌شده) از برگه‌ها حذف شد |
| 2026-09-15 | مقادیر اندازه‌گیری گلوبال شدند (`UINT32_T__G__Meas*` / `BOOL__G__Meas*` در measurement) — هر تسک می‌تواند بخواند و در دیباگر با Live Expressions دیده می‌شود |
| 2026-09-15 | فعال‌شدن اندازه‌گیری: ADC1+DMA1 در `.ioc` (۵ کانال، scan+continuous، کلاک 9MHz به‌جای 36MHz که از سقف 14MHz F103 بالاتر بود)، درایور ADC ST (v1.1.10) به Drivers، bsp_adc واقعی (بافر چرخشی پرشدهٔ سخت‌افزار، بدون interrupt/CPU)، توابع تبدیل measurement (گام‌به‌گام، بدون فرمول خطی)، دوره `MEASUREMENT_PERIOD_MS=10` بالای measurement.h، PB4 (`MCU_INT_24_IN`) به‌عنوان ورودی دیجیتال حضور ورودی، `MODULE_MEASUREMENT=1`؛ Init/Start داخل تسک Measurement تا app.c دست‌نخورده بماند |
| 2026-09-14 | بازنویسی UI طبق درخواست جدید: حذف BatteryLow، زرد در دشارژ خاموش، بوق هوشمند با تابع جدا `Ui_BuzzerBeep()` (اگر <50% هر درصد ثانیه، 40%→40s، اگر <20% طول 2 برابر)، سناریوی شارژ جدید با زرد چشمک‌زن (0% زرد ثابت روشن=21V، 100% خاموش=28V، ON=(100-درصد)*دوره)، ورودی از bool به ولتاژ (آستانه 20V)، باتری 0%=21V و 100%=28V با `Ui_BatteryVoltageToPercent()`، پارامترها بالای فایل/تابع، نام‌گذاری U32_G_ گلوبال و u32_ داخلی |
| 2026-09-14 | اجرای AI: فیکس EspLink README (اضافه شدن «درخت اتصال» اجباری)؛ اسکریپت `tools/check_ai_rules.sh` برای اجرای خودکار قوانین AI_CONTEXT (هدر دوزبانه، قالب ۷ بخشی، جدایی CubeIDE/CubeMX، فلگ ماژول‌ها، .ioc بدون ADC/PWM/UART)؛ تست هاست UI `host_test_ui.py`؛ همه چک‌ها پاس شد |
| 2026-09-14 | سناریوهای UI به سبک خطی یک‌سیکلی (InputOk/BatteryRun/BatteryLow)؛ حذف تسک مرده defaultTask از main.c و هر دو .ioc؛ رفع Init تکراری؛ اصلاح نام `CubeIDE.ioc` در مستندات |
| 2026-09-14 | اسکلت‌های Bsp ADC/UART با تایپ ناقص (opaque) بدون فعال‌کردن درایور کامپایل می‌شوند؛ همه فایل‌های Firmware در Build هستند |
| 2026-09-14 | سناریوهای UI مبتنی بر وضعیت با `Ui_Indicate` (ورودی/درصد باتری)؛ حذف Scenario1/2 از درخت اجرا |
| 2026-09-14 | اصلاح Build پروژه CubeIDE: لینک نسبی Firmware، مسیرهای Include، Exclude اسکلت ADC/UART |
| 2026-09-14 | صفحه اول + درخت کل پروژه؛ پوشه CubeMX و CubeIDE |
