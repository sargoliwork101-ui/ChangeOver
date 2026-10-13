# ماژول Diag - لاگ تشخیصی برد (۱۱ مورد تأییدشده)

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
9. شمارنده‌ها: عدم تعادل، باتری کم، قطع باتری، فنی، اضافه ولتاژ
10. زمان همگام‌سازی ساعت (boardTimeBase)
11. نسخه و UID (magic + version + seq + crc)

## ذخیره
دو صفحه فلش جدا `0x0800F800` و `0x0800FC00` (هر کدام ۱KB) به صورت پینگ‌پنگ با CRC32 IEEE.
ساختار `diag_record_t` حدود ۶۷۶ بایت:
- هدر: magic='DIAG' (0x44494147), version=1, seq
- شمارنده‌های اصلی: totalChargeSec, chargeSecCh1/Ch2, chargeCount, chargeCountCh1/Ch2, uptimeSec, resetCount, boardTimeBase
- کمینه/بیشینه: max/min Vin/Vpack/Vlow/Vhigh, maxImb, maxIch1/Ich2
- شمارنده رویداد: imbalanceCount, lowBatteryCount, batLostCount, techFaultCount, ovCount
- ۱۰ خطا: هر کدام timestamp/faultMask/vIn/vPack/vLow/vHigh/state1/state2 (۳۲ بایت)
- ۲۰ رویداد برق: هر کدام offTime/onTime/durationSec (۱۲ بایت)
- CRC32

ذخیره دیبانس ۶۰ ثانیه بعد از تغییر، یا فوری بعد از رویداد مهم (خطا، قطع برق، شارژ کامل) اگر لازم باشد.
در `Diag_Init` هر دو صفحه خوانده و CRC چک می‌شود، جدیدترین seq سالم انتخاب می‌شود. اگر هیچکدام سالم نبود، پیش‌فرض با min=0xFFFFFFFF ساخته می‌شود و resetCount=1.

## ساعت
برد RTC ندارد. ساعت از گوشی (ادمین پنل ساده) با پیام `TIME_SYNC` می‌آید (یونیکس ثانیه LE).
پایه زمان = ساعت گوشی - آپتایم. ساعت فعلی = پایه + آپتایم.
اگر همگام نشده باشد، BoardTime=0 و آپتایم به عنوان زمان نسبی استفاده می‌شود.
`BoardTime` برای لاگ خطاها و قطع برق استفاده می‌شود تا کاربر تاریخ شمسی ببیند.

## API
- `Diag_Init` - بوت، بعد از `CalLut_Init` در `App_Init` صدا زده می‌شود
- `Diag_Tick` - هر ۱ ثانیه از `TaskControl` (آپتایم + debounce save)
- `SetBoardTime(unixSec)` - از ESP-Link هنگام TIME_SYNC
- `GetBoardTime()` - ساعت فعلی = base + uptime
- `AddChargeSec(channel, sec)` - از TaskControl هر ثانیه اگر کانال فعال است
- `RecordChargeComplete(channel)` - از Charger هنگام ورود به FLOAT
- `RecordFault(mask, snap)` - از Fault/Protection/Charger هنگام ست‌شدن بیت جدید، با snapshot کامل
- `RecordPowerOff/On` - از TaskControl با تشخیص لبه input_present
- `UpdateMinMax(snap)` - از TaskControl هر ۱۰ms
- `GetRecord()` - برای EspLink جهت ارسال چانک‌ها
- `SaveNow()` - ذخیره فوری قبل از ریست

## اینتگریشن برد
- `app.c`: include diag.h و `Diag_Init` تحت `#if MODULE_DIAG`
- `task_control.c`: include diag.h، سه static tick و PrevInput، هر پاس `UpdateMinMax`، تشخیص لبه برای PowerOff/On، هر ۱۰۰ تیک (۱ ثانیه) `AddChargeSec` هر کانال فعال + `Diag_Tick`
- `fault.c`: بعد از هر `Fault_Set(FAULT_CHARGER_BAT_LOST)` صدا زدن `RecordFault` با snapshot
- `charger.c`: هنگام اولین ورود به FLOAT از ABSORB، `RecordChargeComplete(chIdx)` + `RecordFault(TECHNICAL)` در تشخیص فنی
- `protection.c`: `RecordFault(ADC)` اگر snapshot موجود باشد
- `esp_link.c`: `SendDiagChunk(0..4)` و `SendDiagAll` و هندلر `TIME_SYNC` (۴ بایت LE -> SetBoardTime) و `DIAG_REQ` (ارسال ۵ چانک)
- `modules_enable.h`: `MODULE_DIAG=1` با guard `#ifndef` برای تست هاست
- `CubeIDE/.cproject`: افزودن include path Diag

## ارتباط با ESP (پنل ساده)
پیام‌های جدید در `esp_link.h`:
- `0x30 TIME_SYNC` ESP->STM: ۴ بایت LE unix (ثانیه)
- `0x31 DIAG_REQ` ESP->STM: بدون payload، درخواست لاگ کامل
- `0x32 DIAG_DATA` STM->ESP: چانک‌های دیاگ
  - chunk0: totalChargeSec (u32), chargeCount (u32), uptimeSec (u32), resetCount (u32), boardTime (u32 = base+uptime), errCount (u8), powerCount (u8)
  - chunk1: errors ۰..۴ (start=0 count<=5) هر کدام ۸×u32 timestamp/faultMask/vIn/vPack/vLow/vHigh/state1/state2
  - chunk2: errors ۵..۹ (start=5)
  - chunk3: powerEvents ۰..۹ (start=0 count<=10) هر کدام ۳×u32 off/on/duration
  - chunk4: powerEvents ۱۰..۱۹ (start=10)

پنل ساده هر ۱۰ ثانیه `DIAG_REQ` می‌فرستد (اگر ۵ ثانیه از آخرین گذشته) و کش می‌کند.

## تست
- هاست: `audit_consistency.py` و `host_test_*` همه پاس (با MODULE_DIAG=0 برای تست‌های قدیمی)
- برد: با پنل ساده سه‌صفحه‌ای، سیمولاتور `simple_preview.html`، و تست فلش واقعی
- اعتبارسنجی: `Diag_Validation.xlsx` + `Diag_State_Machine.xlsx` ساخته شد با `tools/make_module_state_machines.py`

## قوانین
- NVM با osKernelLock، HAL-free هدر BSP، نام‌گذاری type__name و func__، MISRA، استاتیک RTOS، بدون HAL_Delay، کامنت دو زبانه LRM
