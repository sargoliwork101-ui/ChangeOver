/**
 * @file    README.md
 * @brief   [EN] Independent dual 12 V charger module sheet.
 *          [FA] برگهٔ ماژول دو شارژر مستقل ۱۲ ولت.
 */

# ماژول Charger

## وضعیت

کد کنترل برای دو شارژر مستقل ۱۲ ولت با یک implementation عمومی آماده شده است؛
اما `MODULE_CHARGER = 0` و `CHG_TRANSFORMER_KNOWN = 0` عمداً safe-off هستند تا
پارامتر ترانس، LM393/JIT، offset/gain جریان و کنتاکت NC روی برد تأیید شوند.
تست فعلی فقط Trans2/باتری پایین است و با `CHG_CHANNEL_1_INSTALLED = 0` و
`CHG_CHANNEL_2_INSTALLED = 1` انتخاب می‌شود. برای تغییر مونتاژ فقط همین دو ثابت
در `charger.h` عوض شوند؛ توابع کپی نمی‌شوند.

شارژر در این پروژه **شارژر ۲۴ ولت نیست**. کانال پایین از `VLOW = MID-GND` و
کانال بالا از `VHIGH = V24-MID` استفاده می‌کند. در تست فعلی Trans2 فقط `VLOW`
را می‌خواند و `v_bat24_mv` را setpoint یا شرط battery-missing قرار نمی‌دهد.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-18 | دو ثابت انتخاب کانال، state/duty/protection مستقل و رفتار افزایش duty در جریان کم اضافه شد |
| 2026-09-18 | تست تک‌باتری بدون بررسی پک ۲۴ ولت، توالی relay/JIT و PWM پنجاه کیلوهرتز ثبت شد |
| 2026-09-14 | اسکلت اولیهٔ Charger اضافه شد |

## فایل‌ها

| فایل | نقش |
|---|---|
| `charger.h` / `charger.c` | policy عمومی روی state مستقل هر کانال |
| `../../Config/Inc/app_types.h` | `VLOW` و `VHIGH` مستقل در snapshot |
| `../../Modules/Measurement/measurement.c` | محاسبهٔ `VLOW = MID-GND` و `VHIGH = V24-MID` |
| `../../Bsp/Src/bsp_pwm.c` | اعمال duty به PWM منطقی انتخاب‌شده |
| `../../Bsp/Src/bsp_gpio.c` | منطق coil رله و polarity برد |
| `host_test_charger.py` | تست host سیاست و قراردادهای source |

## توابع

| نام | کار |
|---|---|
| `func__Charger_Init` | صفرکردن هر دو PWM، بازکردن NC و صفرکردن stateهای مستقل |
| `func__Charger_Evaluate` | اجرای یک policy عمومی برای هر کانال نصب‌شده با duty جدا |
| `func__Charger_RegulateChannel` | Bulk/Absorb/Float، current limit و افزایش duty در جریان کم برای یک کانال |
| `func__Charger_HandleJitTrip` | PWM صفر، relay باز، lockout و برنامه‌ریزی retry کانال تریپ‌کرده |
| `func__Charger_ServiceRetry` | relay خاموش/NC بسته، settle و سپس PWM فقط کانال retry |
| `func__Jitter_ClearChannel` | arm مجدد ورودی JIT همان کانال بعد از safe sequence |

ثابت‌های اولیهٔ هر کانال: `14400mV` جذب، `13500mV` شناور، `12800mV` reentry و
حداکثر جریان bulk برابر `675mA` (`0.15C` برای 4.5Ah) است؛ منبع current-limited
در اولین تست برد باید بیرونی روی حدود `50–100mA` محدود شود (`CHG_FIRST_BOARD_TEST_MAX_MA`).
این مقادیر بدون دیتاشیت
قطعی ZICO provisional هستند و Equalization اجرا نمی‌شود.

## پایه‌ها

| کانال | PWM | جریان | JIT | ولتاژ کنترل |
|---|---|---|---|---|
| Trans1 / کانال ۱ | PA0 / TIM2_CH1 | PA1 / ADC_CURRENT1 | PB2 / JITTER1 | `VHIGH = V24-MID` |
| Trans2 / کانال ۲ | PA6 / TIM3_CH1 | PA7 / ADC_CURRENT2 | PB6 / JITTER2 | `VLOW = MID-GND` |

هر دو تایمر با clock `72MHz` و `PSC=0`, `ARR=1439` برای `50kHz` تنظیم شده‌اند.
رلهٔ NC با coil خاموش وصل و با coil روشن باز است؛ عملکرد واقعی کنتاکت باید با
continuity روی برد تأیید شود. polarity لبهٔ LM393 هنوز با اسیلوسکوپ تأیید نشده
و rising/falling نباید صرفاً از نام سیگنال انتخاب شود.

## پیش‌فرض امن

- کانال غیرنصب‌شده همیشه PWM صفر و stopped است.
- نبود snapshot معتبر، حالت FAULT/SAFE، نبود ورودی، ناشناخته‌بودن ترانس یا
  `power_stage_enabled = false` خروجی را safe-off می‌کند.
- جریان کم fault نیست: وقتی ولتاژ زیر target است، duty همان کانال مرحله‌ای زیاد
  می‌شود؛ فقط جریان بیش از `675mA` وارد حفاظت جریان می‌شود.
- JIT: duty فعلی ثبت، هر دو PWM صفر، coil روشن برای بازکردن NC، lockout، retry
  با نصف duty، retry بعدی با ۱۰٪ یا کمتر و تریپ بعدی fault نهایی.
- پیش از retry، coil خاموش می‌شود، NC بسته و settle می‌شود و سپس فقط PWM کانال
  مجاز اعمال می‌شود.

## درخت اتصال

```text
rtos_app.c → TaskControl → task_control.c
  func__Charger_Init() قبل از اولین Evaluate
  func__Jitter_Run() در هر دوره، وقتی JITTER فعال باشد
  func__Charger_Evaluate(&snapshot, app_state)
      ├─ snapshot.v_bat_low_mv  ← MID-GND   (Trans2)
      ├─ snapshot.v_bat_high_mv ← V24-MID   (Trans1)
      ├─ snapshot.i_ch1_ma / i_ch2_ma ← ADC+DMA + LM358 filter
      └─ bsp_pwm / bsp_gpio
```

`host_test_charger.py` فقط policy و source contract را بررسی می‌کند. PASS شدن
host یا syntax به‌تنهایی مجوز اتصال باتری نیست. تست برد باید ابتدا با منبع
current-limited، electronic load یا battery simulator دارای voltage clamp،
اسیلوسکوپ و پایش continuity رله انجام شود؛ باتری واقعی فعلاً ممنوع است.
