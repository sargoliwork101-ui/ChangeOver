# Project History

این فایل تاریخچه‌ی تصمیم‌ها و کارهای انجام‌شده روی پروژه است. هر تغییر مهم باید با تاریخ، دلیل، فایل‌های مرتبط و وضعیت تست در همین فایل ثبت شود.

> تاریخ‌ها بر اساس زمان محلی پروژه در نظر گرفته می‌شوند.

---

## 2026-08-16 — بررسی شماتیک و استخراج معماری اولیه

**وضعیت:** انجام شد

کارهای انجام‌شده:

- فایل `ChangeOver(24V_DC).pdf` بررسی شد.
- ساختار کلی مدار شامل Changeover، Power Supply، Charger، Current Sense، Jitter، MCU و ESP8266 مشخص شد.
- MCU به‌عنوان `STM32F103C8T6` شناسایی شد.
- Pin Map اولیه استخراج شد.
- ورودی‌های ADC، PWMها، ورودی‌های EXTI و خروجی‌های LED/Buzzer مشخص شدند.

فایل‌های مرتبط:

```text
README.md
docs/pin-map.md
BSP/Inc/board_pins.h
```

---

## 2026-08-16 — تصمیم اولیه‌ی FreeRTOS و ماژولار بودن

**وضعیت:** انجام شد، با تصمیم MISRA C به‌روزرسانی شد

تصمیم اولیه:

- استفاده از FreeRTOS
- جداسازی Driver، Application و RTOS Task
- استفاده از Snapshot برای اندازه‌گیری
- استفاده از Fault Manager
- استفاده از Actuator Manager برای مالکیت خروجی‌ها

Taskهای تعیین‌شده:

```text
MeasurementTask
ProtectionTask
ControlTask
CommunicationTask
UiTask
```

---

## 2026-08-16 — کنارگذاشتن معماری C++

**وضعیت:** Superseded

در ابتدا معماری C++ و کلاس‌محور بررسی و یک اسکلت C++ ساخته شد. سپس با توجه به نیاز پروژه، تصمیم نهایی به MISRA C تغییر کرد.

تصمیم نهایی:

- C++ و کلاس‌ها از نسخه‌ی نهایی حذف شوند.
- هر ماژول با فایل `.h` و `.c` پیاده‌سازی شود.
- state داخلی در `struct` نگهداری شود.
- حافظه‌ی پویا در Application استفاده نشود.

فایل مرتبط:

```text
AI_CONTEXT.md
```

---

## 2026-08-16 — تبدیل اسکلت به ماژول‌های MISRA C

**وضعیت:** اسکلت انجام شد؛ تحلیل رسمی MISRA باقی است

کارهای انجام‌شده:

- فایل‌های C++ به ساختار C ماژولار تبدیل شدند.
- BSPهای GPIO، ADC، PWM و UART آماده شدند.
- Measurement Manager با تبدیل Fixed-Point به millivolt/milliamp ایجاد شد.
- Fault Manager با EventGroup استاتیک ایجاد شد.
- Jitter Detector با Queue استاتیک ایجاد شد.
- Controllerهای Charger و Changeover ایجاد شدند.
- Taskهای FreeRTOS با Static Allocation ایجاد شدند.
- Integration با handleهای `hadc1`، `htim2`، `htim3` و `huart1` آماده شد.

تست انجام‌شده:

- Syntax Check با GCC و Stubهای HAL/FreeRTOS
- گزینه‌های هشدار:

```text
-Wall
-Wextra
-Wpedantic
-Wconversion
-Wsign-conversion
-Wshadow
-Werror
```

**محدودیت:** این تست جایگزین Build با HAL، FreeRTOS و Linker واقعی STM32 نیست.

---

## 2026-08-16 — README و سند اختصاصی AI

**وضعیت:** انجام شد

فایل‌های زیر ایجاد و تکمیل شدند:

```text
README.md
AI_CONTEXT.md
docs/architecture.md
docs/misra-c-guidelines.md
docs/deviation-record.md
```

`AI_CONTEXT.md` مرجع دستیار هوش مصنوعی برای تصمیم‌ها، قراردادها، سؤال‌های باز و تاریخچه‌ی تصمیمات است.

---

## 2026-08-16 — تکمیل رابط کاربر

**وضعیت:** اسکلت انجام شد؛ تست روی برد باقی است

ماژول اختصاصی رابط کاربر ایجاد شد:

```text
App/Inc/user_interface.h
App/Src/user_interface.c
```

قابلیت‌های فعلی:

- LED قرمز برای Fault
- LED زرد برای Low Battery و Charging
- LED سبز برای فعال بودن منبع
- الگوی غیرمسدودکننده‌ی Buzzer برای Fault بحرانی
- الگوی کوتاه Buzzer برای Low Battery
- بدون استفاده از Delay یا اجرای Blocking در UI

Task مربوط:

```text
RTOS/Inc/ui_task.h
RTOS/Src/ui_task.c
```

---

## 2026-08-16 — افزودن کامنت‌های مهندسی و MISRA-oriented

**وضعیت:** انجام شد؛ بررسی نهایی با ابزار Static Analysis باقی است

کارهای انجام‌شده:

- برای تمام فایل‌های `.c` و `.h` هدر مستندات فایل اضافه شد.
- برای تمام APIهای public و helperهای داخلی، کامنت تابع اضافه شد.
- برای پارامترها، خروجی، زمینه‌ی ISR، ایمنی و مرزهای HAL/FreeRTOS توضیح اضافه شد.
- استاندارد کامنت‌گذاری در فایل زیر ثبت شد:

```text
docs/commenting-standard.md
```

توجه: کامنت‌گذاری به‌تنهایی انطباق MISRA را اثبات نمی‌کند.

---

## قالب ثبت کار جدید

برای هر Task جدید از قالب زیر استفاده کن:

````markdown
## YYYY-MM-DD — عنوان کار

**وضعیت:** انجام شد / در حال انجام / متوقف / نیازمند اطلاعات

**درخواست کاربر:**

**کارهای انجام‌شده:**

- ...

**فایل‌های تغییرکرده:**

```text
path/to/file.c
path/to/file.h
```

**تست یا اعتبارسنجی:**

- ...

**تصمیم یا نکته‌ی جدید:**

- ...

**کارهای باقی‌مانده:**

- ...
````

---

## 2026-08-16 — راهنمای جایگذاری، کامنت‌گذاری کامل و تاریخچه‌ی توسعه

**وضعیت:** انجام شد

کارهای انجام‌شده:

- File Header برای تمام فایل‌های C و Header اضافه شد.
- Function Documentation برای APIهای public و helperهای داخلی اضافه شد.
- برای ISR، Callback و Task Entry توضیح ایمنی اضافه شد.
- فایل استاندارد کامنت‌گذاری ایجاد شد.
- فایل تاریخچه‌ی همین پروژه ایجاد شد.
- راهنمای مرحله‌به‌مرحله‌ی ساخت پروژه‌ی STM32/CubeMX ایجاد شد.
- محدودیت مهم ثبت شد: کامنت‌ها به‌تنهایی MISRA Compliance را اثبات نمی‌کنند.
- روش ارسال پروژه‌ی واقعی شامل `.ioc` برای جایگذاری آینده ثبت شد.

فایل‌های مرتبط:

```text
docs/commenting-standard.md
docs/STM32_PROJECT_PREPARATION.md
PROJECT_HISTORY.md
```

تست انجام‌شده:

- Syntax Check با GCC و هشدارهای سخت‌گیرانه با موفقیت انجام شد.
- تعداد فایل‌های کدنویسی مستندشده بررسی شد.

کارهای باقی‌مانده:

- Build با پروژه‌ی واقعی CubeMX
- Static Analysis با ابزار MISRA
- تطبیق Deviationها با گزارش ابزار
- تست واقعی LED، Buzzer، ADC، PWM و Fault روی برد

---

## 2026-08-16 — ممیزی نهایی اسکلت و اصلاحات ایمنی/Build

**وضعیت:** انجام شد

اصلاحات نهایی:

- `.gitignore` اصلاح شد تا فایل‌های پروژه‌ی واقعی CubeMX، شامل `.ioc`، `Core`، `Drivers` و `Middlewares` به‌اشتباه از Git حذف نشوند.
- `FreeRTOSConfig.example.h` برای Static Allocation بازتنظیم شد.
- Dynamic Allocation در نمونه‌ی تنظیمات FreeRTOS غیرفعال شد.
- `freertos_hooks.c` برای Idle Task استاتیک و Stack Overflow اضافه شد.
- محاسبات ADC در برابر تقسیم بر صفر و Overflow واسط مقاوم‌تر شدند.
- اسکن UART به‌صورت bounded اصلاح شد تا از خواندن خارج از محدوده جلوگیری شود.
- منطق نتیجه‌ی ساخت Taskها در `firmware_app.c` اصلاح شد.
- در صورت شکست ساخت Taskها، Actuatorها به Safe State برده می‌شوند.
- Deviationهای مربوط به HAL، Hookهای FreeRTOS و Loopهای Task ثبت شدند.
- README، AI_CONTEXT و راهنمای آماده‌سازی STM32 با وضعیت نهایی هماهنگ شدند.

اعتبارسنجی:

- Syntax Check با GCC و هشدارهای سخت‌گیرانه موفق بود.
- Stub Link Check موفق بود.
- تمام فایل‌های C/H دارای File Header هستند.
- تمام APIها و توابع داخلی دارای Function Documentation هستند.

---

## 2026-08-16 — ممیزی نهایی درخواست‌ها

**وضعیت:** انجام شد

چک‌لیست نهایی در فایل زیر ثبت شد:

```text
docs/final-audit.md
```

تمام موارد درخواست‌شده شامل مستندات، AI Context، تاریخچه، کامنت‌گذاری، رابط کاربر، راهنمای STM32، ساختار MISRA C و اعتبارسنجی اسکلت دوباره بررسی شدند.

---

## 2026-08-16 — قرارداد اجباری AI و سامانه‌ی Diagnostics

**وضعیت:** در حال توسعه؛ اسکلت Diagnostics انجام شد

کارهای انجام‌شده:

- فایل `AI_WORKFLOW.md` برای روند اجباری کار AI ایجاد شد.
- AI موظف شد قبل از هر تغییر، استراتژی و ریسک را اعلام کند و تأیید صریح بگیرد.
- AI موظف شد بعد از هر Task، README، AI Context، History و اسناد مرتبط را همگام کند.
- ماژول `diagnostics` برای ذخیره‌ی آخرین رویداد، Severity، مقدار، Fault Mask، State و تعداد تکرار ایجاد شد.
- جدول رسمی کدهای Diagnostics ایجاد شد.
- قالب خط UART برای ارسال Diagnostics به ESP8266 تعریف شد.
- `Esp8266Service` به Diagnostics متصل شد.
- رویدادهای Boot، Init Failure، ADC Failure، Input Transition، Source Transition، Low Battery، Over Current، ESP TX Failure و Stack Overflow در مسیر گزارش قرار گرفتند.
- مسیر Emergency Diagnostics برای Stack Overflow اضافه شد تا در Hook بحرانی Mutex گرفته نشود.

فایل‌های مرتبط:

```text
AI_WORKFLOW.md
App/Inc/diagnostics.h
App/Src/diagnostics.c
docs/debug-diagnostics.md
```

اعتبارسنجی:

- Syntax Check و Stub Link بعد از تغییرات باید دوباره اجرا شوند.

کارهای باقی‌مانده:

- تعیین پروتکل کامل Telemetry با ESP8266
- تعیین روش نمایش Codeها روی رابط ESP
- تست واقعی UART و ثبت رویدادها روی برد

---

## 2026-08-16 — پاک‌سازی Repository و ممیزی نهایی Diagnostics

**وضعیت:** انجام شد

کارهای انجام‌شده:

- فایل‌های موقت خارج از Repository پاک شدند.
- ZIPهای قدیمی پاک شدند تا فقط خروجی نهایی جدید ساخته شود.
- فایل Placeholder غیرضروری `Core/Src/README.txt` حذف شد.
- بخش AI Workflow و Approval Gate تکمیل شد.
- جدول Diagnostics و Protocol ارسال به ESP8266 اضافه شد.
- Diagnostics به Boot، Initialization، ADC، Protection، Changeover، Communication و Stack Overflow متصل شد.
- `diagnostics_report_emergency()` برای Hook بحرانی اضافه شد تا در Stack Overflow Mutex گرفته نشود.
- تست Stub Link و Strict Syntax دوباره با موفقیت اجرا شد.

فایل‌های کلیدی جدید:

```text
AI_WORKFLOW.md
App/Inc/diagnostics.h
App/Src/diagnostics.c
docs/debug-diagnostics.md
```

نتیجه‌ی تست:

```text
52 فایل C/H
198 Function Documentation
Strict Syntax: PASS
Stub Link: PASS
```

---

## 2026-08-16 — تست قالب Diagnostic و Telemetry

**وضعیت:** انجام شد

- قالب Telemetry با فرمت `T,...` به `Esp8266Service` اضافه شد.
- قالب Diagnostics با فرمت `D,...` به ESP8266 ارسال می‌شود.
- تست Host برای تولید خط Diagnostic اجرا شد.
- مقدار نمونه‌ی `D,16386,3,12345,2,7,1` با موفقیت بررسی شد.

نتیجه:

```text
Diagnostics Formatter Test: PASS
```
