# CubeIDE

پروژهٔ STM32CubeIDE در این پوشه است: `Core/`، `Drivers/`، `Middlewares/`، `.project`، `.cproject` و `CubeIDE.ioc`. پوشهٔ `Firmware/` به‌صورت Linked Resource به پروژه متصل است و نباید داخل CubeIDE کپی شود.

## وضعیت تولیدشده و BSP

`Core/Src/main.c` پس از کلاک، سخت‌افزارهای زیر را مقداردهی می‌کند و سپس safe-state و `App_Start()` را اجرا می‌کند:

- ADC1 + DMA1 Channel1 برای PA1/PA2/PA3/PA5/PA7؛
- TIM2_CH1 روی PA0 و TIM3_CH1 روی PA6 برای PWM شارژر؛
- USART1 روی PA9/PA10 برای ESP-Link؛
- EXTI2/EXTI4/EXTI9_5 برای PB2/PB4/PB6.

جزئیات GPIO alternate، ADC analog، DMA، UART و کلاک تایمر در `Core/Src/stm32f1xx_hal_msp.c` است. زنجیرهٔ IRQهای EXTI در `Core/Src/stm32f1xx_it.c` قرار دارد. درایور `stm32f1xx_hal_uart.c/.h` همراه HAL وارد پروژه شده است تا UART حتی با `MODULE_ESP=0` حذف نشود.

## بودجهٔ فلش — بخوانید پیش از اینکه چیزی اضافه کنید

`STM32CubeIDE/STM32F103C8TX_FLASH.ld`:

| ناحیه | شروع | طول |
|---|---|---|
| `FLASH` | `0x8000000` | **۶۲K** |
| `NVM` | `0x800F800` | ۲K |
| `RAM` | `0x20000000` | ۲۰K |

۲ کیلوبایت آخرِ ۶۴ کیلوبایت برای ماندگاری تنظیمات رزرو است، پس کد فقط **۶۲ کیلوبایت** دارد. پروژه دو بار به این سقف خورده (۲۰۲۶-۰۹-۲۷ با ۱۸۴۰ بایت، ۲۰۲۶-۱۰-۰۳ با ۷۸۰ بایت).

**چیزهایی که از قبل گرفته شده‌اند** — دوباره دنبالشان نگردید: `-Os`، `-ffunction-sections`، `-fdata-sections`، `--gc-sections` (هر سه در `.cproject` برای Debug و Release)، `USE_FULL_ASSERT` خاموش، بدون هیچ `printf`، فقط ۱۰ ماژول HAL فعال، و `*(.ARM.exidx*)` در `/DISCARD/`.

**تایمرهای نرم‌افزاری FreeRTOS باید خاموش بمانند.** `configUSE_TIMERS 0` در `Core/Inc/FreeRTOSConfig.h`. دلیلش این نیست که سلیقه است:

- هیچ‌جای این فرم‌ور تایمر نرم‌افزاری نمی‌سازد.
- ولی تا وقتی `configUSE_TIMERS` یک باشد، `tasks.c:2022` بی‌قید `xTimerCreateTimerTask()` را صدا می‌زند. یعنی `timers.c` از خود زمان‌بند قابل‌دسترس است و `--gc-sections` **هرگز** نمی‌تواند حذفش کند؛ و `timers.c` هم `queue.c` را برای صف فرمانش می‌کشد.
- اندازه‌گیری‌شده روی ایمیج لینک‌شده: `queue.c` ۱۷۱۴ + `timers.c` ۱۳۷۶ + `tasks.c` ۶۱۲ بایت فلش، به‌علاوهٔ ۱ کیلوبایت استک تسک تایمر در RAM.

دو `#error` سر راه خاموش‌کردن بود و **هر دو گزینهٔ قابل خاموش‌کردن بودند، نه قانون**: `timers.c:41` فقط به‌خاطر `INCLUDE_xTimerPendFunctionCall=1`، و `freertos_os2.h:208` فقط به‌خاطر روشن‌ماندن Event Flags API شیم CMSIS. هر دو خاموش شدند. در مقابل، `configUSE_COUNTING_SEMAPHORES` **عمداً روشن است** — شیم حتی بدون ساختن سمافور به آن نیاز دارد و خاموش‌کردنش کامپایل `cmsis_os2.c` را می‌شکند.

**CubeMX این تنظیم را در `.ioc` نگه نمی‌دارد.** اگر کد را دوباره تولید کنید، بی‌صدا برمی‌گردد. برای همین `Firmware/Rtos/Src/freertos_hooks.c` یک `#error` خوانا دارد و یک `#pragma message` در کنسول بیلد چاپ می‌کند:

```
note: '#pragma message: ChangeOver flash diet 2026-10-03 ACTIVE:
       configUSE_TIMERS=0, timers.c and queue.c are out (~3 KB)'
```

اگر این خط در لاگ بیلد **نیست**، بیلد شما از این درخت سورس استفاده نکرده است. این مهم است چون پیام `region FLASH overflowed` چه وقتی اصلاح به بیلد نرسیده باشد و چه وقتی کافی نبوده، دقیقاً یک‌شکل است.

**اندازه‌گیری بدون ویندوز:** `python3 tools/measure_flash.py` همان مجموعه سورس را با توولچین LLVM داخل بستهٔ `ziglang` برای Thumb/Cortex-M3 می‌سازد و با همین اسکریپت لینکر لینک می‌کند. عدد مطلقش با CubeIDE یکی **نیست** (کامپایلر و libc فرق دارد)؛ برای مقایسهٔ **اختلاف** دو پیکربندی است. تأیید نهایی همچنان build واقعی است.

## قرارداد تغییر

- mapping فیزیکی و polarity فقط در `Firmware/Config/Inc/board_pins.h` و پورت `Firmware/Bsp/Src` تغییر می‌کند.
- headerهای عمومی BSP در `Firmware/Bsp/Inc` HAL-free هستند.
- پس از تغییر `.ioc`، فایل را به `CubeMX/CubeIDE.ioc` همسان کن و وجود لینک UART در `STM32CubeIDE/.project` را بررسی کن.
- پیش از تحویل: `bash tools/check_firmware_syntax.sh`، `bash tools/check_ai_rules.sh` و build واقعی STM32CubeIDE را اجرا کن. تست Host جایگزین build و تست سخت‌افزار نیست.

درخت کامل API و اتصال‌ها: `Firmware/Bsp/README.md` و صفحهٔ اصلی `README.md`.
