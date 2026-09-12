/**
 * @file    01-led-buzzer-cubeide.md
 * @brief   [EN] CubeIDE bring-up for LED and buzzer only.
 *          [FA] راه‌اندازی CubeIDE فقط برای LED و بازر.
 */

# قدم ۱ — از آردوینو تا CubeIDE: فقط LED و بازر

این فایل راهنمای **همین مرحله** است. ADC، PWM، رله، شارژر و ESP را فعلاً دست نزن.

هدف این مرحله فقط این است:

- پروژه STM32 را در CubeIDE بسازی
- سه LED و بازر را از FreeRTOS روشن/خاموش کنی
- بفهمی هر کلیک در CubeMX **چرا** لازم است

اگر این‌ها را روی برد دیدی، این مرحله تمام است:

1. بعد از Reset، به ترتیب قرمز → زرد → سبز هر کدام حدود نیم‌ثانیه روشن می‌شوند
2. یک بوق کوتاه از بازر
3. بعد با `UI_FLAG` در `task_ui.c`: `1` سبز ۵۰۰/۵۰۰؛ غیر از ۱ سبز ۵۰۰/۱۰۰۰ و قرمز هر ۵۰۰ چشمک

---

## ۱. آردوینو را با STM32 مقایسه کن

در آردوینو تقریباً همه چیز پشت `setup()` و `loop()` قایم است. یک `digitalWrite` می‌زنی و تمام.

در STM32 سه لایه می‌بینی:

| لایه | کی می‌سازدش؟ | معادل آردوینو |
|---|---|---|
| CubeMX / `MX_GPIO_Init` | نرم‌افزار ST با کلیک تو | `pinMode` |
| HAL مثل `HAL_GPIO_WritePin` | کتابخانه ST | `digitalWrite` |
| کد ما در `Firmware/` | خودمان | منطقی که در `loop` می‌نوشتی |

چرا این‌قدر تکه تکه است؟

- میکرو STM32F103 پایه‌های زیادی دارد که هر کدام چند کار می‌توانند بکنند (GPIO، ADC، UART، Timer). باید **از قبل** بگویی این پایه چیست.
- CubeMX همان نقشه را تولید می‌کند تا ما رجیستر را دستی ننویسیم.
- کد محصول را جدا می‌گذاریم تا اگر فردا CubeMX را دوباره Generate کردی، منطق LED پاک نشود.

معادل ذهنی:

```text
Arduino:
  setup()           →  App_Init()
  loop() + delay()  →  TaskUi() + vTaskDelay()
  digitalWrite      →  BspGpio_Write() → HAL_GPIO_WritePin
```

نکته مهم: `delay()` آردوینو کل CPU را نگه می‌دارد. `vTaskDelay` این Task را می‌خواباند تا بعداً FreeRTOS بقیه کارها را هم بتواند اجرا کند. الان فقط یک Task داریم، ولی عادت درست را از همین‌جا می‌گذاریم.

---

## ۲. چه چیزهایی از برد در این مرحله لازم است

از روی شماتیک:

| پایه MCU | نت | قطعه | وقتی MCU می‌شود HIGH |
|---|---|---|---|
| PB0 | `MCU_R_LED` | LED قرمز از طریق Q4 | LED روشن |
| PB1 | `MCU_Y_LED` | LED زرد از طریق Q5 | LED روشن |
| PB10 | `MCU_G_LED` | LED سبز از طریق Q6 | LED روشن |
| PA4 | `MCU_BUZZER` | بازر TMB12A05 از طریق Q7 | بازر صدا می‌دهد |

همه **active-high** هستند: HIGH یعنی ترانزیستور وصل، قطعه روشن.

برق این‌ها از ۵ ولت برد است. اگر برد روشن نباشد یا ۵ ولت نباشد، کد درست باشد هم چیزی نمی‌بینی.

برنامه‌ریز: کانکتور `CON4` (SWD). Debug در CubeMX باید **Serial Wire** باشد نه Full JTAG، چون PB4 در JTAG کامل اشغال می‌شود. در این مرحله PB4 را استفاده نمی‌کنیم، ولی از همین حالا Serial Wire بگذار تا بعداً گیر نکنی.

---

## ۳. نصب نرم‌افزار

1. از سایت ST، **STM32CubeIDE** را نصب کن (ویندوز).
2. اگر ST-Link می‌شناسی، درایورش همراه CubeIDE می‌آید. کابل USB را به پروگرامر بزن.
3. اولین بار که پروژه F103 می‌سازی، CubeIDE پکیج `STM32F1` را می‌پرسد. Download کن.

چرا CubeIDE نه Arduino IDE؟ چون HAL، CubeMX، FreeRTOS و دیباگر SWD در یک جا هستند. برای این برد این محیط استاندارد است.

---

## ۴. ساخت پروژه خالی — قدم به قدم داخل CubeIDE

### ۴.۱ پروژه جدید

1. `File → New → STM32 Project`
2. در تب MCU، جستجو کن: `STM32F103C8T6`
3. همان را انتخاب کن، Next
4. نام پروژه مثلاً `ChangeOver`
5. Language: **C**  (C++ نه)
6. Target binary: Executable
7. Finish

اگر پرسید «Initialize peripherals to default?» معمولاً Yes. بعد صفحه **Pinout & Configuration** باز می‌شود. این همان CubeMX است که داخل IDE نشسته.

### ۴.۲ Debug را Serial Wire کن

چرا: پروگرام و دیباگ از دو سیم SWDIO و SWCLK است. اگر Full JTAG بماند، چند پایه GPIO قفل می‌شوند.

1. سمت چپ `System Core → SYS`
2. Debug: **Serial Wire**
3. Timebase Source: فعلاً `SysTick` بماند؛ وقتی FreeRTOS را روشن کنیم عوض می‌شود

روی تراشه باید PA13 و PA14 به رنگ Debug درآیند.

### ۴.۳ کریستال خارجی

چرا: روی برد کریستال ۸ MHz است (`Y1`). اگر HSE را روشن نکنی، میکرو از اسیلاتور داخلی تقریبی کار می‌کند و بعداً UART خطا می‌گیرد. از همین حالا HSE را درست کن.

1. `System Core → RCC`
2. High Speed Clock (HSE): **Crystal/Ceramic Resonator**

### ۴.۴ کلاک ۷۲ MHz

چرا: F103 حداکثر ۷۲ MHz است. سریع‌تر یعنی محاسبات بعدی دقیق‌تر و Timerها راحت‌تر. اجباری برای چشمک LED نیست، ولی استاندارد همین برد است.

1. تب بالا: **Clock Configuration**
2. PLL Source: HSE
3. HSE: 8 MHz
4. SYSCLK را روی **72** بگذار
5. اگر مربع قرمز شد، عدد را درست نکردی؛ باید همه مسیر سبز شود

### ۴.۵ چهار پایه LED و بازر

برگرد تب **Pinout & Configuration**. روی خود شکل تراشه:

1. پایه `PA4` کلیک → `GPIO_Output`  (بازر)
2. `PB0` → `GPIO_Output`  (قرمز)
3. `PB1` → `GPIO_Output`  (زرد)
4. `PB10` → `GPIO_Output`  (سبز)

بعد سمت چپ `System Core → GPIO` و هر چهار پایه را یکی‌یکی:

| تنظیم | مقدار | چرا |
|---|---|---|
| GPIO output level | **Low** | قبل از اجرای کد، LED و بازر خاموش باشند |
| GPIO mode | Output Push Pull | خروجی معمولی دیجیتال |
| GPIO Pull-up/Pull-down | No pull-up/pull-down | مقاومت روی برد هست |
| Maximum output speed | Low | LED نیاز به سرعت بالا ندارد |

User Label بگذار تا در کد CubeMX خوانا شود:

- PA4 → `MCU_BUZZER`
- PB0 → `MCU_R_LED`
- PB1 → `MCU_Y_LED`
- PB10 → `MCU_G_LED`

این اسم‌ها مال CubeMX هستند. کد ما از `board_pins.h` استفاده می‌کند، نه از این لیبل. لیبل فقط برای خودت در نقشه CubeMX است.

### ۴.۶ FreeRTOS

چرا حالا، نه بعداً؟ چون چشمک را با Task می‌نویسیم، نه با `HAL_Delay` در `while(1)`. اگر اول delay بگذاری، بعداً عادت بد می‌ماند.

1. `Middleware → FREERTOS`
2. Interface: **CMSIS_V2** (رایج در CubeIDE)
3. CubeMX معمولاً می‌گوید HAL timebase را از SysTick دربیاور. **قبول کن** و مثلاً **TIM1** را بگذار.

چرا Timebase جدا؟  
SysTick را FreeRTOS برای تیک خودش می‌خواهد (`vTaskDelay`). اگر HAL هم SysTick را برای `HAL_Delay` بردارد، دو نفر سر یک تایمر دعوا می‌کنند. پس:

- SysTick → FreeRTOS
- TIM1 → ساعت داخلی HAL

در تنظیمات FreeRTOS:

- `configSUPPORT_STATIC_ALLOCATION` = **1**
- اگر دیدی `configSUPPORT_DYNAMIC_ALLOCATION` را Cube روی 1 گذاشته، برای این مرحله جنگ نکن؛ Task خودمان را استاتیک می‌سازیم

اگر CubeMX یک Default Task ساخت، بگذار بماند. ما در `main.c` اصلاً `osKernelStart` را صدا نمی‌زنیم؛ scheduler را خود `App_Start` راه می‌اندازد. Default Task Cube اجرا نمی‌شود. بعداً می‌توانی حذفش کنی.

### ۴.۷ Project Manager

تب **Project Manager**:

- Toolchain: STM32CubeIDE
- `Generate Under Root` می‌تواند تیک داشته باشد
- Code Generator: `Generate peripheral initialization as a pair of .c/.h` 
- **Scan generated files to update code** را بگذار
- مهم: `Keep User Code when re-generating` تا چیزی که بین `USER CODE BEGIN/END` می‌نویسی پاک نشود

بالا چپ دکمه **Generate Code** (چرخ‌دنده زرد) یا `Alt+K`.

---

## ۵. فایل‌های Firmware را به پروژه وصل کن

پروژه CubeIDE یک پوشه `Core/` و `Drivers/` دارد. کد ما جدا است تا Generate بعدی خرابش نکند.

### ۵.۱ کپی پوشه

پوشه `Firmware` همین ریپو را داخل پوشه پروژه CubeIDE کپی کن (هم‌سطح `Core`).

بعد در CubeIDE روی نام پروژه راست‌کلیک → **Refresh**.

### ۵.۲ فقط فایل‌های همین مرحله را Add کن

روی پروژه راست‌کلیک → `Add / Existing Files` یا فایل‌ها را از داخل IDE به Source اضافه کن. همین‌ها کافی است:

```text
Firmware/App/Src/app.c
Firmware/Config/Src/app_config.c
Firmware/Bsp/Src/bsp_gpio.c
Firmware/Modules/Ui/ui.c
Firmware/Rtos/Src/rtos_app.c
Firmware/Rtos/Src/task_ui.c
Firmware/Rtos/Src/task_measurement.c
Firmware/Rtos/Src/task_protection.c
Firmware/Rtos/Src/task_control.c
Firmware/Rtos/Src/task_comm.c
Firmware/Rtos/Src/freertos_hooks.c
```

چهار تسک اسکلت را پاک نکن. با فلگ صفر ساخته نمی‌شوند و داخل حلقه خالی می‌خوابند.

ماژول‌های Measurement / Charger را **به پروژه Add نکن** مگر وقتی آن مرحله شروع شود.

### ۵.۳ Include Path

چرا: کامپایلر باید `"ui.h"` را پیدا کند.

`Project → Properties → C/C++ Build → Settings → MCU GCC Compiler → Include paths`

این‌ها را Add کن (از ریشه پروژه):

```text
Firmware/App/Inc
Firmware/Config/Inc
Firmware/Bsp/Inc
Firmware/Rtos/Inc
Firmware/Modules/Ui
```

Apply and Close.

---

## ۶. تنها تغییری که در `main.c` می‌دهی

`Core/Src/main.c` را باز کن. CubeMX کلی کد تولید کرده. بین دو کامنت زیر را پیدا کن:

```c
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */
```

این را بگذار:

```c
/* USER CODE BEGIN Includes */
#include "app.h"
/* USER CODE END Includes */
```

تابع `main` را پیدا کن. بعد از `MX_GPIO_Init();` و **قبل از** `osKernelInitialize` / `osKernelStart` / حلقه `while (1)` این را بگذار:

```c
  /* USER CODE BEGIN 2 */
  App_Start();
  /* USER CODE END 2 */
```

اگر CubeMX این‌ها را گذاشته، **صدا نزن** (کامنت کن یا حذف داخل USER CODE):

- `osKernelInitialize();`
- `MX_FREERTOS_Init();`
- `osKernelStart();`

چرا: دو بار راه انداختن scheduler یعنی رفتار نامشخص. یک نفر باید رئیس باشد: `App_Start()`.

`App_Start` برنمی‌گردد. `while(1)` پایین `main` فقط برای این است که اگر scheduler خطا داد، میکرو در حلقه خالی بماند.

نمونه کامل در `Firmware/CubeMX/main.c.snippet` است؛ برای این مرحله ADC و Timer در main لازم نیست.

---

## ۷. اگر خطای تکراری Hook گرفتی

فایل `Firmware/Rtos/Src/freertos_hooks.c` حافظه Idle Task را برای حالت استاتیک می‌دهد.

اگر بیلد گفت `vApplicationGetIdleTaskMemory` دو بار تعریف شده:

- یا `freertos_hooks.c` ما را از Build خارج کن (راست‌کلیک روی فایل → Resource Configurations → Exclude)
- یا فایل Hook تولیدشده Cube را Exclude کن

فقط **یک** تعریف باید بماند.

---

## ۸. بیلد و پروگرام

1. نوار بالا: چکش **Build**
2. صفر error. Warning را بخوان؛ فعلاً اگر فقط warning Cube بود ادامه بده
3. برد را با ST-Link وصل کن، تغذیه برد را بده
4. دکمه سبز **Debug** یا Run
5. اگر پرسید ST-Link firmware update، Update کن

باید ببینی: قرمز، زرد، سبز، بوق کوتاه، بعد چشمک سبز.

---

## ۹. اگر کار نکرد

| دیده می‌شود | احتمال | کار |
|---|---|---|
| هیچ LED | تغذیه ۳٫۳ / ۵ ولت، Reset، پروگرام نشده | مولتی‌متر روی VDD و روی PB10 |
| فقط چشمک ندارد، یک LED دائم روشن | Task اجرا نمی‌شود / Delay صفر | ببین `App_Start` صدا شده |
| بیلد error `ui.h not found` | Include path | بخش ۵٫۳ |
| `undefined reference App_Start` | `app.c` به پروژه Add نشده | بخش ۵٫۲ |
| `undefined reference xTaskCreateStatic` | FreeRTOS در CubeMX Enable نشده | بخش ۴٫۶ |
| بازر بی‌صدا، LED هست | پایه PA4 یا ترانزیستور Q7 | مولتی‌متر PA4 هنگام بوق |
| LED معکوس | active-high را اشتباه بستی | شماتیک: HIGH = روشن |

دیباگ: روی `Ui_BoardTest` یک breakpoint بگذار. اگر آمد، RTOS زنده است.

---

## ۱۰. MISRA C یعنی چه؟ چرا اینجا؟

MISRA C یک مجموعه قانون برای C در خودرو و صنعت است تا C «آزاد» باعث باگ‌های خطرناک نشود.

برای LED اجباری قانونی نداریم، ولی از همین مرحله عادت می‌کنیم چون مرحله بعد رله و باتری است.

چیزهایی که در همین کد LED رعایت شده و **چرا**:

1. **`uint32_t` به‌جای `int`** — اندازه `int` روی هر کامپایلر فرق می‌کند. روی ARM باید دقیق بدانی.
2. **`static` برای متغیر داخل فایل** — مثل private در کلاس. Task دیگر نباید مستقیم به شمارنده چشمک دست بزند.
3. **بدون `malloc`** — آردوینو `new` دارد؛ روی F103 فقط ۲۰ کیلوبایت RAM است و تکه تکه شدن حافظه یعنی Reset بی‌دلیل.
4. **`vTaskDelay` فقط از داخل Task** — می‌تواند در `ui.c` باشد چون `TaskUi` آن را صدا می‌زند. `HAL_Delay` در `main` ممنوع است.
5. **عدد جادویی ممنوع** — `500` وسط کد نیست؛ در `app_config.c` اسم دارد. فردا می‌خواهی چشمک سریع‌تر شود، یک جا عوض می‌کنی.
6. **Include Guard** در `.h` — اگر دو فایل `ui.h` را include کنند، تعریف تکراری نمی‌گیری.
7. **بررسی اشاره‌گر NULL در BSP** — اگر اشتباهاً port خالی پاس شد، میکرو به آدرس صفر ننویسد و Hang عجیب نگیری.
8. **آکولاد برای هر `if`** — حتی یک خطی. باگ `if` بدون آکولاد معروف است.
9. **`(void)argument`** — پارامتر اجباری FreeRTOS را عمداً استفاده نمی‌کنیم؛ این یعنی «فراموش نشده، نادیده گرفته شده».

وقتی گفتی این مرحله تمام است، می‌رویم سراغ اندازه‌گیری. تا آن وقت هیچ پایه قدرت را در CubeMX PWM نکن.
