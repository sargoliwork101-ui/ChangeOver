/**
 * @file    README.md
 * @brief   [EN] UI: BatteryRun 2%+0/1, Charging 5%, InputOk 100/95 full hysteresis, phase-preserved non-blocking.
 *          [FA] ماژول UI: هیسترزیس ۲٪ BatteryRun + ۰/۱، ۵٪ Charging، فول ۱۰۰/۹۵ و فاز-حفاظ غیرمسدودکننده.
 */

# ماژول UI

## وضعیت

فعال. `MODULE_UI=1`. UI به **snapshot واقعی Measurement** وصل: `func__Measurement_GetSnapshot(&snap)` و `valid` قبل هر تصمیم چک. `valid false/NULL → safe-off` (همه خاموش)، `Low Battery Alarm` false، بدون stale/manual.

**منبع باتری:** فقط `snapshot.v_bat24_mv` (PA3/ADC1_IN3/`BSP_ADC_CHANNEL_24V_BAT`/raw[2]). `v_in` از PA2/ADC1_IN2/raw[1] فقط ورودی/McuPowerPath.

**هیسترزیس BatteryRun 2% + 0/1:**
- `UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT 2u` (alias `UI_BATTERY_PERCENT_HYSTERESIS_PERCENT`): `|raw-stable|>=2` → update else keep. `57` با `56/58` حفظ، `55/59` تغییر.
- `stable0` تا `raw>=2` روی 0 سپس →1 (نه 2). `stable1`: `raw0→0`, `raw>=3→2`, else 1. جلوگیری از chatter 0↔1 و restart بوق بحرانی.
- 0% بحرانی `raw0→stable0` → یک بوق 10s فقط یک بار سپس همه LED خاموش تا خروج از ناحیه بحرانی (stable 0→1 با raw>=2). 1% مستقل: سه‌بوق هر 20s با چشمک سبز کوتاه، `stable1` از 0% جدا.

**هیسترزیس Charging 5%:**
- `UI_CHARGING_PERCENT_HYSTERESIS_PERCENT 5u`: `chargingStablePercent` فقط `|raw-stable|>=5` به‌روز. `stable57` با `53..61` حفظ، خارج → update. زمان زرد `remaining=100-stable5%` پایدار.
- فرمول حفظ: `remaining=100-chargingStable`, `yellowOn=remaining*period/100(min10)`, `yellowOff=1000-yellowOn` با `chargingStable` نه خام.

**هیسترزیس فول InputOk/Charging:**
- `UI_CHARGING_FULL_ENTER_PERCENT 100u` ورود به InputOk فقط وقتی خام به 100٪ برسد.
- `UI_CHARGING_FULL_EXIT_PERCENT 95u` ماندن در InputOk تا خام <95٪، سپس Charging. جلوگیری پرش اطراف 28V.

**پایداری فاز:** سبز BatteryRun و زرد Charging هرکدام phase/OnMs/OffMs/startTick مستقل. تغییر stable **فاز reset نمی‌کند**؛ زمان جدید از مرز بعدی. فقط ورود/خروج واقعی سناریو، `invalid/NULL` یا `Init` ریست کامل.

API بوق: `func__Ui_Buzzer_Tick(period,duty,beepCount,gap)` غیرمسدودکننده (`0` خاموش، `-1` نامعتبر، مثبت زمان مراجعه).

**Non-blocking:** `InputOk` (سبز ثابت) و `Charging` (زرد فاز-محور) بدون `Rtos_Delay` داخلی؛ `Ui_Tick` <1ms برمی‌گردد، حلقه 10ms تسک UI polling می‌دهد، starvation ندارد. ظاهر سناریوها بدون تغییر.

## سناریوهای توافق‌شده UI

### قواعد ورودی
- `Input >=21000` وصل، `<=20000` قطع، 20-21V حفظ. `UI_INPUT_HYSTERESIS_MV 1000`.
- ثابت‌ها: `CONNECTED_THRESHOLD 21000`, `DISCONNECTED 20000`.

### سناریو ۱: InputOk (non-blocking + فول هیسترزیس)
- شرط: ورودی وصل **و** `ChargingFullActive` (خام 100 وارد، تا <95 حفظ). `Input 24V Battery 28V → سبز ثابت`
- سبز ثابت، زرد/قرمز خاموش، بوق خاموش. بدون delay 500ms؛ 10ms loop.
- خروج به Charging فقط وقتی خام <95.

### سناریو ۲: BatteryRun (2% + 0/1 + فاز-حفاظ)
- شرط: ورودی قطع (`<=20000`). `0V/19V → BatteryRun`
- سبز چشمک با `stablePercent`: `remaining=100-stable`, `periodPer=10ms`, `greenOff=remaining*per(min10)`, `greenOn=1000-greenOff`. مثال 25V `56/57/58→stable57→Off430 On570` ثابت.
- هیسترزیس: `stable0→raw>=2→1`, `stable1 raw0→0 >=3→2 else1`, `>=2 diff>=2→raw`.
- 0% <1% → 10s یک‌بار سپس LED خاموش؛ 1% → سه‌بوق 20s با چشمک سبز کوتاه؛ نویز 0↔1 restart نمی‌کند.
- ثابت‌ها: `BATTERY_RUN_HYSTERESIS 2`, `ZERO_EXIT 2`, `ONE_EXIT 3`, `BLINK_PERIOD 1000`, `GREEN_MIN_OFF 10`.

#### جدول بوق BatteryRun (با stable)
| محدوده | رفتار | دوره | دیوتی | هر بوق | تعداد |
|---|---|---:|---:|---:|---:|
| `stable<1%` | 10s یک‌بار سپس خاموش | یک‌بار | 100% | 10000 | 1 |
| `1%<=stable<10%` | سه‌بوق | 20000 | 31% | 2000 | 3 |
| `10%<=stable<20%` | دو‌بوق | 60000 | 4% | 1150 | 2 |
| `20%<=stable<40%` | یک‌بوق | 60000 | 2% | 1200 | 1 |
| `stable>=40%` | خاموش | — | — | — | — |
گپ 100ms.

### سناریو ۳: Charging (5% + فاز-حفاظ non-blocking)
- شرط: ورودی وصل و `!ChargingFullActive` **و وجودِ کانالِ شارژِ فعال** (`func__Charger_IsAnyChannelActive()` از ۲۰۲۶-۰۹-۱۹: چشمک زرد فقط وقتی شارژر واقعاً کار می‌کند - کانال ۱، ۲ یا هر دو؛ کانال در OFF/JIT-retry/انتظار-ورودی/خطای-نهایی/قطع-باتری یعنی فعال نیست ⇒ سبز ثابت). `Input 24V Battery 25V → سبز ثابت زرد چشمک`
- سبز ثابت، زرد «مانده تا فول» با `chargingStable 5%`: `remaining=100-chargingStable`, `yellowOn=remaining*10ms(min10)`, `yellowOff=1000-yellowOn` — از ۲۰۲۶-۰۹-۱۹ دوباره برگشته به همین معنا با دستور نهایی کاربر: **هرچه پرتر، زرد کوتاه‌تر** (۹۵٪ شارژ ⇒ ۵۰ms از ۱۰۰۰ms روشن؛ باتری خالی ≈ دائم‌روشن). `stable57` با `53..61` حفظ (25V jitter بی‌اثر)، `52/62`→ تغییر.
- زرد فاز-محور: `YellowOn/Off`, `phaseStartTick`, `OnMs/OffMs` حفظ فاز روی تغییر stable، بدون delay 1s. `CHARGING_BLINK_PERIOD 1000`, `YELLOW_MIN_OFF 10`.

### سناریو ۴: InputOverVoltage
- `>28000` ورود، `<=27000` پاک، 27-28V حفظ. سبز ثابت زرد خاموش قرمز 50% 1s بوق 1s هر 10s. `THRESHOLD 28000 HYSTERESIS 1000`.

### سناریو ۵: BatLost (قطع باتری - از پرچم متمرکز Fault)
- **شرط:** فقط قفل‌بودن `FAULT_CHARGER_BAT_LOST` (مالکیت Set/Clear با ماژول Fault؛ UI فقط می‌خواند و هیچ لچی ندارد). هر دو حالت شناسایی Fault اینجا نمایش داده می‌شوند: پمپ >14.8V حین شارژ یا نبود باتری با ورودی سالم (هر دو نیم <6V به‌مدت ۱s).
- **الگو (توافق ۲۰۲۶-۰۹-۱۹):** سبز ثابت (ورودی حاضر است)، زرد خاموش، **قرمز چشمک‌تند ۵۰/۵۰ در دوره ۱s** (متمایز از پالس باریک اضافه‌ولتاژ) و **بوق: ۳ بیپ کوتاه + مکث**، دوره تکرار ۳s (پنجره بیپ ۹۰۰ms، گپ ۱۰۰ms بین بیپ‌ها). `UI_BAT_LOST_*` در `ui_led.h`.
- **جدایی از بوق بحرانی BatteryRun:** آن فقط با ورودی قطع اجرا می‌شود و این پرچم فقط در دنیای ورودی-حاضر ست می‌شود؛ به‌علاوه در `Ui_Tick` این سناریو **بعد از OverVoltage و قبل از همه سناریوهای نرمال** چک و return می‌شود، پس قاطی شدن محال است.
- **خروج:** با پاک‌شدن پرچم توسط Fault (باتری برگشت + ۱s پایدار)، سناریوی قبلی خودبه‌خود برمی‌گردد؛ پاک‌سازی بوق را همان سناریو بعدی با tick صفر انجام می‌دهد.

### خلاصه انتخاب
```
OverVoltage → خطا
else if Fault&FAULT_CHARGER_BAT_LOST → BatLost
else if !InputPresent → BatteryRun (2%+0/1 stable)
else if InputPresent:
  Update Full hysteresis (raw100→InputOk, <95→Charging)
  if FullActive → InputOk
  else Charging (5% stable)
invalid/NULL → safe-off flag false phase reset
BoardTest → یک‌بار قرمز/زرد/سبز + بوق
```

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-19 | چشمک زرد Charging فقط وقتی `func__Charger_IsAnyChannelActive()` (دستور کاربر؛ هرکدام از دو کانال یا هر دو فعال)؛ در غیر این‌صورت در شاخه ورودی-وصل، به‌جای چشمک، سبز ثابت |
| 2026-09-19 | معنای زرد «مانده تا فول» بازگردانده شد (دستور نهایی کاربر): هرچه پرتر روشن کوتاه‌تر |
| 2026-09-19 | سناریو ۵ BatLost از پرچم `FAULT_CHARGER_BAT_LOST` (قرمز چشمک‌تند ۵۰/۵۰ + سبز ثابت + ۳ بیپ/مکث) با اولویت بعد از OverVoltage و قبل از نرمال‌ها؛ جدایی کامل از بوق بحرانی دشارژ. |
| 2026-09-17 | هیسترزیس BatteryRun 2%+0/1، **Charging 5%** (`chargingStable 53..61 حفظ`)، **فول 100/95** (ورود 100 ماندن تا <95)، حفظ فاز سبز/زرد، Charging/InputOk non-blocking، README/Excel/host_test به‌روز، RTOS/stack 2.2. |
| 2026-09-17 | هیسترزیس 2% + فاز سبز + InputOk/Charging non-blocking (قبلی 2.1). |
| 2026-09-16 | snapshot واقعی + safe-off + flag پیوسته 21000/21200؛ Validation 2.0 |
| 2026-09-15 | سناریوهای کامل + هیسترزیس ورودی |
| 2026-09-15 | بوق یک API دوره‌ای |
| 2026-09-15 | جداسازی LED/BUZZER |

## فایل‌ها
| فایل | نقش |
|---|---|
| `ui_led.h/c` | نگاشت PA3/v_bat24, BatteryRun 2%+0/1 (`stable`), Charging 5% (`chargingStable`), فول 100/95, فاز سبز/زرد preserve, InputOk/Charging non-blocking, هیسترزیس ورودی/OverVoltage, Low Alarm 21000/21200, flag |
| `ui_buzzer.h/c` | سرویس بوق دوره‌ای |
| `task_ui.c` | 10ms `GetSnapshot→Ui_Tick` non-blocking |
| `bsp_gpio.c/h` | LED/BUZZER GPIO |
| `app_config.h` | زمان‌بندی LED |
| `host_test_ui.py` | تست بوق + BatteryRun 56/57/58 + 0/1 + Charging 5% (53..61) + Full 100/95 + فاز |
| `UI_Board_Validation.xlsx` | 6 شیت تست 25V jitter + فول + RTOS |
| `RTOS_Mapping_Report.md` | بررسی نگاشت و RTOS/static/stack |

## برگه تست روی برد
`UI_Board_Validation.xlsx` نسخه 2.2 با snapshot واقعی (راهنما/برنامه/مراجع/ایراد/تأیید/فهرست).

- دامنه LED/BUZZER؛ `v_bat PA3/raw2`, `v_in PA2/raw1`, PB4 EXTI, PB5 Q1, PB11 Q17 بدون تغییر.
- `valid` قبل تصمیم؛ invalid→safe-off flag false.
- **BatteryRun 25V:** `57` با `56/58` حفظ فاز، `55/59` تغییر از مرز بعدی؛ `0` 10s یک‌بار، `0↔1` no restart، `1` سه‌بوق، `>=3` خروج 1→2.
- **Charging 25V:** `stable57` با `53..61` زرد ثابت، `52/62` تغییر با حفظ فاز؛ jitter کوچک بی‌اثر.
- **Full:** `99→Charging`, `100→InputOk`, `99/96/95` ماندن InputOk، `94→Charging`.
- InputOk/Charging non-blocking، InputOverVoltage، invalid/NULL، Low Alarm، سوئیچ ورودی، BoardTest.
- RTOS static `DYNAMIC 0`, stack high-water, بدون queue/timer/mutex جدید.

## توابع
| نام | کار |
|---|---|
| `BatteryVoltageToPercent` | 21-28V→0-100% 4 گام |
| `UpdateBatteryStablePercent` | 2%+0/1 56/57/58 حفظ |
| `UpdateChargingStablePercent` | 5% 53..61 حفظ |
| `UpdateChargingFullHysteresis` | 100 ورود <95 خروج |
| `UpdateBatteryRunGreenBlink` | سبز فاز-حفاظ |
| `UpdateChargingYellowBlink` | زرد فاز-حفاظ |
| `Buzzer_Tick` | API بوق |
| `Init` | خاموش+reset stable/phase/full |
| `BoardTest_Start` | تست یک‌بار |
| `ScenarioInputOk` | non-blocking سبز ثابت |
| `ScenarioCharging_Tick` | سبز+زرد 5% non-blocking |
| `ScenarioBatteryRun_Tick` | سبز 2%+0/1 + بوق |
| `ScenarioInputOverVoltage_Tick` | قرمز50%+بوق10s |
| `Ui_Tick` | valid/safe-off + flag21000/21200 + انتخاب سناریو |

## محدودیت‌ها
| ثابت | مقدار | رفتار |
|---|---|---:|
| `BUZZER_MIN_PERIOD_MS` | 1000 | دوره کمتر نامعتبر |
| `BUZZER_MIN_GAP_MS` | 100 | گپ چندبوق |
| `BATTERY_RUN_HYSTERESIS` | 2% | BatteryRun |
| `CHARGING_HYSTERESIS` | 5% | Charging |
| `BATTERY_ZERO_EXIT` | 2% | خروج 0→1 |
| `BATTERY_ONE_EXIT` | 3% | خروج 1→2 |
| `CHARGING_FULL_ENTER` | 100% | ورود InputOk |
| `CHARGING_FULL_EXIT` | 95% | خروج InputOk |

کد `0` خاموش، `-1` نامعتبر.

## پایه‌ها
| پایه | لیبل | نقش | HIGH | منبع |
|---|---|---|---|
| PB0 | MCU_R_LED | قرمز | روشن | — |
| PB1 | MCU_Y_LED | زرد | چشمک 5% | — |
| PB10 | MCU_G_LED | سبز | 2% چشمک | — |
| PA4 | MCU_BUZZER | بازر | فعال | Active |
| PA3 | MCU_ADC_24_BAT پایه13 | ADC1_IN3 raw2 v_bat24 → BatteryRun | — | فقط PA3 |
| PA2 | MCU_ADC_24_IN پایه12 | ADC1_IN2 raw1 v_in → ورودی | — | فقط PA2 |
| PB4 | MCU_INT_24_IN | EXTI | High وصل | ورودی |
| PB5 | MCU_BAT_SWITCH Q1 | McuPowerPath | Low وصل | — |
| PB11 | MCU_PROTECT_BATT Q17 | Changeover | — | — |

## پیش‌فرض امن
Reset/Init، LED/بوق خاموش `flag false`, `stable 0 uninit`, `chargingStable 0`, `full false`, فازها reset. `invalid/NULL` → safe-off flag false فاز reset (stable حفظ). BatteryRun <1% 10s یک‌بار سپس خاموش تا خروج 0% (stable0→1 raw>=2). 1% سه‌بوق. InputOk/Charging خاموش. BoardTest حفظ.

## درخت اتصال
```
task_ui 10ms → GetSnapshot → Ui_Tick
 ├─ !valid → safe-off + phase reset
 ├─ flag 21000/21200
 ├─ input 20/21V + overvoltage 27/28V
 ├─ OverVoltage → red50%+buzzer10s
 ├─ !InputPresent → BatteryRun stable2%+0/1 → green preserve + buzzer
 ├─ InputPresent + raw100→FullActive→InputOk non-blocking
 └─ InputPresent + !FullActive→Charging stable5%→green+Yellow preserve
      └── Buzzer_Tick → BspGpio_Write
```
