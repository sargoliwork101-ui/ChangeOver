/**
 * @file    README.md
 * @brief   [EN] Independent dual 12 V charger module sheet.
 *          [FA] برگهٔ ماژول دو شارژر مستقل ۱۲ ولت.
 */

# ماژول Charger

## وضعیت

کد کنترل برای دو شارژر مستقل ۱۲ ولت با یک implementation عمومی آماده شده است؛
`MODULE_CHARGER = 1` و `MODULE_JITTER = 1` برای پوشش build فعال‌اند، اما
`CHG_MASTER_ENABLE = 0` و `CHG_TRANSFORMER_KNOWN = 0` عمداً safe-off هستند تا
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
| 2026-09-18 | active-low بودن JIT از اتصال LM393 و cutoff باتری ۱۵٫۰V به هر دو مسیر کنترل اضافه شد؛ ترتیب capture duty قبل از Stop اصلاح شد |
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
حداکثر جریان bulk برابر `675mA` (`0.15C` برای 4.5Ah) است. cutoff سخت sense باتری
`2000..15000mV` است؛ حد بالای ۱۵٫۰V پیش از هر مسیر bring-up یا شارژ اعمال می‌شود.
این مقادیر بدون دیتاشیت قطعی ZICO provisional هستند و Equalization اجرا نمی‌شود.
برای اولین تست کوتاه و تحت‌نظارت، منبع ورودی باید بیرونی روی `50mA` محدود باشد و
حداکثر duty همان `2%` بماند؛ این محدودکنندهٔ ورودی معادل current-limit خروجی
firmware نیست. firmware در bring-up فعلاً جریان اندازه‌گیری‌شدهٔ خروجی را نیز
روی `50mA` محدود و در عبور از آن latch می‌کند. افزایش به `100mA` یا `10%` تا
تأیید شکل‌موج مجاز نیست.

## Debug / Watch — متغیرهای قابل مشاهده

این متغیرها فقط برای مشاهده هستند و از Live Expressions/Watch خوانده می‌شوند؛
نوشتن آن‌ها از debugger مجاز نیست. مقادیر `InputMv`، `BatteryMv` و `CurrentMa`
از همان snapshotی می‌آیند که Charger مصرف می‌کند. بازهٔ ADC-engineering برد با
کالیبراسیون فعلی تقریباً `Vin=0..36882mV`، `VLOW=0..19897mV` و
`Current2=0..3267mA` است؛ این بازه‌ها جای کالیبراسیون واقعی روی برد را نمی‌گیرند.

| متغیر | واحد/کد | مقدار مورد انتظار در bring-up | بازهٔ مجاز/تفسیر |
|---|---|---|---|
| `CHG_DEBUG__G__EvaluateCount` | count | در هر Evaluate یک واحد زیاد می‌شود | `0..UINT32_MAX`؛ بعد از reset از صفر |
| `CHG_DEBUG__G__InputMv` | mV | snapshot معتبر و هنگام start حداقل `23000` | `0..36882` برای ADC فعلی؛ زیر `22000` باید safe-off شود |
| `CHG_DEBUG__G__BatteryMv` | mV | برای CH2 بین `2000..15000` | `0..19897`؛ بیرون بازهٔ اعتبار → PWM صفر |
| `CHG_DEBUG__G__CurrentMa` | mA | در مرحلهٔ اول `0..50`؛ نزدیک limit باید duty hold شود | `0..3267` از ADC فعلی؛ بالای `50` در bring-up → final fault |
| `CHG_DEBUG__G__AppliedDutyPermille` | ‰ | فقط `0..20`؛ duty مشاهده‌شدهٔ `15` یعنی `1.5%` | `0..20` در bring-up؛ با master خاموش باید `0` |
| `CHG_DEBUG__G__LastRequestedDutyPermille` | ‰ | همان درخواست قبل از clamp | `0..20` در bring-up؛ در حالت عادی `0..1000` |
| `CHG_DEBUG__G__AppliedChannel` | کد کانال | `2` هنگام PWM2؛ `0` در سکون | فقط `0`, `1`, `2` |
| `CHG_DEBUG__G__Channel2State` | کد state | `4=BRINGUP` در حال تست، `6=INPUT_WAIT` در افت Vin، `7=FINAL_FAULT` در latch | فقط `0..7`: OFF, BULK, ABSORB, FLOAT, BRINGUP, JIT_RETRY_WAIT, INPUT_WAIT, FINAL_FAULT |
| `CHG_DEBUG__G__Channel2JitTrips` | count | صفر در تست سالم | `0..3`؛ تریپ سوم باید fault نهایی کند |
| `CHG_DEBUG__G__InputReady` | bool | `1` فقط بعد از Vin حداقل `23000` | فقط `0` یا `1`; زیر `22000` باید `0` شود |
| `CHG_DEBUG__G__InputLockout` | bool | `0` در شروع؛ پس از sag bring-up برابر `1` و باقی می‌ماند | فقط `0` یا `1`؛ با reset پاک می‌شود |
| `CHG_DEBUG__G__StopReason` | کد | صفر در کنترل عادی؛ `5` برای Vin پایین | `0..9`: NONE, MASTER_OFF, SNAPSHOT_INVALID, APP_SAFE_OR_FAULT, TRANSFORMER_GATE, INPUT_LOW, BATTERY_INVALID, CURRENT_LIMIT, JIT_TRIP, FINAL_FAULT |
| `CHG_DEBUG__G__ResetFlags` | RCC CSR bits | پس از reset علت را نگه می‌دارد و بعد از boot پاک می‌شود | raw `RCC->CSR`; صفر یا ترکیب بیت‌های reset؛ برای brownout/POR/WDT ثبت شود |

نکتهٔ تشخیصی: با `ARR=1439`، `AppliedDutyPermille=15` به حدود `21/1440` شمارش
compare و تقریباً `1.46%` duty تبدیل می‌شود. بنابراین ثابت‌ماندن نزدیک `1.5%`
به‌تنهایی خرابی تایمر نیست؛ باید هم‌زمان `InputMv`, `CurrentMa`, `Channel2State`,
`InputLockout` و `StopReason` ثبت شوند تا مشخص شود PWM واقعاً با `SafeIdle` قطع‌و‌وصل
می‌شود یا فقط در limit نگه داشته شده است.

## پایه‌ها

| کانال | PWM | جریان | JIT | ولتاژ کنترل |
|---|---|---|---|---|
| Trans1 / کانال ۱ | PA0 / TIM2_CH1 | PA1 / ADC_CURRENT1 | PB2 / JITTER1 | `VHIGH = V24-MID` |
| Trans2 / کانال ۲ | PA6 / TIM3_CH1 | PA7 / ADC_CURRENT2 | PB6 / JITTER2 | `VLOW = MID-GND` |

هر دو تایمر با clock `72MHz` و `PSC=0`, `ARR=1439` برای `50kHz` تنظیم شده‌اند.
طبق اتصال شماتیک، ورودی‌های `IN-` LM393 از `Shunt1_Filtered`/`Shunt2_Filtered`
می‌آیند و ورودی‌های `IN+` آستانهٔ مشترک دارند؛ بنابراین خروجی open-collector
`JITT1/JITT2` وقتی جریان از آستانه بالاتر می‌رود low می‌شود. PB2/PB6 در firmware
و CubeMX روی falling edge تنظیم شده‌اند و callback نیز low بودن پایه را دوباره
چک می‌کند. رلهٔ NC با coil خاموش وصل و با coil روشن باز است؛ عملکرد واقعی
کنتاکت باید با continuity روی برد تأیید شود.

## پیش‌فرض امن

- کانال غیرنصب‌شده همیشه PWM صفر و stopped است.
- نبود snapshot معتبر، حالت FAULT/SAFE، نبود ورودی ADC حداقل `22000mV`، ناشناخته‌بودن
  ترانس یا نبود کانال نصب‌شده خروجی را safe-off می‌کند؛ پرچم runtime پنهان جای این
  حفاظت‌ها نیست.
- sense باتری هر کانال باید در بازهٔ `2000..15000mV` باشد؛ زیر حد یعنی battery-missing
  و بالای حد یعنی over-voltage، و هر دو PWM همان کانال صفر می‌شوند.
- جریان کم fault نیست: وقتی ولتاژ زیر target است، duty همان کانال مرحله‌ای زیاد
  می‌شود؛ فقط جریان بیش از `675mA` وارد حفاظت جریان می‌شود.
- JIT active-low است: duty همان کانال قبل از Stop ثبت، فقط PWM همان کانال صفر،
  coil در retryهای اول خاموش/NC بسته می‌ماند، lockout و retry با نصف duty انجام
  می‌شود؛ retry دوم حداکثر ۱۰٪ است و تریپ سوم fault نهایی با هر دو PWM صفر و relay باز است.
- پیش از retry، comparator همان کانال clear می‌شود و سپس فقط PWM کانال مجاز اعمال می‌شود.

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
host یا syntax به‌تنهایی مجوز اتصال باتری، اثبات waveform واقعی MCU یا تأیید
LM393/رله نیست. با تنظیمات تحویلی (`CHG_MASTER_ENABLE=0` و bring-up خاموش) هیچ
سوئیچینگ مجاز نیست. اگر بعداً پس از بازبینی firmware و build، تست سخت‌افزاری
صریحاً فعال شد، فقط یک تست کوتاه و تحت‌نظارت با همان باتری `12V/4.5Ah`، منبع
ورودی محدودشده به `50mA`، حداکثر duty `2%`، اسیلوسکوپ و پایش continuity رله
قابل بررسی است؛ این تست شارژ کامل یا unattended نیست و current-limit ورودی
جای current-limit خروجی firmware را نمی‌گیرد. اگر همین منبع ۵۰mA هم‌زمان تغذیهٔ
MCU و power stage را تأمین کند، افت تغذیه و reset شدن MCU با firmware قابل
تضمین‌کردن نیست؛ bring-up در افت Vin latch می‌شود و علت reset از
`CHG_DEBUG__G__ResetFlags` بررسی می‌شود.
