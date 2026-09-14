/**
 * @file    README.md
 * @brief   [EN] Charger module sheet: PWM duty policy.
 *          [FA] برگه ماژول Charger: سیاست PWM.
 */

# ماژول Charger

## وضعیت

اسکلت. `MODULE_CHARGER = 0`. PWM را Enable نکن. فایل را پاک نکن.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت `Charger_Init` / `Charger_Evaluate` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `charger.h` / `charger.c` | سیاست duty |
| `../../Bsp/Src/bsp_pwm.c` | CCR تایمر — به بیلد LED اضافه نکن |
| `../../Rtos/Src/task_control.c` | تسک مشترک |

## توابع

| نام | کار |
|---|---|
| `Charger_Init` | باید PWM را ۰٪ بگذارد؛ بدنه فعلاً خالی است |
| `Charger_Evaluate` | از snapshot و state هنوز duty حساب نمی‌کند |
| `TaskControl` | مشترک با Changeover / Jitter |

حد بالا: `APP_CONFIG.pwm_max_duty_permille` (الان ۰).

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی |
|---|---|---|---|
| PA0 | `MCU_PWM1` | TIM2_CH1 شارژر ۱ | duty تایمر، نه GPIO خام |
| PA6 | `MCU_PWM2` | TIM3_CH1 شارژر ۲ | duty تایمر، نه GPIO خام |

## پیش‌فرض امن

بعد از Init باید هر دو کانال ۰٪ باشند. بدون Measurement معتبر PWM بالا نرود.
