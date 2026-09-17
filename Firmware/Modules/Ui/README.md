/**
 * @file    README.md
 * @brief   [EN] UI module sheet: LED scenarios with battery hysteresis (2% + 0/1 special) and phase-preserving blink.
 *          [FA] برگه ماژول UI: سناریوهای LED با هیسترزیس باتری (۲٪ + رفتار خاص ۰/۱) و چشمک فاز-حفاظ.
 */

# ماژول UI

## وضعیت

فعال. `MODULE_UI = 1`. UI به **snapshot واقعی Measurement** وصل است: `func__Measurement_GetSnapshot(&snap)` و قبل از هر تصمیم `snapshot.valid` بررسی می‌شود. در `valid==false` یا `NULL`، UI در **safe-off** (همه LED و بوق خاموش)، **Low Battery Alarm فعال نمی‌شود** و `BOOL__G__UiBatteryAlarmIssued = false` و هیچ مقدار دستی/stale استفاده نمی‌شود.

**منبع باتری:** تولید فقط از `snapshot.v_bat24_mv` (PA3 / ADC1_IN3 / `BSP_ADC_CHANNEL_24V_BAT` / `raw[2]`). `v_in` از PA2/ADC1_IN2/raw[1] فقط برای تشخیص ورودی و McuPowerPath است و UI BatteryRun هرگز از آن استفاده نمی‌کند.

**هیسترزیس درصد باتری (BatteryRun-only, 2% + 0/1):**
- `UI_BATTERY_PERCENT_HYSTERESIS_PERCENT = 2u`: درصد پایدار `stablePercent` فقط وقتی تغییر می‌کند که `|raw-stable| >=2`. نوسان `56↔57` یا `57↔58` در باتری ~25V زمان چشمک را تغییر نمی‌دهد؛ `57→55` یا `57→59` تغییر می‌دهد.
- `stable 57, raw 56 → keep 57`, `58 → keep 57`, `55 → 55`, `59 → 59` (تست اجباری).
- **0% بحرانی:** `raw 0 → 0%`, بوق 10 ثانیه فقط یک بار، سپس همه LED خاموش تا خروج از ناحیه بحرانی؛ `stable 0` تا `raw>=2` روی 0 می‌ماند سپس ابتدا به 1 می‌رود، نه مستقیم 2.
- **1% مستقل:** `stable 1` با `raw 0 → 0`, `raw>=3 → 2`, غیره حفظ 1؛ سه‌بوق 1% از 0% جدا است و نویز 0↔1 باعث restart بوق بحرانی نمی‌شود.
- این هیسترزیس فقط برای نمایش/زمان‌بندی BatteryRun است؛ هیسترزیس‌های ورودی `20/21V`, اضافه‌ولتاژ `27/28V` و `Low Battery Alarm 21000/21200` بدون تغییر می‌مانند.

**پایداری فاز چشمک:** `func__Ui_UpdateBatteryRunGreenBlink` هنگام تغییر `stablePercent` فاز فعلی (`BOOL__G__UiBatteryGreenOn`) و `phaseStartTick` را **reset نمی‌کند**؛ فقط `OnMs/OffMs` ذخیره به‌روز و زمان جدید از مرز فاز بعدی اعمال می‌شود. شروع فاز فقط در ورود/خروج واقعی BatteryRun، `snapshot invalid/NULL` یا `Ui_Init` مجاز است.

API بوق فقط یک تابع است:
```c
int32_t func__Ui_Buzzer_Tick(periodMs, dutyPercent, beepCount, gapMs)
```
غیرمسدودکننده؛ `0` خاموشی معتبر، `-1` نامعتبر، مثبت زمان مراجعه پیشنهادی (10% کوچک‌ترین بخش).

**Non-blocking:** `InputOk` (سبز ثابت) و `Charging` (زرد چشمک) اکنون فاز-محور و **بدون `Rtos_Delay` مسدودکننده** هستند؛ `func__Ui_Tick()` سریع برمی‌گردد و حلقه 10ms تسک UI واکنش را کند نمی‌کند. ظاهر سناریوها بدون تغییر، فقط زمان‌بندی پایدار و non-blocking.

## سناریوهای توافق‌شده UI

> همه ولتاژها بر حسب میلی‌ولت؛ نام ثابت کنار سناریو.

### قواعد مشترک تشخیص ورودی و هیسترزیس

- `Input >= 21000` وصل، `Input <=20000` قطع، بین 20-21V حفظ حالت قبلی. `UI_INPUT_HYSTERESIS_MV=1000`.
- ثابت‌ها: `UI_INPUT_CONNECTED_THRESHOLD_MV`, `DISCONNECTED`, `HYSTERESIS`.

### سناریو ۱: InputOk (non-blocking)

- شرط: ورودی وصل و باتری `>=28V`.
- `Input 24V, Battery 28V → سبز دائم، زرد/قرمز خاموش`
- **اجرای جدید:** سبز ثابت، بوق خاموش، **بدون delay 500ms** داخل سناریو؛ تسک UI هر 10ms صدا می‌زند، واکنش سریع.

### سناریو ۲: BatteryRun (با هیسترزیس 2% و فاز-حفاظ)

- شرط: ورودی قطع (`<=20000`).
- `Input 0V/19V → BatteryRun`, هیسترزیس 20/21V.
- سبز چشمک‌زن، زرد/قرمز خاموش. دیوتی سبز از **درصد پایدار** `stablePercent` (نه خام) محاسبه می‌شود:
  ```
  remaining = 100 - stablePercent
  periodPerPercent = 1000/100 =10ms
  greenOffMs = remaining*periodPerPercent (min 10ms)
  greenOnMs = 1000 - greenOffMs
  ```
  مثال 25V: `raw 56/57/58 → stable 57 → Off 430ms On 570ms` ثابت (جلوگیری از jitter طولانی).
- هیسترزیس درصد:
  - `stable 0`: تا `raw>=2` روی 0، سپس →1.
  - `stable 1`: `raw 0→0`, `>=3→2`, else 1.
  - `stable >=2`: `|raw-stable|>=2` → به‌روز، else حفظ. مثال اجباری بالا.
- 0% و 1% جدا: 0% → 10s بوق یک بار سپس خاموش تا خروج؛ 1% → سه‌بوق هر 20s با چشمک سبز مربوطه؛ نویز 0↔1 باعث restart بحرانی نمی‌شود.
- ثابت‌ها: `UI_BATTERY_PERCENT_HYSTERESIS_PERCENT=2`, `ZERO_EXIT=2`, `ONE_EXIT=3`, `BLINK_PERIOD=1000`, `GREEN_MIN_OFF=10`.

#### جدول بوق BatteryRun (با درصد پایدار)

| محدوده پایدار | رفتار بوق | دوره | دیوتی | هر بوق | تعداد | ثابت‌ها |
|---|---|---:|---:|---:|---:|---|
| `stable <1%` (0%) | یک بوق ممتد سپس خاموش تا خروج از 0% | یک‌بار | 100% | 10000ms | 1 | `CRITICAL_PERCENT` 1, `CRITICAL_PERIOD` 10000 |
| `1%<=stable<10%` | سه بوق | 20000ms | 31% | 2000ms | 3 | `TRIPLE_PERCENT` 10 |
| `10%<=stable<20%` | دو بوق | 60000ms | 4% | 1150ms | 2 | `DOUBLE_PERCENT` 20 |
| `20%<=stable<40%` | یک بوق | 60000ms | 2% | 1200ms | 1 | `START_PERCENT` 40 |
| `stable>=40%` | خاموش | — | — | — | — | — |

گپ `100ms`, دیوتی‌های تقریبی از integer.

### سناریو ۳: Charging (non-blocking)

- شرط: ورودی وصل و باتری `<28V` (`Input 24V, Battery 25V → سبز روشن، زرد چشمک، قرمز خاموش`).
- سبز دائم روشن، زرد مانده تا فول غیرخطی: `remaining=100-raw(PA3)`, `periodPer=10ms`, `yellowOn=remaining*periodPer (min 10ms)`, `yellowOff=1000-yellowOn`. هرچه به 100% نزدیک‌تر دیوتی زرد کمتر.
- **اجرای جدید:** زرد چشمک اکنون **غیرمسدودکننده فاز-محور** (حالت `YellowOn/Off`, `phaseStartTick`, `OnMs/OffMs` ذخیره و حفظ فاز روی تغییر درصد). بدون `Rtos_Delay`؛ `Ui_Tick` هر 10ms سریع برمی‌گردد. ثابت‌ها: `CHARGING_BLINK_PERIOD 1000`, `YELLOW_MIN_OFF 10`.

### سناریو ۴: InputOverVoltage

- `Input>28000` فعال، `<=27000` پاک، بین 27-28V حفظ. سبز ثابت، زرد خاموش، قرمز 50% هر 1s، بوق 1s هر 10s. ثابت‌ها `OVERVOLTAGE_THRESHOLD 28000`, `HYSTERESIS 1000`, `LED_PERIOD 1000`, `DUTY 50`.

### خلاصه انتخاب سناریوها

```
InputOverVoltage → خطای اضافه‌ولتاژ
else if InputDisconnected (<=20000) → BatteryRun
else if InputConnected && Battery<28V → Charging
else InputOk
snapshot invalid/NULL → safe-off (همه خاموش, flag false)
BoardTest → یک‌بار قرمز/زرد/سبز + بوق کوتاه
```

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-17 | هیسترزیس درصد باتری 2% + رفتار خاص 0/1، حفظ فاز چشمک سبز (عدم reset روی jitter 56/57/58)، InputOk/Charging non-blocking فاز-محور، به‌روزرسانی README/Excel/host_test، بررسی RTOS/stack، تست 25V. |
| 2026-09-16 | اتصال UI به snapshot واقعی، safe-off در invalid، فلگ پیوسته Low Battery Alarm، حذف manual battery؛ UI_Board_Validation 2.0. |
| 2026-09-16 | افزودن UI_Board_Validation.xlsx. |
| 2026-09-15 | ساخت کامل InputOk/Charging/BatteryRun/InputOverVoltage، هیسترزیس ورودی. |
| 2026-09-15 | ساده‌سازی بوق به یک API دوره‌ای. |
| 2026-09-15 | جداسازی LED و BUZZER، فرمول دوره‌ای جدید. |
| 2026-09-15 | محدودیت‌های ایمنی 1000ms/100ms و زمان مراجعه 10%. |
| 2026-09-15 | مستندات دو بخش LED/BUZZER و APP_CONFIG. |
| 2026-09-14 | اجرای AI: check_ai_rules + host_test. |

## فایل‌ها

| فایل | نقش |
|---|---|
| `ui_led.h` / `ui_led.c` | منطق LED: نگاشت PA3/v_bat24, هیسترزیس 2% + 0/1, حفظ فاز چشمک سبز, InputOk/Charging non-blocking (زرد فاز-محور), هیسترزیس ورودی, InputOverVoltage, Low Battery Alarm Flag از `snapshot.v_bat24_mv` (21000/21200). مالک `BOOL__G__UiBatteryAlarmIssued`. |
| `ui_buzzer.h` / `ui_buzzer.c` | سرویس بوق: دوره/دیوتی/گپ/مراجعه بعدی, BSP_GPIO_BUZZER. |
| `../../Rtos/Src/task_ui.c` | تسک UI هر 10ms: `GetSnapshot` → `Ui_Tick`; non-blocking, سایر تسک‌ها starvation ندارند. |
| `../../Bsp/Src/bsp_gpio.c` | `BspGpio_Write` برای LED/BUZZER. |
| `../../Bsp/Inc/bsp_gpio.h` | سیگنال‌های منطقی LED. |
| `../../Config/Inc/app_config.h` | زمان‌بندی LED و نگاشت ولتاژ. |
| `host_test_ui.py` | تست هاست بوق + هیسترزیس 56/57/58, 57→55/59, 0% critical 10s, 0↔1, 1→2 و فاز چشمک. |
| `UI_Board_Validation.xlsx` | برگه تست برد (6 شیت) با سناریوهای جدید 25V و هیسترزیس و Stack/RTOS. |

## برگه تست و اعتبارسنجی روی برد

فایل `UI_Board_Validation.xlsx` نسخه 2.1 برای ولیدیشن با **snapshot واقعی** است (6 شیت: راهنما/برنامه تست/مراجع سناریو/ثبت ایراد/تأیید نهایی/فهرست‌ها).

- دامنه: فقط LED/BUZZER؛ `snapshot.v_bat24_mv` از PA3/ADC1_IN3/raw[2] و `v_in` از PA2/ADC1_IN2/raw[1]؛ PB4 فقط EXTI، PB5 فقط McuPowerPath، PB11 فقط Changeover.
- `snapshot.valid` قبل از هر تصمیم؛ invalid → safe-off, flag false.
- **تست جدید 25V:** باتری 25V (`~57%`) با نویز 56/57/58 → `stable 57` حفظ و فاز چشمک reset نشود (Off 430ms On 570ms ثابت). `raw 57→55/59` → stable تغییر کند و فاز از مرز بعدی با زمان جدید ادامه یابد.
- انتهای بازه: `raw 0 → 0%` بوق 10s یک بار سپس LED خاموش؛ `0↔1` نوسان → بوق بحرانی restart نشود؛ `raw1 →1%` سه‌بوق؛ `1% raw2` حفظ 1، `>=3` →2.
- سناریوها: InputOk (سبز ثابت non-blocking)، Charging (زرد فاز-محور)، BatteryRun (سبز فاز-حفاظ + بوق 4 بازه با stable)، InputOverVoltage، invalid/NULL، Low Alarm، سوئیچ ورودی وصل/قطع.
- RTOS/Stack: حافظه static، `SUPPORT_DYNAMIC_ALLOCATION=0`, `cb_mem/stack_mem` معتبر، stack high-water، بدون queue/timer/mutex جدید.
- برای هر ردیف مقدار واقعی، وضعیت، نام و تاریخ ثبت شود؛ برگه تأیید نهایی پس از بستن ایرادها.

## توابع

| نام | کار | فایل |
|---|---:|---|
| `func__Ui_BatteryVoltageToPercent` | ولتاژ 21-28V به 0-100% (4 گام) | `ui_led.c` |
| `func__Ui_UpdateBatteryStablePercent` | هیسترزیس 2% + 0/1 (56/57/58 حفظ, 0→1→2) | `ui_led.c` |
| `func__Ui_UpdateBatteryRunGreenBlink` | چشمک سبز BatteryRun فاز-حفاظ (عدم reset روی jitter) | `ui_led.c` |
| `func__Ui_UpdateChargingYellowBlink` | چشمک زرد Charging فاز-محور non-blocking | `ui_led.c` |
| `func__Ui_Buzzer_Tick` | API بوق دوره‌ای | `ui_buzzer.c` |
| `func__Ui_Init` | خاموش + flag false + reset فازها و stable | `ui_led.c` |
| `func__Ui_BoardTest_Start` | تست قرمز/زرد/سبز + بوق | `ui_led.c` |
| `func__Ui_ScenarioInputOk` | **non-blocking** سبز ثابت | `ui_led.c` |
| `func__Ui_ScenarioCharging_Tick` | **non-blocking** سبز ثابت + زرد فاز-محور | `ui_led.c` |
| `func__Ui_ScenarioBatteryRun_Tick` | سبز چشمک با stable + بوق 4 بازه (0% یک 10s) | `ui_led.c` |
| `func__Ui_ScenarioInputOverVoltage_Tick` | قرمز 50% + بوق 10s | `ui_led.c` |
| `func__Ui_Tick` | valid/safe-off, flag hysteresis 21000/21200, هیسترزیس ورودی, انتخاب سناریو | `ui_led.c` |

## محدودیت‌های ایمنی و کد بازگشتی

| ثابت | مقدار | رفتار |
|---|---:|---|
| `UI_BUZZER_MIN_PERIOD_MS` | 1000ms | دوره کمتر نامعتبر |
| `UI_BUZZER_MIN_GAP_MS` | 100ms | گپ چندبوق کمتر نامعتبر |
| `UI_BUZZER_CHECK_PERCENT` | 10% | 10% کوچک‌ترین بخش |
| `UI_BUZZER_MIN_CHECK_MS` | 1ms | حداقل مراجعه |
| `UI_BATTERY_PERCENT_HYSTERESIS_PERCENT` | 2% | هیسترزیس BatteryRun |
| `UI_BATTERY_ZERO_EXIT_THRESHOLD` | 2% | خروج 0→1 |
| `UI_BATTERY_ONE_EXIT_THRESHOLD` | 3% | خروج 1→2 |

کدهای `0` خاموشی، `-1` نامعتبر، مثبت زمان مراجعه.

فرمول بوق همان قبل:
```
dutyWindow=period*duty/100, gap*(count-1), availableOn, beepOn, tail, nextCheck=max(1,10%*min)
```

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی | منبع |
|---|---|---|---:|---|
| PB0 | `MCU_R_LED` | LED قرمز | روشن | — |
| PB1 | `MCU_Y_LED` | LED زرد | چشمک شارژ | — |
| PB10 | `MCU_G_LED` | LED سبز | InputOk ثابت / BatteryRun چشمک (stable) | — |
| PA4 | `MCU_BUZZER` | بازر | بوق | Active Buzzer |
| PA3 | `MCU_ADC_24_BAT` (پایه13) | ADC1_IN3 → raw[2] → `snapshot.v_bat24_mv` → **BatteryRun** | — | فقط PA3/v_bat |
| PA2 | `MCU_ADC_24_IN` (پایه12) | ADC1_IN2 → raw[1] → `snapshot.v_in_mv` → ورودی/McuPowerPath | — | فقط PA2/v_in |
| PB4 | `MCU_INT_24_IN` | EXTI وصل/قطع | High وصل | فقط ورودی |
| PB5 | `MCU_BAT_SWITCH` Q1 | فقط McuPowerPath | Low وصل | — |
| PB11 | `MCU_PROTECT_BATT` Q17 | فقط Changeover | — | — |

قطبیت شماتیکی؛ UI فقط `BSP_GPIO_*`.

## پیش‌فرض امن

بعد از Reset/`Init`، LED/بوق خاموش و `flag false`, `stable 0 uninit`, فازها reset. `snapshot invalid/NULL` → safe-off، flag false، فازها reset (stable برای 0/1 حفظ می‌ماند تا نویز باعث restart نشود). در BatteryRun زیر 1% بوق 10s یک بار سپس خاموش تا خروج از 0% (stable 0→1 با raw>=2). 1% سه‌بوق مستقل. در InputOk/Charging بوق خاموش. BoardTest رفتار تست حفظ.

## درخت اتصال

```
task_ui.c (10ms) → GetSnapshot(&snap) → Ui_Tick(&snap)
  ├─ !valid → safe-off + resets (green/ yellow/ stable)
  ├─ update flag v_bat 21000/21200
  ├─ update input 20/21V + overvoltage 27/28V
  ├─ InputOverVoltage → red 50% + buzzer 10s
  ├─ BatteryRun(v_bat stable) → green phase-preserved (stable) + buzzer 4 bands (0% 10s once)
  ├─ Charging(v_bat raw) → green steady + yellow phase-preserved
  └─ InputOk → green steady (non-blocking)
     └── Buzzer_Tick → BspGpio_Write → BSP_GPIO_*
```

