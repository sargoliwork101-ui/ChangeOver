/**
 * @file    README.md
 * @brief   [EN] Changeover module sheet: input vs battery path.
 *          [FA] برگه ماژول Changeover: مسیر ورودی یا باتری.
 */

# ماژول Changeover

## وضعیت

اسکلت. `MODULE_CHANGEOVER = 0`. GPIO قدرت را Enable نکن. فایل را پاک نکن.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت `Changeover_Init` / `Changeover_Evaluate` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `changeover.h` / `changeover.c` | ماشین حالت |
| `../../Rtos/Src/task_control.c` | تسک مشترک با Charger و Jitter |
| `../../Config/Inc/app_types.h` | `app_state_t` |

`changeover.c` را به بیلد LED اضافه نکن.

## توابع

| نام | کار |
|---|---|
| `Changeover_Init` | `s_state = APP_STATE_BOOT` |
| `Changeover_Evaluate` | اگر خطا باشد `FAULT`، وگرنه `IDLE`. مسیر INPUT/BATTERY هنوز نیست |
| `TaskControl` | فقط اگر Changeover یا Charger یا Jitter یک باشد ساخته می‌شود |

حالت‌ها: `BOOT`، `IDLE`، `INPUT`، `BATTERY`، `FAULT`، `SAFE`.

## پایه‌ها

قطبیت از شماتیک است، روی برد اندازه نشده.

| پایه | لیبل | نقش | HIGH یعنی (شماتیک) |
|---|---|---|---|
| PB5 | `MCU_CONTROL_PS` | سوئیچ باتری به تغذیه کنترل | باتری از PSU کنترل جدا |
| PB7 | `MCU_RELAY` | رله شارژر | رله وصل |
| PB11 | `MCU_PROTECT_BATT` | قطع مسیر باتری Q17 | مسیر باتری خاموش |

## پیش‌فرض امن

Init فقط state را BOOT می‌کند؛ پایه را High نمی‌کند. تا اندازه‌گیری قطبیت، این خروجی‌ها را از CubeMX هم Low بگذار.

## درخت اتصال

صدا زده می‌شود از (وقتی فلگ ۱ شود):

```text
rtos_app.c → TaskControl → task_control.c
  Changeover_Evaluate(&snap, faults)
```

این ماژول صدا می‌زند:

```text
changeover.c
  app_types.h     app_state_t ، measurement_snapshot_t ، fault_mask_t
```

snapshot از Measurement می‌آید، faults از Fault؛ خود Changeover آن‌ها را include نمی‌کند، از آرگومان می‌گیرد. GPIO قدرت هنوز از اینجا زده نمی‌شود.
