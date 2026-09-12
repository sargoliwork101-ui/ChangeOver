# CubeMX / CubeIDE

**مرحله فعلی LED/بازر:** به‌جای این فایل، اول این را بخوان:

`Firmware/docs/01-led-buzzer-cubeide.md`

این صفحه تنظیمات کامل برد برای بعد است. الان ADC و PWM را روشن نکن.

پروژه‌ی Cube را جدا بساز. این پوشه‌ی `Firmware/` را به‌عنوان کد Application به آن Add کن.

## Peripheral

| Peripheral | پایه | نکته |
|---|---|---|
| SYS Debug | SWDIO/SWCLK | Serial Wire |
| HSE | PD0/PD1 | کریستال ۸ MHz |
| ADC1 | PA1 PA2 PA3 PA5 PA7 | Scan + DMA، circular |
| TIM2 CH1 PWM | PA0 | MCU_PWM1، Duty شروع ۰ |
| TIM3 CH1 PWM | PA6 | MCU_PWM2، Duty شروع ۰ |
| USART1 | PA9 TX / PA10 RX | 115200 8N1 |
| EXTI | PB2 PB4 PB6 | rising/falling بعداً |
| GPIO out | PA4 PA8 PB0 PB1 PB5 PB7 PB10 PB11 | سطح Reset در جدول پایین |
| GPIO in | PB3 | SWO اگر لازم شد |

## سطح GPIO در Reset (مهم)

با سخت‌افزار فعلی:

| پایه | نت | پیشنهاد Reset | چرا |
|---|---|---|---|
| PB7 | PROTECT_CHARGER / Relay | LOW | رله خاموش (active high) |
| PA0 PA6 | PWM | LOW | بدون پالس |
| PB5 | BAT_SWITCH / CONTROL_PS | LOW | High = قطع تغذیه باتری MCU؛ Low یعنی Q3 روشن می‌ماند |
| PB11 | PROTECT_BATT | **اندازه بگیر** | High روی شماتیک مسیر باتری به بار را قطع می‌کند |
| PA8 | ESP_CHPD | LOW | ESP خاموش (`R37` pulldown) |
| PB0 PB1 PB10 PA4 | LED / Buzzer | LOW | خاموش |

PB4 را در CubeMX از NJTRST آزاد کن (Serial Wire).

## Include path

```text
Firmware/App/Inc
Firmware/Config/Inc
Firmware/Bsp/Inc
Firmware/Rtos/Inc
Firmware/Modules/Actuator
Firmware/Modules/Measurement
Firmware/Modules/Changeover
Firmware/Modules/Charger
Firmware/Modules/Protection
Firmware/Modules/Jitter
Firmware/Modules/Ui
Firmware/Modules/EspLink
Firmware/Modules/Fault
```

همه‌ی `.c`های زیر را به پروژه Add کن:

```text
Firmware/App/Src
Firmware/Config/Src
Firmware/Bsp/Src
Firmware/Rtos/Src
Firmware/Modules/*/  (هر ماژول یک .c)
```

اگر CubeMX خودش `vApplicationGetIdleTaskMemory` را ساخت، فایل `Rtos/Src/freertos_hooks.c` را از Build خارج کن تا نماد تکراری نگیری.

## FreeRTOS در CubeMX

پیشنهاد این اسکلت:

- FreeRTOS را Enable کن
- `configSUPPORT_STATIC_ALLOCATION = 1`
- `configSUPPORT_DYNAMIC_ALLOCATION = 0`
- Task پیش‌فرض Cube را خالی بگذار یا حذف کن
- `osKernelStart()` را از `main.c` بردار اگر خود `App_Start()` تابع `vTaskStartScheduler()` را صدا می‌زند

اگر می‌خواهی `osKernelStart()` مال Cube بماند، آن وقت `App_Init()` را از Default Task صدا بزن و `Rtos_Start()` را صدا نزن. یکی را انتخاب کن؛ هر دو با هم scheduler را دو بار راه نمی‌اندازند.

این اسکلت فرض می‌کند **روش اول**: `App_Start()` خودش scheduler را شروع می‌کند.

برای قدم ۱ (چشمک LED) کافی است GPIO و FreeRTOS بیلد شوند. ADC/TIM/UART را در CubeMX از همین حالا روی پین درست تنظیم کن تا بعداً پین عوض نشود، ولی ماژول نرم‌افزاری‌شان تا فلگ Enable صفر است صدا زده نمی‌شود.

## Handleهای مورد انتظار

```c
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_adc1;
```

نام‌ها اگر در CubeMX فرق کرد، فقط `app.c` را عوض کن.
