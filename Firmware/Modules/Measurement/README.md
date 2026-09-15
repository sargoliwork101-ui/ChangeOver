/**
 * @file    README.md
 * @brief   [EN] Measurement module sheet: ADC to mV/mA.
 *          [FA] برگه ماژول Measurement: ADC به میلی‌ولت/میلی‌آمپر.
 */

# ماژول Measurement

## وضعیت

اسکلت. `MODULE_MEASUREMENT = 0`. ADC را Enable نکن. فایل را پاک نکن.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت `func__Measurement_Init` / `Run` / `GetSnapshot` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `measurement.h` / `measurement.c` | تبدیل و آخرین نمونه |
| `../../Bsp/Src/bsp_adc.c` | HAL ADC (اسکلت) — به بیلد LED اضافه نکن |
| `../../Rtos/Src/task_measurement.c` | تسک؛ فلگ صفر = delay |
| `../../Config/Inc/app_types.h` | `measurement_snapshot_t` |

## توابع

| نام | کار |
|---|---|
| `func__Measurement_Init` | `s_snap` را صفر می‌کند؛ `valid = false` |
| `func__Measurement_Run` | یک فریم ADC می‌گیرد؛ تا DMA نباشد `valid` را false می‌گذارد. تبدیل mV هنوز نیست |
| `func__Measurement_GetSnapshot` | کپی آخرین نمونه؛ `NULL` یا نامعتبر → false |
| `TaskMeasurement` | تا فلگ صفر فقط `vTaskDelay(1000)` |

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی |
|---|---|---|---|
| PA1 | `MCU_CURRENT1` (ADC1_IN1) | جریان کانال ۱ | آنالوگ |
| PA2 | `MCU_24_IN` (ADC1_IN2) | ولتاژ ورودی ۲۴ | آنالوگ |
| PA3 | `MCU_24_BAT` (ADC1_IN3) | ولتاژ باتری ۲۴ | آنالوگ |
| PA5 | `MCU_12_BAT` (ADC1_IN5) | ولتاژ باتری ۱۲ | آنالوگ |
| PA7 | `MCU_CURRENT2` (ADC1_IN7) | جریان کانال ۲ | آنالوگ |
| PB4 | `MCU_INT_24_IN` | حضور ورودی ۲۴ (نیاز به SWD نه JTAG) | دیجیتال؛ قطبیت شماتیک، هنوز اندازه نشده |

ترتیب ADC باید با `bsp_adc.h` یکی بماند.

## پیش‌فرض امن

بعد از Init هیچ نمونه‌ای معتبر نیست (`valid = false`). خروجی GPIO ندارد.

## درخت اتصال

صدا زده می‌شود از (وقتی فلگ ۱ شود):

```text
rtos_app.c → TaskMeasurement → task_measurement.c
  func__Measurement_Init / func__Measurement_Run / func__Measurement_GetSnapshot
task_protection.c → func__Measurement_GetSnapshot
task_control.c    → func__Measurement_GetSnapshot
task_comm.c       → func__Measurement_GetSnapshot
```

این ماژول صدا می‌زند:

```text
measurement.c
  bsp_adc.h / bsp_adc.c   BspAdc_GetRaw
  app_types.h             measurement_snapshot_t
```
