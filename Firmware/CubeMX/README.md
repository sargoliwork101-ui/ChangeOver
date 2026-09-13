/**
 * @file    README.md
 * @brief   [EN] LED/buzzer CubeMX notes. ADC/PWM are later.
 *          [FA] یادداشت CubeMX برای LED و بازر. ADC/PWM بعداً.
 */

# CubeMX — فقط LED و بازر

پروژه Generateشده Cube اینجا نیست. از نو بساز.

راهنمای قدم‌به‌قدم: `Firmware/Modules/Ui/README.md`

الان فقط:

- Serial Wire
- HSE 8 MHz → SYSCLK 72 MHz
- GPIO: PA4 بازر، PB0 قرمز، PB1 زرد، PB10 سبز — Output, Low
- FreeRTOS CMSIS_V2 (Timebase = TIM1)

ADC، PWM، UART، رله را روشن نکن.

پروژه Cube را **کنار** `Firmware` بساز، مثلاً:

`ChangeOver/CubeIDE/`

Workspace در CubeIDE باید پوشهٔ `ChangeOver` باشد، نه خود پوشهٔ پروژه.

بعد از Generate فقط داخل USER CODE:

```c
#include "app.h"
...
App_Start();
```

الگوی کامل: `main.c.snippet`
