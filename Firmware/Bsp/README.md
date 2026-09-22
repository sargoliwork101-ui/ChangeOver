# قرارداد BSP برد ChangeOver

## وضعیت

این پوشه قرارداد و پورت BSP برد `STM32F103C8T6` را نگه می‌دارد. قراردادهای عمومی در `Firmware/Bsp/Inc` مستقل از HAL هستند؛ نگاشت فیزیکی فقط در `Firmware/Bsp/Src` و `Firmware/Config/Inc/board_pins.h` قرار دارد. همهٔ backendها حتی وقتی فلگ ماژول محصول صفر است در Build باقی می‌مانند.

## مرز عمومی و پورت برد

```text
Firmware/Modules و Firmware/Rtos
        │ فقط API منطقی
        ▼
Firmware/Bsp/Inc/*.h       ← HAL-free، بدون پایه/هندل/نوع STM32
        │
        ▼
Firmware/Bsp/Src/*.c       ← پورت برد فعلی، HAL و main.h خصوصی
        │
        ├── Firmware/Config/Inc/board_pins.h
        └── CubeIDE/Core/Src و .ioc  ← کلاک، init و IRQهای سخت‌افزار
```

ماژول‌ها نباید `board_pins.h`، `main.h`، `stm32f1xx_hal.h`، `GPIO_TypeDef`، `ADC_HandleTypeDef`، `TIM_HandleTypeDef` یا `UART_HandleTypeDef` را include یا استفاده کنند.

## نگاشت منطقی ADC

ترتیب فریم ADC قرارداد عمومی است و ترتیب فیزیکی در پورت برد ثابت می‌شود:

| قرارداد منطقی | پایه و کانال فعلی برد | تبدیل |
|---|---|---|
| `BSP_ADC_CHANNEL_CURRENT1` | PA1 / ADC12_IN1 | جریان شارژ ۱، mA — نمونهٔ سنکرون وسط ON از `TIM2_CC2` |
| `BSP_ADC_CHANNEL_24V_IN` | PA2 / ADC1_IN2 | ورودی ۲۴V، mV |
| `BSP_ADC_CHANNEL_24V_BAT` | PA3 / ADC1_IN3 | باتری ۲۴V، mV |
| `BSP_ADC_CHANNEL_12V_BAT` | PA5 / ADC1_IN5 | باتری ۱۲V، mV |
| `BSP_ADC_CHANNEL_CURRENT2` | PA7 / ADC12_IN7 | جریان شارژ ۲، mA — نمونهٔ سنکرون وسط ON از `TIM3_TRGO` |

`bsp_adc.c` از ADC1 با scan پنج‌کاناله، DMA1 Channel1، circular buffer دو فریمی و خواندن نیمهٔ کامل استفاده می‌کند. `bsp_measurement.c` مرجع ۳۳۰۰mV، ADC دوازده‌بیتی، تقسیم‌های مقاومتی ۲۴V/۱۲V، شانت ۱۰mΩ و gain برابر ۱۰۱ را نگه می‌دارد. از ۲۰۲۶-۰۹-۲۲ (دستور کاربر) فرمول جریان ADC→mA **مرحله‌به‌مرحله از مدار** ساخته شده (`func__BspMeasurement_ConvertCurrent`): آفست صفر پر-کانال → شمارش به mV پایه ADC (×3300/4095) → خنثی‌سازی تقسیم دائمی ورودی MCU با R41=1k/R42=10k (×11/10) → خنثی‌سازی گین غیروارونگر ۱۰۱ تقویت‌کننده → ولتاژ شانت mV به mA با شانت 10mΩ → گین پرمیل بنچ پر-کانال (۱۰۸۵ پرمیل = ۱٫۰۸۵ برابر نقطهٔ بنچ Trans2؛ Trans1 موقتاً کپی). تمام ضرب‌ها روی numerator/denominator در uint64 حمل و فقط **یک تقسیم نهایی** انجام می‌شود تا خطای گردشدن تجمعی نگیرد؛ نتیجهٔ عددی با فرمول قبلی هم‌ارز است (~۰٫۸۸mA به ازای هر count پیش از گین بنچ).

از ۲۰۲۶-۰۹-۲۲ (دستور کاربر) دو جایگاه جریان از نمونه‌برداری سنکرون می‌آیند: ADC2 (خصوصی همین پورت، بدون CubeMX) برای هر کانال یک تبدیل regular تریگر-خارجی انجام می‌دهد که لبهٔ سخت‌افزاری‌اش دقیقاً وسط پنجرهٔ ON همان گیت است — کانال ۱ با تریگر `TIM2_CC2` و کانال ۲ با `TIM3_TRGO` (منبع OC2REF). زمان نمونه‌برداری ۷٫۵ کلاک (۶۲۵ns) است، انتظار پایان تبدیل با بودجهٔ ۳۰µs از شمارندهٔ سیکل DWT محدود شده و وقفه/DMA در کار نیست؛ اگر نمونهٔ سنکرون ممکن نشود (گیت پارک/timeout) مقدار اسکن غیرهمزمان همان جایگاه به‌عنوان جایگزین در فریم می‌ماند. `func__BspPwm_IsGatePulsing` تشخیص می‌دهد لبهٔ وسط ON اصلاً می‌آید یا نه.

## نگاشت GPIO و EXTI

| سیگنال منطقی | پایهٔ فعلی | نوع | قطبیت/وضعیت امن فیزیکی |
|---|---|---|---|
| `BSP_GPIO_BUZZER` | PA4 | خروجی | active-high، Low |
| `BSP_GPIO_ESP_CHPD` | PA8 | خروجی | active-high، Low |
| `BSP_GPIO_LED_RED` | PB0 | خروجی | active-high، Low |
| `BSP_GPIO_LED_YELLOW` | PB1 | خروجی | active-high، Low |
| `BSP_GPIO_LED_GREEN` | PB10 | خروجی | active-high، Low |
| `BSP_GPIO_BATTERY_SWITCH` | PB5 | خروجی | active-low، High |
| `BSP_GPIO_RELAY` | PB7 | خروجی | active-high، Low |
| `BSP_GPIO_PROTECT_BATTERY` | PB11 | خروجی | active-low، High |
| `BSP_GPIO_INPUT_24V_PRESENT` | PB4 | ورودی + EXTI | تشخیص منطقی سطح High |
| `BSP_GPIO_JITTER1` | PB2 | ورودی + EXTI | خروجی LM393 active-low، فقط falling معتبر است |
| `BSP_GPIO_JITTER2` | PB6 | ورودی + EXTI | خروجی LM393 active-low، فقط falling معتبر است |

`bsp_exti.c` callback HAL را به eventهای `BSP_EXTI_JITTER1`، `BSP_EXTI_JITTER2` و `BSP_EXTI_INPUT_DETECT` تبدیل می‌کند. PB2/PB6 روی falling و PB4 روی هر دو لبه فعال هستند؛ callback برای JIT پایین‌بودن پایه را نیز چک می‌کند. IRQهای واقعی در `stm32f1xx_it.c` پاک‌سازی و به callback تحویل می‌شوند.

## نگاشت PWM و UART

| قرارداد منطقی | پریفرال/پایه فعلی | وضعیت ماژول |
|---|---|---|
| `BSP_PWM_CHARGER_1` | TIM2_CH1 / PA0، 50kHz | `MODULE_CHARGER=1`، با `CHG_MASTER_ENABLE=0` safe-off |
| `BSP_PWM_CHARGER_2` | TIM3_CH1 / PA6، 50kHz | `MODULE_CHARGER=1`، با `CHG_MASTER_ENABLE=0` safe-off |
| `BspUart` byte stream | USART1 TX/RX / PA9/PA10، 115200 8-N-1 | `MODULE_ESP=0`، backend موجود |

`func__BspPwm_SetDutyPermille` دامنهٔ ۰ تا ۱۰۰۰ را اعمال می‌کند و صفر یعنی گیت پایین (compare=0). دو کانال PWM با **درهم‌گذاری فاز ۱۸۰ درجهٔ فریزشده** (نیم‌دوره = ۱۰µs در ۵۰kHz، دستور کاربر ۲۰۲۶-۰۹-۲۱: پالس کانال دو دقیقاً نیم‌دوره بعد از **استارت** پالس کانال یک) کار می‌کنند: در `func__BspPwm_Init` خروجی‌ها بدون شمارنده فعال می‌شوند، فازها نوشته می‌شوند (TIM2 از CNT=0 و TIM3 از CNT=ARR/2، محاسبه از ARR واقعی) و آنگاه **هر دو CEN با دو نوشتن رجیستری پشت‌سرهم** روشن می‌شوند — لغزش چند ده نانوثانیه (یافتهٔ بنچ: تأخیر چندمیکروثانیه‌ای HAL_TIM_PWM_Start فاز را غیرقطعی می‌کرد). پس‌ازاین شمارنده‌ها هرگز متوقف یا بازنویسی نمی‌شوند؛ خاموش‌کردن کانال فقط با compare=0 است. چون هر دو روی یک کلاک ۷۲MHz و ARR یکسان‌اند رانش صفر است. از ۲۰۲۶-۰۹-۲۲ هر تایمر یک **کانال داخلی CH2 تریگر نمونه‌برداری** هم دارد (حالت PWM 2 با `CCR2 = CCR1/2` که `SetOneDuty` آن را دنبال می‌کند): لبهٔ بالارونده‌اش دقیقاً وسط پنجرهٔ ON است و `TIM2_CC2` مستقیم و `TIM3_TRGO` (با MMS=OC2REF) به ADC2 جریان سنکرون وصل می‌شوند؛ پایه‌های فیزیکی CH2 (PA1/PA7) آنالوگ می‌مانند. `func__BspPwm_IsGatePulsing` اعلام می‌کند گیت پالس می‌زند (compare>0). `func__BspUart_Write` ارسال کامل با timeout محدود ۱۰۰ms دارد و `func__BspUart_ReadByte` non-blocking است.

## توابع و وضعیت startup

ترتیب startup فعلی در `main.c` چنین است:

```text
HAL_Init → SystemClock_Config
  → MX_GPIO_Init / MX_DMA_Init / MX_ADC1_Init
  → MX_TIM2_Init / MX_TIM3_Init / MX_USART1_UART_Init
  → BspGpio_Init   : safe levels for all digital outputs
  → BspPwm_Init    : compare=0 and PWM stopped
  → BspUart_Init   : select USART1 backend
  → BspExti_Init   : clear pending logical event flags
  → App_Start
```

ADC توسط `task_measurement.c` با `func__BspAdc_Init` و `func__BspAdc_Start` آغاز می‌شود تا مالکیت شروع acquisition با Measurement بماند. تا پیش از فریم معتبر، دادهٔ Measurement نامعتبر است.

## درخت اتصال

```text
CubeIDE/Core/Src/main.c
  ├── main.h / generated handles
  ├── stm32f1xx_hal_msp.c
  │     ├── ADC1 + DMA1 Channel1 + PA1/2/3/5/7 analog
  │     ├── TIM2/TIM3 clocks + PA0/PA6 alternate function
  │     └── USART1 clock + PA9/PA10 alternate function
  ├── stm32f1xx_it.c
  │     └── EXTI2 / EXTI4 / EXTI9_5 → HAL_GPIO_EXTI_IRQHandler
  └── Firmware/Bsp/Src
        ├── bsp_gpio.c       ← GPIO logical state + board polarity
        ├── bsp_adc.c        ← normalized ADC+DMA frame + PWM-synchronized ADC2 current samples
        ├── bsp_measurement.c← board voltage/current calibration
        ├── bsp_pwm.c        ← charger PWM backend
        ├── bsp_uart.c       ← ESP-Link USART1 backend
        └── bsp_exti.c       ← logical event latches

Firmware/Rtos/Src/task_measurement.c → bsp_adc + bsp_measurement
Firmware/Modules/Ui                 → bsp_gpio
Firmware/Modules/Charger             → bsp_pwm (after module enable)
Firmware/Modules/EspLink             → bsp_gpio + bsp_uart (after module enable)
Firmware/Modules/Jitter              → bsp_exti (after module enable)
```

## قواعد Agentهای ماژول

1. فقط APIهای منطقی بالا را مصرف کن؛ mapping فیزیکی را کپی نکن.
2. هیچ فایل CubeMX، `board_pins.h`، `main.h` یا درایور HAL را برای تغییر محلی دست نزن.
3. قبل از اجرای actuator، دادهٔ Measurement معتبر و policy ایمنی را بررسی کن.
4. Charger باید در Init، هر دو PWM را صفر کند؛ EspLink باید CH_PD را خاموش شروع کند؛ Jitter باید eventهای قبلی را پاک کند.
5. تغییر mapping یا polarity فقط با دستور Agent ارشد و در پورت BSP انجام می‌شود.
