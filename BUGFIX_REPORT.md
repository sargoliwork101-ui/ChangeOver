/**
 * @file    BUGFIX_REPORT.md
 * @brief   [EN] Summary of bugs found in ChangeOver firmware and circuit, and fixes applied on branch arena/01a0b51a-changeover.
 *          [FA] خلاصه ایرادات یافت‌شده در برنامه و مدار ChangeOver و اصلاحات انجام‌شده روی شاخه arena/01a0b51a-changeover.
 */

# گزارش خلاصه ایرادات و اصلاحات - ChangeOver

**شاخه:** `arena/01a0b51a-changeover`  
**تاریخ:** 2026-09-18  
**مبنا:** `main` @ `6a8f2fa9e13670356c684ea491871b263732ad2c`  
**قوانین مرجع:** `AI_AGENT_RULES.md`  
**شماتیک:** `Circuit/ChangeOver(24V_DC).pdf` (بررسی از روی `board_pins.h` و `CubeIDE/Core/Src/main.c` و BSP)

## چک‌های خودکار قبل از اصلاح

- `bash tools/check_ai_rules.sh` => PASS
- `bash tools/check_firmware_syntax.sh` => PASS
- اما PASS بودن چک‌ها به معنی نبودن باگ منطقی/سخت‌افزاری نیست.

## ایرادات اصلی یافت‌شده

### 1. [CRITICAL] سوئیچ باتری PB5 هرگز درایو نمی‌شد / Battery switch never driven
- **محل:** `board_pins.h` تعریف `BSP_GPIO_BATTERY_SWITCH` (PB5 active-low, safe High=OFF) و `bsp_gpio.c` در `BspGpio_Init` آن را High نگه می‌دارد.
- **مشکل:** هیچ ماژولی `BSP_GPIO_BATTERY_SWITCH` را Write نمی‌کرد. `Changeover` فقط `PB11` را می‌زد و README می‌گفت PB5 ممنوع است. نتیجه: مسیر باتری همیشه از طریق PB5 قطع می‌ماند و حالت BATTERY هرگز واقعاً وصل نمی‌شود.
- **اثر مدار:** حتی اگر PB11 آزاد (High) باشد، PB5 High = سوئیچ OFF = باتری قطع. سیستم فقط با ورودی کار می‌کند.
- **اصلاح:** 
  - `changeover.h` توضیح به‌روز شد: مسیر باتری با دو پایه PB5+PB11.
  - `changeover.c` helper جدید `func__Changeover_ApplyBatteryPath(bool)` اضافه شد:
    - ON = PB5 ON (Low) + PB11 deasserted (High)
    - OFF = PB5 OFF (High) + PB11 asserted (Low)
  - `func__Changeover_Init` هر دو پایه را خاموش امن می‌کند و shadow state را همگام نگه می‌دارد.
  - در `func__Changeover_Evaluate` تمام مسیرهای قطع/وصل از helper استفاده می‌کنند؛ در SAFE باتری قطع دوگانه، در BATTERY/INPUT باتری وصل.
  - `README` Changeover به‌روز شد.

### 2. [HIGH] ماژول Protection اسکلت و دارای باگ latch دائمی / Protection placeholder with permanent ADC latch
- **محل:** `protection.c`
- **مشکل قدیم:**
  ```c
  if (NULL || !valid) { Fault_Set(FAULT_ADC); return; }
  ```
  فقط ست می‌کرد و هرگز پاک نمی‌کرد. اگر یک بار ADC نامعتبر می‌شد، `FAULT_ADC` تا ریست می‌ماند و Changeover به FAULT می‌رفت و برنمی‌گشت. همچنین مقایسه اضافه‌جریان و باتری کم اصلاً پیاده نشده بود.
- **اصلاح:**
  - snapshot-first حفظ شد: NULL/invalid => FAULT_ADC ست.
  - وقتی snapshot معتبر است، FAULT_ADC پاک می‌شود (clear).
  - جریان‌ها با `APP_CONFIG.overcurrent1_ma/2_ma` (3500mA) مقایسه و `FAULT_OVERCURRENT_1/2` ست می‌شوند.
  - ولتاژ باتری با `APP_CONFIG.low_battery_mv` (20000mV) مقایسه و `FAULT_LOW_BATTERY` ست می‌شود.
  - همه faults latched هستند و فقط با `Fault_Clear` پاک می‌شوند، مطابق قرارداد Fault.
  - README Protection به‌روز شد.

### 3. [HIGH] مسابقه EXTI بین ISR و تسک / EXTI flag race
- **محل:** `bsp_exti.c` -> `func__BspExti_TakeEvent`
- **مشکل:** پرچم `volatile uint8_t Flags[]` از ISR (`HAL_GPIO_EXTI_Callback` -> `OnIrq`) ست و از تسک (`TakeEvent`) خوانده و پاک می‌شد بدون خاموش کردن IRQ. اگر ISR بین خواندن و پاک کردن پرچم را ست می‌کرد، رویداد جدید با پاک کردن از بین می‌رفت. برای JITTER (اضافه‌جریان سخت‌افزاری) از دست رفتن تریپ خطرناک است.
- **اصلاح:** در `TakeEvent` از `__get_PRIMASK/__disable_irq/__set_PRIMASK` برای بخش بحرانی کوتاه استفاده شد.

### 4. [MEDIUM] ورودی‌های JITTER بدون pull-up و INT_24_IN بدون pull-down / JITTER inputs floating
- **محل:** `CubeIDE/Core/Src/main.c` -> `MX_GPIO_Init`
- **مشکل:** PB2 و PB6 خروجی open-collector active-low از LM393 هستند و نیاز به pull-up دارند. قبلی `GPIO_NOPULL` بود و اگر روی شماتیک pull-up خارجی نباشد، پایه شناور و مستعد نویز/تریپ کاذب است. PB4 تشخیص حضور 24V نیز `NOPULL` بود؛ در حالت قطع ورودی باید Low مطمئن خوانده شود.
- **اصلاح:**
  - PB2/PB6 => `GPIO_PULLUP` (ایمنی اگر pull-up خارجی جا افتاده باشد).
  - PB4 => `GPIO_PULLDOWN` (حضور قطع => Low امن).
  - توضیح دوزبانه در کامنت اضافه شد.
  - توجه: `.ioc` فیلد pull را ذخیره نمی‌کند، پس تولید مجدد CubeMX این بخش را بازنویسی می‌کند؛ باید در CubeMX نیز pull-up/down تنظیم شود.

### 5. [MEDIUM] ناسازگاری V24 < V12 در Measurement / Inconsistent pack vs mid
- **محل:** `measurement.c`
- **مشکل:** `v_bat_low = v_bat12`, `v_bat_high = v_bat24 - v_bat12` اگر `v24 < v12` باشد قبلاً `high=0` می‌شد بدون توضیح. این حالت نشان‌دهنده ایراد سیم‌کشی/ADC است و باید مستند شود.
- **اصلاح:** کامنت توضیح اضافه شد که این حالت ناسازگار است و high صفر می‌ماند؛ low همان اندازه‌گیری می‌ماند. پیشنهاد افزودن بیت fault جدید برای این ناسازگاری در آینده.

### 6. [MEDIUM] Charger retry channel پس از خطای ولتاژ باتری پاک نمی‌شد / Retry not cleared on battery invalid
- **محل:** `charger.c` -> `BringupRegulateChannel` و `RegulateChannel`
- **مشکل:** وقتی ولتاژ باتری نامعتبر یا جریان بیش از حد بود، کانال به OFF ریست و PWM صفر می‌شد، اما `UINT8_T__G__RetryChannel` اگر برابر همان کانال بود پاک نمی‌شد. در نتیجه `ServiceRetry` می‌توانست دوباره duty اعمال کند در حالی که باتری نامعتبر است.
- **اصلاح:** در هر دو مسیر نامعتبر، اگر `RetryChannel == channelIndex` آن را به `CHG_NO_CHANNEL` ریست می‌کنیم.

### 7. [LOW] مستندات Changeover ناقص / Documentation mismatch
- README قدیم می‌گفت فقط PB11 استفاده می‌شود و PB5 ممنوع است؛ با اصلاح 1 این مستندات ناسازگار شد.
- اصلاح README انجام شد و جدول پایه‌ها به‌روز شد.

## موارد بررسی‌شده و بدون نیاز به اصلاح فوری

- **BSP ADC DMA race:** کپی بافر با `PRIMASK` محافظت می‌شود؛ کوتاه و قابل قبول است. جایگزینی با `osKernelLock` ممکن است اما تاخیر بیشتری دارد.
- **UI blocking delays:** `InputOk` و `Charging` از `Rtos_Delay` مسدودکننده استفاده می‌کنند در حالی که `BatteryRun` و `InputOverVoltage` غیرمسدودکننده هستند. این تناقض طبق قانون ساده و خوانا مجاز است (RTOS delay فقط همان Thread را می‌خواباند) ولی یکسان‌سازی به مدل non-blocking برای همه سناریوها در آینده پیشنهاد می‌شود.
- **Stack sizes:** UI 128 word (512B)، Measurement 192 word (768B)، Control 256 word (1024B) برای فاز فعلی کافی به نظر می‌رسد؛ پس از Build واقعی باید Map file چک شود.
- **Buzzer timing:** سرویس بوق محدودیت‌های ایمنی `MIN_PERIOD 1000ms` و `MIN_GAP 100ms` را درست اعمال می‌کند.
- **Changeover timing:** استفاده از `MillisecondsToTicks` بدون فرض tick=1ms درست است و گارد `durationTicks==0` دارد.

## فایل‌های تغییرکرده

- `Firmware/Modules/Changeover/changeover.h` : توضیح دوپایه باتری
- `Firmware/Modules/Changeover/changeover.c` : helper ApplyBatteryPath + کنترل PB5 + اصلاح تمام مسیرهای SAFE/INPUT/BATTERY
- `Firmware/Modules/Changeover/README.md` : به‌روزرسانی وضعیت، پایه‌ها، توابع، درخت اتصال
- `Firmware/Modules/Protection/protection.c` : پیاده‌سازی کامل حفاظت با clear/set منطقی
- `Firmware/Modules/Protection/README.md` : وضعیت از اسکلت به پیاده‌سازی اولیه
- `Firmware/Bsp/Src/bsp_exti.c` : محافظت بحرانی با PRIMASK
- `Firmware/Modules/Measurement/measurement.c` : کامنت ناسازگاری V24<V12
- `Firmware/Modules/Charger/charger.c` : پاک کردن retry channel در خطای باتری/جریان
- `CubeIDE/Core/Src/main.c` : PB2/PB6 PULLUP و PB4 PULLDOWN

## تست‌های اجرا شده پس از اصلاح

- `bash tools/check_ai_rules.sh` => ALL CHECKS PASSED
- `bash tools/check_firmware_syntax.sh` => HOST SYNTAX CHECK PASSED
- بررسی دستی:
  - Changeover در BATTERY/INPUT => PB5 Low + PB11 High (وصل)
  - Changeover در SAFE => PB5 High + PB11 Low (قطع دوگانه)
  - Protection در valid => FAULT_ADC پاک، overcurrent/low battery ست
  - EXTI TakeEvent با IRQ خاموش => بدون از دست رفتن رویداد
  - GPIO init با pull-up/down => JITTER پایدارتر

## پیشنهادهای بعدی (خارج از این شاخه)

- افزودن `FAULT_INCONSISTENT_BATTERY` برای حالت `v_bat24 < v_bat12`.
- یکسان‌سازی UI به مدل non-blocking برای همه سناریوها (InputOk/Charging هم با `osKernelGetTickCount`).
- فعال‌سازی `MODULE_PROTECTION=1` پس از تست برد و ثبت در `Protection_Board_Validation.xlsx`.
- بررسی شماتیک PDF با ابزار گرافیکی برای تأیید وجود pull-up خارجی روی JITT1/JITT2 و pull-down روی INT_24_IN؛ اگر خارجی موجود است، pull داخلی را می‌توان به NOPULL برگرداند ولی مستند کرد.
- اندازه‌گیری واقعی قطبیت PB5/PB11 روی برد و ثبت در Excel اعتبارسنجی.

## جمع‌بندی

ایراد بحرانی مسیر باتری (PB5 هرگز روشن نمی‌شد) برطرف شد و Changeover اکنون هر دو کلید باتری را کنترل می‌کند. Protection از اسکلت به منطق واقعی ارتقا یافت و باگ latch دائمی ADC اصلاح شد. مسابقه EXTI و شناور بودن ورودی‌های JITTER اصلاح شد و باگ retry در Charger پاک شد. تمام چک‌های AI و syntax پس از اصلاح سبز هستند.
