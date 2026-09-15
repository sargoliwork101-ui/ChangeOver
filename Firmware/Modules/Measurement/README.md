/**
 * @file    README.md
 * @brief   [EN] Measurement module sheet: ADC to mV/mA.
 *          [FA] برگهٔ ماژول Measurement: ADC به میلی‌ولت/میلی‌آمپر.
 */

# ماژول Measurement

## وضعیت

**فعال.** `MODULE_MEASUREMENT = 1`. ADC1 + DMA1 در `.ioc` روشن است (۵ کانال، scan، continuous، کلاک 9MHz) و تسک measurement ساخته می‌شود. مقادیر تبدیل‌شده **گلوبال**‌اند (`UINT32_T__G__Meas*` / `BOOL_T__G__Meas*`)؛ فقط تسک measurement می‌نویسد و هر ماژولی می‌تواند بخواند (اول `BOOL_T__G__MeasDataValid` را چک کنید).

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-15 | مقادیر مشترک گلوبال شدند (`UINT32_T__G__MeasInputVoltageMv/Battery24Mv/Battery12Mv/Current1Ma/Current2Ma` + `BOOL_T__G__MeasInputPresent/DataValid`) — فقط تسک measurement می‌نویسد، همه می‌خوانند؛ در دیباگر با Live Expressions قابل مشاهده. پیشوند Meas* عمداً متفاوت از متغیرهای تست UI (task_ui.c) است تا لینک تداخل نکند |
| 2026-09-15 | فعال شد: ADC1+DMA چرخشی (بافر ۱۰ نصف‌واژه، بدون interrupt، بدون CPU)، توابع تبدیل گام‌به‌گام (CountsToMv / V24 / V12 / CurrentToMa)، دوره `MEASUREMENT_PERIOD_MS=10` بالای measurement.h، ورودی حضور ورودی از PB4 (`MCU_INT_24_IN`)، Init/Start داخل تسک (app.c دست‌نخورده ماند) |
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگهٔ ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت `func__Measurement_Init` / `Run` / `GetSnapshot` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `measurement.h` / `measurement.c` | ثابت‌های تبدیل + توابع تبدیل و آخرین snapshot |
| `../../Bsp/Src/bsp_adc.c` / `bsp_adc.h` | لایهٔ برد: استارت ADC+DMA، بافر چرخشی پرشدهٔ سخت‌افزار، `GetRaw` |
| `../../Rtos/Src/task_measurement.c` | تسک: Init+Start یک‌بار، بعد هر `MEASUREMENT_PERIOD_MS` یک `Run` |
| `../../Config/Inc/app_types.h` | `measurement_snapshot_t` (تأیید نشده تغییر نکند — تسک‌های بعدی از آن می‌خوانند) |
| `../../Config/Inc/modules_enable.h` | کلید `MODULE_MEASUREMENT` |
| `../../../CubeMX/CubeIDE.ioc` | ADC1 (CH1/2/3/5/7) + DMA1_Ch1 circular N=10 + کلاک ADC 9MHz |

## توابع

| نام | کار |
|---|---|
| `func__Measurement_Init` | snapshot را صفر می‌کند؛ `valid = false` |
| `func__Measurement_Run` | ۵ عدد خام را از بافر DMA کپی و به mV/mA تبدیل می‌کند؛ `input_present` را از PB4 می‌خواند؛ `valid = true` |
| `func__Measurement_GetSnapshot` | کپی آخرین snapshot؛ `NULL` یا نامعتبر → `false` |
| `func__Measurement_CountsToMv` | خام (0..4095) → mV در پایهٔ ADC (0..3300) |
| `func__Measurement_V24CountsToMv` | خام → mV منبع ۲۴ (ورودی/باتری)؛ برگردان تقسیم 76K/6.8K؛ سقف ~37V |
| `func__Measurement_V12CountsToMv` | خام → mV منبع ۱۲ (باتری)؛ برگردان تقسیم 41K/6.8K؛ سقف ~20V |
| `func__Measurement_CurrentCountsToMa` | خام → mA شارژ؛ ÷ گین 101 → ÷ شانت 10mΩ؛ 1A ≈ 1010mV |
| `func__TaskMeasurement` | Init+Start یک‌بار، 1ms انتظار فریم اول، بعد هر 10ms یک `Run` (`vTaskDelayUntil`) |
| `func__BspAdc_Init` (Bsp) | نگه‌داشتن هندل `hadc1` + صفر کردن بافر DMA |
| `func__BspAdc_Start` (Bsp) | `HAL_ADC_Start_DMA` (continuous + circular) — بعد از این، سخت‌افزار مستقل است |
| `func__BspAdc_IsFrameReady` (Bsp) | true بعد از Start موفق |
| `func__BspAdc_GetRaw` (Bsp) | کپی ۵ عدد اسلات دوم بافر DMA در critical section کوتاه (چند سیکل) |

## مقدارهای گلوبال (مشترک)

فقط تسک measurement می‌نویسد (هر `MEASUREMENT_PERIOD_MS`)؛ هر ماژول/تسک بعد از `#include "measurement.h"` می‌خواند:

| گلوبال | واحد | منبع |
|---|---|---|
| `UINT32_T__G__MeasInputVoltageMv` | mV | PA2 — ولتاژ ورودی ۲۴ (نمونه: 24000 = 24V) |
| `UINT32_T__G__MeasBattery24Mv` | mV | PA3 — باتری ۲۴ |
| `UINT32_T__G__MeasBattery12Mv` | mV | PA5 — باتری ۱۲ |
| `UINT32_T__G__MeasCurrent1Ma` | mA | PA1 — جریان شارژ کانال ۱ (۲۴) |
| `UINT32_T__G__MeasCurrent2Ma` | mA | PA7 — جریان شارژ کانال ۲ (۱۲) |
| `BOOL_T__G__MeasInputPresent` | — | PB4 (شماتیک: HIGH = ورودی وصل؛ هنوز اندازه‌گیری نشده) |
| `BOOL_T__G__MeasDataValid` | — | true از اولین فریم تبدیل‌شده |

`snapshot` (`func__Measurement_GetSnapshot`) هم همان داده + `valid` را یک‌جا کپی می‌دهد؛ هر دو هم‌زمان معتبرند (هر دو از یک‌جای Run نوشته می‌شوند).

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی |
|---|---|---|---|
| PA1 | `ADC_CURRENT1` (ADC1_IN1) | جریان شارژ کانال ۱ (شانت R64 + LM358) | آنالوگ (0..3.3V) |
| PA2 | `MCU_ADC_24_IN` (ADC1_IN2) | ولتاژ ورودی ۲۴ (R46+R11/R12) | آنالوگ (0..3.3V) |
| PA3 | `MCU_ADC_24_BAT` (ADC1_IN3) | ولتاژ باتری ۲۴ (R47+R13/R14) | آنالوگ (0..3.3V) |
| PA5 | `MCU_ADC_12_BAT` (ADC1_IN5) | ولتاژ باتری ۱۲ (R48+R15/R16) | آنالوگ (0..3.3V) |
| PA7 | `ADC_CURRENT2` (ADC1_IN7) | جریان شارژ کانال ۲ (شانت R68 + LM358) | آنالوگ (0..3.3V) |
| PB4 | `MCU_INT_24_IN` | حضور ورودی ۲۴ (دیجیتال، از R46+R10) | دیجیتال؛ شماتیک = ورودی وصل (~2.5V)؛ هنوز روی برد اندازه‌گیری نشده |

ترتیب کانال‌ها (`BSP_ADC_CHANNEL_*` در `bsp_adc.h`) با ترتیب رنک‌های `.ioc` یکی است.
سیگنال‌های آنالوگ قبل از پایه توسط باتری‌های فلتر C20..C22 و کلنگ‌های BAT54S (جریان‌ها) محافظت می‌شوند (شماتیک).

## پیش‌فرض امن

بعد از Init، `valid = false` است و هیچ snapshot معتبری وجود ندارد؛ تا `Start` موفق، `GetRaw` فقط `false` می‌دهد. خروجی GPIO ندارد (فقط ورودی).

## درخت اتصال

صدا زده می‌شود از:

```text
rtos_app.c → TaskMeasurement → task_measurement.c   (MODULE_MEASUREMENT = 1)
  func__Measurement_Init / func__Measurement_Run
task_protection.c / task_control.c / task_comm.c  (بعداً: func__Measurement_GetSnapshot)
```

این ماژول صدا می‌زند:

```text
measurement.c
  bsp_adc.h / bsp_adc.c   func__BspAdc_GetRaw
  board_pins.h            PIN_INT_24_IN (PB4)
  app_types.h             measurement_snapshot_t
task_measurement.c
  main.h                  hadc1 (هندل ساخت‌شده CubeMX)
  bsp_adc.h               func__BspAdc_Init / func__BspAdc_Start
  measurement.h           ثابت‌های دوره + توابع تبدیل
```

حافظه: بافر DMA = 2×5×2 = 20 بایت استاتیک؛ استک تسک `TASK_STACK_MEASUREMENT` = 192 word (768 بایت) — بعد از Build، مصرف کل RAM/Flash را از Map file چک کنید (قانون مدیریت حافظه).
