/**
 * @file    README.md
 * @brief   [EN] ChangeOver project front page: overview and file connections.
 *          [FA] صفحه اول پروژه ChangeOver: معرفی و درخت اتصال فایل‌ها.
 */

# ChangeOver

تغییر مسیر تغذیه ۲۴ ولت DC: ورودی یا باتری. MCU: `STM32F103C8T6`.

**مرحله فعلی:** LED و بازر فعال هستند؛ ADC+DMA و Measurement فعال است و فقط ولتاژ ورودی به UI وصل شده است. ولتاژ باتری فعلاً از ورودی تست دستی Live Expressions می‌آید. PWM، UART و رله خاموش هستند.

شماتیک: `Circuit/ChangeOver(24V_DC).pdf`

قوانین دستیار: `AI_AGENT_RULES.md`

## پوشه‌ها

| پوشه | چیست |
|---|---|
| `Circuit/` | شماتیک |
| `Firmware/` | کد محصول (App, Bsp, Config, Modules, Rtos) |
| `CubeMX/` | فایل `.ioc` |
| `CubeIDE/` | پروژه STM32CubeIDE بعد از Generate |

`main.c` فقط کلاک، HAL، `MX_*_Init`، بعد `App_Start()`. منطق محصول در `Firmware/`.

کلید ماژول‌ها: `Firmware/Config/Inc/modules_enable.h` — اکنون `MODULE_UI = 1` و `MODULE_MEASUREMENT = 1` هستند؛ سایر ماژول‌ها خاموش‌اند.

## اجرا الان (اتصال مرحله‌ای Measurement به UI)

```text
CubeIDE/Core/Src/main.c
  App_Start()                         Firmware/App/Src/app.c
    App_Init()                        نقطهٔ آماده‌سازی عمومی برنامه
    Rtos_Start()                      Firmware/Rtos/Src/rtos_app.c
      TaskMeasurement                 Firmware/Rtos/Src/task_measurement.c
        ADC1 + DMA1                   سخت‌افزار، بافر چرخشی ۵ کاناله
          Measurement_Run()           تبدیل ADC خام به mV/mA و انتشار snapshot
      TaskUi                          Firmware/Rtos/Src/task_ui.c
        ورودی معتبر ADC               UINT32_T__G__MeasInputVoltageMv، بر حسب mV
        ورودی نامعتبر ADC              صفر امن، یعنی ورودی قطع
        باتری در مرحله فعلی            UINT32_T__G__BatteryVoltageMv، تست دستی mV
        Ui_Tick(inputMv, batteryMv)    انتخاب InputOk / Charging / BatteryRun / InputOverVoltage
```

در این مرحله فقط مسیر ولتاژ ورودی به UI وصل شده است:

- پورت فعلی برد، سیگنال منطقی ورودی ۲۴ ولت را از کانال فیزیکی `PA2 / ADC1_IN2` می‌گیرد و به mV تبدیل می‌کند؛ Measurement فقط قرارداد منطقی را می‌بیند.
- تا معتبرشدن اولین فریم ADC، ورودی UI برابر صفر و قطع در نظر گرفته می‌شود.
- ولتاژ باتری هنوز از `UINT32_T__G__BatteryVoltageMv` خوانده می‌شود و با Live Expressions قابل تغییر است.
- اتصال ورودی باعث کندشدن یا هنگ‌کردن نمی‌شود؛ ADC و DMA توسط سخت‌افزار کار می‌کنند و Task Measurement هر ۱۰ms فقط یک فریم کوتاه را تبدیل می‌کند.
- مقدار `BOOL__G__MeasDataValid` معتبرشدن اولین فریم را مشخص می‌کند.

### سناریوهای فعلی

1. **InputOverVoltage**: ورودی بیشتر از `28V` خطا را فعال می‌کند؛ در `27V` یا کمتر پاک می‌شود.
2. **InputOk**: ورودی وصل (`>=21V`) و باتری کامل (`>=28V`) → سبز ثابت، زرد/قرمز/بوق خاموش.
3. **Charging**: ورودی وصل و باتری کمتر از `28V` → سبز ثابت، زرد متناسب با مانده شارژ چشمک‌زن، قرمز/بوق خاموش.
4. **BatteryRun**: ورودی قطع (`<=20V`) → سبز متناسب با درصد باتری چشمک‌زن، زرد و قرمز خاموش و بوق طبق چهار بازه BatteryRun.

بین `20V` و `21V` وضعیت قبلی اتصال ورودی حفظ می‌شود. محدودهٔ درصد باتری در منطق UI برابر `21V = 0%` تا `28V = 100%` است، اما در مرحلهٔ فعلی مقدار باتری هنوز از ورودی تست دستی خوانده می‌شود.

ثابت‌های سیاست UI در `Firmware/Modules/Ui/ui_led.h` و محدودیت‌های سرویس بوق در `ui_buzzer.h` هستند. همهٔ Threadها با CMSIS-RTOS2 و حافظهٔ ثابت ساخته می‌شوند؛ FreeRTOS فقط Backend فعلی CMSIS-RTOS2 است و تخصیص پویا خاموش است.

## درخت اتصال کل پروژه

```text
ChangeOver
├── README.md                          ← همین صفحه
├── Circuit/
│   └── ChangeOver(24V_DC).pdf
├── CubeMX/
│   └── CubeIDE.ioc                    ← کپی تنظیمات مکعب (هم‌نام پروژه، بعد از Generate کپی شود)
├── CubeIDE/                           ← HAL، main.c، CMSIS-RTOS2 با Backend فعلی FreeRTOS
│   └── Core/Src/main.c ──#include──► Firmware/App/Inc/app.h
├── tools/
│   ├── check_ai_rules.sh              ← اجرای قوانین AI_AGENT_RULES.md
│   ├── check_firmware_syntax.sh       ← syntax check سمت Host برای Core و Firmware
│   └── (host tests در Modules/Ui)
└── Firmware/
    ├── App/
    │   app.c ──► rtos_app.h
    ├── Config/
    │   board_pins.h                  ← فقط برای پورت BSP برد فعلی
    │   app_config.c / app_config.h
    │   app_types.h
    │   modules_enable.h
    │   rtos_config.h
    ├── Bsp/
    │   bsp_gpio.c ◄── Ui ، EspLink (سیگنال‌های منطقی)
    │   bsp_adc.c  ◄── Measurement (فریم ADC normalized)
    │   bsp_measurement.c ◄── کالیبراسیون مدار برد
    │   bsp_pwm.c  ◄── Charger (اسکلت، رابط منطقی)
    │   bsp_uart.c ◄── EspLink (اسکلت، رابط منطقی)
    │   bsp_exti.c ◄── Jitter (اسکلت، رویداد منطقی)
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
| 2026-09-15 | چک کامل UI با `AI_AGENT_RULES.md` و اصلاحات: braces MISRA در `ui_buzzer.c`، بازر در `ui_led.c` فقط از طریق API ماژول بازر (جداسازی کامل)، شارژ بازر را صریح خاموش می‌کند، نام `BUZZER_STATE_T__G__State`، پاک‌سازی `task_ui.c`؛ مقادیر measurement به سبک قانون `BOOL__G__` اصلاح شد؛ مستندات قدیمی `ui.h`/`ui.c` (حذف‌شده) از برگه‌ها حذف شد |
| 2026-09-15 | مقادیر اندازه‌گیری گلوبال شدند (`UINT32_T__G__Meas*` / `BOOL__G__Meas*` در measurement) — هر تسک می‌تواند بخواند و در دیباگر با Live Expressions دیده می‌شود |
| 2026-09-15 | فعال‌شدن اندازه‌گیری: ADC1+DMA1 در `.ioc` (۵ کانال، scan+continuous، کلاک 9MHz به‌جای 36MHz که از سقف 14MHz F103 بالاتر بود)، درایور ADC ST (v1.1.10) به Drivers، bsp_adc واقعی (بافر چرخشی پرشدهٔ سخت‌افزار، بدون interrupt/CPU)، توابع تبدیل measurement (گام‌به‌گام، بدون فرمول خطی)، دوره `MEASUREMENT_PERIOD_MS=10` بالای measurement.h، PB4 (`MCU_INT_24_IN`) به‌عنوان ورودی دیجیتال حضور ورودی، `MODULE_MEASUREMENT=1`؛ Init/Start داخل تسک Measurement تا app.c دست‌نخورده بماند |
| 2026-09-14 | بازنویسی UI طبق درخواست جدید: حذف BatteryLow، زرد در دشارژ خاموش، بوق هوشمند با تابع جدا `Ui_BuzzerBeep()` (اگر <50% هر درصد ثانیه، 40%→40s، اگر <20% طول 2 برابر)، سناریوی شارژ جدید با زرد چشمک‌زن (0% زرد ثابت روشن=21V، 100% خاموش=28V، ON=(100-درصد)*دوره)، ورودی از bool به ولتاژ (آستانه 20V)، باتری 0%=21V و 100%=28V با `Ui_BatteryVoltageToPercent()`، پارامترها بالای فایل/تابع، نام‌گذاری U32_G_ گلوبال و u32_ داخلی |
| 2026-09-14 | اجرای AI: فیکس EspLink README (اضافه شدن «درخت اتصال» اجباری)؛ اسکریپت `tools/check_ai_rules.sh` برای اجرای خودکار قوانین AI_AGENT_RULES (هدر دوزبانه، قالب ۷ بخشی، جدایی CubeIDE/CubeMX، فلگ ماژول‌ها، .ioc بدون ADC/PWM/UART)؛ تست هاست UI `host_test_ui.py`؛ همه چک‌ها پاس شد |
| 2026-09-14 | سناریوهای UI به سبک خطی یک‌سیکلی (InputOk/BatteryRun/BatteryLow)؛ حذف تسک مرده defaultTask از main.c و هر دو .ioc؛ رفع Init تکراری؛ اصلاح نام `CubeIDE.ioc` در مستندات |
| 2026-09-14 | اسکلت‌های Bsp ADC/UART با تایپ ناقص (opaque) بدون فعال‌کردن درایور کامپایل می‌شوند؛ همه فایل‌های Firmware در Build هستند |
| 2026-09-14 | سناریوهای UI مبتنی بر وضعیت با `Ui_Indicate` (ورودی/درصد باتری)؛ حذف Scenario1/2 از درخت اجرا |
| 2026-09-14 | اصلاح Build پروژه CubeIDE: لینک نسبی Firmware، مسیرهای Include، Exclude اسکلت ADC/UART |
| 2026-09-14 | صفحه اول + درخت کل پروژه؛ پوشه CubeMX و CubeIDE |
