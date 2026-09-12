/**
 * @file    README.md
 * @brief   [EN] UI module guide: LEDs, buzzer, CubeMX setup.
 *          [FA] راهنمای ماژول UI: ال‌ای‌دی، بازر، ستاپ CubeMX.
 */

# ماژول UI — LED و بازر

این فایل راهنمای **همین بخش** است. ADC، PWM، رله، شارژر و ESP را اینجا روشن نکن.

---

## ۱. این بخش چه کار می‌کند

ماژول UI فقط چهار خروجی را می‌زند:

| پایه | قطعه | HIGH یعنی |
|---|---|---|
| PB0 | LED قرمز (Q4) | روشن |
| PB1 | LED زرد (Q5) | روشن |
| PB10 | LED سبز (Q6) | روشن |
| PA4 | بازر (Q7) | صدا |

همه **active-high** هستند: پایهٔ میکرو ۳٫۳ ولت → ترانزیستور وصل → قطعه روشن.

بعد از Reset برنامه این ترتیب را اجرا می‌کند:

1. `Ui_BoardTest` یک‌بار: قرمز ۵۰۰ ms، زرد ۵۰۰ ms، سبز ۵۰۰ ms، بوق ۱۵۰ ms
2. `UI_FLAG` در `task_ui.c`:
   - `1` → `Ui_Scenario1`: سبز ۵۰۰ روشن / ۵۰۰ خاموش، قرمز خاموش
   - غیر از `1` → `Ui_Scenario2`: سبز ۵۰۰ روشن / ۱۰۰۰ خاموش، قرمز هر ۵۰۰ چشمک

اگر این را روی برد دیدی، این بخش درست کار می‌کند.

---

## ۲. فایل‌های همین بخش

| فایل | نقش |
|---|---|
| `ui.h` / `ui.c` | الگو و نوشتن پایه‌ها |
| `task_ui.c` | تسک FreeRTOS؛ تست برد بعد FLAG |
| `bsp_gpio.c` | معادل `digitalWrite` روی HAL |
| `board_pins.h` | شماره پایه‌ها |
| `app.c` | از `main` می‌آید، `Ui_Init` بعد FreeRTOS |
| `app_config.c` | زمان‌ها (۵۰۰، ۱۵۰، …) |

الگوی چشمک داخل `ui.c` است، نه داخل فایل Task.  
`vTaskDelay` داخل `ui.c` مجاز است چون از **داخل TaskUi** صدا می‌شود.

`delay()` آردوینو کل CPU را قفل می‌کند. `vTaskDelay` همین تسک را می‌خواباند؛ بعداً تسک‌های دیگر هم می‌توانند کار کنند.

---

## ۳. تعویض سناریو

`Firmware/Rtos/Src/task_ui.c`:

```c
#define UI_FLAG  1u   /* 1 = سناریو ۱ ، غیر از ۱ = سناریو ۲ */
```

فقط همین عدد را عوض کن، دوباره بیلد و پروگرام کن. دو سناریو هم‌زمان اجرا نمی‌شوند.

---

## ۴. ستاپ CubeMX / CubeIDE — قدم به قدم

هدف: میکرو بداند کدام پایه GPIO است و FreeRTOS تیک ۱ ms داشته باشد. بدون این‌ها کد ما کامپایل می‌شود ولی چشمک زمان درست ندارد.

### ۴.۱ نصب

1. از سایت ST، **STM32CubeIDE** را نصب کن.
2. کابل ST-Link را بزن. درایور همراه IDE است.
3. اولین پروژهٔ F103، پکیج `STM32F1` را Download کن.

چرا CubeIDE: HAL، CubeMX، FreeRTOS و دیباگر SWD در یک جا هستند.

### ۴.۲ پروژه جدید

1. `File → New → STM32 Project`
2. MCU: `STM32F103C8T6`
3. Language: **C**
4. Finish. اگر «Initialize peripherals to default?» پرسید: Yes

صفحهٔ Pinout همان CubeMX داخل IDE است.

### ۴.۳ Debug = Serial Wire

چرا: پروگرام با دو سیم SWDIO / SWCLK است. Full JTAG چند GPIO را قفل می‌کند (از جمله PB4).

1. `System Core → SYS`
2. Debug: **Serial Wire**
3. Timebase: فعلاً SysTick؛ با FreeRTOS عوض می‌شود

PA13 و PA14 باید رنگ Debug بگیرند.

### ۴.۴ کریستال خارجی

چرا: روی برد کریستال ۸ MHz (`Y1`) است. بدون HSE، کلاک داخلی تقریبی است و بعداً UART خطا می‌گیرد.

1. `System Core → RCC`
2. HSE: **Crystal/Ceramic Resonator**

### ۴.۵ کلاک ۷۲ MHz

چرا: حداکثر F103 همین است. برای LED اجباری نیست؛ استاندارد همین برد است.

1. تب **Clock Configuration**
2. PLL Source: HSE ، HSE = 8 MHz
3. SYSCLK = **72**
4. مسیر باید سبز باشد، نه قرمز

### ۴.۶ چهار پایه LED و بازر

روی شکل تراشه:

1. `PA4` → `GPIO_Output` (بازر)
2. `PB0` → `GPIO_Output` (قرمز)
3. `PB1` → `GPIO_Output` (زرد)
4. `PB10` → `GPIO_Output` (سبز)

بعد `System Core → GPIO`، برای هر چهار تا:

| تنظیم | مقدار | چرا |
|---|---|---|
| GPIO output level | **Low** | قبل از کد، خاموش باشند |
| GPIO mode | Output Push Pull | خروجی معمولی |
| Pull-up/Pull-down | No pull | مقاومت روی برد هست |
| Maximum output speed | Low | LED سرعت بالا نمی‌خواهد |

User Label (فقط برای خواندن نقشه Cube، کد ما از `board_pins.h` می‌خواند):

- PA4 `MCU_BUZZER`
- PB0 `MCU_R_LED`
- PB1 `MCU_Y_LED`
- PB10 `MCU_G_LED`

ADC، PWM، UART را در این مرحله Enable نکن.

### ۴.۷ FreeRTOS

چرا حالا: چشمک با Task نوشته شده، نه با `HAL_Delay` در `while(1)`.

1. `Middleware → FREERTOS`
2. Interface: **CMSIS_V2**
3. اگر گفت HAL timebase را از SysTick بردار: **قبول کن** و **TIM1** را بگذار

چرا Timebase جدا:

- SysTick → تیک FreeRTOS (`vTaskDelay`)
- TIM1 → ساعت HAL

اگر هر دو SysTick را بردارند، تأخیر خراب می‌شود.

FreeRTOS:

- `configSUPPORT_STATIC_ALLOCATION` = **1**
- Task پیش‌فرض Cube را بگذار بماند؛ ما `osKernelStart` را صدا نمی‌زنیم پس اجرا نمی‌شود

### ۴.۸ Project Manager

- Toolchain: STM32CubeIDE
- `Keep User Code when re-generating` روشن باشد
- Generate Code (چرخ‌دنده زرد / `Alt+K`)

---

## ۵. وصل کردن پوشه Firmware

کد محصول جدا از فایل‌های Generateشده است تا Generate بعدی منطقت را پاک نکند.

1. پوشه `Firmware` را هم‌سطح `Core` داخل پروژه CubeIDE کپی کن
2. روی پروژه راست‌کلیک → Refresh
3. این `.c`ها را Add کن:

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

چهار تسک اسکلت را پاک نکن. با فلگ صفر ساخته نمی‌شوند.

`measurement.c` / `charger.c` را حالا Add نکن.

4. Include path  
   `Project → Properties → C/C++ Build → Settings → MCU GCC Compiler → Include paths`:

```text
Firmware/App/Inc
Firmware/Config/Inc
Firmware/Bsp/Inc
Firmware/Rtos/Inc
Firmware/Modules/Ui
```

چرا: بدون این‌ها `#include "ui.h"` پیدا نمی‌شود.

---

## ۶. تنها تغییر `main.c`

داخل `USER CODE` بنویس تا Generate پاک نکند.

```c
/* USER CODE BEGIN Includes */
#include "app.h"
/* USER CODE END Includes */
```

بعد از `MX_GPIO_Init();`:

```c
/* USER CODE BEGIN 2 */
App_Start();
/* USER CODE END 2 */
```

این‌ها را **صدا نزن**:

- `osKernelInitialize();`
- `MX_FREERTOS_Init();`
- `osKernelStart();`

چرا: دو بار روشن کردن scheduler رفتار نامشخص است. رئیس `App_Start()` است (می‌رود داخل `vTaskStartScheduler`).

`App_Start` برنمی‌گردد.

---

## ۷. بیلد و پروگرام

1. چکش Build — صفر error
2. اگر گفت `vApplicationGetIdleTaskMemory` دو بار تعریف شده: یکی از `freertos_hooks.c` یا فایل Hook مکعب را Exclude کن
3. تغذیه برد + ST-Link
4. Debug / Run

باید ببینی: قرمز، زرد، سبز، بوق، بعد سناریوی FLAG.

---

## ۸. اگر کار نکرد

| دیده می‌شود | کار |
|---|---|
| هیچ LED | VDD ۳٫۳ و ۵ ولت را اندازه بگیر؛ پروگرام شده؟ |
| بیلد `ui.h not found` | Include path بخش ۵ |
| `undefined reference App_Start` | `app.c` Add نشده |
| `undefined reference xTaskCreateStatic` | FreeRTOS در CubeMX Enable نیست |
| بازر بی‌صدا، LED هست | PA4 / Q7 |
| LED معکوس | HIGH باید روشن باشد |

Breakpoint روی `Ui_BoardTest`: اگر آمد، RTOS زنده است.

---

## ۹. MISRA در همین بخش — چرا این‌طور نوشتیم

MISRA C قانون C برای کار صنعتی است. از LED شروع می‌کنیم چون مرحلهٔ بعد رله و باتری است.

| قانون در کد | چرا |
|---|---|
| زمان‌ها در `app_config.c` | عدد جادویی وسط منطق ممنوع؛ یک جا عوض می‌کنی |
| `static` روی `green()` / `red()` | خصوصی این فایل؛ Task مستقیم GPIO نزند |
| بدون `malloc` | RAM میکرو کم است؛ استک تسک از قبل رزرو شده |
| `vTaskDelay` فقط از context تسک | از `main` قبل از scheduler حرام است |
| آکولاد برای هر `if` | حتی یک خطی؛ باگ معروف |
| `(void)argument` | پارامتر FreeRTOS عمداً استفاده نشده، فراموش نشده |
| `NULL` چک در BSP | نوشتن روی آدرس صفر = هنگ |
| Include Guard در `.h` | اینکلود دو بار = تعریف تکراری |

وقتی این بخش روی برد درست شد بگو؛ بعد می‌رویم سراغ مرحله بعد.
