/**
 * @file    README.md
 * @brief   [EN] Charger module sheet: PWM duty policy.
 *          [FA] برگه ماژول Charger: سیاست PWM.
 */

# ماژول Charger

## وضعیت

اسکلت. `MODULE_CHARGER = 0`. backend PWM عمداً در `.ioc` و Build فعال است، اما خروجی‌ها در startup صفر و متوقف هستند. فایل ماژول را پاک نکن.

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

جدول زیر فقط مرجع فیزیکی برد فعلی است؛ Charger باید از کانال منطقی `bsp_pwm.h` استفاده کند و نباید پایه یا هندل تایمر را بشناسد.

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

`bsp_pwm.c` در startup از مسیر BSP به‌صورت safe مقداردهی می‌شود؛ Charger پس از فعال‌سازی باید فقط `func__BspPwm_SetDutyPermille` و `func__BspPwm_StopAll` را صدا بزند.
