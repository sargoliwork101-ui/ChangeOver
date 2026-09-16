/**
 * @file    README.md
 * @brief   [EN] Measurement module sheet: ADC to mV/mA.
 *          [FA] برگهٔ ماژول Measurement: ADC به میلی‌ولت/میلی‌آمپر.
 */

# ماژول Measurement

## وضعیت

**فعال.** `MODULE_MEASUREMENT = 1`. پورت فعلی برد فریم‌های normalized را از BSP ارائه می‌کند و تسک Measurement ساخته می‌شود. کد ماژول فقط فریم normalized و API منطقی BSP را مصرف می‌کند. مقادیر تبدیل‌شده **گلوبال**‌اند (`UINT32_T__G__Meas*` / `BOOL__G__Meas*`)؛ فقط تسک Measurement می‌نویسد و هر ماژولی می‌تواند بخواند (ابتدا `BOOL__G__MeasDataValid` را چک کنید؛ این پرچم پس از سه فریم کامل و پایدار ADC معتبر می‌شود).

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-16 | اصلاح ADC/Measurement: کلاک ADC روی 12MHz (PCLK2/6)، کالیبراسیون F1، خواندن نیمهٔ کامل DMA با CNDTR، ضرایب صحیح تقسیم ولتاژ، محاسبهٔ دقیق‌تر جریان و snapshot اتمیک شد |
| 2026-09-15 | مقادیر مشترک گلوبال شدند (`UINT32_T__G__MeasInputVoltageMv/Battery24Mv/Battery12Mv/Current1Ma/Current2Ma` + `BOOL__G__MeasInputPresent/DataValid`) — فقط تسک measurement می‌نویسد، همه می‌خوانند؛ در دیباگر با Live Expressions قابل مشاهده. پیشوند Meas* عمداً متفاوت از متغیرهای تست UI (task_ui.c) است تا لینک تداخل نکند |
| 2026-09-15 | فعال شد: ADC1+DMA چرخشی (بافر ۱۰ نصف‌واژه، بدون interrupt، بدون CPU)، توابع تبدیل گام‌به‌گام (CountsToMv / V24 / V12 / CurrentToMa)، دوره `MEASUREMENT_PERIOD_MS=10` بالای measurement.h، ورودی حضور ورودی از PB4 (`MCU_INT_24_IN`)، Init/Start داخل تسک (app.c دست‌نخورده ماند) |
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگهٔ ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت `func__Measurement_Init` / `Run` / `GetSnapshot` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `measurement.h` / `measurement.c` | قرارداد عمومی، توابع نما و آخرین snapshot؛ بدون جزئیات برد |
| `../../Bsp/Src/bsp_adc.c` / `bsp_adc.h` | لایهٔ برد: استارت ADC+DMA و ارائهٔ فریم خام استاندارد؛ هندل و کانال فیزیکی خصوصی است |
| `../../Rtos/Src/task_measurement.c` | تسک: Init+Start یک‌بار، بعد هر `MEASUREMENT_PERIOD_MS` یک `Run` |
| `../../Bsp/Src/bsp_measurement.c` / `bsp_measurement.h` | کالیبراسیون مخصوص مدار برد؛ مرجع ADC، تقسیم مقاومتی، گین و شانت در این Port هستند |
| `../../Config/Inc/app_types.h` | `measurement_snapshot_t` مشترک بین ماژول‌ها |
| `../../Config/Inc/modules_enable.h` | کلید `MODULE_MEASUREMENT` |
| `../../../CubeMX/CubeIDE.ioc` | ADC1 (CH1/2/3/5/7) + DMA1_Ch1 circular N=10 + کلاک ADC 12MHz |

## توابع

| نام | کار |
|---|---|
| `func__Measurement_Init` | snapshot را صفر می‌کند؛ `valid = false` |
| `func__Measurement_Run` | یک فریم normalized را از BSP می‌گیرد و به mV/mA تبدیل می‌کند؛ `input_present` را از سیگنال منطقی BSP می‌خواند؛ پس از سه فریم کامل و پایدار `valid = true` می‌شود |
| `func__Measurement_GetSnapshot` | کپی آخرین snapshot؛ `NULL` یا نامعتبر → `false` |
| `func__Measurement_CountsToMv` | خام استاندارد → mV پایه، با کالیبراسیون BSP برد |
| `func__Measurement_V24CountsToMv` | خام استاندارد → mV منبع ۲۴، با تقسیم برد در BSP |
| `func__Measurement_V12CountsToMv` | خام استاندارد → mV منبع ۱۲، با تقسیم برد در BSP |
| `func__Measurement_CurrentCountsToMa` | خام استاندارد → mA شارژ، با گین و شانت برد در BSP |
| `func__TaskMeasurement` | Init+Start یک‌بار، سپس هر 10ms یک `Run` (`osDelayUntil` با تبدیل قابل‌حمل میلی‌ثانیه/تیک) |
| `func__BspAdc_Init` (Bsp) | آماده‌سازی Backend ADC برد و صفر کردن بافر DMA؛ هندل و پایه‌ها در BSP پنهان هستند |
| `func__BspAdc_Start` (Bsp) | شروع backend ADC+DMA برد؛ جزئیات peripheral و منابع وقفه در پورت برد خصوصی است |
| `func__BspAdc_IsFrameReady` (Bsp) | true بعد از کالیبراسیون و Start موفق |
| `func__BspAdc_GetRaw` (Bsp) | انتخاب نیمهٔ کامل با CNDTR و کپی پایدار ۵ کانال با بررسی قبل/بعد شمارنده |

## مقدارهای گلوبال (مشترک)

فقط تسک measurement می‌نویسد (هر `MEASUREMENT_PERIOD_MS`)؛ هر ماژول/تسک بعد از `#include "measurement.h"` می‌خواند:

| گلوبال | واحد | منبع |
|---|---|---|
| `UINT32_T__G__MeasInputVoltageMv` | mV | ورودی منطقی ۲۴ ولت (نمونه: 24000 = 24V) |
| `UINT32_T__G__MeasBattery24Mv` | mV | باتری منطقی ۲۴ ولت |
| `UINT32_T__G__MeasBattery12Mv` | mV | باتری منطقی ۱۲ ولت |
| `UINT32_T__G__MeasCurrent1Ma` | mA | جریان شارژ منطقی کانال ۱ |
| `UINT32_T__G__MeasCurrent2Ma` | mA | جریان شارژ منطقی کانال ۲ |
| `BOOL__G__MeasInputPresent` | — | سیگنال منطقی حضور ورودی ۲۴ ولت |
| `BOOL__G__MeasDataValid` | — | true پس از سه فریم کامل و پایدار ADC؛ مستقل از وجود ورودی ۲۴ ولت |

`snapshot` (`func__Measurement_GetSnapshot`) هم همان داده + `valid` را یک‌جا کپی می‌دهد؛ هر دو هم‌زمان معتبرند (هر دو از یک‌جای Run نوشته می‌شوند).

## پایه‌ها

این ماژول فقط قرارداد منطقی زیر را می‌بیند. ترتیب فیزیکی ADC، پایه‌ها، قطبیت و کالیبراسیون در BSP برد تعریف می‌شوند و در این ماژول تکرار نمی‌شوند.

| شناسهٔ منطقی | واحد | نقش |
|---|---|---|
| `BSP_ADC_CHANNEL_CURRENT1` | mA | جریان شارژ منطقی کانال ۱ |
| `BSP_ADC_CHANNEL_24V_IN` | mV | ورودی منطقی ۲۴ ولت |
| `BSP_ADC_CHANNEL_24V_BAT` | mV | باتری منطقی ۲۴ ولت |
| `BSP_ADC_CHANNEL_12V_BAT` | mV | باتری منطقی ۱۲ ولت |
| `BSP_ADC_CHANNEL_CURRENT2` | mA | جریان شارژ منطقی کانال ۲ |
| `BSP_GPIO_INPUT_24V_PRESENT` | bool | حضور منطقی ورودی ۲۴ ولت |

جزئیات اتصال فیزیکی برد فعلی در `CubeMX/README.md` و پیاده‌سازی BSP قرار دارد؛ تغییر MCU یا برد نباید این ماژول را مجبور به تغییر کند.

## پیش‌فرض امن

بعد از Init، `valid = false` است و هیچ snapshot معتبری وجود ندارد. پس از Start موفق، سه فریم کامل و پایدار برای warm-up لازم است؛ تا آن زمان `GetSnapshot` همچنان داده را نامعتبر گزارش می‌کند. نبودن ورودی ۲۴ ولت خطای ADC نیست و می‌تواند همراه با `valid = true` و ولتاژ ورودی تقریباً صفر باشد. خروجی GPIO ندارد (فقط ورودی).

## درخت اتصال

صدا زده می‌شود از:

```text
rtos_app.c → TaskMeasurement → task_measurement.c   (MODULE_MEASUREMENT = 1)
  func__Measurement_Init / func__Measurement_Run
task_ui.c  (اکنون فقط UINT32_T__G__MeasInputVoltageMv را برای UI می‌خواند)
task_protection.c / task_control.c / task_comm.c  (بعداً: func__Measurement_GetSnapshot)
```

این ماژول صدا می‌زند:

```text
measurement.c
  bsp_adc.h / bsp_adc.c   func__BspAdc_GetRaw
  bsp_gpio.h              BSP_GPIO_INPUT_24V_PRESENT (سیگنال منطقی حضور ورودی)
  app_types.h             measurement_snapshot_t
task_measurement.c
  bsp_adc.h               func__BspAdc_Init / func__BspAdc_Start
  bsp_adc.c               Port برد فعلی و هندل ADC خصوصی آن
  measurement.h           قرارداد عمومی، ثابت دوره + توابع نمای تبدیل
  bsp_measurement.h/c      کالیبراسیون مخصوص مدار برد
```

حافظه: بافر DMA = 2×5×2 = 20 بایت استاتیک؛ استک تسک `TASK_STACK_MEASUREMENT` = 192 word (768 بایت) — بعد از Build، مصرف کل RAM/Flash را از Map file چک کنید (قانون مدیریت حافظه).
