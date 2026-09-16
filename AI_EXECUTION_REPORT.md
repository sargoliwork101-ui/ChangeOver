# گزارش اجرای AI

**آخرین به‌روزرسانی:** 2026-09-16<br>
**شاخه:** `arena/01a0a923-changeover`<br>
**مالک گزارش:** Agent ارشد پروژه<br>
**قوانین مرجع:** `AI_AGENT_RULES.md`

## خلاصهٔ وضعیت

- فقط ولتاژ ورودی از Measurement/ADC به Task UI متصل است.
- ولتاژ باتری هنوز از `UINT32_T__G__BatteryVoltageMv` و Live Expressions به‌صورت دستی تأمین می‌شود.
- خواندن GPIO در Measurement از لایهٔ BSP انجام می‌شود.
- FreeRTOS فقط از تخصیص استاتیک استفاده می‌کند و Dynamic allocation خاموش است.
- فایل‌های `.ioc` در `CubeMX/` و `CubeIDE/` یکسان هستند.
- اسناد AI به دو سند مرکزی تقسیم شده‌اند: `AI_AGENT_RULES.md` و همین گزارش.
- README هر ماژول متعلق به Agent همان ماژول است و طبق قالب موجود تکمیل می‌شود؛ در این گزارش تغییر دستی READMEهای ماژول ثبت نشده است.

## اتصال فعلی Measurement و UI

- ADC1 با DMA پنج کانال را دریافت می‌کند.
- Measurement هر `10ms` یک فریم را تبدیل و منتشر می‌کند.
- `UINT32_T__G__MeasInputVoltageMv` از `PA2 / ADC1_IN2` ورودی ولتاژ UI است.
- `BOOL__G__MeasDataValid` معتبرشدن فریم را مشخص می‌کند.
- قبل از اولین فریم معتبر، ورودی UI صفر و از نظر سناریو قطع در نظر گرفته می‌شود.
- `UINT32_T__G__MeasBattery24Mv` هنوز به UI وصل نشده است.
- اعلان `func__BspGpio_Read` با اضافه‌شدن `#include "bsp_gpio.h"` برای Measurement کامل شده است.

## زمان‌بندی و رفتار RTOS

این اتصال سربار یا عملیات مسدودکنندهٔ جدیدی اضافه نمی‌کند:

- ADC و DMA توسط سخت‌افزار اجرا می‌شوند.
- Measurement در دورهٔ ۱۰ میلی‌ثانیه‌ای کار کوتاهی انجام می‌دهد.
- UI فقط یک مقدار ۳۲ بیتی و وضعیت اعتبار فریم را می‌خواند.
- `HAL_Delay` یا حلقهٔ انتظار جدیدی اضافه نشده است.
- تأخیرهای `vTaskDelay` موجود در سناریوهای قبلی UI افزایش نیافته‌اند.

## مدیریت حافظه

- `configSUPPORT_STATIC_ALLOCATION = 1`
- `configSUPPORT_DYNAMIC_ALLOCATION = 0`
- Taskها با `xTaskCreateStatic` ساخته می‌شوند.
- حافظهٔ Idle و Timer در `freertos_hooks.c` استاتیک است.
- `heap_4.c` در مخزن Vendor باقی مانده، اما از Build و لینک پروژه خارج شده است.

## اعتبارسنجی انجام‌شده

- `./tools/check_ai_rules.sh` — موفق
- `python3 Firmware/Modules/Ui/host_test_ui.py` — موفق
- `git diff --check` — موفق
- مقایسهٔ `CubeMX/CubeIDE.ioc` و `CubeIDE/CubeIDE.ioc` — یکسان
- بررسی متنی نبودن `HAL_Delay` و تخصیص پویا در `Firmware/` — موفق

## محدودیت‌های اعتبارسنجی

- Build و لینک واقعی STM32 انجام نشده است؛ `arm-none-eabi-gcc` در محیط موجود نیست.
- تحلیل رسمی MISRA با ابزار اختصاصی انجام نشده است.
- ADC، قطبیت پایه‌ها و رفتار LED/BUZZER هنوز روی برد واقعی تأیید نشده‌اند.
- تست Host جایگزین تست عملی برد نیست؛ نتایج تست واقعی باید در `Firmware/Modules/Ui/UI_Board_Validation.xlsx` ثبت شوند.
