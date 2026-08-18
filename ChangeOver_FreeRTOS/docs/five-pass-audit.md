# Five-Pass Audit Report

تاریخ: 2026-08-17

این گزارش در پنج Pass مستقل روی Repository اجرا شده است.

## Pass 1 — ساختار و پاک‌سازی

### بررسی

- پوشه‌ی STM32 و ESP از هم جدا هستند.
- فایل اصلی ESP با پسوند `.ino` وجود دارد.
- صفحات Web در `ESP8266/data/` جدا هستند.
- فایل‌های موقت و ZIPهای قدیمی خارج از Repository حذف شدند.
- فایل PDF اصلی عمداً داخل Git Repository کپی نشده است.
- `.gitignore` فایل‌های واقعی CubeMX را ignore نمی‌کند.

### نتیجه

```text
PASS
```

## Pass 2 — Compile، Link و Dependency

### STM32

با Stubهای HAL و FreeRTOS:

```text
C99 strict syntax: PASS
Stub link: PASS
-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror: PASS
```

### ESP8266

با Stubهای Arduino، WiFi، WebServer و LittleFS:

```text
Default feature syntax: PASS
All feature flags temporarily enabled: PASS
Host parser test: PASS
```

### محدودیت

Build واقعی با `STM32CubeIDE` و هسته‌ی واقعی Arduino ESP8266 هنوز انجام نشده است.

## Pass 3 — ایمنی و کدنویسی Embedded

### کنترل‌ها

- STM32 با C و الگوی `module.h/module.c` است.
- حافظه‌ی پویا در Application STM32 فعال نیست.
- FreeRTOS Objects به‌صورت Static تعریف شده‌اند.
- ISR فقط از مسیر Queue/Notification مناسب استفاده می‌کند.
- خروجی‌های قدرت از ActuatorManager عبور می‌کنند.
- Safe State برای Boot، ADC، Over Current، Init Failure و Stack Overflow وجود دارد.
- Stack Overflow مسیر Emergency Diagnostic بدون Mutex دارد.
- ESP Featureها مرحله‌ای و پیش‌فرض محدود هستند.
- ESP Command به STM32 پیش‌فرض خاموش است.
- JSON و Store APIها سقف رکورد دارند.

### نتیجه

```text
PASS با Deviationهای ثبت‌شده
```

Deviationها در `docs/deviation-record.md` ثبت شده‌اند.

## Pass 4 — پروتکل، Web و Storage

### بررسی

- صفحات `/`، `/debug`، `/charts`، `/test` و `/settings` وجود دارند.
- APIهای Health، Status، Diagnostics، Telemetry، Features و Auto Test وجود دارند.
- پاک‌کردن Diagnostics با Endpoint جدا انجام می‌شود.
- Diagnostics و Telemetry در RAM قابل تست هستند.
- Persistent Storage با Feature Flag جدا فعال می‌شود.
- Runtime Statistics قابلیت ذخیره‌سازی مستقل دارد.
- قالب `T,...` و `D,...` پشتیبانی می‌شود.
- Handshake، Session، Sequence، ACK، Heartbeat، CRC16 و Power-Down Prepare/Ready اسکلت دارند.
- ESP می‌تواند قبل از خاموشی، آماده‌بودن خود را اعلام کند.

### نتیجه

```text
PASS برای اسکلت نمونه
```

### محدودیت

تا زمان اضافه‌شدن Pending Queue و Replay سمت STM32، عدم از دست‌رفتن داده در زمان خاموشی ESP تضمین نهایی ندارد.

## Pass 5 — RAM، Flash، Stack، Heap و LittleFS

### STM32

- پنج Task اصلی: 5120 bytes Stack با Idle Task.
- ADC DMA buffer: حدود 20 bytes.
- Jitter Queue و Static RTOS Objects جداگانه باید از Map واقعی خوانده شوند.
- Communication buffers داخل Stack مربوط به Task قرار دارند.
- Budget و حدود پذیرش در `docs/memory-budget.md` ثبت شده است.

### ESP8266

- Diagnostic RAM Ring: حدود 1536 bytes.
- Telemetry RAM Ring: حدود 3840 bytes.
- Line Buffer: حدود 192 bytes.
- API پاسخ‌ها به 32 Diagnostic و 60 Telemetry محدود شده‌اند.
- Web Pages با LittleFS Stream می‌شوند.
- Free Heap در Dashboard نمایش داده می‌شود.

### نتیجه

```text
برآورد اولیه PASS؛ اندازه‌گیری واقعی نیازمند Map و Hardware Build است.
```

## خلاصه‌ی موارد اصلاح‌شده در ممیزی

- محدودکردن پاسخ JSON برای کنترل Heap و Fragmentation
- اصلاح Handshake برای استفاده از Peer Session
- اضافه‌کردن CRC16 به فریم‌های Handshake
- اضافه‌کردن Runtime Storage اختیاری
- اضافه‌کردن Memory Budget
- اضافه‌کردن گزارش دوزبانه به History
- ثبت قرارداد Approval و Strategy برای AI

## Baseline پس از ممیزی

هیچ Feature محصولی Approved نیست. Power Stage، ESP Link و تمام ESP Featureهای اختیاری در حالت پیش‌فرض خاموش هستند.

## نتیجه‌ی نهایی

Repository برای مرحله‌ی ساخت پروژه‌ی واقعی آماده است، اما عبارت «کاملاً تأییدشده» فقط بعد از این موارد مجاز است:

1. Build واقعی STM32 با `.ioc`
2. مشاهده‌ی Map و Size واقعی
3. بررسی Stack High Water Mark
4. Build واقعی ESP8266 Arduino
5. Upload LittleFS
6. تست هر Feature طبق Phase
7. تست UART، Handshake و Power-Down روی برد
8. تست Reset و بازیابی فایل‌ها
9. Static Analysis رسمی MISRA برای STM32
