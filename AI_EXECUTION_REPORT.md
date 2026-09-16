# گزارش اجرای AI

**آخرین به‌روزرسانی:** 2026-09-16<br>
**شاخه:** `arena/01a0a923-changeover`<br>
**مالک گزارش:** Agent ارشد پروژه<br>
**قوانین مرجع:** `AI_AGENT_RULES.md`

## خلاصهٔ وضعیت

- مهاجرت رابط برنامه به CMSIS-RTOS2 در commit `20a8ca7` انجام شده است.
- مرز سخت‌افزار در commit `4995ac3` تثبیت شده و کالیبراسیون Measurement در commit `bbc7ca5` به BSP منتقل شده است.
- UI، Measurement، EspLink و Rtos دیگر `board_pins.h`، `main.h` یا headerهای STM32 را مستقیماً مصرف نمی‌کنند.
- Headerهای عمومی BSP (`bsp_gpio.h`, `bsp_adc.h`, `bsp_measurement.h`, `bsp_exti.h`, `bsp_pwm.h`, `bsp_uart.h`) HAL-free هستند.
- جزئیات HAL، هندل ADC، نگاشت پایه‌ها و کالیبراسیون مدار فقط در پورت برد/فایل‌های platform-specific باقی می‌مانند.
- Agentهای ماژول باید پیش از شروع `AI_AGENT_RULES.md` و README ماژول خود را بخوانند؛ مرز BSP قرارداد پایه است و تغییر سراسری فقط با دستور Agent ارشد انجام می‌شود.

## اتصال فعلی Measurement و UI

- پورت فعلی ADC پنج مقدار آنالوگ را با ترتیب normalized در `bsp_adc.h` ارائه می‌کند.
- `bsp_measurement.c` تبدیل ADC به mV/mA، تقسیم مقاومتی، گین و شانت برد فعلی را نگه می‌دارد.
- Measurement هر `10ms` یک فریم کامل را از BSP می‌گیرد و منتشر می‌کند.
- فقط سیگنال منطقی `BSP_GPIO_INPUT_24V_PRESENT` در Measurement مصرف می‌شود؛ پایه و قطبیت فیزیکی در BSP است.
- فقط ولتاژ منطقی ورودی به Task UI متصل است.
- ولتاژ باتری هنوز از `UINT32_T__G__BatteryVoltageMv` و Live Expressions به‌صورت دستی تأمین می‌شود.
- قبل از اولین فریم معتبر، ورودی UI صفر و از نظر سناریو قطع در نظر گرفته می‌شود.

## قرارداد BSP

- GPIO از شناسه‌های منطقی `BSP_GPIO_*` استفاده می‌کند.
- ADC از موقعیت‌های normalized مانند `BSP_ADC_CHANNEL_24V_IN` استفاده می‌کند.
- Measurement منطق تبدیل را نگه نمی‌دارد و از `bsp_measurement` استفاده می‌کند.
- EXTI رویدادهای منطقی را با `bsp_exti_src_t` ارائه می‌کند.
- PWM با `func__BspPwm_Init(void)` و کانال منطقی کار می‌کند؛ هندل تایمر عمومی نیست.
- UART با `func__BspUart_Init(void)` و جریان بایت کار می‌کند؛ هندل UART عمومی نیست.
- در صورت تغییر MCU یا برد، فقط پورت BSP و فایل‌های platform-specific تغییر می‌کنند؛ منطق Module/App کپی نمی‌شود.

## زمان‌بندی و رفتار RTOS

- CMSIS-RTOS2 رابط عمومی برنامه است و FreeRTOS فقط backend داخلی است.
- Threadها با `osThreadNew` و `cb_mem`/`stack_mem` استاتیک ساخته می‌شوند.
- تبدیل میلی‌ثانیه به tick از `osKernelGetTickFreq()` در `rtos_time` انجام می‌شود.
- از `HAL_Delay`، API مستقیم FreeRTOS در منطق محصول و تخصیص پویا استفاده نمی‌شود.
- snapshot Measurement هنگام انتشار/کپی با `osKernelLock` و `osKernelRestoreLock` محافظت می‌شود و به دستور مخصوص هستهٔ MCU وابسته نیست.
- ADC و DMA توسط backend برد اجرا می‌شوند؛ Task Measurement فقط فریم کامل را مصرف می‌کند.

## اعتبارسنجی انجام‌شده

- `bash tools/check_ai_rules.sh` — موفق
- `bash tools/check_firmware_syntax.sh` — موفق؛ syntax تمام سورس‌های CubeIDE/Core و Firmware با GCC سمت Host
- `python3 Firmware/Modules/Ui/host_test_ui.py` — موفق
- تست Host تبدیل‌های `bsp_measurement` — موفق
- تست Host پورت‌های منطقی EXTI/PWM/UART — موفق
- `git diff --check` — موفق
- preprocessing مسیرهای فعال — موفق
- جست‌وجوی وابستگی HAL/STM32 در App/Modules/Rtos و Headerهای عمومی BSP — بدون وابستگی مستقیم

## محدودیت‌های اعتبارسنجی

- Build و لینک واقعی STM32، symbol/map و اندازه‌گیری RAM/Flash هنوز اجرا نشده است؛ `arm-none-eabi-gcc` و STM32CubeIDE در محیط موجود نیستند.
- تحلیل رسمی MISRA با ابزار اختصاصی انجام نشده است.
- ADC، قطبیت پایه‌ها و رفتار LED/BUZZER هنوز روی برد واقعی تأیید نشده‌اند.
- تست Host/syntax جایگزین تست عملی برد نیست؛ نتایج تست واقعی باید در `Firmware/Modules/Ui/UI_Board_Validation.xlsx` و برگه‌های اعتبارسنجی مربوط ثبت شوند.
