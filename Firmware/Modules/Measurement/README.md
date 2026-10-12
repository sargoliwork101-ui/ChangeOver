/**
 * @file    README.md
 * @brief   [EN] Measurement module sheet: ADC to mV/mA.
 *          [FA] برگهٔ ماژول Measurement: ADC به میلی‌ولت/میلی‌آمپر.
 */

# ماژول Measurement

## وضعیت

**فعال.** `MODULE_MEASUREMENT = 1`. پورت فعلی برد فریم‌های normalized را از BSP ارائه می‌کند و تسک Measurement ساخته می‌شود. کد ماژول فقط فریم normalized و API منطقی BSP را مصرف می‌کند. مقادیر تبدیل‌شده **گلوبال**‌اند (`UINT32_T__G__Meas*` / `BOOL__G__Meas*`)؛ فقط تسک Measurement می‌نویسد و هر ماژولی می‌تواند بخواند (ابتدا `BOOL__G__MeasDataValid` را چک کنید؛ این پرچم پس از سه فریم کامل و پایدار ADC معتبر می‌شود).

از ۲۰۲۶-۰۹-۲۲ (دستور کاربر):
- دورهٔ تسک `MEASUREMENT_PERIOD_MS = 1` است تا نمونه‌های سنکرون جریان حداکثر ۱ms عمر داشته باشند.
- **جریان شارژ از زنجیرهٔ فیلتر کلیددار عبور می‌کند**: پورت برد دو جایگاه CURRENT1/CURRENT2 فریم را با نمونهٔ خامِ سنکرون با PWM پر می‌کند (تبدیل سخت‌افزاری ADC2 با تریگر لبهٔ وسط پنجرهٔ ON گیت — `TIM2_CC2` و `TIM3_TRGO`)؛ این ماژول نمونهٔ خام را اول از **مدین** (پیش‌فرض ۳؛ اندازهٔ زنده از پنل ۱..۱۵، زوج هم مجاز، ۱..۲ = عبور مستقیم) و **میانگین متحرک** (پیش‌فرض ۱۰ نمونه؛ پنجرهٔ زنده از پنل ۱..۳۰۰، هر کانال جدا) عبور می‌دهد و بعد به mA تبدیل می‌کند (دستور کاربر ۲۰۲۶-۰۹-۲۷: فیلتر روی دادهٔ خام ADC). هر دو با ثابت کامپایل `MEASUREMENT_CURRENT_MEDIAN3_ENABLE` / `MEASUREMENT_CURRENT_AVERAGE_ENABLE` در `measurement.h` جداگانه روشن/خاموش می‌شوند (۱u=فعال، ۰u=کامپایل‌نشده؛ پیش‌فرض اندازه‌ها `MEASUREMENT_CURRENT_AVERAGE_WINDOW_DEFAULT` = ۱۰ و مدین ۳؛ ثابت `MEASUREMENT_CURRENT_AVERAGE_WINDOW` = ۳۰۰ فقط **سقف** آرایهٔ استاتیک است). ترتیب ثابت است: اول مدین، بعد میانگین. اگر نمونهٔ سنکرون ممکن نشود (گیت پارک/timeout)، مقدار اسکن غیرهمزمان همان جایگاه می‌ماند و از همان زنجیره رد می‌شود.
- فرمول تبدیل ADC→mA در `bsp_measurement` مرحله‌به‌مرحله از مدار ساخته شده (شمارش→mV پایه → خنثی‌سازی تقسیم R41/R42 → خنثی‌سازی گین ۱۰۱ → شانت 10mΩ → گین پرمیل بنچ)؛ جزئیات در README ماژول Bsp.
- ولتاژهای باتری مدین-۵ ضد spike روی شمارش خام دو کانال V24/V12 دارند (دستور کاربر ۲۰۲۶-۰۹-۲۷: فیلتر روی خام، تبدیل/مشتق بعدش؛ Vin بدون فیلتر مثل قبل).

## فایل‌ها

| فایل | نقش |
|---|---|
| `measurement.h` / `measurement.c` | قرارداد عمومی، توابع نما و آخرین snapshot؛ بدون جزئیات برد. درون‌یابی جدول بنچ از ۲۰۲۶-۱۰-۰۶ جدول کمتر از دو نقطه را رد می‌کند (گیره در برابر کم‌ریزی شاخص) |
| `calibration.h` | ثابت VREFINT و سوئیچ ردیابی VDDA؛ جدول بنچ دیگر اینجا کامپایل نمی‌شود (در فلش است، ماژول CalLut) |
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
| `func__Measurement_Run` | یک فریم normalized را از BSP می‌گیرد (جریان‌ها = نمونهٔ سنکرون وسط ON از پورت برد)، اول فیلترها را روی شمارش خام اجرا می‌کند (جریان هر کانال: مدین-۳ → میانگین متحرک ۱۰تایی؛ ولتاژ باتری: مدین-۵ روی شمارش خام V24/V12) و بعد به mV/mA تبدیل می‌کند؛ جریان هر کانال بعد از تبدیل chain-mA از LUT توانی بنچ خودش رد می‌شود (توان mW ÷ ولتاژ زندهٔ باتری = mA واقعی باتری، `iest`)؛ `input_present` را از سیگنال منطقی BSP می‌خواند؛ پس از سه فریم کامل و پایدار `valid = true` می‌شود |
| `func__Measurement_GetSnapshot` | کپی آخرین snapshot؛ `NULL` یا نامعتبر → `false` |
| `func__Measurement_CountsToMv` | خام استاندارد → mV پایه، با کالیبراسیون BSP برد |
| `func__Measurement_V24CountsToMv` | خام استاندارد → mV منبع ۲۴، با تقسیم برد در BSP |
| `func__Measurement_V12CountsToMv` | خام استاندارد → mV منبع ۱۲، با تقسیم برد در BSP |
| `func__Measurement_Current1CountsToMa` / `Current2CountsToMa` | شمارش خام هر کانال → mA باتری: جدول جریان بنچ همان کانال روی محور شمارش (اگر جدول در فلش باشد)؛ در غیر این صورت خط گین و آفست پر-کانال در BSP (پیش‌فرض گین ۳۰۰۰‰ و آفست ۰) در `calibration.h` (۱۷ انکر ch1 از v1.19، ۱۴ انکر ch2 از v1.18). **جفت‌شدگی:** هر جدول فقط با آفست/گینِ بنچ خودش معتبر است — تغییر گین = بازفیت جدول |
| `func__TaskMeasurement` | Init+Start یک‌بار، سپس هر `MEASUREMENT_PERIOD_MS` (۱ms) یک `Run` (`osDelayUntil` با تبدیل قابل‌حمل میلی‌ثانیه/تیک) |
| `func__BspAdc_Init` (Bsp) | آماده‌سازی Backend ADC برد و صفر کردن بافر DMA؛ هندل و پایه‌ها در BSP پنهان هستند |
| `func__BspAdc_Start` (Bsp) | شروع backend ADC+DMA برد؛ جزئیات peripheral و منابع وقفه در پورت برد خصوصی است |
| `func__BspAdc_IsFrameReady` (Bsp) | true بعد از کالیبراسیون و Start موفق |
| `func__BspAdc_GetRaw` (Bsp) | انتخاب نیمهٔ کامل با CNDTR و کپی پایدار ۵ کانال با بررسی قبل/بعد شمارنده؛ سپس جایگذاری نمونهٔ سنکرون وسط ON در جایگاه‌های CURRENT1/CURRENT2 (فقط پورت برد) |

## مقدارهای گلوبال (مشترک)

فقط تسک measurement می‌نویسد (هر `MEASUREMENT_PERIOD_MS`)؛ هر ماژول/تسک بعد از `#include "measurement.h"` می‌خواند:

| گلوبال | واحد | منبع |
|---|---|---|
| `UINT32_T__G__MeasInputVoltageMv` | mV | ورودی منطقی ۲۴ ولت (نمونه: 24000 = 24V) |
| `UINT32_T__G__MeasBattery24Mv` | mV | فقط مانیتور پک ۲۴ ولت |
| `UINT32_T__G__MeasBattery12Mv` | mV | نود MID / باتری پایین |
| `UINT32_T__G__MeasBatteryLowMv` | mV | `VLOW = MID-GND` برای Trans2 |
| `UINT32_T__G__MeasBatteryHighMv` | mV | `VHIGH = V24-MID` برای Trans1 |
| `UINT32_T__G__MeasCurrent1Ma` | mA | جریان شارژ منطقی کانال ۱ |
| `UINT32_T__G__MeasCurrent2Ma` | mA | جریان شارژ منطقی کانال ۲ |
| `BOOL__G__MeasInputPresent` | — | سیگنال منطقی حضور ورودی ۲۴ ولت |
| `BOOL__G__MeasDataValid` | — | true پس از سه فریم کامل و پایدار ADC؛ مستقل از وجود ورودی ۲۴ ولت |

`snapshot` (`func__Measurement_GetSnapshot`) هم همان داده + `valid` را یک‌جا کپی می‌دهد؛ فیلدهای `v_bat_low_mv` و `v_bat_high_mv` برای دو شارژر مستقل هستند و `v_bat24_mv` فقط مانیتور پک است.

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
task_ui.c  (سناریوهای LED/بازر از snapshot کامل: ولتاژها + جریان‌ها + وضعیت شارژر)
task_control.c / task_comm.c  (حلقهٔ شارژر و تله‌متری ESP از `func__Measurement_GetSnapshot` + گلوبال‌ها)
task_protection.c  (فلگ ماژول ۰ — فعلاً مصرف‌کننده نیست)
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

حافظه: بافر DMA = 2×5×2 = 20 بایت استاتیک؛ استک تسک `TASK_STACK_MEASUREMENT` = 192 word (768 بایت) — بعد از Build، مصرف کل RAM/Flash را از Map file چک کنید (قانون مدیریت حافظه). بک‌اند سنکرون ADC2 در پورت برد حدود ۵۰ بایت RAM استاتیک اضافه می‌کند (هندل خصوصی `ADC_HANDLETYPEDEF__G__HadcSync` در `bsp_adc.c`)؛ بافرهای فیلتر جریان هم با هر دو کلید روشن RAM استاتیک اضافه می‌کنند — پنجرهٔ میانگین `۲×MEASUREMENT_CURRENT_AVERAGE_WINDOW(۳۰۰)×4` = **۲۴۰۰ بایت** + تاریخچهٔ مدین `۲×۱۵×4` = ۱۲۰ بایت + شمارنده‌ها — و با خاموش‌کردن هر کلید کاملاً از کامپایل خارج می‌شوند؛ مصرف دقیق بعد از Build واقعی CubeIDE از Map file ثبت شود.

## تستر

`Tester/` — تست هاست زنجیرهٔ اندازه‌گیری با فریم ADC مصنوعی (۲۸۶ بررسی). اجرا با
`Tester/run_host_test_measurement.sh`؛ داخل `tools/check_firmware_syntax.sh` هم اجرا
می‌شود. توضیح کامل در `Tester/README.md`.

## ماشین حالت

`Measurement_State_Machine.xlsx` — حالت‌ها و گذارهای همین ماژول، رنگی و
راست‌به‌چپ با فونت وزیرمتن. از برگهٔ «ماشین حالت» در فایل اعتبارسنجی همین
پوشه هم به آن لینک هست و خودش به نمای سیستمی
(`Documentation/System_State_Machine.xlsx`) برمی‌گردد. بازتولید:
`python3 tools/make_module_state_machines.py`.

## تاریخچه

تاریخچهٔ تغییرات در `git log` همین مخزن نگهداری می‌شود.
