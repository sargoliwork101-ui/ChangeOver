# AI_CONTEXT — سند زنده‌ی پروژه برای دستیار هوش مصنوعی

> این فایل برای حفظ پیوستگی تصمیم‌ها، قراردادها و اسکلت پروژه نوشته شده است. هر دستیار هوش مصنوعی که روی این Repository کار می‌کند باید ابتدا این فایل و سپس `README.md` را بخواند. این فایل جایگزین نیازمندی رسمی ایمنی یا سند طراحی سخت‌افزار نیست.

---

## 1. هویت پروژه

- نام پروژه: `ChangeOver_FreeRTOS`
- هدف: Firmware ماژولار برای مدار Changeover با ورودی/باتری ۲۴ ولت و دو کانال شارژر
- MCU: `STM32F103C8T6`
- IDE هدف: `STM32CubeIDE`
- لایه‌ی سخت‌افزار: `STM32 HAL`
- RTOS: `FreeRTOS`
- زبان نهایی: **C مطابق هدف MISRA C**
- وضعیت سخت‌افزار: طراحی شماتیک؛ هنوز تست عملی و Bring-up کامل انجام نشده است
- فایل مرجع سخت‌افزار: `ChangeOver(24V_DC).pdf`

---

## 2. تصمیم‌های قطعی پروژه

### تصمیم 001 — زبان و معماری

در گفت‌وگو معماری C++ و کلاس‌محور بررسی شد. تصمیم بعدی و معتبر این است:

> پروژه باید بر اساس **MISRA C** نوشته شود.

در نتیجه، پیشنهاد قبلی C++ کنار گذاشته شده است. در پروژه‌ی نهایی:

- کلاس C++ استفاده نمی‌شود.
- هر ماژول یک `.h` و یک `.c` دارد.
- state ماژول‌ها در `struct` نگهداری می‌شود.
- API ماژول‌ها با توابع `*_init`, `*_update`, `*_get` و مشابه آن ارائه می‌شود.
- اگر کاربر دوباره درباره‌ی کلاس پرسید، تضاد آن با MISRA C توضیح داده شود و معادل C پیشنهاد شود.

### تصمیم 002 — FreeRTOS

Taskهای مستقل فقط برای مسئولیت‌های هم‌زمان ساخته می‌شوند؛ هر ماژول الزاماً Task جدا ندارد.

Taskهای اصلی:

```text
MeasurementTask
ProtectionTask
ControlTask
CommunicationTask
UiTask
```

Task، Queue، Mutex و EventGroup در Application باید Static باشند.

### تصمیم 003 — مالکیت خروجی‌ها

برای جلوگیری از Race Condition:

- منطق تغییر خروجی‌های قدرت از مسیر `ActuatorManager` عبور می‌کند.
- `ControlTask` مالک منطقی تغییر وضعیت توان است.
- `ProtectionTask` Fault را ثبت می‌کند؛ منطق مرکزی باید در چرخه‌ی کنترل خروجی را امن کند.
- ISR نباید مستقیماً منطق توان را اجرا کند.

### تصمیم 004 — ایمنی پیش‌فرض

تا وقتی اولین فریم کامل ADC دریافت نشده است:

- PWM خاموش است.
- رله‌ی شارژر خاموش است.
- Battery Switch در حالت امن است.
- خروجی توان فعال نمی‌شود.

هیچ مقدار موقت شماتیک نباید بدون تأیید کاربر برای تست توان نهایی تلقی شود.

---

## 3. Pin Map مورد توافق

```text
PA0  = PWM1 / TIM2_CH1
PA1  = ADC Current1 / ADC1_IN1
PA2  = ADC 24V Input / ADC1_IN2
PA3  = ADC 24V Battery / ADC1_IN3
PA4  = Buzzer
PA5  = ADC 12V Battery / ADC1_IN5
PA6  = PWM2 / TIM3_CH1
PA7  = ADC Current2 / ADC1_IN7
PA8  = ESP8266 CH_PD
PA9  = USART1_TX
PA10 = USART1_RX
PA13 = SWDIO
PA14 = SWCLK
PB0  = Red LED
PB1  = Yellow LED
PB2  = Jitter1 / EXTI2
PB4  = 24V Input Detect / EXTI4
PB5  = Battery Switch
PB6  = Jitter2 / EXTI6
PB7  = Charger Protection / Relay
PB10 = Green LED
PB11 = Battery Protection
```

نکته: PB4 با Full JTAG تداخل دارد؛ Debug باید Serial Wire باشد.

---

## 4. قرارداد ADC DMA

ترتیب فعلی Rankها:

```text
0: Current1      PA1
1: Input24V      PA2
2: Battery24V    PA3
3: Battery12V    PA5
4: Current2      PA7
```

اگر این ترتیب در CubeMX تغییر کرد، کد و این فایل باید با هم به‌روزرسانی شوند.

---

## 5. چیزهایی که نباید بدون تأیید تغییر کنند

- تغییر زبان پروژه از C به C++
- فعال‌کردن حافظه‌ی پویا برای مسیرهای کنترلی
- تولید PWM با Delay یا GPIO software
- تغییر Pin Map
- فرض‌کردن active level خروجی‌ها
- فرض‌کردن ضرایب ADC و جریان
- فعال‌کردن خودکار PWM توان
- حذف Fault و حفاظت برای ساده‌شدن تست
- اجرای پردازش سنگین داخل ISR
- حذف Deviation Record مربوط به HAL و FreeRTOS

---

## 6. موارد باز و سؤال‌های پاسخ‌داده‌نشده

این موارد باید قبل از پیاده‌سازی نهایی از کاربر پرسیده و سپس در این فایل ثبت شوند:

1. فرکانس PWM1 و PWM2 چیست؟
2. آیا PWM1 و PWM2 باید هم‌فاز یا Sync باشند؟
3. حداکثر Duty Cycle هر کانال چقدر است؟
4. باتری دقیقاً چه شیمی و محدوده‌ی ولتاژی دارد؟
5. Low Battery و Recovery دقیقاً چه آستانه‌هایی هستند؟
6. جریان نامی و Over Current هر کانال چقدر است؟
7. ضریب تبدیل Current1 و Current2 پس از اندازه‌گیری چیست؟
8. active-high یا active-low بودن خروجی‌های PB5، PB7 و PB11 چیست؟
9. هنگام وجود هم‌زمان ورودی و باتری، سیاست Changeover چیست؟
10. Break-Before-Make مورد نیاز چند میلی‌ثانیه است؟
11. پروتکل UART با ESP8266 چیست؟
12. آیا ESP8266 فقط Telemetry می‌گیرد یا فرمان کنترل هم می‌دهد؟
13. رفتار مطلوب در Watchdog Reset چیست؟
14. آیا ورودی ۲۴ ولت دیجیتال PB4 باید با EXTI کار کند یا Polling کافی است؟

تا زمان پاسخ کاربر، برای مقادیر نامعلوم از Placeholder امن استفاده شود و PWM فعال نشود.

---

## 7. روش کار دستیار هوش مصنوعی

هر بار که روی پروژه کار می‌کنی:

1. ابتدا `AI_WORKFLOW.md` را بخوان.
2. سپس `AI_CONTEXT.md`، `README.md` و فایل‌های مرتبط را بررسی کن.
3. تصمیم‌های قبلی را بی‌دلیل عوض نکن.
4. اگر تصمیمی تغییر کرد، بخش Decision Log را به‌روزرسانی کن.
5. هیچ ادعای MISRA Compliance کامل نکن مگر اینکه ابزار تحلیل و گزارش وجود داشته باشد.
6. در صورت نبود اطلاعات سخت‌افزاری، Placeholder را با مقدار دلخواه پر نکن.
7. قبل از تغییر معماری یا Pin Map، از کاربر تأیید بگیر.
8. کد جدید باید با الگوی `module.h/module.c`، API مشخص و state محدود نوشته شود.
9. تمام مقادیر سخت‌افزاری و Thresholdها باید در `Config/Inc/app_config.h` متمرکز باشند.
10. برای هر تغییر مهم، یک ردیف به Decision Log اضافه کن.

---

## 8. قالب ثبت تصمیم جدید

برای اضافه‌کردن تصمیم جدید از این قالب استفاده کن:

```text
### تصمیم NNN — عنوان

تاریخ:
وضعیت: Proposed / Accepted / Superseded
تصمیم:
دلیل:
اثر روی کد:
اثر روی سخت‌افزار:
فایل‌های مرتبط:
```

---

## 9. Decision Log

### تصمیم 005 — تبدیل اسکلت اولیه به MISRA C

تاریخ: 2026-08-16
وضعیت: Accepted
تصمیم: اسکلت C++ قبلی باید به ساختار ماژولار C تبدیل شود تا هدف MISRA C رعایت شود.
دلیل: درخواست مستقیم کاربر و نیاز به ساختار قابل تحلیل استاتیک در Firmware.
اثر روی کد: پسوندها `.h/.c`، حذف کلاس‌ها، استفاده از struct و APIهای ماژولی.
اثر روی سخت‌افزار: ندارد.
فایل‌های مرتبط: `README.md`، `docs/misra-c-guidelines.md`، کل پوشه‌های `App` و `RTOS`.

### تصمیم 006 — سند زنده برای AI

تاریخ: 2026-08-16
وضعیت: Accepted
تصمیم: فایل `AI_CONTEXT.md` مرجع ثبت حرف‌ها، اسکلت‌ها و قرارهای فنی پروژه باشد.
دلیل: حفظ پیوستگی بین جلسات و جلوگیری از تغییر ناخواسته‌ی تصمیم‌ها.
اثر روی کد: دستیار باید قبل از تغییرات این فایل را بخواند و تصمیم‌های جدید را ثبت کند.
اثر روی سخت‌افزار: ندارد.

---

## 10. آخرین وضعیت اسکلت

- [x] ساختار لایه‌ای پروژه
- [x] Pin Map اولیه
- [x] تقسیم Taskهای FreeRTOS
- [x] مدل Measurement / Fault / Actuator
- [x] Wrapperهای HAL
- [x] فایل‌های MISRA C با پسوند `.h/.c`
- [x] README فنی
- [x] سند AI قابل توسعه
- [ ] CubeMX `.ioc` واقعی
- [ ] Build با HAL و FreeRTOS واقعی
- [ ] تست روی برد
- [ ] گزارش تحلیل استاتیک MISRA
- [ ] کالیبراسیون ADC و جریان

---

## 11. رابط کاربر

ماژول رابط کاربر در این نسخه آماده شده است:

```text
App/Inc/user_interface.h
App/Src/user_interface.c
RTOS/Inc/ui_task.h
RTOS/Src/ui_task.c
```

قرارداد فعلی:

- Red LED: هر Fault
- Yellow LED: Low Battery یا Charging
- Green LED: فعال بودن یکی از منابع بدون Fault
- Buzzer: الگوی کوتاه غیرمسدودکننده برای Fault بحرانی و Low Battery
- هیچ Delay یا پردازش Blocking در رابط کاربر مجاز نیست.
- `indicator_manager` قبلی با `user_interface` جایگزین شده است.

هر تغییر در رنگ، الگوی صدا یا اولویت آلارم باید در `PROJECT_HISTORY.md` ثبت شود.

---

## 12. مستندسازی کد

تمام فایل‌های `.c` و `.h` باید دارای:

- File Header
- Function Documentation
- توضیح پارامترها
- توضیح Return Value
- Safety Note
- ISR/Callback Note در صورت نیاز
- اشاره به Deviation در صورت وجود

استاندارد کامنت‌گذاری در این فایل است:

```text
docs/commenting-standard.md
```

کامنت‌ها به‌تنهایی اثبات MISRA نیستند.

---

## 13. آماده‌سازی پروژه‌ی واقعی STM32

قبل از جایگذاری، کاربر باید پروژه‌ی واقعی CubeIDE/CubeMX را طبق این فایل بسازد:

```text
docs/STM32_PROJECT_PREPARATION.md
```

سپس ZIP شامل `.ioc`، کد تولیدشده‌ی CubeMX، HAL/FreeRTOS و پوشه‌های این پروژه را ارسال کند.

---

## 14. تاریخچه‌ی کامل کار

تاریخچه‌ی Taskها و تصمیم‌های انجام‌شده در فایل زیر ثبت می‌شود:

```text
PROJECT_HISTORY.md
```

هر دستیار باید قبل از شروع کار جدید این فایل را بخواند و بعد از پایان Task آن را به‌روزرسانی کند.

---

## 15. وضعیت FreeRTOS Static Allocation

برای هدف MISRA و کنترل حافظه:

```text
configSUPPORT_STATIC_ALLOCATION = 1
configSUPPORT_DYNAMIC_ALLOCATION = 0
configUSE_TIMERS = 0 در اسکلت نمونه
```

Hookهای موردنیاز در این فایل قرار دارند:

```text
RTOS/Src/freertos_hooks.c
```

اگر CubeMX نسخه‌ی دیگری از Hookها تولید کرد، فقط یک تعریف نهایی از هر Hook باید باقی بماند.

---

## 16. ممیزی نهایی فعلی

آخرین ممیزی اسکلت در 2026-08-16 انجام شد:

- فایل‌های `.c/.h` بدون فایل C++ باقی مانده‌اند.
- همه‌ی 50 فایل کدنویسی File Header دارند.
- APIها و توابع داخلی کامنت مستند دارند.
- رابط کاربر مستقل با LED و Buzzer وجود دارد.
- FreeRTOS نمونه برای Static Allocation تنظیم شده است.
- Dynamic Allocation در نمونه‌ی Config غیرفعال است.
- Hookهای Idle Task و Stack Overflow وجود دارند.
- `.gitignore` فایل‌های پروژه‌ی واقعی CubeMX را حذف نمی‌کند.
- Syntax و Stub Link با موفقیت انجام شده‌اند.

این وضعیت هنوز جایگزین Build روی STM32، تحلیل رسمی MISRA و تست سخت‌افزار نیست.

---

## 17. گزارش ممیزی نهایی

برای هر تحویل نهایی، چک‌لیست زیر باید بررسی شود:

```text
docs/final-audit.md
```

این گزارش باید قبل از ارسال ZIP جدید به‌روزرسانی شود.

---

## 18. قرارداد اجباری روند کار AI

قبل از هر تغییر واقعی، AI باید استراتژی، فایل‌های درگیر، ریسک، تست و Rollback را به کاربر اعلام کند و تا تأیید صریح کاربر صبر کند.

قرارداد کامل در این فایل است:

```text
AI_WORKFLOW.md
```

بعد از هر Task، حداقل این اسناد باید بررسی و در صورت نیاز به‌روزرسانی شوند:

```text
PROJECT_HISTORY.md
README.md
AI_CONTEXT.md
docs/debug-diagnostics.md
docs/deviation-record.md
```

---

## 19. Diagnostics و ESP8266

ماژول Diagnostics و جدول کدها:

```text
App/Inc/diagnostics.h
App/Src/diagnostics.c
docs/debug-diagnostics.md
```

قالب خط ارتباطی:

```text
D,<code>,<severity>,<value>,<fault_mask>,<state>,<occurrence_count>\r\n
```

هر Code پایدار است و نباید برای معنی جدید reuse شود. تغییر کد یا Protocol بدون ثبت History و تأیید کاربر ممنوع است.

---

## 20. Clean Repository Policy

فایل‌های موقت، ZIPهای قدیمی و Placeholderهای غیرضروری نباید داخل تحویل نهایی بمانند. خروجی نهایی فقط شامل Repository و یک ZIP نهایی خارج از آن است.

فایل PDF شماتیک در مسیر Upload کاربر نگه داشته شده و عمداً داخل Repository کپی نشده است؛ چون فایل مرجع سخت‌افزار است و برای Git Source Code ضروری نیست.
