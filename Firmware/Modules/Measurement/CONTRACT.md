# قرارداد ماژول Measurement

این برگه یک چیز است: **قولِ بیرونیِ** این ماژول. کد داخل ماژول هر وقت لازم شد عوض می‌شود و این برگه دست نمی‌خورد. این برگه فقط وقتی عوض می‌شود که «قرارِ بیرونی» عوض شود — یعنی چیزی که ماژول‌های دیگر رویش حساب می‌کنند.

برای همین، هر کسی داخل ماژول کار می‌کند حق ندارد این فایل را شل کند. تغییر این فایل تصمیم «صاحب مرز» است، نه تصمیم نویسندهٔ کد داخل ماژول.

## خلاصهٔ ماشین‌خوان

در قدم بعد، بازرس (`tools/check_module_contract.py`) همین بلوک را می‌خواند و با هدر مقایسه می‌کند: هر تابع و هر گلوبالی که در هدر اعلان شده باید اینجا باشد، و هر چه اینجا هست باید در هدر باشد.

```contract
version: 1
module: Measurement
header: measurement.h
may_include: Config, CalLut, Bsp, Fault
used_by: Charger, EspLink, McuPowerPath
globals_read_by: EspLink
public_functions: 18
public_globals: 20
snapshot_type: measurement_snapshot_t
```

## ۱. چه قولی می‌دهد — توابع (۱۸ عدد)

| نام | کار | خروجی و واحد |
|---|---|---|
| `func__Measurement_Init` | پاک کردن آخرین snapshot | — (`valid = false`) |
| `func__Measurement_Run` | یک فریم از BSP، فیلتر، تبدیل، انتشار | — (مقدارها را می‌نویسد) |
| `func__Measurement_GetSnapshot` | کپی آخرین snapshot | `bool`؛ `false` = هنوز معتبر نیست |
| `func__Measurement_CountsToMv` | شمارش ADC → ولتاژ پایه | `uint32_t`، mV، ۰..۳۳۰۰ |
| `func__Measurement_V24CountsToMv` | شمارش → ولتاژ ورودی ۲۴ با تقسیم برد | `uint32_t`، mV، ۰..~۳۷۰۰۰ |
| `func__Measurement_Battery24CountsToMv` | شمارش → ولتاژ **پک** ۲۴ با تقسیم کاربر (۶.۸k/۶۹.۲k) | `uint32_t`، mV |
| `func__Measurement_V12CountsToMv` | شمارش → ولتاژ باتری ۱۲ | `uint32_t`، mV، ۰..~۲۰۰۰۰ |
| `func__Measurement_Current1CountsToMa` | شمارش کانال ۱ → جریان سمت باتری | `uint32_t`، mA |
| `func__Measurement_Current2CountsToMa` | شمارش کانال ۲ → جریان سمت باتری | `uint32_t`، mA |
| `func__Measurement_CurrentIsBatteryCalibrated` | آیا خروجی mA از قبل سمت باتری است؟ | `bool`؛ `true` = مصرف‌کننده تبدیل دوم **نکند** |
| `func__Measurement_CurrentCountsToShuntUv` | شمارش → ولتاژ خالص شانت | `uint32_t`، uV |
| `func__Measurement_SetFilterMedianSize` | تنظیم پنجرهٔ مدین (۱..۱۵، زوج هم مجاز) | `uint8_t` = مقدار اعمال‌شده |
| `func__Measurement_SetFilterAverageWindow` | تنظیم پنجرهٔ میانگین (۱..۳۰۰، بیرون بازه گیره می‌خورد) | `uint16_t` = مقدار اعمال‌شده |
| `func__Measurement_GetFilterMedianSize` | خواندن پنجرهٔ مدین | `uint8_t` |
| `func__Measurement_GetFilterAverageWindow` | خواندن پنجرهٔ میانگین | `uint16_t` |
| `func__Measurement_SetVoltageOffsetMv` | تنظیم آفست ولتاژ کانال ۰/۱/۲ (حداکثر ±۵۰۰۰ mV) | `int32_t` = مقدار اعمال‌شده، mV |
| `func__Measurement_GetVoltageOffsetMv` | خواندن آفست ولتاژ کانال | `int32_t`، mV |
| `func__Measurement_SuperviseAdc` | ناظر ADC: ۶۰ ثانیه تلاش بی‌صدا، حداکثر یک ریست، ۶۰ ثانیه دیگر، بعد ثبت `FAULT_ADC` | — |

## ۲. چه قولی می‌دهد — وضعیت مشترک (۲۰ گلوبال)

قانون: **فقط تسک Measurement می‌نویسد.** هر ماژول دیگری فقط می‌خواند و باید اول `BOOL__G__MeasDataValid` را چک کند.

| نام | واحد | معنی |
|---|---|---|
| `UINT32_T__G__MeasInputVoltageMv` | mV | ولتاژ ورودی |
| `UINT32_T__G__MeasBattery24Mv` | mV | ولتاژ پک ۲۴ ولت |
| `UINT32_T__G__MeasBattery12Mv` | mV | ولتاژ باتری ۱۲ ولت |
| `UINT32_T__G__MeasBatteryLowMv` | mV | نیمهٔ پایین (برابر ولتاژ ۱۲) |
| `UINT32_T__G__MeasBatteryHighMv` | mV | نیمهٔ بالا = ۲۴ منهای ۱۲؛ اگر ۲۴ < ۱۲ باشد صفر |
| `UINT32_T__G__MeasVddaMv` | mV | مرجع ADC؛ ۰ = در دسترس نیست |
| `UINT32_T__G__MeasVinRawCounts` | counts | خام ADC ورودی، بدون هیچ تصحیح |
| `UINT32_T__G__MeasV24RawCounts` | counts | خام ADC کانال ۲۴ ورودی |
| `UINT32_T__G__MeasV12RawCounts` | counts | خام ADC کانال ۱۲ |
| `UINT32_T__G__MeasVrefintRawCounts` | counts | خام ADC مرجع داخلی |
| `UINT32_T__G__MeasCurrent1Ma` | mA | جریان کانال ۱، **فیلترشده** |
| `UINT32_T__G__MeasCurrent2Ma` | mA | جریان کانال ۲، **فیلترشده** |
| `UINT32_T__G__MeasCurrent1RawCounts` | counts | خام ADC جریان ۱ (همان فریم) |
| `UINT32_T__G__MeasCurrent1ShuntUv` | uV | ولتاژ شانت خالص، بدون آفست و تریم |
| `UINT32_T__G__MeasCurrent1MaUnfiltered` | mA | جریان ۱ همان فریم، بدون فیلتر |
| `UINT32_T__G__MeasCurrent2RawCounts` | counts | خام ADC جریان ۲ |
| `UINT32_T__G__MeasCurrent2ShuntUv` | uV | ولتاژ شانت خالص کانال ۲ |
| `UINT32_T__G__MeasCurrent2MaUnfiltered` | mA | جریان ۲ همان فریم، بدون فیلتر |
| `BOOL__G__MeasInputPresent` | — | ورودی ۲۴ ولت وصل است |
| `BOOL__G__MeasDataValid` | — | داده معتبر است (بعد از سه فریم کامل و پایدار) |

## ۳. snapshot

نوع `measurement_snapshot_t` در `Firmware/Config/Inc/app_types.h` تعریف شده است و خانه‌هایش دقیقاً همین‌ها هستند: `v_in_mv`, `v_bat24_mv`, `v_bat12_mv`, `v_bat_low_mv`, `v_bat_high_mv`, `i_ch1_ma`, `i_ch2_ma`, `input_present`, `valid` — همه با همان واحدهای بالا.

## ۴. این ماژول از چه چیزی استفاده می‌کند (بیرون‌رو)

| به | چه چیزی | چند نقطه (اندازه‌گیری‌شده) |
|---|---|---|
| `Config` | `app_types.h`، `modules_enable.h` | ۲ include |
| `Bsp` | `bsp_adc.h`، `bsp_measurement.h`، `bsp_gpio.h` | ۳ include، ۱۵ تابع BSP صدا زده می‌شود (ADC، کالیبراسیون برد، GPIO) |
| `CalLut` | جدول بنچ روی فلش | ۱ include، ۸ نقطه فراخوانی |
| `Fault` | ثبت `FAULT_ADC` | ۱ include (پشت `MODULE_FAULT`)، ۲ نقطه فراخوانی |

## ۵. چه کسی از این ماژول استفاده می‌کند (درون‌رو)

| از | چه چیزی | چند نقطه (اندازه‌گیری‌شده) |
|---|---|---|
| `EspLink` | ۱۹ گلوبال مشترک + `GetSnapshot` | ۱۹ خواندن، ۱۰ فراخوانی |
| `McuPowerPath` | فقط `GetSnapshot` | ۱ فراخوانی |
| `Charger` | **فقط نوع `measurement_snapshot_t`** — هیچ تابع و هیچ گلوبالی از این ماژول | ۰ فراخوانی، ۰ خواندن |
| تسک‌های `Rtos` | `Run` و `SuperviseAdc` هر ۱ میلی‌ثانیه | چسب برنامه، جزو ماژول‌ها نیست |

نکتهٔ قابل تصمیم (ثبت شده، تصمیمش با شما): سطر `Charger` نشان می‌دهد این ماژول `measurement.h` را include می‌کند فقط برای یک نوع که خانه‌اش `app_types.h` است. این تنها یاختهٔ «include بی‌مصرف» در کل برنامه است.

## ۶. قواعدی که با هر تغییر باید دست‌نخورده بمانند

۱. **نویسنده یکی است:** فقط تسک Measurement این ۲۰ گلوبال را می‌نویسد. هیچ ماژول دیگری حق نوشتن ندارد (امروز هم هیچ‌کدام نمی‌نویسد — با جست‌وجو تأیید شد).
۲. **اول validity، بعد مقدار:** خواندن هر گلوبال بدون چک `BOOL__G__MeasDataValid` ممنوع.
۳. **سه فریم:** داده تا سه فریم کامل و پایدار ADC معتبر نمی‌شود.
۴. **خام واقعاً خام است:** هر نام `*RawCounts` / `*ShuntUv` / `*MaUnfiltered` باید بدون فیلتر و بدون آفست بماند؛ این‌ها مرجع کالیبراسیون‌اند.
۵. **تبدیل دوم ممنوع:** اگر `func__Measurement_CurrentIsBatteryCalibrated` مقدار `true` داد، مصرف‌کننده حق ندارد دوباره تبدیل کند.
۶. **واحدها ثابت‌اند:** mV برای ولتاژ، mA برای جریان، uV برای شانت، counts برای خام. هیچ واحدی عوض نمی‌شود و هیچ عددی «نسبت به قبل» تفسیر نمی‌شود.
۷. **نام کلیدهای تنظیم:** آفست کانال‌ها ۰ = ورودی، ۱ = ۲۴، ۲ = ۱۲ است؛ عوض کردن این نگاشت، تغییر قرارداد است.
۸. **بدون تسک تازه:** این ماژول تسک، صف یا فلگ RTOS نمی‌سازد؛ زمان‌بندی فقط در `Firmware/Rtos` است.

## ۷. تغییر قرارداد یعنی چه

این‌ها **تغییر قرارداد** است و فقط با تأیید صاحب مرز انجام می‌شود:

- حذف یا تغییر نام هر تابع یا گلوبالی که بالاست؛
- تغییر واحد یا معنی هر مقدار؛
- اضافه‌شدن وابستگی تازه به ماژول دیگر (هر `#include` تازه از پوشهٔ ماژول دیگر)؛
- عوض‌شدن شرط معتبر بودن داده (قاعدهٔ سه فریم، یا `valid`)؛
- تغییر نگاشت کانال‌ها در کلیدهای تنظیم.

این‌ها **تغییر قرارداد نیست** و آزاد است: هر کاری داخل `measurement.c`، هر ثابت داخلی، هر بهینه‌سازی، هر تست تازه در `Tester/` — تا وقتی خروجی‌های بالا با همان واحد و همان معنی بمانند.

## ۸. چطور بررسی می‌شود

```
python3 tools/check_module_contract.py            # گزارش امروز
python3 tools/check_module_contract.py --strict   # تخطی از مرز را رد می‌کند
```

امروز وضعیت این ماژول: **۱۸ تابع اعلان‌شده، ۱۸ تابع تعریف‌شده، صفر تابع بیرون‌افتاده** (بازرس اندازه گرفت). یعنی هر چه در کد تعریف شده، در هدر اعلان دارد و پنهان نیست.
