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
- بازبینی کامل کد و READMEها انجام شد؛ مسیرهای قدیمی Agent، APIهای قدیمی BSP و توضیحات مستقیم پایه در مستندات اصلاح شدند.
- لایهٔ کامل شماتیک تثبیت شد: GPIO و polarity/safe-state، ADC+DMA و calibration، TIM2/TIM3 PWM، USART1، EXTIهای PB2/PB4/PB6، MSP/IRQ و درایور HAL UART.

## اتصال فعلی Measurement و UI

- پورت فعلی ADC پنج مقدار آنالوگ را با ترتیب normalized در `bsp_adc.h` ارائه می‌کند.
- `bsp_measurement.c` تبدیل ADC به mV/mA، تقسیم مقاومتی، گین و شانت برد فعلی را نگه می‌دارد.
- Measurement هر `10ms` یک فریم کامل را از BSP می‌گیرد و منتشر می‌کند.
- فقط سیگنال منطقی `BSP_GPIO_INPUT_24V_PRESENT` در Measurement مصرف می‌شود؛ پایه و قطبیت فیزیکی در BSP است.
- فقط ولتاژ منطقی ورودی به Task UI متصل است.
- ولتاژ باتری هنوز از `UINT32_T__G__BatteryVoltageMv` و Live Expressions به‌صورت دستی تأمین می‌شود.
- قبل از اولین فریم معتبر، ورودی UI صفر و از نظر سناریو قطع در نظر گرفته می‌شود.

## قرارداد BSP

- GPIO از شناسه‌های منطقی `BSP_GPIO_*` استفاده می‌کند و قطبیت active-high/active-low در پورت خصوصی اعمال می‌شود؛ PB5 و PB11 در وضعیت امن physical High هستند.
- ADC از موقعیت‌های normalized مانند `BSP_ADC_CHANNEL_24V_IN` استفاده می‌کند؛ map فیزیکی فعلی PA1/PA2/PA3/PA5/PA7 است.
- Measurement منطق تبدیل را نگه نمی‌دارد و از `bsp_measurement` استفاده می‌کند؛ تقسیم‌های ۲۴V/۱۲V و شانت/gain در port هستند.
- EXTI رویدادهای منطقی `JITTER1`، `JITTER2` و `INPUT_DETECT` را با `bsp_exti_src_t` ارائه می‌کند؛ IRQهای واقعی در Core متصل هستند.
- PWM با `func__BspPwm_Init(void)` و کانال منطقی کار می‌کند؛ TIM2_CH1/PA0 و TIM3_CH1/PA6 در public API دیده نمی‌شوند و startup صفر/stop است.
- UART با `func__BspUart_Init(void)` و جریان بایت کار می‌کند؛ USART1/PA9/PA10 و baud در public API دیده نمی‌شوند.
- در صورت تغییر MCU یا برد، فقط پورت BSP و فایل‌های platform-specific تغییر می‌کنند؛ منطق Module/App کپی نمی‌شود.

## دستور واگذاری به Agentهای فرعی

پس از تأیید این مرحله، هر Agent ماژول باید روی branch اختصاصی خودش کار کند و این قرارداد را اجرا کند:

1. پیش از تغییر، `AI_AGENT_RULES.md`، README همان ماژول و `Firmware/Bsp/README.md` را بخواند؛ `main`، `.ioc`، `board_pins.h` و `AI_EXECUTION_REPORT.md` را تغییر ندهد.
2. فقط headerهای منطقی `Firmware/Bsp/Inc` را include کند. استفاده از پایه، `main.h`، HAL، handle، timer/channel یا ترتیب فیزیکی ADC در Module/Rtos ممنوع است.
3. از این APIها استفاده کند: GPIO با `func__BspGpio_*`، ADC با `func__BspAdc_*` و `func__BspMeasurement_*`، PWM با `func__BspPwm_*`، UART با `func__BspUart_*` و EXTI با `func__BspExti_*`.
4. قبل از هر actuator، `BOOL__G__MeasDataValid`/snapshot معتبر و policy ایمنی را بررسی کند؛ Charger بدون دادهٔ معتبر duty غیرصفر ندهد، EspLink با CH_PD خاموش شروع شود و Jitter eventهای قبلی را پاک کند.
5. تغییر mapping یا polarity را در Agent محلی انجام ندهد؛ اگر mismatch شماتیک پیدا شد، به Agent ارشد گزارش کند.
6. تغییرات را فقط در branch خودش commit کند، `bash tools/check_ai_rules.sh`، `bash tools/check_firmware_syntax.sh` و تست مربوط به ماژول را اجرا کند و hash commit، فایل‌های تغییرکرده، تست‌ها و issueهای سخت‌افزاری را گزارش دهد.

## زمان‌بندی و رفتار RTOS

- CMSIS-RTOS2 رابط عمومی برنامه است و FreeRTOS فقط backend داخلی است.
- Threadها با `osThreadNew` و `cb_mem`/`stack_mem` استاتیک ساخته می‌شوند.
- تبدیل میلی‌ثانیه به tick از `osKernelGetTickFreq()` در `rtos_time` انجام می‌شود.
- از `HAL_Delay`، API مستقیم FreeRTOS در منطق محصول و تخصیص پویا استفاده نمی‌شود.
- snapshot Measurement هنگام انتشار/کپی با `osKernelLock` و `osKernelRestoreLock` محافظت می‌شود و به دستور مخصوص هستهٔ MCU وابسته نیست.
- ADC و DMA توسط backend برد اجرا می‌شوند؛ Task Measurement فقط فریم کامل را مصرف می‌کند.

## اعتبارسنجی انجام‌شده

- `bash tools/check_ai_rules.sh` — موفق
- `bash tools/check_firmware_syntax.sh` — موفق؛ syntax سورس‌های CubeIDE/Core و Firmware با GCC سمت Host
- syntax درایور `stm32f1xx_hal_uart.c` با includeهای STM32 و warningهای مخصوص host-width — موفق
- `python3 Firmware/Modules/Ui/host_test_ui.py` — موفق
- `git diff --check` — موفق
- بررسی دو `.ioc`: byte-identical، بدون PB9 اضافی، بدون key تکراری و با TIM2/TIM3/USART1/EXTI کامل — موفق
- preprocessing مسیرهای فعال — موفق
- جست‌وجوی وابستگی HAL/STM32 در App/Modules/Rtos و Headerهای عمومی BSP — بدون وابستگی مستقیم
- ممیزی سازگاری READMEها با APIهای فعلی و درخت اتصال — اصلاح و تأیید شد

## محدودیت‌های اعتبارسنجی

- Build و لینک واقعی STM32، symbol/map و اندازه‌گیری RAM/Flash هنوز اجرا نشده است؛ `arm-none-eabi-gcc` و STM32CubeIDE در محیط موجود نیستند.
- تحلیل رسمی MISRA با ابزار اختصاصی انجام نشده است.
- ADC، قطبیت پایه‌ها و رفتار LED/BUZZER هنوز روی برد واقعی تأیید نشده‌اند.
- تست Host/syntax جایگزین تست عملی برد نیست؛ نتایج تست واقعی باید در `Firmware/Modules/Ui/UI_Board_Validation.xlsx` و برگه‌های اعتبارسنجی مربوط ثبت شوند.
