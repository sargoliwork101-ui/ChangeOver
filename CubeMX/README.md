/**
 * @file    README.md
 * @brief   [EN] CubeMX .ioc lives here. Not product firmware. Guide for adding peripherals safely.
 *          [FA] فایل .ioc مکعب اینجاست. راهنمای اضافه کردن پریفرال بدون بهم ریختن برنامه.
 */

# CubeMX

فقط فایل تنظیمات مکعب: `CubeIDE.ioc` (نام پروژه مکعب `CubeIDE` است)

`Firmware` را اینجا نگذار. `Core` / `Drivers` هم اینجا نماند.

## Save / Generate

1. STM32CubeMX را **جدا** باز کن (نه از داخل CubeIDE)
2. Project Manager:
   - Name: `CubeIDE`
   - Location: یک سطح بالاتر — ریشهٔ ریپو `ChangeOver`
   - Toolchain: **STM32CubeIDE**
3. GENERATE CODE → خروجی می‌رود به `../CubeIDE/`
4. فایل `.ioc` را در **همین پوشه** هم بگذار (کپی از `../CubeIDE/*.ioc`) تا جای مکعب گم نشود

## آیا اضافه کردن پریفرال برنامه را بهم می‌ریزد؟ / Does adding a peripheral break the program?

**خیر، اگر قوانین زیر رعایت شود — نه، برنامه بهم نمی‌ریزد.**

### چرا نمی‌ریزد (ساختار پروژه)

- `Firmware/` بیرون از `CubeIDE/` است و به صورت **Linked Resource** در `.project` لینک شده:
  - `PARENT-2-PROJECT_LOC/Firmware` → یعنی ریشهٔ ریپو
  - CubeMX فقط `CubeIDE/Core/`, `Drivers/`, `Middlewares/` را بازنویسی می‌کند، نه `Firmware/`
- `main.c` فقط کلاک، `MX_GPIO_Init()`، و `App_Start()` را صدا می‌زند. منطق محصول در `Firmware/` است.
- `main.c` دارای `USER CODE BEGIN/END` است. CubeMX کد داخل این بلوک‌ها را نگه می‌دارد:
  - `#include \"app.h\"` در `Includes`
  - `App_Start()` در `USER CODE BEGIN 2`
- `CubeIDE.ioc` در این پروژه `defaultTask` ندارد (حذف شده). Threadها در `Firmware/Rtos/Src/rtos_app.c` با CMSIS-RTOS2 و حافظهٔ ثابت ساخته می‌شوند (`osThreadNew` با `cb_mem` و `stack_mem`).
- Include pathهای `Firmware` در `STM32CubeIDE/.cproject` هستند:
  - `../../../Firmware/App/Inc`, `Bsp/Inc`, `Config/Inc`, `Rtos/Inc`, `Modules/Ui`, ...
  - CubeMX معمولاً این‌ها را نگه می‌دارد، ولی بعد از Generate چک کن که پاک نشده باشند.

### چه اتفاقی می‌افتد وقتی یک پریفرال اضافه می‌کنی (مثلاً ADC1)

1. CubeMX یک `MX_ADC1_Init()` و `ADC_HandleTypeDef hadc1` در `main.c` / `main.h` می‌سازد و در `main()` صدا می‌زند (قبل از `App_Start()`).
2. `stm32f1xx_hal_msp.c` را بازنویسی می‌کند تا پایه‌های PA1, PA2, PA3, PA5, PA7 را Analog کند (این پایه‌ها الان آزاد هستند، تداخلی با LEDها PB0/PB1/PB10/PA4 ندارند).
3. درایور `stm32f1xx_hal_adc.c` را به `Drivers/` اضافه می‌کند.
4. **Firmware بهم نمی‌ریزد** چون:
   - `BspAdc_Init(&hadc1)` باید در `App_Init()` یا `main.c` USER CODE صدا زده شود تا هندل به Bsp برسد. اگر نزنی، `BspAdc_GetRaw()` فقط `false` برمی‌گرداند، کرش نمی‌کند.
   - تا `MODULE_MEASUREMENT=0` باشد، تسک measurement ساخته نمی‌شود، پس ADC حتی اگر در CubeMX فعال باشد، از سمت Firmware استفاده نمی‌شود.

### چک‌لیست امن برای اضافه کردن پریفرال

1. **قبل از Generate** بگو کدام فایل‌ها عوض می‌شوند (قانون AI)
2. در CubeMX پین‌ها را چک کن که با LED/بازر تداخل نداشته باشد:
   - LED: PB0, PB1, PB10
   - Buzzer: PA4
   - SWD: PA13, PA14 (باید SWD بماند، نه Full JTAG)
   - آزاد برای ADC: PA1, PA2, PA3, PA5, PA7
3. بعد از Generate:
   - `CubeIDE/Core/Src/main.c` → آیا `App_Start()` هنوز در `USER CODE BEGIN 2` هست؟
   - `STM32CubeIDE/.cproject` → آیا Include pathهای `Firmware` هنوز هستند؟
   - `CubeIDE.ioc` را کپی کن به `CubeMX/CubeIDE.ioc`
   - `modules_enable.h` را فقط وقتی فلگ را می‌خواهی 1 کنی عوض کن
4. Build کن، اگر خطای Include دادی، Pathها را دوباره اضافه کن

### وضعیت فعلی: LED/بازر + ADC

- `MODULE_UI=1` و `MODULE_MEASUREMENT=1`. بقیه 0.
- ADC1: ۵ کانال (PA1/PA2/PA3/PA5/PA7 = IN1/IN2/IN3/IN5/IN7)، scan + continuous، sampling 55.5 cycle، کلاک **12MHz** (PCLK2/6 — بیشترین prescaler مجاز با PCLK2 برابر 72MHz؛ سقف ADC در F103 = 14MHz).
- DMA1 Channel1: circular، N=10 (دو فریم ۵ کاناله)، بدون interrupt — بافر را سخت‌افزار پر می‌کند.
- PB4 = GPIO_Input با لیبل `MCU_INT_24_IN` (حضور ورودی ۲۴، دیجیتال).
- هندل `hadc1` از `main.h` به `task_measurement.c` می‌رسد و در آن‌جا به `func__BspAdc_Init` داده می‌شود (`app.c` دست‌نخورده است).
- PWM و UART را Enable نکن مگر همان مرحله را کاربر خواسته باشد (قانون AI).

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-16 | اصلاح مرحلهٔ ADC: کلاک از 9MHz به 12MHz با PCLK2/6 (بیشترین مقدار قانونی F103 با PCLK2=72MHz)، همسان‌سازی `.ioc`های CubeMX و CubeIDE و افزودن درایورهای HAL ADC/ADCEx به پروژه |
| 2026-09-15 | لیبل (User Label) برای همه پایه‌های ADC در `.ioc`: PA1=`ADC_CURRENT1`، PA2=`MCU_ADC_24_IN`، PA3=`MCU_ADC_24_BAT`، PA5=`MCU_ADC_12_BAT`، PA7=`ADC_CURRENT2`، PB4=`MCU_INT_24_IN` (یکی با نام‌های شماتیک و برگه‌ی Measurement) |
| 2026-09-15 | ADC1 + DMA1 چرخشی (5 کانال، 9MHz) و PB4 (MCU_INT_24_IN) به `.ioc` اضافه شد؛ کلاک ADC از 36MHz به 9MHz (سقف 14MHz)؛ درایور ADC v1.1.10 به CubeIDE/Drivers |
| 2026-09-14 | اضافه شدن راهنمای اضافه کردن پریفرال بدون بهم ریختن برنامه + توضیح Linked Resource و USER CODE و چک‌لیست امن |
| 2026-09-14 | فایل .ioc فقط LED/بازر، ADC/PWM خاموش |

