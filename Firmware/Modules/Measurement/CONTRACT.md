# قرارداد Measurement

```contract
version: 1
module: Measurement
header: measurement.h
may_include: Config, CalLut, Bsp, Fault
used_by: Charger, EspLink, McuPowerPath
globals_read_by: EspLink
public_functions: 18
public_globals: 20
```

## ورودی و خروجی توابع

| تابع | ورودی | خروجی |
|---|---|---|
| `func__Measurement_Init` | — | — |
| `func__Measurement_Run` | — | — (مقدارها را می‌نویسد) |
| `func__Measurement_GetSnapshot` | `measurement_snapshot_t *out` | `bool` (`false` = معتبر نیست) |
| `func__Measurement_CountsToMv` | `uint16_t counts` | `uint32_t` mV، ۰..۳۳۰۰ |
| `func__Measurement_V24CountsToMv` | `uint16_t counts` | `uint32_t` mV، ۰..~۳۷۰۰۰ |
| `func__Measurement_Battery24CountsToMv` | `uint16_t counts` | `uint32_t` mV (پک ۲۴) |
| `func__Measurement_V12CountsToMv` | `uint16_t counts` | `uint32_t` mV، ۰..~۲۰۰۰۰ |
| `func__Measurement_Current1CountsToMa` | `uint16_t counts` | `uint32_t` mA |
| `func__Measurement_Current2CountsToMa` | `uint16_t counts` | `uint32_t` mA |
| `func__Measurement_CurrentIsBatteryCalibrated` | `uint8_t channel` (۰ یا ۱) | `bool` |
| `func__Measurement_CurrentCountsToShuntUv` | `uint16_t counts` | `uint32_t` uV |
| `func__Measurement_SetFilterMedianSize` | `uint8_t size` (۱..۱۵) | `uint8_t` = اعمال‌شده |
| `func__Measurement_SetFilterAverageWindow` | `uint32_t samples` (۱..۳۰۰) | `uint16_t` = اعمال‌شده |
| `func__Measurement_GetFilterMedianSize` | — | `uint8_t` |
| `func__Measurement_GetFilterAverageWindow` | — | `uint16_t` |
| `func__Measurement_SetVoltageOffsetMv` | `uint8_t channel` (۰/۱/۲)، `int32_t mv` (±۵۰۰۰) | `int32_t` = اعمال‌شده، mV |
| `func__Measurement_GetVoltageOffsetMv` | `uint8_t channel` (۰/۱/۲) | `int32_t` mV |
| `func__Measurement_SuperviseAdc` | `uint32_t nowMs` | — |

## خروجی‌های مشترک (گلوبال)

| نام | واحد |
|---|---|
| `UINT32_T__G__MeasInputVoltageMv` | mV |
| `UINT32_T__G__MeasBattery24Mv` | mV |
| `UINT32_T__G__MeasBattery12Mv` | mV |
| `UINT32_T__G__MeasBatteryLowMv` | mV |
| `UINT32_T__G__MeasBatteryHighMv` | mV |
| `UINT32_T__G__MeasVddaMv` | mV |
| `UINT32_T__G__MeasVinRawCounts` | counts |
| `UINT32_T__G__MeasV24RawCounts` | counts |
| `UINT32_T__G__MeasV12RawCounts` | counts |
| `UINT32_T__G__MeasVrefintRawCounts` | counts |
| `UINT32_T__G__MeasCurrent1Ma` | mA |
| `UINT32_T__G__MeasCurrent2Ma` | mA |
| `UINT32_T__G__MeasCurrent1RawCounts` | counts |
| `UINT32_T__G__MeasCurrent1ShuntUv` | uV |
| `UINT32_T__G__MeasCurrent1MaUnfiltered` | mA |
| `UINT32_T__G__MeasCurrent2RawCounts` | counts |
| `UINT32_T__G__MeasCurrent2ShuntUv` | uV |
| `UINT32_T__G__MeasCurrent2MaUnfiltered` | mA |
| `BOOL__G__MeasInputPresent` | — |
| `BOOL__G__MeasDataValid` | — |

## دستورها

1. فقط تسک Measurement می‌نویسد؛ بقیه فقط می‌خوانند و اول `BOOL__G__MeasDataValid`.
2. هر `*RawCounts` / `*ShuntUv` / `*MaUnfiltered` بدون فیلتر و بدون آفست می‌ماند.
3. اگر `CurrentIsBatteryCalibrated` مقدار `true` داد، مصرف‌کننده تبدیل دوم نمی‌کند.
4. واحدها عوض نمی‌شوند: mV، mA، uV، counts.
5. تغییر نام، حذف، یا تغییر واحد هر سطر این برگه = تغییر قرارداد؛ با اجازهٔ صاحب مرز. کار داخل `measurement.c` آزاد است.
