# CubeIDE

پروژهٔ STM32CubeIDE در این پوشه است: `Core/`، `Drivers/`، `Middlewares/`، `.project`، `.cproject` و `CubeIDE.ioc`. پوشهٔ `Firmware/` به‌صورت Linked Resource به پروژه متصل است و نباید داخل CubeIDE کپی شود.

## وضعیت تولیدشده و BSP

`Core/Src/main.c` پس از کلاک، سخت‌افزارهای زیر را مقداردهی می‌کند و سپس safe-state و `App_Start()` را اجرا می‌کند:

- ADC1 + DMA1 Channel1 برای PA1/PA2/PA3/PA5/PA7؛
- TIM2_CH1 روی PA0 و TIM3_CH1 روی PA6 برای PWM شارژر؛
- USART1 روی PA9/PA10 برای ESP-Link؛
- EXTI2/EXTI4/EXTI9_5 برای PB2/PB4/PB6.

جزئیات GPIO alternate، ADC analog، DMA، UART و کلاک تایمر در `Core/Src/stm32f1xx_hal_msp.c` است. زنجیرهٔ IRQهای EXTI در `Core/Src/stm32f1xx_it.c` قرار دارد. درایور `stm32f1xx_hal_uart.c/.h` همراه HAL وارد پروژه شده است تا UART حتی با `MODULE_ESP=0` حذف نشود.

## قرارداد تغییر

- mapping فیزیکی و polarity فقط در `Firmware/Config/Inc/board_pins.h` و پورت `Firmware/Bsp/Src` تغییر می‌کند.
- headerهای عمومی BSP در `Firmware/Bsp/Inc` HAL-free هستند.
- پس از تغییر `.ioc`، فایل را به `CubeMX/CubeIDE.ioc` همسان کن و وجود لینک UART در `STM32CubeIDE/.project` را بررسی کن.
- پیش از تحویل: `bash tools/check_firmware_syntax.sh`، `bash tools/check_ai_rules.sh` و build واقعی STM32CubeIDE را اجرا کن. تست Host جایگزین build و تست سخت‌افزار نیست.

درخت کامل API و اتصال‌ها: `Firmware/Bsp/README.md` و صفحهٔ اصلی `README.md`.
