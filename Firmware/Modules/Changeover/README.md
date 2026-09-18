/**
 * @file    README.md
 * @brief   [EN] Changeover module sheet: timed battery cut/reconnect with 3000ms.
 *          [FA] برگه ماژول Changeover: قطع/وصل باتری با فیلتر ۳ ثانیه.
 */

# ماژول Changeover

## وضعیت

پیاده‌سازی کامل منطق قطع/وصل با فیلتر 3000ms، `MODULE_CHANGEOVER = 1` است تا منطق واقعی روی تسک کنترل اجرا و روی برد تست شود. فقط از داده‌های مجاز استفاده می‌کند: `snapshot.valid`, `snapshot.v_bat24_mv`, `snapshot.input_present`, `fault_mask`, `BOOL__G__UiBatteryAlarmIssued`. تبدیل زمان فقط با `rtos_time.h` و بدون فرض `tick=1ms`. این ماژول مالک دو خروجی منطقی مسیر توان است: `BSP_GPIO_PROTECT_BATTERY` (قطع باتری، active-high) و `BSP_GPIO_BATTERY_SWITCH` (تغذیه PSU کنترل MCU از باتری، active-low)؛ `BSP_GPIO_RELAY` ممنوع است و تغییر نمی‌کند. `board_pins.h`، HAL و نام پایه فیزیکی در ماژول ممنوع است.

کلید توان باتری پیش از این مالک نداشت و هرگز نوشته نمی‌شد؛ در نتیجه با حذف ورودی، MCU پیش از هر تصمیمی بی‌تغذیه می‌ماند و سناریوی BatteryRun و پایش قطع باتری غیرممکن بود. سیاست فعلی: در حالت `BATTERY` و `SAFE` کلید روشن (MCU زنده می‌ماند)، در حالت `INPUT` خاموش (باتری به نت ورودی تزریق نمی‌کند)، در `BOOT/IDLE/FAULT` بدون تغییر.

منطق (snapshot-first):
- اگر `snapshot==NULL` یا `snapshot.valid==false` → هیچ تصمیمی، `state` حفظ، `PB11` حفظ، تایمرهای pending reset، `fault` هم در این حالت `state` را تغییر نمی‌دهد، زمان نامعتبر جزو 3000ms حساب نمی‌شود.
- فقط وقتی `snapshot` معتبر است و `fault_mask != FAULT_NONE` → `state=APP_STATE_FAULT` بدون تغییر هیچ pin.
- قطع با گیت: `v_bat24_mv<21000` و `BOOL__G__UiBatteryAlarmIssued==true` به‌مدت پیوسته 3000ms → قطع باتری (PB11 Low-Active).
- قطع مستقل: `v_bat24_mv<20800` مستقل از فلگ، به‌مدت پیوسته 3000ms → قطع باتری.
- وصل مجدد: `input_present==true` و `v_bat24_mv≥21200` به‌مدت پیوسته 3000ms → وصل باتری (PB11 High-Safe).

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-16 | پیاده‌سازی snapshot-first با `MillisecondsToTicks`، نگاشت حالت `BATTERY/INPUT/SAFE/FAULT` (بدون `IDLE` برای معتبر)، هندل `invalid` قبل از `fault`، فقط `BSP_GPIO_PROTECT_BATTERY` و ساخت `Changeover_Board_Validation.xlsx` با 21 سناریو (شامل fault+invalid). |
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت `func__Changeover_Init` / `func__Changeover_Evaluate` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `changeover.h` / `changeover.c` | ماشین حالت snapshot-first با تایمر 3000ms (`MillisecondsToTicks`) و فقط `BSP_GPIO_PROTECT_BATTERY`؛ آستانه‌ها `CHANGEOVER_*` |
| `../../Rtos/Src/task_control.c` | تسک مشترک؛ `func__Measurement_GetSnapshot(&snap)` + `func__Fault_Get()` + `func__Changeover_Evaluate(&snap, faults)`؛ فلگ UI به‌صورت سراسری `BOOL__G__UiBatteryAlarmIssued` (بدون API تکراری) |
| `../../Rtos/Src/task_ui.c` | مالک فلگ `BOOL__G__UiBatteryAlarmIssued` (تعریف در `ui_led.c`، هیسترزیس 21000/21200) |
| `../../Config/Inc/app_types.h` | `app_state_t`, `measurement_snapshot_t`, `fault_mask_t` |
| `Changeover_Board_Validation.xlsx` | برگه اعتبارسنجی با 21 سناریو (invalid، fault+invalid، fault+valid، فول، 21000+flag، 20800 مستقل، <3s، 3s، reconnect <3s/3s، PB5/PB7) |

`changeover.c` را به بیلد LED اضافه نکن.

## توابع

| نام | کار |
|---|---|
| `func__Changeover_Init` | `state=APP_STATE_BOOT`، تایمرها inactive، آینه‌کردن وضعیت امن BSP: `protect=true` (قطع باتری) و کلید باتری خاموش؛ پرچم‌ها برابر همان سطح‌ها |
| `func__Changeover_Evaluate(snap, faults)` | اگر `snap==NULL`/`!valid` → حفظ state/پین‌ها و reset تایمر (fault هم FAULT نمی‌شود)؛ وگرنه اگر `fault!=0` → `FAULT` بدون pin؛ وگرنه ارزیابی قطع (`v<20800` مستقل و `v<21000&&flag` گیت) و وصل (`input&&v>=21200`) هر کدام پیوسته 3000ms روی `BSP_GPIO_PROTECT_BATTERY` و سپس سیاست کلید باتری برای حالت برگشتی؛ زمان با `func__Rtos_MillisecondsToTicks` و بدون فرض `tick=1ms` |
| `TaskControl` | فقط اگر Changeover یا Charger یا Jitter یک باشد ساخته می‌شود |

حالت‌ها: `BOOT`، `INPUT`، `BATTERY`، `FAULT`، `SAFE` — `IDLE` برای حالت معتبر استفاده نمی‌شود؛ پس از `BOOT` و با `snapshot` معتبر بدون cut → `BATTERY` (input false) یا `INPUT` (input true)، پس از cut → `SAFE`، پس از reconnect → `INPUT`؛ `MODULE_CHANGEOVER` در build تست برد 1 است.

## پایه‌ها

Changeover از دو سیگنال منطقی `BSP_GPIO_PROTECT_BATTERY` (PB11 فیزیکی) و `BSP_GPIO_BATTERY_SWITCH` (PB5 فیزیکی) استفاده می‌کند و مالک هر دو است. پایهٔ فیزیکی و قطبیت در BSP پنهان است؛ ماژول `board_pins.h` و HAL را include نمی‌کند.

جدول زیر فقط مرجع فیزیکی برد فعلی است:

| پایه منطقی | پایه فیزیکی | لیبل | نقش | HIGH یعنی (شماتیک) |
|---|---|---|---|---|
| `BSP_GPIO_PROTECT_BATTERY` | PB11 | `MCU_LOW_BAT` / `MCU_PROTECT_BATT` | قطع مسیر باتری با Q17 و `GATE_ON_OFF` پایین | High = قطع باتری (active-high)؛ High امن شروع |
| `BSP_GPIO_BATTERY_SWITCH` | PB5 | `MCU_BAT_SWITCH` | تغذیه PSU کنترل MCU از باتری با Q1/Q2/Q3 | Low = کلید روشن (active-low)؛ High امن شروع |
| `BSP_GPIO_RELAY` (PB7) | PB7 | `MCU_PROTECT_CHARGER` | — | **ممنوع: تغییر نمی‌کند** |

قطبیت از شماتیک است، روی برد اندازه نشده.

## پیش‌فرض امن

- بعد از `func__Changeover_Init`، `state=BOOT` و `PB11` منطقی `true` (قطع باتری، برابر Safe High فیزیکی) و کلید باتری `false` (خاموش)؛ `BSP` در `func__BspGpio_Init` قبلاً همان سطح‌های امن را اعمال کرده است و Init پرچم‌ها را با آن‌ها هم‌خوان می‌کند.
- اگر `snapshot==NULL` یا `!valid` → `state` و `PB11` حفظ، تایمرهای pending reset، حتی `fault!=0` هم `FAULT` نمی‌شود و زمان نامعتبر جزو 3000ms حساب نمی‌شود.
- فقط وقتی `snapshot` معتبر است و `fault!=0` → `state=FAULT` بدون تغییر هیچ pin (تایمرها reset).
- زمان‌گیری فقط با `osKernelGetTickCount()` و `func__Rtos_MillisecondsToTicks(CHANGEOVER_DURATION_MS)` و بدون فرض `tick=1ms`؛ `MillisecondsToTicks` نرخ تیک را از `osKernelGetTickFreq()` می‌گیرد.
- تا اندازه‌گیری قطبیت، `PB11` در CubeMX نیز High (Safe) نگه داشته شود.

## درخت اتصال

صدا زده می‌شود از (وقتی فلگ ۱ شود):

```text
rtos_app.c → TaskControl → task_control.c
  measurement_snapshot_t snap; fault_mask_t faults;
  (void)func__Measurement_GetSnapshot(&snap); // valid, v_bat24_mv, input_present
  faults = func__Fault_Get();
  // UI owns flag: ui_led.c → BOOL__G__UiBatteryAlarmIssued (continuous, hysteresis 21000/21200)
  state = func__Changeover_Evaluate(&snap, faults);
    ├── snap==NULL || !valid → preserve state/PB11, reset timers, fault هم FAULT نمی‌شود (invalid not counted)
    ├── else if fault!=0 → state=FAULT, no pin change
    ├── else if v<20800 for 3000ms → BSP_GPIO_PROTECT_BATTERY = true (active-high, cut) → SAFE
    ├── else if v<21000 && flag==true for 3000ms → cut → SAFE
    ├── else if input==true && v>=21200 for 3000ms → BSP_GPIO_PROTECT_BATTERY = false (reconnect) → INPUT
    └── else valid without cut → INPUT (input true) یا BATTERY (input false)
```

این ماژول صدا می‌زند:

```text
changeover.c
  ├── bsp_gpio.h → func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true/false) // true=cut/protect, false=reconnect
  └── bsp_gpio.h → func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true/false) // true=MCU on battery, false=off
  ├── rtos_time.h → func__Rtos_MillisecondsToTicks(CHANGEOVER_DURATION_MS) + osKernelGetTickCount()
  ├── cmsis_os2.h → osKernelGetTickCount()
  ├── ui_led.h (extern) → BOOL__G__UiBatteryAlarmIssued (read only)
  └── app_types.h → app_state_t, measurement_snapshot_t, fault_mask_t

snapshot از Measurement می‌آید، faults از Fault، flag از UI؛ Changeover آن‌ها را include نمی‌کند جز flag به‌صورت extern. فقط BOARD_PINS/HAL ممنوع.
```
```
