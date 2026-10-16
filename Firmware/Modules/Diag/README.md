/**
 * @file    README.md
 * @brief   [EN] Diag module sheet: persistent diagnostics log and its u32 record.
 *          [FA] برگه ماژول Diag: لاگ تشخیصی ماندگار و رکورد u32 آن.
 */

# ماژول Diag - لاگ تشخیصی برد (۱۱ مورد تأییدشده)

## وضعیت

ماژول فعال است: `MODULE_DIAG = 1` در `modules_enable.h` (با guard `#ifndef` برای تست هاست). رکورد دیاگ روی دو صفحهٔ فلش به‌صورت پینگ‌پنگ نگهداری می‌شود و پنل ساده آن را با چانک‌های `DIAG_DATA` می‌خواند.

## هدف

برد باید بتواند آمار کار و خطاها را ماندگار نگه دارد تا پنل ساده سه‌صفحه‌ای نمایش دهد:

1. زمان کل شارژ هر کانال + مجموع (ثانیه)
2. تعداد شارژ کامل هر کانال + مجموع
3. آپتایم کل + از آخرین ریست (ثانیه)
4. تعداد ریست
5. ماکس/مین ولتاژها: Vin/Vpack/Vlow/Vhigh و اختلاف ماکس
6. جریان ماکس هر کانال (mA)
7. ۱۰ خطای آخر با ساعت - مثل دیاگ ماشین
8. لاگ ۲۰ قطع/وصل برق شهر با مدت
9. شمارنده‌ها: عدم تعادل، باتری کم، قطع باتری (`FAULT_CHARGER_BAT_LOST`)، فنی (`FAULT_CHARGER_TECHNICAL`)، `FAULT_ADC`
10. زمان همگام‌سازی ساعت (boardTimeBase)
11. نسخه و UID (magic + version + seq + crc)

## ذخیره

دو صفحه فلش جدا `0x0800F800` و `0x0800FC00` (هر کدام 1KB) به صورت پینگ‌پنگ با CRC32 IEEE.

ساختار `diag_record_t` حدود 676 بایت:

- هدر: magic='DIAG' (0x44494147), version=1, seq
- شمارنده‌های اصلی: totalChargeSec, chargeSecCh1/Ch2, chargeCount, chargeCountCh1/Ch2, uptimeSec, resetCount, boardTimeBase
- کمینه/بیشینه: max/min Vin/Vpack/Vlow/Vhigh, maxImb, maxIch1/Ich2
- شمارنده رویداد: imbalanceCount, lowBatteryCount, batLostCount (`FAULT_CHARGER_BAT_LOST`), techFaultCount (`FAULT_CHARGER_TECHNICAL`), ovCount (`FAULT_ADC`؛ نام فیلد قدیمی است و به‌خاطر چیدمان رکورد فلش مانده)
- ۱۰ خطا: هر کدام timestamp/faultMask/vIn/vPack/vLow/vHigh/state1/state2 (32 بایت)
- ۲۰ رویداد برق: هر کدام offTime/onTime/durationSec (12 بایت)
- CRC32

ذخیره دیبانس 60 ثانیه بعد از تغییر، یا فوری بعد از رویداد مهم (خطا، قطع برق، شارژ کامل) اگر لازم باشد.
از اصلاح ممیزی 2026-10-15، خودِ نوشتن فلش در `func__Diag_SaveNow` با شارژرِ معلق و هر دو گیت PWM متوقف انجام می‌شود: پاک‌کردن صفحه روی این قطعهٔ تک‌بانک کل CPU را حدود ۲۰ تا ۴۰ میلی‌ثانیه نگه می‌دارد و ذخیره‌های NVM و CalLut همین حالا هم سوییچینگ داخل آن پنجرهٔ کور را ممنوع کرده‌اند. چون `SaveNow` در تسک کنترل (مالک ارزیابی شارژر) اجرا می‌شود، توقف بی‌درنگ است و آینه‌های duty دست‌نخورده می‌مانند تا پاس بعدی تنظیم را یکپارچه ادامه دهد.
در `func__Diag_Init` هر دو صفحه خوانده و CRC چک می‌شود، جدیدترین seq سالم انتخاب می‌شود. اگر هیچ‌کدام سالم نبود، پیش‌فرض با min=0xFFFFFFFF ساخته می‌شود و resetCount=1.

از اصلاح RAM/استک 2026-10-15، خواندن و داوری صفحه‌ها در `Init` و `SaveNow` فقط با بافر کمکی `Stage` انجام می‌شود و بین دو صفحه فقط شماره‌های seq به‌صورت اسکالر می‌مانند؛ هیچ نسخهٔ رکوردی روی استک صدازننده یا در استاتیک اضافه نمی‌نشیند (قبلاً قاب `SaveNow` حدود ۲KB از استک ۱۰۲۴ بایتی تسک کنترل می‌خواست). سنجش هاست با `tools/measure_ram.py`: عمق بدترین‌حالت تسک کنترل 384/1024 بایت و فضای آزاد RAM برابر 1544 بایت (برآورد clang).

## ساعت

برد RTC ندارد. ساعت از گوشی (ادمین پنل ساده) با پیام `TIME_SYNC` می‌آید (یونیکس ثانیه LE).
پایه زمان = ساعت گوشی − آپتایم. ساعت فعلی = پایه + آپتایم.
اگر همگام نشده باشد، BoardTime=0 و آپتایم به عنوان زمان نسبی استفاده می‌شود.
`BoardTime` برای لاگ خطاها و قطع برق استفاده می‌شود تا کاربر تاریخ ببیند (پنل ساده آن را شمسی نشان می‌دهد).

## فایل‌ها

| فایل | نقش |
|---|---|
| `diag.h` / `diag.c` | رکورد دیاگ، دو صفحهٔ پینگ‌پنگ، API لاگ |
| `Tester/` | تست هاست این ماژول (اگر افزوده شود) |
| `Diag_State_Machine.xlsx` | کتاب ماشین حالت همین ماژول |
| `Diag_Validation.xlsx` | برگهٔ اعتبارسنجی و برنامهٔ تست |

## توابع

| نام | کار |
|---|---|
| `func__Diag_Init` | بوت، بعد از `func__CalLut_Init` در `func__App_Init` صدا زده می‌شود |
| `func__Diag_Tick` | هر 1 ثانیه از `TaskControl` (آپتایم + debounce save) |
| `func__Diag_SetBoardTime` | از ESP-Link هنگام TIME_SYNC |
| `func__Diag_GetBoardTime` | ساعت فعلی = base + uptime |
| `func__Diag_AddChargeSec` | از TaskControl هر ثانیه اگر کانال فعال است |
| `func__Diag_RecordChargeComplete` | از Charger هنگام ورود به FLOAT |
| `func__Diag_RecordFault` | از Fault/Protection/Charger هنگام ست‌شدن بیت جدید، با snapshot کامل |
| `func__Diag_RecordPowerOff` / `func__Diag_RecordPowerOn` | از TaskControl با تشخیص لبه input_present |
| `func__Diag_UpdateMinMax` | از TaskControl هر 10ms |
| `func__Diag_GetRecord` | برای EspLink جهت ارسال چانک‌ها |
| `func__Diag_SaveNow` | ذخیره فوری قبل از ریست |

## پایه‌ها

این ماژول پایهٔ سخت‌افزاری مستقیم ندارد؛ فقط از طریق `bsp_flash` (دراپ‌های فلش داخلی) و لینک ESP کار می‌کند. هیچ پایهٔ GPIO در کد دیاگ خوانده یا نوشته نمی‌شود و هدر عمومی‌اش مستقل از HAL است.

## پیش‌فرض امن

اگر هیچ صفحهٔ سالمی خوانده نشود، رکورد پیش‌فرض ساخته می‌شود: `min = 0xFFFFFFFF`، شمارنده‌ها صفر، `resetCount = 1`. نوشتن فلش فقط در `func__Diag_SaveNow` و با CRC انجام می‌شود؛ تا وقتی نوشتن موفق نشود پرچم `Dirty` باقی می‌ماند و دادهٔ RAM از دست نمی‌رود.

## اینتگریشن برد

- `app.c`: include `diag.h` و `func__Diag_Init` تحت `#if MODULE_DIAG`
- `task_control.c`: include `diag.h`، سه static tick و PrevInput، هر پاس `func__Diag_UpdateMinMax`، تشخیص لبه برای PowerOff/On، هر 100 تیک (1 ثانیه) `func__Diag_AddChargeSec` هر کانال فعال + `func__Diag_Tick`
- `fault.c`: بعد از هر `func__Fault_Set(FAULT_CHARGER_BAT_LOST)` صدا زدن `func__Diag_RecordFault` با snapshot
- `charger.c`: هنگام اولین ورود به FLOAT از ABSORB، `func__Diag_RecordChargeComplete(chIdx)` + `func__Diag_RecordFault(TECHNICAL)` در تشخیص فنی
- `protection.c`: `func__Diag_RecordFault(ADC)` اگر snapshot موجود باشد
- `esp_link.c`: `SendDiagChunk(0..4)` و `SendDiagAll` و هندلر `TIME_SYNC` (4 بایت LE → `func__Diag_SetBoardTime`) و `DIAG_REQ` (ارسال 5 چانک)
- `modules_enable.h`: `MODULE_DIAG=1` با guard `#ifndef` برای تست هاست
- `CubeIDE/.cproject`: افزودن include path Diag

## ارتباط با ESP (پنل ساده)

پیام‌های جدید در `esp_link.h`:

- `0x30 TIME_SYNC` ESP→STM: 4 بایت LE unix (ثانیه)
- `0x31 DIAG_REQ` ESP→STM: بدون payload، درخواست لاگ کامل
- `0x32 DIAG_DATA` STM→ESP: چانک‌های دیاگ
  - chunk0: totalChargeSec (u32), chargeCount (u32), uptimeSec (u32), resetCount (u32), boardTime (u32 = base+uptime), errCount (u8), powerCount (u8)
  - chunk1: errors 0..4 (start=0 count<=5) هر کدام 8×u32 timestamp/faultMask/vIn/vPack/vLow/vHigh/state1/state2
  - chunk2: errors 5..9 (start=5)
  - chunk3: powerEvents 0..9 (start=0 count<=10) هر کدام 3×u32 off/on/duration
  - chunk4: powerEvents 10..19 (start=10)

پنل ساده هر 10 ثانیه `DIAG_REQ` می‌فرستد (اگر 5 ثانیه از آخرین گذشته) و کش می‌کند.

## درخت اتصال

```text
task_control.c (1 ثانیه) → func__Diag_Tick
task_control.c (10ms)    → func__Diag_UpdateMinMax
task_control.c (لبهٔ ورودی) → func__Diag_RecordPowerOff / func__Diag_RecordPowerOn
charger.c (FLOAT)        → func__Diag_RecordChargeComplete / func__Diag_RecordFault
esp_link.c (TIME_SYNC)   → func__Diag_SetBoardTime
esp_link.c (DIAG_REQ)    → func__Diag_GetRecord → DIAG_DATA (5 چانک)
```

این ماژول صدا می‌زند:

```text
diag.c
  bsp_flash.h / bsp_flash.c   خواندن و نوشتن صفحه‌های فلش
```

## تست

- هاست: `audit_consistency.py` و `host_test_*` همه پاس (با MODULE_DIAG=0 برای تست‌های قدیمی)
- نگهبان نگاشت بیت شمارنده‌های خطا: `python3 tools/check_diag_fault_bits.py` (نباید هیچ‌وقت در `func__Diag_RecordFault` شمارهٔ خام بیت برگردد)
- برد: با پنل ساده سه‌صفحه‌ای، سیمولاتور `simple_preview.html`، و تست فلش واقعی
- اعتبارسنجی: `Diag_Validation.xlsx` + `Diag_State_Machine.xlsx` (ورودی‌اش در `tools/make_module_state_machines.py` ثبت است)

## قوانین

- NVM با `osKernelLock`، هدر BSP مستقل از HAL، نام‌گذاری `TYPE__NAME` و `func__`، MISRA، استاتیک RTOS، بدون `HAL_Delay`، کامنت دو زبانه با علامت LRM

## تاریخچه

تاریخچهٔ تغییرات در `git log` همین مخزن نگهداری می‌شود. ممیزی سراسری 2026-10-15: این README به قالب هفت‌بخشی بقیهٔ ماژول‌ها درآمد و ماژول در `tools/make_module_state_machines.py` ثبت شد.
