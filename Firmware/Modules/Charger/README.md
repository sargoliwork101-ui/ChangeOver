/**
 * @file    README.md
 * @brief   [EN] Charger module: PWM duty policy.
 *          [FA] ماژول شارژر: سیاست وظیفه PWM.
 */

# ماژول Charger

توضیح کامل **همین ماژول** همین‌جاست.

الان **خاموش** است (`MODULE_CHARGER 0`). PWM را در CubeMX Enable نکن. فایل‌ها را پاک نکن.

## کار ماژول

از snapshot و حالت Changeover، درصد PWM دو کانال شارژ را حساب می‌کند. خودش تایمر را invert نمی‌کند؛ به BSP می‌گوید duty چند باشد.

قانون ایمنی که در کامنت Init آمده: بعد از Init باید PWM **۰٪** بماند تا رله/مسیر اشتباه شارژ نکند.

حد بالای duty: `APP_CONFIG.pwm_max_duty_permille` (هزارم).

## پایه‌هایی که بعداً مال این ماژول‌اند

حالا نزن:

| پایه | تایمر | نقش |
|---|---|---|
| PA0 | TIM2_CH1 | `MCU_PWM1` |
| PA6 | TIM3_CH1 | `MCU_PWM2` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `charger.h` / `charger.c` | سیاست |
| `../../Bsp/Src/bsp_pwm.c` | نوشتن CCR تایمر (اسکلت) |
| `../../Rtos/Src/task_control.c` | تسک مشترک |

`charger.c` و `bsp_pwm.c` را به بیلد LED اضافه نکن.

## توابع همین الان در کد

- `Charger_Init` — بدنه خالی؛ وقتی پیاده شود باید PWM را صفر بگذارد.
- `Charger_Evaluate` — `snap` و `state` را دور می‌ریزد. هنوز duty حساب نمی‌کند.

بدون Measurement معتبر و بدون state امن Changeover، این ماژول نباید PWM را بالا ببرد.
