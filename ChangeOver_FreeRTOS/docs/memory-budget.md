# Memory Budget and Review

تاریخ ممیزی: 2026-08-17

این سند برای جلوگیری از فعال‌کردن قابلیت‌ها بدون توجه به RAM، Flash و Heap تهیه شده است.

> اندازه‌گیری نهایی فقط با Build واقعی CubeIDE/Arduino و فایل Map معتبر است. اعداد زیر Budget و برآورد مهندسی هستند، نه جایگزین Map.

## STM32F103C8T6

### منابع هدف

```text
Flash: حدود 64 KiB برای کلاس C8
SRAM : حدود 20 KiB
```

مقدار واقعی باید با Part Number، Linker و Map پروژه تأیید شود.

### Stackهای Static FreeRTOS

`StackType_t` روی Cortex-M3 معمولاً 32 بیتی است:

| Task | Stack words | برآورد RAM |
|---|---:|---:|
| MeasurementTask | 256 | 1024 bytes |
| ProtectionTask | 192 | 768 bytes |
| ControlTask | 256 | 1024 bytes |
| CommunicationTask | 256 | 1024 bytes |
| UiTask | 192 | 768 bytes |
| Idle Task | 128 | 512 bytes |
| **جمع Stackها** | **1280** | **5120 bytes** |

علاوه بر Stackها:

- DMA buffer ADC: پنج کلمه‌ی ۳۲ بیتی، حدود ۲۰ bytes
- Jitter Queue buffer: حدود ۱۲۸ bytes با فرض `sizeof(event)=8`؛ مقدار نهایی باید با Compiler واقعی اندازه‌گیری شود.
- Static TCB، Semaphore، EventGroup و Queue storage به اندازه‌ی واقعی FreeRTOS اضافه می‌شوند.
- `firmware_app_t` شامل Context و همه‌ی Stackهای Task است و در SRAM استاتیک قرار می‌گیرد.
- Bufferهای Telemetry و Diagnostics سمت STM32 داخل Stack `CommunicationTask` هستند:
  - Telemetry: 128 bytes
  - Diagnostic: 96 bytes

### Budget پیشنهادی STM32

تا زمان داشتن Map:

```text
SRAM preferred: کمتر از 12 KiB برای .data + .bss
SRAM review limit: کمتر از 14 KiB
Flash preferred: کمتر از 56 KiB
حداقل Headroom: 25 درصد برای Stack هر Task
```

اگر `.data + .bss` از 14 KiB بیشتر شد، قابلیت جدید اضافه نشود و ابتدا Stack/Static Objectها و Linker بررسی شوند.

### الزام Build واقعی

در پروژه‌ی STM32:

```text
-Wl,-Map=ChangeOver.map
arm-none-eabi-size firmware.elf
```

باید موارد زیر بررسی شوند:

- `.text`
- `.rodata`
- `.data`
- `.bss`
- FreeRTOS Static Objects
- Stack High Water Mark هر Task
- مقدار Heap واقعی؛ در حالت Dynamic Allocation خاموش نباید به Heap تکیه شود.

## ESP8266-01

مصرف واقعی به نوع Flash، Core و Partition بستگی دارد و باید با `ESP.getFreeHeap()` و گزارش Build بررسی شود.

### حافظه‌ی استاتیک Storeها

برآورد روی Xtensa با Alignment معمول:

```text
EspDiagnosticRecord  حدود 24 bytes
64 Diagnostic records حدود 1536 bytes
EspTelemetryRecord  حدود 32 bytes
120 Telemetry records حدود 3840 bytes
Line buffer حدود 192 bytes
```

صفحه‌های Web از LittleFS Stream می‌شوند و نباید کل فایل‌ها در RAM کپی شوند.

### محدودیت پاسخ API

برای جلوگیری از JSON بسیار بزرگ و Fragmentation:

```text
Diagnostics API: حداکثر 32 رکورد آخر
Telemetry API: حداکثر 60 رکورد آخر
```

تعداد کل رکوردها و مقدار `truncated` در JSON اعلام می‌شود.

### Heap Budget پیشنهادی ESP

بعد از Boot و فعال‌کردن Web Server:

```text
Free Heap ترجیحی: بیشتر از 20 KiB
Free Heap بحرانی: کمتر از 12 KiB
```

اگر Heap به محدوده‌ی بحرانی رسید:

1. Charts را خاموش کن.
2. تعداد رکورد API را کم کن.
3. JSONهای بزرگ را صفحه‌بندی کن.
4. Storage flush را Batch و کم‌تعداد کن.
5. از Stringهای موقت زیاد جلوگیری کن.

### Flash LittleFS

فایل‌های اصلی:

```text
/diagnostics.bin
/telemetry.bin
/runtime_stats.bin
/data/*
```

نوشتن Log در هر فریم ممنوع است. Storeها Flush دوره‌ای و رویدادی دارند.

## قواعد فعال‌سازی Feature

هر Feature جدید باید اثر حافظه‌ای خود را در History ثبت کند:

```text
Feature:
Static RAM increase:
Peak stack increase:
Flash increase:
LittleFS increase:
Measured free heap:
Test result:
```

## معیار عدم پذیرش

قابلیت جدید تا زمان اصلاح پذیرفته نیست اگر:

- Build از SRAM/Flash Budget عبور کند.
- Stack High Water Mark کم‌تر از 25 درصد Headroom شود.
- ESP در APIهای بزرگ Heap را به محدوده‌ی بحرانی ببرد.
- LittleFS بعد از Reset فایل معتبر را نتواند بخواند.
- فعال‌کردن Feature باعث Watchdog Reset یا Boot Loop شود.
