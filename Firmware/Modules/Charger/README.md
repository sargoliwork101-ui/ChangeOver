/**
 * @file    README.md
 * @brief   [EN] Independent dual 12 V charger module sheet.
 *          [FA] برگهٔ ماژول دو شارژر مستقل ۱۲ ولت.
 */

# ماژول Charger

## وضعیت

کد کنترل برای دو شارژر مستقل ۱۲ ولت با یک implementation عمومی آماده شده است؛
`MODULE_CHARGER = 1`، `CHG_MASTER_ENABLE = 1` و `CHG_TRANSFORMER_KNOWN = 1`
(تصمیم کاربر ۲۰۲۶-۰۹-۲۲: روشن‌بودن شارژر) و هر دو کانال نصب‌اند
(`CHG_CHANNEL_1_INSTALLED = 1` و `CHG_CHANNEL_2_INSTALLED = 1`)؛
همهٔ وضعیت و زمان‌بندی پر-کانال مستقل است
(دیوتی، جریان، شستشو/تیپر، retry، تمبر ثبات). موارد فیزیکی مشترک: رلهٔ NC
(خطای نهایی هر کانال، ورودی هر دو را قطع می‌کند) و صف احیای JIT که تک‌تک احیا
می‌کند. برای تغییر مونتاژ فقط همین ثابت‌ها در `charger.h` عوض شوند.

**مود تست دستی سراسری (v1.2، دستور کاربر ۲۰۲۶-۰۹-۲۳):** پارامتر ۱۹ از پنل ESP
(پروتکل v1.2، `ESP_AGENT_SPEC.md` بخش 19) هر دو کانال را به مود دستی می‌برد:
ماشین حالت، رمپ، پنجرهٔ اعتبار باتری، BAT_LOST و توقف ۱۴٫۴V تعلیق می‌شوند و duty
هر کانال مستقیماً از پارامتر ۱۶/۱۸ اعمال می‌شود (`func__Charger_ManualDriveChannel`).
کف سخت‌افزاری می‌ماند: حضور ورودی ۲۴V، قطع سخت‌افزاری JIT (شمارش تریپ جمع می‌شود؛
سومین تریپ مثل مود خودکار FINAL_FAULT می‌قفلد و re-arm فقط با ری‌فرست duty از
پنل است، بدون ریتری خودکار)، قطع ۱۵٫۰V هر کانال (`CHG_MAX_VALID_BATTERY_MV`،
برگشت خودکار زیرش)، سقف duty و قطع کانال ESP (پارامتر ۱۱/۱۲). dead-man لینک
(`CHG_MANUAL_WATCHDOG_MS = 3000ms`) داخل تسک شارژر می‌گذرد: ۳ ثانیه سکوت ESP
هر دو duty را صفر و شارژر را خودکار برمی‌گرداند؛ ورود/خروج فقط در زمینهٔ تسک
شارژر انجام می‌شود. حین مود دستی، `func__Fault_Evaluate` کامل فریز است.

**مسیر جریان (دستور کاربر؛ LUT از v1.13):** Measurement نمونهٔ سنکرونِ وسط پنجرهٔ ON
پالس PWM را اول از زنجیرهٔ فیلتر کلیددار روی شمارش خام عبور می‌دهد — مدین-۳ و بعد
میانگین متحرک ۱۰ نمونه (کلیدهای کامپایل `MEASUREMENT_CURRENT_MEDIAN3_ENABLE` /
`MEASUREMENT_CURRENT_AVERAGE_ENABLE` در `measurement.h`؛ ثابت در ماژول
Measurement) — بعد با جدول جریان بنچ (روی محور شمارش) به mA باتری می‌رسد. شارژر خودش هیچ فیلتری اضافه نمی‌کند: همان `iest` را به
باند ۶۳۰..۶۵۰mA و تیپر می‌دهد (EMA جریان حذف ماند؛ rate-limit
پله‌های duty و JIT سخت‌افزاری جلوی hunting را می‌گیرند).

شارژر در این پروژه **شارژر ۲۴ ولت نیست**. کانال پایین از `VLOW = MID-GND` و
کانال بالا از `VHIGH = V24-MID` استفاده می‌کند. در تست فعلی Trans2 فقط `VLOW`
را می‌خواند و `v_bat24_mv` را setpoint یا شرط battery-missing قرار نمی‌دهد.

## قطع باتری (فقط آینهٔ پرچم)

از ۲۰۲۶-۰۹-۱۹ مالکیت تشخیص قطع باتری با ماژول **Fault** است
(`FAULT_BAT_DISCONNECT_MV` 14.8V/150ms = ۱۵ پاس پشت‌سر برای امضای پمپ حین شارژ و
`FAULT_BAT_ABSENT_MV` 6V/1s برای نبود باتری با ورودی سالم؛ بازیابی `FAULT_BATTERY_BACK_MV` 7V/1s — فاصلهٔ ۱V هیسترزیس (جداسازی آستانه به دستور کاربر ۲۰۲۶-۰۹-۲۲).
شارژر در بخش میانی `Evaluate` (پیش از گیت‌های اصلی) فقط بیت `FAULT_CHARGER_BAT_LOST` را می‌خواند و آن را به
`CHG_STATE_BAT_LOST` آینه می‌کند: با قفل‌شدن بیت یک‌بار PWM قطع می‌شود و با
پاک‌شدنش کانال به OFF آزاد می‌شود تا BULK با رمپ نرم از ۱٪ شروع شود. هیچ
آستانه یا تایمری دیگر در این ماژول برای قطع باتری نیست. UI هم همان بیت را با
سناریوی BatLost نشان می‌دهد.

## سناریو ۶ — زمان شارژ باتری خراب / Dead-battery charge timer

[EN] The timer accumulates only while a channel is in `BULK` or `ABSORB`. A
completed `FLOAT` cycle clears it; a shorter stop preserves it, while any
non-charging pause that reaches parameter 126 (default `60000 ms`) clears it.
An invalid input puts the channel in `INPUT_WAIT`: the final active interval is
accounted for before `SafeIdle`, and the pause clock is serviced on every
waiting pass. This keeps a long input outage from being counted as continuous
charging toward the 24-hour dead-battery verdict.

[FA] شمارنده فقط در `BULK` و `ABSORB` جمع می‌شود. با رسیدن به `FLOAT` صفر
می‌شود؛ مکث کوتاه مقدار را نگه می‌دارد، اما هر توقف شارژی که به زمان شناسهٔ
۱۲۶ (پیش‌فرض `۶۰۰۰۰ ms`) برسد، شمارنده را صفر می‌کند. ورودی نامعتبر کانال را
به `INPUT_WAIT` می‌برد: زمان آخرین دور فعال پیش از `SafeIdle` ثبت می‌شود و
ساعت مکث در هر دور انتظار بررسی می‌شود. بنابراین قطع طولانی ورودی به‌اشتباه
شارژ پیوستهٔ ۲۴ ساعته برای تشخیص باتری خراب محسوب نمی‌شود.

ریست تأییدشدهٔ پنل با صفرکردن اسلات زمان‌اجرای ۲۰۳، ماسک قفل و دفترچهٔ زمان
زندهٔ همان کانال را با هم پاک می‌کند؛ این پاک‌سازی زیر قفل زمان‌بند انجام
می‌شود تا ساعت قدیمی بعد از آزادشدن دوباره قفل را فوری فعال نکند. اگر باتری
هنوز خراب باشد، شمارش از صفر آغاز می‌شود. تشخیص تعویض فیزیکی باتری همچنان مسیر
خودکار مستقلی است.

## سناریو ۷ — خطای فنی برد / Technical board fault

این سناریو مخصوص خرابی طبقهٔ قدرت و به‌ویژه سوختن ترانزیستور شارژر است. تشخیص
در `func__Charger_EvaluateTechnicalFault()` انجام می‌شود و به‌ازای هر کانال
روی duty واقعی اعمال‌شده و جریان همان کانال تصمیم می‌گیرد:

| امضای خرابی | شرط دقیق |
|---|---|
| اتصال‌کوتاه/سوختن ترانزیستور | رلهٔ شارژر باز (`func__Charger_IsRelayOpen()`)، duty واقعی برابر `0‰` و JIT همان کانال ثبت شده باشد |
| قطع‌شدن/سوختن ترانزیستور | duty واقعی **بیشتر از `200‰`** (`>20%`) و جریان اندازه‌گیری‌شدهٔ همان کانال دقیقاً `0mA` و ولتاژ باتری همان کانال `≥ 6000 mV` (و کانال `BAT_LOST` نباشد) |

با اولین تطبیق، `FAULT_CHARGER_TECHNICAL` (بیت ۷) در ماژول Fault ست و
`BOOL__G__TechnicalFaultLockout` در Charger قفل می‌شود. از آن لحظه:

- `func__Charger_FinalDisconnect()` در هر پاس هر دو PWM را صفر و رله را باز نگه می‌دارد؛
- تغییر تنظیمات پنل، retry JIT، مود دستی، NVM یا تغییر `app_state` قفل را آزاد نمی‌کند؛
- فقط `func__Charger_Init()` در بوت بعدی، یعنی reset یا power-cycle، قفل RAM را پاک می‌کند؛
- ارزیابی بعد از `func__Jitter_Run()` و پیش از ادامهٔ Changeover/Charger انجام می‌شود تا
  همان پاس وارد مسیر خطا شود.

این fault در NVM ذخیره نمی‌شود؛ بیت Fault نیز با `func__Fault_Init()` در reset
پاک می‌شود، بنابراین بعد از راه‌اندازی مجدد شرایط اندازه‌گیری دوباره بررسی می‌شوند.

## فایل‌ها

| فایل | نقش |
|---|---|
| `charger.h` / `charger.c` | policy عمومی روی state مستقل هر کانال؛ تعداد کانال‌ها تنها در `CHG_CHANNEL_COUNT` (در `charger.h`) تعریف شده و همهٔ حلقه‌ها، بررسی‌های مرزی و ابعاد آرایه‌ها از همان یک ثابت می‌آیند (پاک‌سازی ۲۰۲۶-۱۰-۰۶) |
| `../../Config/Inc/app_types.h` | `VLOW` و `VHIGH` مستقل در snapshot |
| `../../Modules/Measurement/measurement.c` | محاسبهٔ `VLOW = MID-GND` و `VHIGH = V24-MID` |
| `../../Bsp/Src/bsp_pwm.c` | اعمال duty به PWM منطقی انتخاب‌شده |
| `../../Bsp/Src/bsp_gpio.c` | منطق coil رله و polarity برد |
| `Tester/` | پوشهٔ تست ماژول — `host_test_charger.py` (تست host سیاست و قراردادهای source) + `pid_tuning_sim.py` (شبیه‌ساز شارژ) + `README.md` توضیحات |

## توابع

| نام | کار |
|---|---|
| `func__Charger_Init` | صفرکردن هر دو PWM، بازکردن NC و صفرکردن stateهای مستقل |
| `func__Charger_Evaluate` | اجرای یک policy عمومی برای هر کانال نصب‌شده با duty جدا |
| `func__Charger_RegulateChannel` | Bulk/Absorb/Float، current limit و افزایش duty در جریان کم برای یک کانال |
| `func__Charger_EvaluateTechnicalFault` | ارزیابی دو امضای خرابی ترانزیستور سناریوی ۷ از snapshot/JIT و ست‌کردن قفل فنی |
| `func__Charger_GetAppliedDutyPermille` / `func__Charger_IsRelayOpen` | خواندن duty واقعی و وضعیت رله برای تشخیص سخت‌افزار، نه duty درخواستی PID |
| `func__Charger_IsTechnicalFaultLocked` | خواندن وضعیت قفل فنی سناریوی ۷ برای diagnostic |
| `func__Charger_HandleJitTrip` | PWM صفر، relay باز، lockout و برنامه‌ریزی retry کانال تریپ‌کرده (در مود دستی: پارک بدون ریتری؛ شمارش تریپ جمع می‌شود) |
| `func__Charger_ServiceRetry` | relay خاموش/NC بسته، settle و سپس PWM فقط کانال retry (فقط مود خودکار) |
| `func__Jitter_ClearChannel` | arm مجدد ورودی JIT همان کانال بعد از safe sequence |
| `func__Charger_SetManualTestMode` | درخواست ورود/خروج مود تست دستی (مصرف در تسک شارژر) |
| `func__Charger_IsManualTestModeActive` | مود دستی واقعاً فعال است؟ (گیت dispatch و فریز Fault) |
| `func__Charger_SetDutyFixedPermille` | ذخیرهٔ duty فرمان (پارامتر ۱۶/۱۸)؛ در مود دستی نوشتن تازه = سیگنال re-arm کانال پارک‌شدهٔ JIT |
| `func__Charger_SetChannelEspEnable` / `GetChannelEspEnable` | قطع/وصل کانال از پنل (پارامتر ۱۱/۱۲)؛ قطع = duty صفر |

ثابت‌های اولیهٔ هر کانال: `14400mV` جذب، `13500mV` شناور، `12800mV` reentry و
سقف جریان bulk به‌صورت پیش‌فرض `650mA` است (`CHG_BULK_CURRENT_MAX_MA`؛ باند
تنظیم ۶۳۰..۶۵۰mA با کف = سقف − ۲۰، از پنل تا `900mA` قابل افزایش). cutoff سخت sense باتری
`2000..15000mV` است؛ حد بالای ۱۵٫۰V پیش از هر مسیر bring-up یا شارژ اعمال می‌شود.
این مقادیر بدون دیتاشیت قطعی ZICO provisional هستند و Equalization اجرا نمی‌شود.
برای اولین تست کوتاه و تحت‌نظارت، منبع ورودی باید بیرونی روی `50mA` محدود باشد و
حداکثر duty همان `2%` بماند؛ این محدودکنندهٔ ورودی معادل current-limit خروجی
firmware نیست. افزایش به `100mA` یا `10%` تا تأیید شکل‌موج مجاز نیست.

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
- نبود snapshot معتبر، حالت FAULT/SAFE، نبود ورودی ADC در بازه `22000..30000mV` (حد پایین 22V و بالا 30V، دستور کاربر 2026-10-12)، ناشناخته‌بودن
  ترانس یا نبود کانال نصب‌شده خروجی را safe-off می‌کند؛ پرچم runtime پنهان جای این
  حفاظت‌ها نیست.
- استراحت حرارتی (v1.88، دستور کاربر 2026-10-12): هر کانال اگر 1 ساعت مداوم در BULK با جریان ماکس (≥ bulkMax-50mA) کار کند، 10 دقیقه به حالت THERMAL_REST می‌رود (PWM صفر، رله بسته تا مصرف کم بماند) و سپس از OFF با رمپ نرم ادامه می‌دهد. SafeIdle این لچ را پاک نمی‌کند. فعلا سنسور دما نداریم، این استراحت جایگزین است.
- sense باتری هر کانال باید در بازهٔ `2000..15000mV` باشد؛ زیر حد یعنی battery-missing
  و بالای حد یعنی over-voltage، و هر دو PWM همان کانال صفر می‌شوند.
- جریان کم fault نیست: وقتی ولتاژ زیر target است، duty همان کانال مرحله‌ای زیاد
  می‌شود؛ فقط جریان بیش از سقف سختِ تنظیم‌شده وارد حفاظت جریان می‌شود —
  پیش‌فرض کارخانه `950mA` (`CHG_CURRENT_HARD_FAULT_DEFAULT_MA`) و سقف تنظیم
  `3000mA` (`CHG_CURRENT_HARD_FAULT_MAX_MA`).
- JIT active-low است: duty همان کانال قبل از Stop ثبت، فقط PWM همان کانال صفر،
  coil در retryهای اول خاموش/NC بسته می‌ماند، lockout و retry با نصف duty انجام
  می‌شود؛ retry دوم حداکثر ۱۰٪ است و تریپ سوم fault نهایی با هر دو PWM صفر و relay باز است.
- پیش از retry، comparator همان کانال clear می‌شود و سپس فقط PWM کانال مجاز اعمال می‌شود.
- مود تست دستی (v1.2): ورود فقط با پارامتر ۱۹ از پنل و بعد از تأیید کاربر؛ خروج
  با پارامتر ۱۹، ۳ ثانیه سکوت لینک (dead-man) یا قطع سخت‌افزاری نهایی. در این مود
  کف سخت‌افزاری (ورودی، JIT، ۱۵٫۰V، سقف duty، قطع کانال) فعال می‌ماند و
  `Fault_Evaluate` فریز است؛ بستن پنل ESP هم مود را (حداکثر ۱۰ ثانیه بعد) پایان می‌دهد.

## درخت اتصال

```text
rtos_app.c → TaskControl → task_control.c
  func__Charger_Init() قبل از اولین Evaluate
  func__Jitter_Run() در هر دوره، وقتی JITTER فعال باشد
  func__Charger_EvaluateTechnicalFault(&snapshot)  ← سناریوی ۷، بعد از JIT
  func__Charger_Evaluate(&snapshot, app_state)
      ├─ اگر technical lockout: هر دو PWM=0 و relay open
      ├─ snapshot.v_bat_low_mv  ← MID-GND   (Trans2)
      ├─ snapshot.v_bat_high_mv ← V24-MID   (Trans1)
      ├─ snapshot.i_ch1_ma / i_ch2_ma ← ADC+DMA + LM358 filter
      └─ bsp_pwm / bsp_gpio
```

`Tester/host_test_charger.py` فقط policy و source contract را بررسی می‌کند (۵۱ تست، شامل تشخیص و lockout سناریوی ۷، قرارداد مود دستی و پین‌های LUT). PASS شدن
host یا syntax به‌تنهایی مجوز اتصال باتری، اثبات waveform واقعی MCU یا تأیید
LM393/رله نیست. شارژر اکنون با `CHG_MASTER_ENABLE=1` فعال است (تصمیم کاربر
۲۰۲۶-۰۹-۲۲)؛ تست‌های سخت‌افزاری جدید باید با منبع ورودی محدودشده، پایش جریان و
اسیلوسکوپ و تحت نظارت انسان انجام شوند، و تست مود دستی فقط با بار مقاومتی یا
electronic load با voltage clamp (مقاومت آزاد شبیه‌ساز باتری نیست —
`CHG_MIN_VALID_BATTERY_MV` در `charger.h`).
current-limit ورودی جای current-limit خروجی firmware را نمی‌گیرد.

## ماشین حالت

`Charger_State_Machine.xlsx` — حالت‌ها و گذارهای همین ماژول، رنگی و
راست‌به‌چپ با فونت وزیرمتن. از برگهٔ «ماشین حالت» در فایل اعتبارسنجی همین
پوشه هم به آن لینک هست و خودش به نمای سیستمی
(`Documentation/System_State_Machine.xlsx`) برمی‌گردد. بازتولید:
`python3 tools/make_module_state_machines.py`.

## تاریخچه

تاریخچهٔ تغییرات در `git log` همین مخزن نگهداری می‌شود.
