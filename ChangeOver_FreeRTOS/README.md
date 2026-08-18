# ChangeOver FreeRTOS — MISRA C Firmware

Firmware اسکلت ماژولار برای مدار `ChangeOver(24V_DC)` بر پایه‌ی:

- `STM32F103C8T6`
- `STM32CubeIDE`
- `STM32 HAL`
- `FreeRTOS`
- هدف کدنویسی: `MISRA C:2012` به‌همراه Amendment 2 و Technical Corrigendum در صورت پشتیبانی ابزار تحلیل

این پروژه برای کنترل یک سیستم ۲۴ ولت شامل Changeover، حفاظت باتری، دو کانال شارژر فلای‌بک، اندازه‌گیری ولتاژ و جریان، تشخیص Jitter، نشانگرها و ارتباط با ESP8266 طراحی شده است.

> **وضعیت تأیید:** هیچ‌کدام از قابلیت‌های توان، Changeover، Charger PWM، Relay، Battery Switch، ESP Commands یا Handshake هنوز به‌عنوان محصول تأیید نشده‌اند. این Repository فقط Prototype/Scaffold است.

> **وضعیت فعلی:** این مخزن اسکلت اولیه‌ی قابل توسعه است، نه Firmware نهایی و نه گواهی MISRA. HAL شرکت ST و هسته‌ی FreeRTOS کد ثالث هستند و باید در فایل Deviation Record مستندسازی شوند. هیچ PWM توانمندی تا زمانی که کالیبراسیون و تست سخت‌افزار انجام نشده، به‌صورت خودکار فعال نمی‌شود.

---

## 1. تصمیم معماری

در ابتدا معماری C++ کلاس‌محور بررسی شد، اما تصمیم نهایی پروژه **MISRA C** است. بنابراین:

- در پروژه‌ی اصلی کلاس C++ نداریم.
- هر بخش به‌صورت یک **ماژول C** با یک فایل `.h` و یک فایل `.c` پیاده‌سازی می‌شود.
- وضعیت داخلی ماژول‌ها داخل `struct` قرار می‌گیرد.
- دسترسی به وضعیت از طریق توابع API انجام می‌شود.
- توابع و داده‌های داخلی هر ماژول تا حد ممکن `static` هستند.
- هیچ `malloc`، `free`، recursion، exception، RTTI یا حافظه‌ی پویا در Application استفاده نمی‌شود.
- Taskها، Queueها، EventGroupها و Mutexهای Application به‌صورت Static ساخته می‌شوند.

این تصمیم برای Firmware سمت STM32 است. نمونه‌ی Arduino ESP8266 به‌دلیل الزام `.ino` در عمل C++ کامپایل می‌شود و با قواعد Embedded Safe C++، بدون Exception و حافظه‌ی پویا در مسیر Runtime، در پوشه‌ی مستقل `ESP8266/` نگهداری می‌شود.

در C، نزدیک‌ترین معادل «هر کلاس در فایل جدا» این الگو است:

```text
charger_controller.h
charger_controller.c

charger_controller_t
charger_controller_init()
charger_controller_update()
charger_controller_set_duty()
```

---

## 2. وضعیت تأیید قابلیت‌ها

جدول رسمی وضعیت تأیید در این فایل قرار دارد:

```text
docs/approval-status.md
```

Baseline عمداً ایمن و خاموش است:

```text
APP_CONFIG.power_stage_enabled = false
APP_CONFIG.esp_link_enabled = false
ESP_FEATURE_STM_COMMANDS = 0
ESP_FEATURE_HANDSHAKE = 0
```

هیچ Feature توان یا Command نباید بدون Strategy، Approval و تست سخت‌افزاری فعال شود.

## 3. ساختار پروژه

```text
ChangeOver_FreeRTOS/
├── AI_CONTEXT.md                 سند زنده‌ی تصمیم‌ها و قراردادهای پروژه برای AI
├── AI_WORKFLOW.md                قرارداد اجباری روند کار AI و Approval
├── PROJECT_HISTORY.md            تاریخچه‌ی Taskها و تصمیم‌های انجام‌شده
├── README.md                     راهنمای اصلی پروژه
├── .gitignore
│
├── Core/
│   └── Inc/                      نقاط اتصال به main تولیدشده توسط CubeMX
│
├── BSP/
│   ├── Inc/                      Wrapperهای سخت‌افزار
│   └── Src/
│       ├── board_pins.c          تعریف Pin Map
│       ├── gpio_driver.c
│       ├── adc_driver.c
│       ├── pwm_driver.c
│       └── uart_driver.c
│
├── Config/
│   ├── Inc/
│   │   └── app_config.h          ضرایب، آستانه‌ها و سطح فعال سیگنال‌ها
│   └── Src/
│       └── app_config.c          مقادیر پیش‌فرض قابل بازبینی
│
├── App/
│   ├── Inc/
│   └── Src/
│       ├── measurement_manager.c
│       ├── actuator_manager.c
│       ├── fault_manager.c
│       ├── diagnostics.c
│       ├── battery_protection.c
│       ├── jitter_detector.c
│       ├── charger_controller.c
│       ├── changeover_controller.c
│       ├── user_interface.c
│       ├── esp8266_service.c
│       └── system_controller.c
│
├── RTOS/
│   ├── Inc/
│   └── Src/
│       ├── measurement_task.c
│       ├── protection_task.c
│       ├── control_task.c
│       ├── communication_task.c
│       ├── ui_task.c
│       └── freertos_hooks.c
│
├── Integration/
│   ├── Inc/
│   └── Src/
│       ├── firmware_app.c
│       └── firmware_entry.c
│
├── ESP8266/                    نمونه‌ی Arduino Web Debugger برای ESP8266-01
│   ├── ESP8266_WebDebugger.ino
│   ├── *.h / *.cpp
│   └── data/                   صفحات Dashboard، Debugger، Charts و Test
│
├── CubeMX/
│   ├── README.md
│   └── FreeRTOSConfig.example.h
│
└── docs/
    ├── architecture.md
    ├── pin-map.md
    ├── misra-c-guidelines.md
    ├── commenting-standard.md
    ├── debug-diagnostics.md
    ├── deviation-record.md
    ├── STM32_PROJECT_PREPARATION.md
    ├── final-audit.md
    ├── memory-budget.md
    ├── five-pass-audit.md
    ├── approval-status.md
    └── commissioning-checklist.md
```

---

## 4. خلاصه‌ی سخت‌افزار

بر اساس PDF شماتیک:

- ورودی `24V_IN_CON`
- ورودی `24V_BAT_CON`
- مسیر باتری `12V_BAT_CON`
- خروجی `OUT`
- انتخاب منبع توسط مدار Changeover و MOSFETهای قدرت
- دو مبدل فلای‌بک برای شارژ
- سنجش جریان با Shunt و LM358
- سنجش ولتاژ با تقسیم مقاومتی
- تشخیص Jitter1 و Jitter2
- رله‌ی حفاظت شارژر
- سه LED و یک Buzzer
- هدر SWD و ارتباط UART با ESP8266

---

## 5. Pin Map

| پایه | نام منطقی | عملکرد |
|---|---|---|
| PA0 | `MCU_PWM1` | PWM شارژر ۱، TIM2_CH1 |
| PA1 | `MCU_ADC_CURRENT1` | جریان کانال ۱، ADC1_IN1 |
| PA2 | `MCU_ADC_24_IN` | ولتاژ ورودی ۲۴ ولت، ADC1_IN2 |
| PA3 | `MCU_ADC_24_BAT` | ولتاژ باتری ۲۴ ولت، ADC1_IN3 |
| PA4 | `MCU_BUZZER` | بازر |
| PA5 | `MCU_ADC_12_BAT` | ولتاژ مسیر/باتری ۱۲ ولت، ADC1_IN5 |
| PA6 | `MCU_PWM2` | PWM شارژر ۲، TIM3_CH1 |
| PA7 | `MCU_ADC_CURRENT2` | جریان کانال ۲، ADC1_IN7 |
| PA8 | `MCU_ESP_CHPD` | فعال‌سازی CH_PD ESP8266 |
| PA9 | `MCU_TX` | USART1_TX |
| PA10 | `MCU_RX` | USART1_RX |
| PA13 | `SWDIO` | دیباگ |
| PA14 | `SWCLK` | دیباگ |
| PB0 | `MCU_R_LED` | LED قرمز |
| PB1 | `MCU_Y_LED` | LED زرد |
| PB2 | `MCU_JITTER1` | ورودی EXTI2 |
| PB3 | `SWO` | در شماتیک برای Trace؛ با Debug بررسی شود |
| PB4 | `MCU_INT_24_IN` | تشخیص وجود ورودی ۲۴ ولت، EXTI4 |
| PB5 | `MCU_BAT_SWITCH` | کنترل Battery Switch |
| PB6 | `MCU_JITTER2` | ورودی EXTI6 |
| PB7 | `MCU_PROTECT_CHARGER` | کنترل حفاظت/رله شارژر |
| PB10 | `MCU_G_LED` | LED سبز |
| PB11 | `MCU_PROTECT_BATT` | حفاظت باتری در افت ولتاژ |

جزئیات در `docs/pin-map.md` و `BSP/Inc/board_pins.h` قرار دارد.

رابط کاربر در `App/Src/user_interface.c` و `RTOS/Src/ui_task.c` پیاده‌سازی شده است.

---

## 6. ترتیب ADC DMA

در اسکلت فعلی ترتیب Rankهای ADC به شکل زیر فرض شده است:

```text
Index 0: PA1 — Current 1
Index 1: PA2 — 24V Input
Index 2: PA3 — 24V Battery
Index 3: PA5 — 12V Battery / Battery Common
Index 4: PA7 — Current 2
```

اگر ترتیب Rankها در CubeMX متفاوت است، یا اگر ADC با Trigger تایمری استفاده می‌شود، باید این قرارداد در `measurement_manager.c` و CubeMX هم‌زمان تغییر کند.

---

## 7. تقسیم‌بندی FreeRTOS

هیچ‌کدام از Taskها PWM را نرم‌افزاری تولید نمی‌کنند. PWM فقط توسط Timer سخت‌افزاری تولید می‌شود.

| Task | اولویت پیشنهادی | دوره / رویداد | مسئولیت |
|---|---:|---:|---|
| `MeasurementTask` | بالا | رویداد DMA | دریافت فریم ADC و تبدیل به Snapshot |
| `ProtectionTask` | خیلی بالا | ۵ ms | بررسی Low Battery و Over Current |
| `ControlTask` | بالا | ۱۰ ms | اجرای State Machine و کنترل خروجی‌ها |
| `CommunicationTask` | معمولی | ۱۰۰ ms | Telemetry اولیه برای ESP8266 |
| `UiTask` | پایین | ۱۰۰ ms | LED و Buzzer |

قواعد ارتباطی:

- Snapshot اندازه‌گیری با Mutex استاتیک محافظت می‌شود.
- Faultها با EventGroup استاتیک نگهداری می‌شوند.
- Jitter از ISR به Queue استاتیک منتقل می‌شود.
- ISR فقط Event ثبت می‌کند و منطق اصلی در Task اجرا می‌شود.
- خروجی‌های حساس از مسیر `ActuatorManager` کنترل می‌شوند.
- تا دریافت اولین فریم کامل ADC، کنترل توان در حالت امن باقی می‌ماند.

---

## 8. State Machine اولیه

```text
BOOT
  ↓
SELF_TEST
  ├── ورودی 24V موجود → INPUT_SOURCE
  ├── ورودی موجود نیست → BATTERY_SOURCE
  ├── افت باتری → LOW_BATTERY
  ├── اضافه‌جریان → OVER_CURRENT
  └── خطای سخت‌افزاری/ADC → FAULT
```

سیاست فعلی اسکلت:

- در حالت عادی، ورودی ۲۴ ولت بر باتری اولویت دارد.
- در نبود ورودی، مسیر باتری انتخاب می‌شود.
- در Low Battery، مسیر باتری قطع می‌شود.
- در Over Current یا خطای سخت‌افزاری، خروجی‌ها به حالت امن می‌روند.
- الگوریتم CC/CV شارژر هنوز پیاده‌سازی نهایی نشده است.
- Break-Before-Make باید قبل از تست قدرت به‌صورت دقیق پیاده‌سازی و اندازه‌گیری شود.

این سیاست‌ها تا زمان تست عملی برد، **قرارداد نهایی ایمنی** محسوب نمی‌شوند.

---

## 9. قواعد MISRA C

هدف پروژه، کدنویسی Application مطابق `MISRA C:2012` است.

قواعد اجرایی اصلی:

- برای Application و Objectهای FreeRTOS، Static Allocation استفاده می‌شود و Dynamic Allocation باید در پروژه‌ی نهایی غیرفعال باشد.
- Hookهای حافظه‌ی Idle Task و Stack Overflow در `RTOS/Src/freertos_hooks.c` قرار دارند.

1. کامپایل با هشدارهای حداکثری و تبدیل هشدار به خطا در CI.
2. استفاده از نوع‌های مشخص مثل `uint32_t`، `uint16_t` و `bool`.
3. عدم استفاده از حافظه‌ی پویا در Application.
4. عدم استفاده از recursion، `goto` و loop بدون شرط قابل اثبات.
5. محدودکردن scope متغیرها و توابع به `static` در صورت عدم نیاز به Export.
6. استفاده از include guard برای تمام Headerها.
7. عدم استفاده از Function-like Macro در Application.
8. عدم استفاده از magic number؛ مقادیر در `app_config.h` یا enumهای مشخص قرار می‌گیرند.
9. بررسی مقدار بازگشتی HAL و FreeRTOS در Wrapperها و مسیرهای بحرانی.
10. عدم انجام پردازش سنگین در ISR.
11. عدم استفاده از `printf` در مسیرهای زمان‌حساس.
12. عدم فعال‌کردن PWM توان بدون کالیبراسیون و تست.
13. هر Deviation از MISRA باید در `docs/deviation-record.md` ثبت شود.

توجه: MISRA بودن Application به‌صورت خودکار به معنی MISRA بودن HAL، FreeRTOS یا Startup code نیست. این قسمت‌ها Third-Party هستند و باید در Scope تحلیل، Exclude یا Deviation آن‌ها شفاف ثبت شود.

---

## 10. رابط کاربر

ماژول `user_interface` مسئول LEDها و Buzzer است و از منطق اصلی کنترل جدا نگه داشته شده است:

```text
App/Inc/user_interface.h
App/Src/user_interface.c
RTOS/Inc/ui_task.h
RTOS/Src/ui_task.c
```

الگوی فعلی:

```text
Red LED    → Fault
Yellow LED → Low Battery / Charging
Green LED  → فعال بودن منبع بدون Fault
Buzzer     → الگوی غیرمسدودکننده برای Fault و Low Battery
```

## 11. دیباگ و Diagnostics

ماژول Diagnostics برای قابل‌مشاهده‌کردن رفتار Firmware و ارسال وضعیت به ESP8266 آماده شده است:

```text
App/Inc/diagnostics.h
App/Src/diagnostics.c
docs/debug-diagnostics.md
```

قالب خط ارسالی:

```text
D,<code>,<severity>,<value>,<fault_mask>,<state>,<occurrence_count>\r\n
```

هر Diagnostic Code معنای ثابت دارد و نباید reuse شود. جدول کامل کدها، Severity، State و Fault Mask در `docs/debug-diagnostics.md` قرار دارد.

`AI_WORKFLOW.md` نیز الزام می‌کند هر تغییر در کد، Diagnostic یا پروتکل ESP در اسناد و History ثبت شود.

## 12. ESP8266 Web Debugger

نمونه‌ی Arduino برای `ESP8266-01` در پوشه‌ی جدا قرار دارد:

```text
ESP8266/ESP8266_WebDebugger.ino
ESP8266/data/index.html
ESP8266/data/debug.html
ESP8266/data/charts.html
ESP8266/data/test.html
ESP8266/data/settings.html
```

این Web Server صفحات زیر را فراهم می‌کند:

```text
/          Dashboard
/debug     Diagnostic Debugger
/charts    نمودار Telemetry
/test      تست مرحله‌ای برد
/settings  وضعیت Feature Flagها
```

ذخیره‌سازی دائمی با LittleFS و دریافت `T,...` و `D,...` در اسکلت آماده شده است. Handshake، Sequence، ACK و CRC در Feature Flag جدا هستند و قبل از فعال‌سازی باید سمت STM32 و ESP با هم تست شوند.

فایل راهنمای ESP:

```text
ESP8266/README_ESP8266.md
```

## 13. راه‌اندازی در CubeMX / CubeIDE

1. پروژه‌ی `STM32F103C8T6` بساز.
2. HSE کریستال ۸ MHz را فعال کن و Clock را مطابق برد تنظیم کن؛ مقدار پیشنهادی اولیه ۷۲ MHz است.
3. Debug را روی **Serial Wire** قرار بده تا PB4 از NJTRST آزاد شود.
4. ADC1 را با Scan و DMA و پنج Rank مطابق بخش ADC تنظیم کن.
5. `TIM2_CH1` روی PA0 و `TIM3_CH1` روی PA6 را PWM کن.
6. `USART1` را روی PA9/PA10 فعال کن.
7. EXTI را روی PB2، PB4 و PB6 تنظیم کن.
8. خروجی‌های PA4، PA8، PB0، PB1، PB5، PB7، PB10 و PB11 را تنظیم کن.
9. فایل‌های این مخزن را به پروژه اضافه کن.
10. Include Pathهای زیر را اضافه کن:

```text
BSP/Inc
Config/Inc
App/Inc
RTOS/Inc
Integration/Inc
Core/Inc
```

11. در `main.c` بعد از تمام `MX_*_Init()`ها، `firmware_init()` را صدا بزن. فقط اگر `firmware_is_ready()` مقدار true داشت، `vTaskStartScheduler()` را اجرا کن.
12. handleهای مورد انتظار Integration عبارت‌اند از:

```text
hadc1
htim2
htim3
huart1
```

راهنمای جزئی‌تر در `CubeMX/README.md` و `Integration/README.md` است.

راهنمای کامل آماده‌سازی پروژه برای ارسال در `docs/STM32_PROJECT_PREPARATION.md` قرار دارد.

Budget و روش بررسی RAM/Flash/Heap در `docs/memory-budget.md` ثبت شده است. قبل از فعال‌کردن Featureهای جدید، مقدار واقعی از Map و Free Heap خوانده شود.

---

## 14. پارامترهای موقت و موارد نیازمند تأیید

مقادیر زیر از روی شماتیک به‌صورت اولیه درج شده‌اند و برای تولید قابل اتکا نیستند:

- `Low Battery = 20.0 V`
- `Low Battery Recovery = 21.0 V`
- `Over Current 1 = 10 A`
- `Over Current 2 = 10 A`
- ضریب جریان هر دو کانال
- active-high یا active-low بودن Battery Switch
- active-high یا active-low بودن رله‌ی شارژر
- active-high یا active-low بودن حفاظت باتری
- active-high یا active-low بودن LED و Buzzer
- فرکانس PWM
- حداکثر Duty Cycle
- پروتکل ESP8266

این مقادیر در `Config/Inc/app_config.h` متمرکز شده‌اند.

---

## 15. نکات سخت‌افزاری مهم

- PB4 در Full JTAG با `NJTRST` تداخل دارد؛ Serial Wire لازم است.
- خروجی LM358 با تغذیه‌ی ۵ ولت باید از نظر محدوده‌ی ۳٫۳ ولت ADC تأیید شود.
- نسبت تقسیم‌های ADC باید با مقاومت‌های Block Diagram، از جمله 68K و 33K، محاسبه شود.
- نام‌های `MCU_BAT_SWITCH`، `GATE_ON_OFF`، `LOW_BAT_MICRO` و `MCU_PROTECT_BATT` باید در نسخه‌ی نهایی شماتیک یکسان‌سازی شوند.
- رفتار Break-Before-Make و جلوگیری از اتصال ناخواسته‌ی دو منبع باید با اسیلوسکوپ بررسی شود.
- حفاظت نرم‌افزاری جایگزین فیوز، محدودکننده‌ی جریان و حفاظت سخت‌افزاری نیست.

---

## 16. نقشه‌ی راه توسعه

### فاز ۱ — اسکلت و Bring-up

- [ ] تولید پروژه‌ی CubeMX واقعی
- [ ] Build بدون خطا با GCC و HAL واقعی
- [ ] تست GPIO و LED
- [ ] تست SWD و Watchdog
- [ ] تست UART

### فاز ۲ — اندازه‌گیری

- [ ] تست ADC با مولتی‌متر
- [ ] کالیبراسیون ولتاژها
- [ ] کالیبراسیون Current1 و Current2
- [ ] تست Jitter1 و Jitter2

### فاز ۳ — کنترل کم‌خطر

- [ ] تست PWM بدون اتصال مرحله‌ی قدرت
- [ ] تأیید فرکانس و Duty
- [ ] تست رله با منبع محدود
- [ ] تست Battery Switch با ولتاژ پایین و بار محدود

### فاز ۴ — منطق کامل

- [ ] پیاده‌سازی CC/CV
- [ ] پیاده‌سازی Break-Before-Make
- [ ] تکمیل Fault Recovery
- [ ] تکمیل پروتکل ESP8266
- [ ] تحلیل MISRA و ثبت Deviationها
- [ ] تست HIL و تست بار

---

## 17. تاریخچه و نگهداری تصمیم‌ها

- تاریخچه‌ی Taskها در `PROJECT_HISTORY.md` ثبت می‌شود و Entryهای جدید باید فارسی و انگلیسی باشند.
- قراردادهای قابل استفاده برای AI در `AI_CONTEXT.md` ثبت می‌شوند.
- روند اجباری Approval و اجرای Taskها در `AI_WORKFLOW.md` ثبت شده است.
- استاندارد کامنت‌گذاری در `docs/commenting-standard.md` است.
- چک‌لیست ممیزی نهایی در `docs/final-audit.md` است.

## 18. وضعیت انطباق

این مخزن **هدف MISRA C دارد، اما هنوز Claim انطباق کامل نمی‌کند**. برای Claim واقعی باید موارد زیر انجام شود:

- تعیین نسخه‌ی دقیق MISRA و Scope تحلیل
- انتخاب ابزار تحلیل استاتیک
- فعال‌سازی Rule Set کامل
- تحلیل HAL/FreeRTOS یا ثبت Exclusion/Deviation
- رفع خطاهای Mandatory و Required
- بررسی Advisoryها و تصمیم مستند برای هر مورد
- Code Review
- تست واحد و تست سخت‌افزار
- ثبت گزارش نسخه‌دار تحلیل
