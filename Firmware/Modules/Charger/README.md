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
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت `func__Charger_Init` / `func__Charger_Evaluate` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `charger.h` / `charger.c` | سیاست duty |
| `../../Bsp/Src/bsp_pwm.c` | CCR تایمر — به بیلد LED اضافه نکن |
| `../../Rtos/Src/task_control.c` | تسک مشترک |

## توابع

| نام | کار |
|---|---|
| `func__Charger_Init` | باید PWM را ۰٪ بگذارد؛ بدنه فعلاً خالی است |
| `func__Charger_Evaluate` | از snapshot و state هنوز duty حساب نمی‌کند |
| `TaskControl` | مشترک با Changeover / Jitter |

حد بالا: `APP_CONFIG.pwm_max_duty_permille` (الان ۰).

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی |
|---|---|---|---|
| PA0 | `MCU_PWM1` | TIM2_CH1 شارژر ۱ | duty تایمر، نه GPIO خام |
| PA6 | `MCU_PWM2` | TIM3_CH1 شارژر ۲ | duty تایمر، نه GPIO خام |

## پیش‌فرض امن

بعد از Init باید هر دو کانال ۰٪ باشند. بدون Measurement معتبر PWM بالا نرود.

## درخت اتصال

صدا زده می‌شود از (وقتی فلگ ۱ شود):

```text
rtos_app.c → TaskControl → task_control.c
  func__Charger_Evaluate(&snap, state)
```

این ماژول صدا می‌زند:

```text
charger.c
  charger.h → app_types.h
  app_config.h / app_config.c    pwm_max_duty_permille
```

`bsp_pwm.c` هنوز از charger صدا زده نمی‌شود.
