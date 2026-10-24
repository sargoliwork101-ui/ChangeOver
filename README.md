/**
 * @file    README.md
 * @brief   [EN] ChangeOver project front page: overview and file connections.
 *          [FA] صفحه اول پروژه ChangeOver: معرفی و درخت اتصال فایل‌ها.
 */

# ChangeOver

تغییر مسیر تغذیه ۲۴ ولت DC: ورودی یا باتری. MCU: `STM32F103C8T6`.

**مرحله فعلی (۲۰۲۶-۱۰-۲۳، فرم‌ور v2 — لینک CRC-16 و NAK، کالیبراسیون بنچ و جدول زندهٔ میکرو):** محصول کامل است و روی بنچ واقعی کالیبره شده: دو شارژر مستقل ۱۲V با PID دوحلقه‌ای CC/CV و جدول جریان بنچ (شمارش خام → mA واقعی باتری، دهم میلی‌آمپر، بدون مسیر توان) به‌همراه مسیر ارسال مستقیم جدول به فلش میکرو (`CalLut`)، نمونه‌برداری سنکرون جریان با مقسم‌های اندازه‌گیری‌شده، ESP-Link با قاب نسخهٔ ۲ و CRC-16 (**۱۶۹ شناسهٔ سیم: ۰..۱۶۸**، **ماندگاری فلش NVM رکورد نسخهٔ ۱۲ با ۱۶۸ جای**، پیام NAK برای تنظیمی که برد نپذیرفت) و **پنل وب v1.89**. چهرهٔ LED/بوق سناریوهای عدم‌توازن، باتری خراب، خطای فنی و باتری ضعیف (سناریوی ۸، شناسه‌های ۱۴۷..۱۵۷: قرمز چشمک‌زن با دوره/سهم مستقل، بوق مستقل، مدت استراحت پیش از تلاش مجدد، پنجرهٔ محافظ چرخه و کلید جدا نگه‌داشتن باتری از خروجی) تیک «فعال» هر کارت سناریو (شناسه‌های ۱۵۸..۱۶۴) فقط چراغ و بوق همان سناریو را روی برد خاموش می‌کند و تشخیص و محافظت را دست نمی‌زند؛ سناریوی ۸ فقط باتری فرسوده را علامت می‌زند نه باتری خالی: باتریِ زیر آستانهٔ استراحت (۱۶۵، پیش‌فرض ۲۵٬۰۰۰ میلی‌ولت) بدون برق شهر دوباره وصل نمی‌شود و مسیرش تا برگشت برق شهر بسته می‌ماند؛ و همچنین تنظیمات وای‌فای و مصرف باتری (شناسه‌های ۱۴۳..۱۴۶ شامل قطع کامل وای‌فای زیر ۲۰٪ باتری) مستقل و از پنل تنظیم‌شدنی است؛ استراحت حرارتی شارژر (شناسه‌های ۱۶۶..۱۶۸: آستانهٔ جریان با پیش‌فرض ۶۰۰ (۵۰ کمتر از سقف ۶۵۰) و حالت خودکار، مدت شارژ تا استراحت و طول استراحت) از پنل قابل تنظیم است؛ مود تست دستی هم با ددمن ۳ ثانیه پیاده شده است. همهٔ ماژول‌ها فعال‌اند (`MODULE_UI/MEASUREMENT/CHARGER/JITTER/FAULT/CHANGEOVER/MCU_POWER_PATH/ESP=1`؛ شارژر با `CHG_MASTER_ENABLE=1`)؛ فقط `MODULE_PROTECTION=0` خاموش است ولی backendاش در Build مانده.

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
        ADC1 + DMA1                   سخت‌افزار، بافر چرخشی ۶ کاناله (۵ پایه + VREFINT)
          Measurement_Run()           تبدیل ADC خام به mV/mA و انتشار snapshot
      TaskUi                          Firmware/Rtos/Src/task_ui.c
        سناریوهای LED/بازر از snapshot کامل (ولتاژها + جریان‌ها + وضعیت شارژر)
      TaskControl                     Firmware/Rtos/Src/task_control.c
        حلقهٔ شارژر دوکاناله (BULK/ABSORB/FLOAT + مود تست دستی + JIT retry)
      TaskComm                        Firmware/Rtos/Src/task_comm.c
        ESP-Link: پارامترها + تله‌متری ۱۰۰ms + ماندگاری فلش (NVM) + مدیریت ترکیبی کاهش مصرف وای‌فای روی باتری
```

## قرارداد تثبیت‌شدهٔ BSP

جزئیات کامل در `Firmware/Bsp/README.md` ثبت شده است. APIهای عمومی HAL-free عبارت‌اند از:

| حوزه | API منطقی | backend فیزیکی فعلی |
|---|---|---|
| GPIO | `func__BspGpio_Init/Write/Read` | PA4، PA8، PB0/PB1/PB10، PB5/PB7/PB11 و PB2/PB4/PB6 |
| ADC | `func__BspAdc_Init/Start/GetRaw` | ADC1 + DMA1 Channel1، شش تبدیل (پنج کانال PA1/PA2/PA3/PA5/PA7 + VREFINT داخلی) |
| Calibration | `func__BspMeasurement_*` | تقسیم‌های ۲۴V/۱۲V، شانت و gain در پورت برد |
| PWM | `func__BspPwm_Init/SetDutyPermille/StopAll` | TIM2_CH1 روی PA0 و TIM3_CH1 روی PA6 |
| UART | `func__BspUart_Init/Write/ReadByte` | USART1 روی PA9/PA10؛ init مکعب 115200، ران‌تایم 921600 8-N-1 با DMA دوطرفه |
| EXTI | `func__BspExti_Init/OnIrq/TakeEvent` | PB2/PB6 falling برای JIT active-low؛ PB4 هر دو لبه |

وضعیت امن startup: بازر، ESP و LEDها Low؛ رله Low؛ کنترل‌های active-low باتری روی High؛ compare هر دو PWM صفر و event flagها پاک هستند. تغییر پایه یا قطبیت فقط در `board_pins.h` و پورت BSP مجاز است.

مسیر دادهٔ زنده (همه وصل‌اند):

- پورت برد، سیگنال منطقی ورودی ۲۴ ولت را از کانال فیزیکی `PA2 / ADC1_IN2` می‌گیرد و به mV تبدیل می‌کند؛ Measurement فقط قرارداد منطقی را می‌بیند.
- تا معتبرشدن اولین فریم ADC، ورودی UI برابر صفر و قطع در نظر گرفته می‌شود.
- ولتاژ هر دو نیم‌باتری (`VLOW`/`VHIGH`) و جریان واقعی هر دو کانال از snapshot واقعی می‌آیند (کالیبره‌شده با بنچ).
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
├── Documentation/
│   ├── Program_Map.xlsx               ← نقشهٔ کل برنامه (فایل‌ها، توابع، ۱۴۳ شناسه، پایه‌ها، فلش، دروازه‌ها) — ساختهٔ tools/make_program_map.py
│   ├── System_State_Machine.xlsx      ← ماشین حالت کل برنامه؛ هر ماژول هم کتاب خودش را کنار برگهٔ اعتبارسنجی دارد
├── CubeMX/
│   └── CubeIDE.ioc                    ← کپی تنظیمات مکعب (هم‌نام پروژه، بعد از Generate کپی شود)
├── CubeIDE/                           ← HAL، main.c، CMSIS-RTOS2 با Backend فعلی FreeRTOS
│   └── Core/Src/main.c ──#include──► Firmware/App/Inc/app.h
├── tools/
│   ├── check_ai_rules.sh              ← اجرای قوانین AI_AGENT_RULES.md
│   ├── check_firmware_syntax.sh       ← syntax check سمت Host برای Core و Firmware (از ۲۰۲۶-۱۰-۰۶ روی سورس‌های Firmware هشدارهای سخت‌گیرانهٔ shadow/undef/prototypes/cast-qual هم با ‎-Werror‎ اعمال می‌شود)
│   ├── audit_consistency.py           ← ممیز نامتغیرهای بین‌فایلی (۴۴۶ نامتغیر؛ شامل پوشش اعتبارسنجی، آینه‌بودن شبیه‌ساز، ماشین حالت هر ماژول، و اعداد کهنهٔ مستندات در برابر کد)
│   ├── make_module_state_machines.py  ← ساخت ماشین حالت جدا برای هر ماژول + لینک در فایل اعتبارسنجی همان ماژول
│   ├── make_state_machine_xlsx.py     ← کتاب ماشین حالت کل برنامه (Documentation/System_State_Machine.xlsx)
│   ├── make_program_map.py            ← نقشهٔ کل برنامه: فایل‌ها، توابع، ۱۴۳ شناسهٔ ESP، پایه‌ها، فلش، دروازه‌ها (Documentation/Program_Map.xlsx)
│   ├── fix_rtl_comments.py            ← چک/اصلاح علامت‌های راست‌به‌چپ داخل کامنت‌های فارسی
│   ├── make_panel_preview.py          ← ساخت کپی آفلاین پنل (ESP_Calibration/panel_preview.html)
│   ├── panel_preview_server.js        ← سرور محلی همان کپی آفلاین (پورت ۳۰۰۰)
│   ├── stamp_panel.py                 ← مهر نسخه/تاریخ ساخت روی هدر پنل
│   ├── measure_flash.py               ← اندازهٔ فلش ایمیج (مقایسهٔ دلتا، نه عدد مطلق)
│   ├── measure_ram.py                 ← بودجهٔ RAM + بدترین عمق پشتهٔ هر تسک و وقفه (capstone)
│   └── (host tests در Modules/*/Tester: Ui, Charger, Imbalance, Changeover, Fault, Protection, Jitter, McuPowerPath, CalLut, Measurement, EspLink, FlashLock — همه داخل check_firmware_syntax.sh اجرا می‌شوند)
├── Programmer/                        ← ابزار لپ‌تاپ (نه کد برد): فلش کردن برد با نگه‌داشتن پارامترها و جدول کالیبراسیون
│   ├── first_flash_changeover.py      ← اولین فلش برد نو: تأیید برد نو، نوشتن، تأیید حالت کارخانه
│   ├── flash_changeover.py            ← پشتیبان ناحیهٔ داده → پاک‌کردن و نوشتن برنامه → برگرداندن → قفل اختیاری
│   ├── stm32_cli.py                   ← لایهٔ مشترک هر دو اسکریپت (پیدا کردن و اجرای ابزار)
│   └── SCRIPTS.md                     ← توضیح کامل هر اسکریپت و روش اجرا
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
        Ui              فعال (+ host_test_ui.py؛ سناریوهای چراغ/بوق)
        Measurement     فعال (ADC → mV/mA + LUT بنچ + snapshot)
        Charger         فعال (دو شارژر ۱۲V + PID دوحلقه‌ای + مود دستی، + host_test_charger.py)
        EspLink         فعال (پروتکل v2 + CRC-16 + NAK + NVM نسخهٔ ۱۲ + پنل v1.84)
        Fault           فعال (تشخیص قطع باتری + نظارت)
        Jitter          فعال (رویداد EXTI → صف retry شارژر)
        Changeover      فعال (انتخاب ورودی/باتری + APP_STATE + وتوی سناریو ۵/۶)
        McuPowerPath    فعال (Q1/PB5 مستقل)
        Imbalance       فعال (سناریوی ۵: تشخیص عدم‌توازن نیم‌باتری‌ها)
        CalLut          فعال (بلوک فلش جدا برای جدول بنچ ارسالی از پنل)
        Protection      خاموش (فلگ ۰، backend در Build)
        FlashLock       خاموش (فلگ ۰، قفل یک‌بارمصرف خواندن حافظه؛ فقط برای بیلد تحویل روشن شود)
```

برگهٔ هر ماژول (توابع، پایه‌ها، درخت همان ماژول): `Firmware/Modules/<نام>/README.md`
