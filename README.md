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
```

### سناریوهای فعلی (لیست درخواستی)

1. **InputOk**: `V_in >=20V` و باتری فول (28V) → سبز ثابت
2. **BatteryRun**: `V_in <20V` → سبز چشمک (روشن=درصد باتری)، زرد خاموش، بوق هوشمند: اگر <50% هر درصد ثانیه یک بوق (40%→40s)، اگر <20% طول بوق 2 برابر (250→500ms) — تابع `Ui_BuzzerBeep()` جدا
3. **Charging**: `V_in >=20V` و باتری <100% → سبز ثابت، زرد چشمک مانده تا فول: 0% (21V) زرد ثابت روشن، 100% (28V) خاموش، ON=(100-درصد)*دوره

پارامترها و تایم‌ها بالای هر فایل/تابع (`ui.c` و `task_ui.c`) تعریف شده‌اند تا بدون عوض کردن کل برنامه تغییر کنند. نام متغیرها با پیشوند تایپ: `U32_G_` گلوبال (حروف بزرگ)، `u32_` داخلی (حروف کوچک).

تسک‌های measurement / protection / control / comm فایل دارند؛ با فلگ صفر ساخته نمی‌شوند.


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
| 2026-09-14 | بازنویسی UI طبق درخواست جدید: حذف BatteryLow، زرد در دشارژ خاموش، بوق هوشمند با تابع جدا `Ui_BuzzerBeep()` (اگر <50% هر درصد ثانیه، 40%→40s، اگر <20% طول 2 برابر)، سناریوی شارژ جدید با زرد چشمک‌زن (0% زرد ثابت روشن=21V، 100% خاموش=28V، ON=(100-درصد)*دوره)، ورودی از bool به ولتاژ (آستانه 20V)، باتری 0%=21V و 100%=28V با `Ui_BatteryVoltageToPercent()`، پارامترها بالای فایل/تابع، نام‌گذاری U32_G_ گلوبال و u32_ داخلی |
| 2026-09-14 | اجرای AI: فیکس EspLink README (اضافه شدن «درخت اتصال» اجباری)؛ اسکریپت `tools/check_ai_rules.sh` برای اجرای خودکار قوانین AI_CONTEXT (هدر دوزبانه، قالب ۷ بخشی، جدایی CubeIDE/CubeMX، فلگ ماژول‌ها، .ioc بدون ADC/PWM/UART)؛ تست هاست UI `host_test_ui.py`؛ همه چک‌ها پاس شد |
| 2026-09-14 | سناریوهای UI به سبک خطی یک‌سیکلی (InputOk/BatteryRun/BatteryLow)؛ حذف تسک مرده defaultTask از main.c و هر دو .ioc؛ رفع Init تکراری؛ اصلاح نام `CubeIDE.ioc` در مستندات |
| 2026-09-14 | اسکلت‌های Bsp ADC/UART با تایپ ناقص (opaque) بدون فعال‌کردن درایور کامپایل می‌شوند؛ همه فایل‌های Firmware در Build هستند |
| 2026-09-14 | سناریوهای UI مبتنی بر وضعیت با `Ui_Indicate` (ورودی/درصد باتری)؛ حذف Scenario1/2 از درخت اجرا |
| 2026-09-14 | اصلاح Build پروژه CubeIDE: لینک نسبی Firmware، مسیرهای Include، Exclude اسکلت ADC/UART |
| 2026-09-14 | صفحه اول + درخت کل پروژه؛ پوشه CubeMX و CubeIDE |
