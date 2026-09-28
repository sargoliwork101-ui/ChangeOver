/**
 * @file    README.md
 * @brief   [EN] ChangeOver project front page: overview and file connections.
 *          [FA] صفحه اول پروژه ChangeOver: معرفی و درخت اتصال فایل‌ها.
 */

# ChangeOver

تغییر مسیر تغذیه ۲۴ ولت DC: ورودی یا باتری. MCU: `STM32F103C8T6`.

**مرحله فعلی (۲۰۲۶-۰۹-۲۸، فرم‌ور v1.19):** محصول کامل است و روی بنچ واقعی کالیبره شده: دو شارژر مستقل ۱۲V با LUT توانی بنچ پر-کانال (mA واقعی باتری)، نمونه‌برداری سنکرون جریان، ESP-Link با پروتکل v1.17b (۸۳ پارامتر، ماندگاری فلش NVM v5) و پنل وب v1.17b. همهٔ ماژول‌ها فعال‌اند (`MODULE_UI/MEASUREMENT/CHARGER/JITTER/FAULT/CHANGEOVER/MCU_POWER_PATH/ESP=1`؛ شارژر با `CHG_MASTER_ENABLE=1`)؛ فقط `MODULE_PROTECTION=0` خاموش است ولی backendاش در Build مانده.

شماتیک: `Circuit/ChangeOver(24V_DC).pdf`

قوانین دستیار: `AI_AGENT_RULES.md`

## پوشه‌ها

| پوشه | چیست |
|---|---|
| `Circuit/` | شماتیک |
| `Firmware/` | کد محصول (App, Bsp, Config, Modules, Rtos) |
| `CubeMX/` | فایل `.ioc` |
| `CubeIDE/` | پروژه STM32CubeIDE بعد از Generate |

`main.c` فقط کلاک، HAL، `MX_*_Init`، وضعیت امن BSP و بعد `App_Start()` را مدیریت می‌کند. منطق محصول در `Firmware/` و نگاشت فیزیکی فقط در پورت BSP برد است.

کلید ماژول‌ها: `Firmware/Config/Inc/modules_enable.h` — همه `1` هستند جز `MODULE_PROTECTION = 0`؛ بیلدِ پوش‌شده بدون هیچ تغییری با پنل کار می‌کند (`MODULE_ESP = 1` پیش‌فرض از ۲۰۲۶-۰۹-۲۷).

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
        سناریوهای LED/بازر از snapshot کامل (ولتاژها + جریان‌ها + وضعیت شارژر)
      TaskControl                     Firmware/Rtos/Src/task_control.c
        حلقهٔ شارژر دوکاناله (BULK/ABSORB/FLOAT + مود تست دستی + JIT retry)
      TaskComm                        Firmware/Rtos/Src/task_comm.c
        ESP-Link: پارامترها + تله‌متری ۱۰۰ms + ماندگاری فلش (NVM)
```

## قرارداد تثبیت‌شدهٔ BSP

جزئیات کامل در `Firmware/Bsp/README.md` ثبت شده است. APIهای عمومی HAL-free عبارت‌اند از:

| حوزه | API منطقی | backend فیزیکی فعلی |
|---|---|---|
| GPIO | `func__BspGpio_Init/Write/Read` | PA4، PA8، PB0/PB1/PB10، PB5/PB7/PB11 و PB2/PB4/PB6 |
| ADC | `func__BspAdc_Init/Start/GetRaw` | ADC1 + DMA1 Channel1، پنج کانال PA1/PA2/PA3/PA5/PA7 |
| Calibration | `func__BspMeasurement_*` | تقسیم‌های ۲۴V/۱۲V، شانت و gain در پورت برد |
| PWM | `func__BspPwm_Init/SetDutyPermille/StopAll` | TIM2_CH1 روی PA0 و TIM3_CH1 روی PA6 |
| UART | `func__BspUart_Init/Write/ReadByte` | USART1 روی PA9/PA10؛ init مکعب 115200، ران‌تایم 921600 8-N-1 با DMA دوطرفه |
| EXTI | `func__BspExti_Init/OnIrq/TakeEvent` | PB2/PB6 falling برای JIT active-low؛ PB4 هر دو لبه |

وضعیت امن startup: بازر، ESP و LEDها Low؛ رله Low؛ کنترل‌های active-low باتری روی High؛ compare هر دو PWM صفر و event flagها پاک هستند. تغییر پایه یا قطبیت فقط در `board_pins.h` و پورت BSP مجاز است.

مسیر دادهٔ زنده (همه وصل‌اند):

- پورت برد، سیگنال منطقی ورودی ۲۴ ولت را از کانال فیزیکی `PA2 / ADC1_IN2` می‌گیرد و به mV تبدیل می‌کند؛ Measurement فقط قرارداد منطقی را می‌بیند.
- تا معتبرشدن اولین فریم ADC، ورودی UI برابر صفر و قطع در نظر گرفته می‌شود.
- ولتاژ هر دو نیم‌باتری (`VLOW`/`VHIGH`) و جریان واقعی هر دو کانال از snapshot واقعی می‌آیند (کالیبره‌شده با بنچ، فرم‌ور v1.19).
- اتصال ورودی باعث کندشدن یا هنگ‌کردن نمی‌شود؛ ADC و DMA توسط سخت‌افزار کار می‌کنند و Task Measurement هر ۱ms فقط یک فریم کوتاه را تبدیل می‌کند.
- مقدار `BOOL__G__MeasDataValid` معتبرشدن فریم‌ها را مشخص می‌کند (بعد از سه فریم کامل و پایدار).

### سناریوهای فعلی

1. **InputOverVoltage**: ورودی بیشتر از `28V` خطا را فعال می‌کند؛ در `27V` یا کمتر پاک می‌شود.
2. **InputOk**: ورودی وصل (`>=21V`) و باتری کامل (`>=28V`) → سبز ثابت، زرد/قرمز/بوق خاموش.
3. **Charging**: ورودی وصل و باتری کمتر از `28V` → سبز ثابت، زرد متناسب با مانده شارژ چشمک‌زن، قرمز/بوق خاموش. از v1.20 (دستور ۲۰۲۶-۰۹-۲۸): «مانده تا فول» روی کف ۲٪ می‌نشیند و درصد ۱۰۰ وسط شارژ دیگر زرد را خاموش نمی‌کند — تا وقتی کانال شارژری پمپ می‌کند زرد حداقل یک چشمک مرئی در هر دوره می‌زند (کف مرئی ۱۵۰ms، شناسهٔ ۶۹) و کاملاً خاموش فقط با قطع شارژر (اتمام شارژ / بدون کانال فعال) است.
4. **BatteryRun**: ورودی قطع (`<=20V`) → سبز متناسب با درصد باتری چشمک‌زن، زرد و قرمز خاموش و بوق طبق چهار بازه BatteryRun.

بین `20V` و `21V` وضعیت قبلی اتصال ورودی حفظ می‌شود. محدودهٔ درصد باتری در منطق UI برابر `21V = 0%` تا `28V = 100%` است و مقدار باتری از نیم‌باتری‌های واقعی خوانده می‌شود. فهرست کامل سناریوها (قطع باتری، خطاها، آینهٔ آلارم‌ها): README ماژول Ui.

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
    │   bsp_pwm.c  ◄── Charger (درایو TIM2/TIM3 + گیت پارک)
    │   bsp_uart.c ◄── EspLink (USART1 با 921600 + DMA دوطرفه)
    │   bsp_exti.c ◄── Jitter (رویداد منطقی JIT1/JIT2/INPUT)
    ├── Rtos/
    │   rtos_app.c ──► task_ui.c          (MODULE_UI، فعال)
    │              ──► task_measurement.c (MODULE_MEASUREMENT، فعال)
    │              ──► task_protection.c  (فلگ ۰)
    │              ──► task_control.c     (MODULE_CHARGER، فعال)
    │              ──► task_comm.c        (MODULE_ESP، فعال)
    │   freertos_hooks.c
    └── Modules/
        Ui              فعال (+ host_test_ui.py)
        Measurement     فعال (ADC → mV/mA + LUT بنچ + snapshot)
        Charger         فعال (دو شارژر ۱۲V + مود دستی، + host_test_charger.py)
        EspLink         فعال (پروتکل v1.17b + NVM v5 + پنل v1.17b)
        Fault           فعال (تشخیص قطع باتری + نظارت)
        Jitter          فعال (رویداد EXTI → صف retry شارژر)
        Changeover      فعال (انتخاب ورودی/باتری + APP_STATE)
        McuPowerPath    فعال (Q1/PB5 مستقل)
        Protection      خاموش (فلگ ۰، backend در Build)
```

برگهٔ هر ماژول (توابع، پایه‌ها، درخت همان ماژول): `Firmware/Modules/<نام>/README.md`

## تاریخچه این صفحه

| تاریخ | تغییر |
|---|---|
| 2026-09-28 | **v1.21 — رفع چرخهٔ سه‌بوق کاذب حین شارژ (دستور کاربر):** قانون پمپ ۱۴٫۸V فالت از «هر کانالی پمپ کند + چک هر دو نیم» به مسلح‌شدن per-half اصلاح شد — هر نیم فقط وقتی کانال شارژر خودش در BULK/ABSORB است قضاوت می‌شود (`func__Charger_IsChannelActive` جدید؛ کانال پارک‌شده پمپی ندارد و نمی‌تواند نیمش را بالا بفرستد؛ `vhigh` مشتق‌شده = V24−V12 با بار کانال دیگر حرکت می‌کرد و آلارم خیالی قطع‌باتری می‌ساخت). تشخیص قطعِ واقعی سیم حین شارژ دست‌نخورده؛ تست هاست شارژر ۳۷/۳۷ + همهٔ چک‌ها PASS |
| 2026-09-28 | **v1.20 — چشمک زرد شارژ تا قطع شارژر (دستور کاربر):** کف «مانده تا فول» ۲٪ (`UI_CHARGING_YELLOW_MIN_REMAINING_PERCENT`)؛ درصد ۱۰۰ وسط شارژ (سقف نگاشت ۷۴/۷۵ حین شستشوی ابزورب تا ۱ ساعت) دیگر زرد را کاملاً خاموش نمی‌کند و چهرهٔ فول حین پمپ به چهرهٔ شارژ راه می‌دهد — زرد فقط با قطع شارژر (اتمام شارژ/بدون کانال فعال) خاموش می‌شود؛ کف مرئی ۱۵۰ms (شناسهٔ ۶۹) پابرجا؛ تست هاست UI به‌روز + همهٔ چک‌ها PASS |
| 2026-09-28 | **همگام‌سازی مستندات با کد (بدون تغییر منطق):** اصلاح اعداد قدیمی در برگه‌ها — پروتکل v1.17b با ۸۳ پارامتر (فول/هیسترزیس ۷۷..۸۲)، رکورد NVM نسخهٔ ۵ با ۸۳ جای، تست هاست شارژر ۳۶/۳۶، سقف جریان bulk ۶۵۰mA و حفاظت سخت ۹۵۰mA، پنجرهٔ میانگین جریان (پیش‌فرض ۱۰، سقف ۳۰۰)، رفتار واقعی `EspLink_Init/Run` و `BspUart_Write`؛ راستی‌آزمایی ریاضی جدول‌های کالیبراسیون جریان/ولتاژ — همهٔ چک‌ها PASS |
| 2026-09-27 | **به‌روزرسانی کامل صفحه (فرم‌ور v1.19):** همهٔ ماژول‌ها فعال جز Protection؛ کالیبراسیون بنچ دوکاناله (LUT توانی + مقسم پک 66200)؛ پروتکل v1.16 با ۷۷ پارامتر و ماندگاری فلش؛ پنل v1.16x؛ تست هاست ۳۴/۳۴ + همهٔ گیت‌ها سبز |
| 2026-09-16 | تثبیت BSP کامل از روی شماتیک: GPIO و safe-state، ADC+DMA و کالیبراسیون، TIM2/TIM3 PWM، USART1/ESP-Link، EXTI و IRQ/MSP؛ همسان‌سازی `.ioc`ها، افزودن HAL UART به Build و ثبت قرارداد Agentهای بعدی |
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
