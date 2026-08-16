# راهنمای کامل آماده‌سازی پروژه‌ی STM32 برای جایگذاری Firmware

این راهنما را قبل از ساخت فایل پروژه‌ی STM32 انجام بده. وقتی پروژه را برای من می‌فرستی، فایل `.ioc` و کد تولیدشده‌ی CubeMX باید داخل ZIP باشند.

---

## مرحله ۱ — ایجاد پروژه

1. `STM32CubeIDE` را باز کن.
2. از مسیر `File -> New -> STM32 Project` وارد ساخت پروژه شو.
3. در بخش MCU Selector، دقیقاً قطعه‌ی زیر را انتخاب کن:

```text
STM32F103C8T6
```

4. نام پروژه را مثلاً این بگذار:

```text
ChangeOver_Controller
```

5. Toolchain را `STM32CubeIDE` انتخاب کن.
6. گزینه‌ی ذخیره‌ی پروژه با تنظیمات پیش‌فرض Cube را قبول کن.
7. قبل از هر تغییر، یک بار پروژه را Generate و Build کن تا مطمئن شو پروژه‌ی خام سالم است.

---

## مرحله ۲ — تنظیم Clock

طبق شماتیک، کریستال MCU برابر ۸ MHz است:

```text
HSE = 8 MHz
```

تنظیم پیشنهادی اولیه:

```text
PLL Source = HSE
PLL Multiplier = x9
SYSCLK = 72 MHz
AHB = 72 MHz
APB1 = 36 MHz یا کمتر
APB2 = 72 MHz
ADC Clock = حداکثر 14 MHz؛ مثلاً PCLK2 / 6
```

اگر Clock واقعی برد متفاوت است، آن را حدس نزن و مقدار واقعی را ثبت کن.

---

## مرحله ۳ — Debug و پین‌های رزرو

در بخش `SYS`:

```text
Debug = Serial Wire
```

Full JTAG را فعال نکن، چون PB4 در شماتیک برای تشخیص ورودی ۲۴ ولت استفاده شده است.

پین‌های دیباگ:

```text
PA13 = SWDIO
PA14 = SWCLK
```

PB3 در شماتیک با SWO نام‌گذاری شده است. اگر Trace لازم نیست، PB3 را آزاد بگذار. اگر Trace لازم است، تداخل آن با تنظیم Debug را جداگانه بررسی کن.

---

## مرحله ۴ — تنظیم GPIO

### خروجی‌ها

| پایه | تنظیم |
|---|---|
| PA4 | GPIO Output — Buzzer |
| PA8 | GPIO Output — ESP CH_PD |
| PB0 | GPIO Output — Red LED |
| PB1 | GPIO Output — Yellow LED |
| PB5 | GPIO Output — Battery Switch |
| PB7 | GPIO Output — Charger Relay/Protection |
| PB10 | GPIO Output — Green LED |
| PB11 | GPIO Output — Battery Protection |

برای جلوگیری از فعال‌شدن ناخواسته، Initial Output Level را مطابق active level واقعی سخت‌افزار تنظیم کن. اگر active level را هنوز نمی‌دانی، خروجی‌ها را در کد و CubeMX در حالت خاموش امن نگه دار.

### ورودی‌ها و EXTI

| پایه | تنظیم |
|---|---|
| PB2 | GPIO EXTI — Jitter1 |
| PB4 | GPIO EXTI — 24V Input Detect |
| PB6 | GPIO EXTI — Jitter2 |

برای Jitter، لبه‌ی موردنیاز را طبق شکل موج واقعی انتخاب کن. اگر تشخیص حضور/قطع ورودی مهم است، PB4 را روی هر دو لبه تنظیم کن؛ اگر فقط Presence مهم است، Polling دوره‌ای هم در کد وجود دارد.

---

## مرحله ۵ — تنظیم ADC1 و DMA

ADC1 را با Scan Conversion و DMA تنظیم کن.

ترتیب Rankها باید دقیقاً این باشد:

```text
Rank 1: PA1 — ADC1_IN1 — Current1
Rank 2: PA2 — ADC1_IN2 — Input24V
Rank 3: PA3 — ADC1_IN3 — Battery24V
Rank 4: PA5 — ADC1_IN5 — Battery12V
Rank 5: PA7 — ADC1_IN7 — Current2
```

در کد فعلی DMA به Buffer پنج‌عضوی متصل می‌شود. اگر ترتیب Rank تغییر کند، باید هم CubeMX و هم `measurement_manager.c` تغییر کند.

تنظیمات پیشنهادی اولیه:

```text
DMA Data Width = مطابق تنظیم ADC/HAL
DMA Mode = Circular برای Continuous Sampling
DMA Priority = High یا طبق سیستم نهایی
ADC Resolution = 12-bit
```

برای کنترل دقیق جریان بهتر است Trigger تایمری استفاده شود، اما فرکانس نمونه‌برداری باید بعد از مشخص‌شدن فرکانس PWM تعیین شود.

---

## مرحله ۶ — تنظیم PWM

### PWM1

```text
Pin  = PA0
Timer = TIM2_CH1
```

### PWM2

```text
Pin  = PA6
Timer = TIM3_CH1
```

فرکانس PWM هنوز از طرف کاربر نهایی نشده است. فعلاً می‌توانی Timerها را فعال کنی، اما PWM توان نباید به مرحله‌ی قدرت وصل یا فعال شود.

فرمول فرکانس:

```text
f_pwm = TimerClock / ((PSC + 1) * (ARR + 1))
```

باید مشخص شود:

- فرکانس PWM چند کیلوهرتز است؟
- دو کانال هم‌فرکانس هستند؟
- دو کانال باید Sync یا هم‌فاز باشند؟
- حداکثر Duty Cycle چقدر است؟

---

## مرحله ۷ — تنظیم USART1

```text
PA9  = USART1_TX
PA10 = USART1_RX
```

تنظیم اولیه‌ی قابل تغییر:

```text
Baud Rate = 115200
Word Length = 8 bit
Parity = None
Stop Bits = 1
Mode = TX/RX
```

پروتکل ESP8266 هنوز نهایی نشده است. فعلاً ارتباط را با PWM خاموش تست کن.

---

## مرحله ۸ — افزودن FreeRTOS

در CubeMX، **Native FreeRTOS را فعال کن**؛ این اسکلت مستقیماً از APIهای Native مثل `xTaskCreateStatic` و `xQueueCreateStatic` استفاده می‌کند. اگر CMSIS-RTOS2 را انتخاب کردی، باید Static Memory Attributeهای آن را جداگانه تنظیم و Integration را تطبیق بدهی.

پروژه‌ی ما از Static Allocation استفاده می‌کند، بنابراین بررسی کن:

```c
#define configSUPPORT_STATIC_ALLOCATION 1
#define configSUPPORT_DYNAMIC_ALLOCATION 0
```

فایل `RTOS/Src/freertos_hooks.c` حافظه‌ی Idle Task و Hook مربوط به Stack Overflow را فراهم می‌کند. اگر CubeMX فایل Hook مشابه ساخت، فقط یک نسخه را نگه دار.

تنظیمات پیشنهادی اولیه:

```text
Tick Rate = 1000 Hz
Use Preemption = Enabled
Max Priorities >= 7
Task Notifications = Enabled
Mutexes = Enabled
Event Groups = Enabled
```

فایل `CubeMX/FreeRTOSConfig.example.h` فقط نمونه است. آن را کورکورانه جایگزین FreeRTOSConfig واقعی نکن؛ تنظیمات تولیدشده‌ی CubeMX را با آن مقایسه و Merge کن.

---

## مرحله ۹ — Generate و Build پایه

1. در CubeMX روی `Generate Code` بزن.
2. پروژه‌ی خام را Build کن.
3. اگر پروژه‌ی خام Build نشد، قبل از اضافه‌کردن Firmware مشکل CubeMX را حل کن.
4. از فایل `.ioc` یک نسخه نگه دار.

---

## مرحله ۱۰ — جایگذاری فایل‌های این Repository

پوشه‌های زیر را از Repository به ریشه‌ی پروژه‌ی CubeIDE کپی کن:

```text
BSP/
Config/
App/
RTOS/
Integration/
```

فایل زیر را هم اضافه کن:

```text
Core/Inc/app_entry.h
```

در تنظیمات Compiler، این Include Pathها را اضافه کن:

```text
BSP/Inc
Config/Inc
App/Inc
RTOS/Inc
Integration/Inc
Core/Inc
```

تمام فایل‌های `.c` را به Build اضافه کن. فایل‌های `README.md` و `docs` نیازی به Build ندارند.

---

## مرحله ۱۱ — اتصال به main.c

بعد از Initialize شدن تمام Peripheralها، این ترتیب را در `main.c` داشته باش:

```c
HAL_Init();
SystemClock_Config();

MX_GPIO_Init();
MX_DMA_Init();
MX_ADC1_Init();
MX_TIM2_Init();
MX_TIM3_Init();
MX_USART1_UART_Init();

firmware_init();
if (firmware_is_ready())
{
    vTaskStartScheduler();
}
```

اگر `firmware_is_ready()` مقدار false برگرداند، Scheduler را اجرا نکن و علت ساخت‌نشدن Task یا Handle را بررسی کن.

ترتیب Initialize مهم است؛ `firmware_init()` نباید قبل از آماده‌شدن HAL، GPIO، DMA، ADC، Timer و UART اجرا شود.

---

## مرحله ۱۲ — Callbackهای تکراری

در پروژه باید فقط یک تعریف نهایی از این Callbackها وجود داشته باشد:

```c
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef * hadc);
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);
```

اگر CubeMX یا کد موجود این Callbackها را تعریف کرده است، آن‌ها را حذف نکن؛ منطقشان را با `Integration/Src/firmware_entry.c` و `BSP/Src/adc_driver.c` ادغام کن تا Duplicate Symbol ایجاد نشود.

---

## مرحله ۱۳ — Compiler و تحلیل

در صورت پشتیبانی GCC، برای Application این هشدارها را فعال کن:

```text
-Wall
-Wextra
-Wpedantic
-Wconversion
-Wsign-conversion
-Wshadow
-Werror
```

استاندارد زبان را روی C99 یا تنظیم مورد تأیید پروژه قرار بده. HAL و FreeRTOS ممکن است هشدارهای مستقل داشته باشند؛ Scope تحلیل را جدا کن و Deviation آن‌ها را ثبت کن.

---

## مرحله ۱۴ — قبل از ارسال پروژه برای جایگذاری

ZIP ارسالی باید شامل این موارد باشد:

```text
ProjectName.ioc
Core/
Drivers/
Middlewares/
BSP/
Config/
App/
RTOS/
Integration/
```

این موارد را حذف کن:

```text
Debug/
Release/
.settings/
.metadata/
فایل‌های رمز عبور یا کلیدها
```

نام پیشنهادی فایل:

```text
ChangeOver_Controller_YYYYMMDD.zip
```

پس از دریافت پروژه، کارهایی که انجام می‌دهیم:

1. تطبیق handleهای واقعی با Integration
2. تطبیق Pinها و Clock
3. حذف Callbackهای Duplicate
4. Build واقعی با HAL و FreeRTOS
5. رفع خطاهای Compile و Link
6. بررسی Static Allocation
7. بررسی Ruleهای MISRA با ابزار موجود
8. تکمیل UI و الگوهای Buzzer
9. تست مرحله‌ای GPIO، ADC، PWM، UART و Fault
