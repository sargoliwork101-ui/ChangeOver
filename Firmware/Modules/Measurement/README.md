/**
 * @file    README.md
 * @brief   [EN] Measurement module: ADC counts to millivolt / milliamp.
 *          [FA] ماژول اندازه‌گیری: شمارش ADC به میلی‌ولت / میلی‌آمپر.
 */

# ماژول Measurement

توضیح کامل **همین ماژول** همین‌جاست.

الان **خاموش** است (`MODULE_MEASUREMENT 0` در `modules_enable.h`). `App_Start` آن را صدا نمی‌زند. ADC را در CubeMX Enable نکن تا مرحلهٔ UI تمام شود. فایل‌ها را پاک نکن.

## کار ماژول

ولتاژ و جریان برد را از ADC می‌خواند و به واحد مهندسی تبدیل می‌کند تا بقیهٔ ماژول‌ها با «شمارش خام ADC» کار نکنند.

خروجی یک `measurement_snapshot_t` است (`app_types.h`):

| فیلد | معنی |
|---|---|
| `v_in_mv` | ولتاژ ورودی ۲۴ ولت، میلی‌ولت |
| `v_bat24_mv` | باتری ۲۴ ولت |
| `v_bat12_mv` | باتری ۱۲ ولت |
| `i_ch1_ma` / `i_ch2_ma` | جریان دو کانال شارژ، میلی‌آمپر |
| `input_present` | ورودی هست یا نه |
| `valid` | این نمونه قابل استفاده است |

## پایه‌هایی که بعداً مال این ماژول‌اند

از `board_pins.h` — حالا در CubeMX نزن:

| پایه | ADC | نقش |
|---|---|---|
| PA1 | ADC1_IN1 | جریان ۱ |
| PA2 | ADC1_IN2 | ۲۴ ولت ورودی |
| PA3 | ADC1_IN3 | ۲۴ ولت باتری |
| PA5 | ADC1_IN5 | ۱۲ ولت باتری |
| PA7 | ADC1_IN7 | جریان ۲ |

ترتیب کانال باید با `bsp_adc.h` یکی باشد.

## فایل‌ها

| فایل | نقش |
|---|---|
| `measurement.h` / `measurement.c` | تبدیل و نگه‌داشتن آخرین نمونه |
| `../../Bsp/Src/bsp_adc.c` | خواندن HAL ADC (اسکلت) |
| `../../Rtos/Src/task_measurement.c` | تسک؛ با فلگ صفر فقط `vTaskDelay(1000)` |
| `../../Config/Inc/app_types.h` | نوع `measurement_snapshot_t` |

این `.c` را به بیلد مرحلهٔ LED اضافه نکن: `measurement.c` ، `bsp_adc.c`. تسک `task_measurement.c` را در پروژه بگذار؛ با `#if MODULE_MEASUREMENT` خالی می‌ماند.

## توابع همین الان در کد

- `Measurement_Init` — همهٔ فیلدهای `s_snap` را صفر می‌کند، `valid = false`.
- `Measurement_Run` — `BspAdc_GetRaw` را می‌زند؛ تا DMA راه نیفتد `valid` را false می‌گذارد و برمی‌گردد. تبدیل mV/mA هنوز نوشته نشده.
- `Measurement_GetSnapshot` — اگر `out` برابر `NULL` باشد false (MISRA: ننویس روی آدرس صفر). وگرنه کپی `s_snap`؛ موفقیت فقط وقتی `valid` باشد.

وقتی این مرحله شروع شود، Protection و Changeover از همین snapshot می‌خوانند، نه مستقیم از ADC.
