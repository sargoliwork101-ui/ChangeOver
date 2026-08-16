# MISRA C Guidelines

## هدف

هدف Application Code، انطباق با `MISRA C:2012` به‌همراه Amendment 2 و Technical Corrigendum در صورت پشتیبانی ابزار است.

این پروژه هنوز گواهی MISRA ندارد. انطباق باید با Static Analysis، Code Review، Test و ثبت Deviation اثبات شود.

## الگوی فایل‌ها

```text
module_name.h
module_name.c
```

در Header:

- Include guard
- typedefهای public
- prototypeهای public
- ثابت‌های public موردنیاز

در Source:

- state و helperهای private با `static`
- بررسی Pointer و Handle
- بررسی Return Value
- عدم Export کردن جزئیات غیرضروری

## قواعد اجرایی

- C استاندارد مورد هدف: C99 مورد پشتیبانی Compiler و تنظیم پروژه.
- `-Wall -Wextra -Wconversion -Wsign-conversion -Wshadow -Werror` تا حد امکان فعال شود.
- از `uint8_t`، `uint16_t`، `uint32_t`، `int32_t` و `bool` استفاده شود.
- تبدیل‌های عددی حساس صریح باشند.
- از `float` در مسیر ADC/کنترل استفاده نشده و تبدیل‌ها به millivolt/milliamp انجام می‌شوند.
- حافظه‌ی پویا در Application ممنوع است.
- Task، Queue، Mutex و EventGroup استاتیک باشند.
- ISR کوتاه باشد و API مناسب `FromISR` استفاده شود.
- از macroهای function-like در Application استفاده نشود.
- magic numberها در Config قرار بگیرند.
- هیچ `goto`، recursion یا loop بی‌نهایت بدون انتظار RTOS استفاده نشود.
- هر while loop باید با رفتار زمان‌بندی یا انتظار مشخص قابل توضیح باشد.
- هر Deviation در `docs/deviation-record.md` اضافه شود.

## کد ثالث

این اجزا Third-Party هستند:

- STM32 HAL
- CMSIS
- FreeRTOS Kernel
- Startup و Linker code

برای آن‌ها یکی از این رویکردها باید در پروژه‌ی واقعی انتخاب شود:

1. تحلیل کامل و رفع خطاهای ابزار
2. Exclude رسمی با دامنه‌ی مشخص
3. Deviation Record با دلیل، ریسک و کنترل جبرانی

## ابزار پیشنهادی

نام ابزار به انتخاب تیم است؛ نمونه‌ها:

- PC-lint Plus
- Helix QAC
- LDRA
- QA-C
- Cppcheck به‌عنوان بررسی کمکی، نه اثبات انطباق کامل

## CI پیشنهادی

```text
1. Build Debug
2. Build Release
3. Compile with warnings as errors
4. Static analysis
5. Unit tests for pure modules
6. Archive report and deviations
```
