/**
 * @file    README.md
 * @brief   [EN] Changeover module sheet: timed battery cut/reconnect with 3000ms.
 *          [FA] برگه ماژول Changeover: قطع/وصل باتری با فیلتر ۳ ثانیه.
 */

# ماژول Changeover

## وضعیت

پیاده‌سازی کامل منطق قطع/وصل با فیلتر 3000ms، `MODULE_CHANGEOVER = 1` است تا منطق واقعی روی تسک کنترل اجرا و روی برد تست شود. فقط از داده‌های مجاز استفاده می‌کند: `snapshot.valid`, `snapshot.v_bat24_mv`, `snapshot.input_present`, `fault_mask`, `BOOL__G__UiBatteryAlarmIssued`. تبدیل زمان فقط با `rtos_time.h` و بدون فرض `tick=1ms`. مسیر باتری با دو پایه منطقی `BSP_GPIO_PROTECT_BATTERY` (PB11) و `BSP_GPIO_BATTERY_SWITCH` (PB5) کنترل می‌شود؛ `PB7` (رله) در اختیار Charger است و توسط Changeover تغییر نمی‌کند. `board_pins.h`، HAL و پایه فیزیکی در ماژول ممنوع است.

منطق (snapshot-first):
- اگر `snapshot==NULL` یا `snapshot.valid==false` → هیچ تصمیمی، `state` حفظ، `PB11/PB5` حفظ، تایمرهای pending reset، `fault` هم در این حالت `state` را تغییر نمی‌دهد، زمان نامعتبر جزو 3000ms حساب نمی‌شود.
- فقط وقتی `snapshot` معتبر است و `fault_mask != FAULT_NONE` → `state=APP_STATE_FAULT` بدون تغییر هیچ pin.
- قطع با گیت: `v_bat24_mv<21000` و `BOOL__G__UiBatteryAlarmIssued==true` به‌مدت پیوسته 3000ms → قطع باتری (PB5 OFF + PB11 asserted).
- قطع مستقل: `v_bat24_mv<20800` مستقل از فلگ، به‌مدت پیوسته 3000ms → قطع باتری (PB5 OFF + PB11 asserted).
- وصل مجدد: `input_present==true` و `v_bat24_mv≥21200` به‌مدت پیوسته 3000ms → وصل باتری (PB5 ON + PB11 deasserted).
- در حالت عادی BATTERY/INPUT → باتری وصل (PB5 ON + PB11 deasserted)، در SAFE → باتری قطع (PB5 OFF + PB11 asserted).

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
| `changeover.h` / `changeover.c` | ماشین حالت snapshot-first با تایمر 3000ms (`MillisecondsToTicks`) و دو پایه باتری `BSP_GPIO_PROTECT_BATTERY` + `BSP_GPIO_BATTERY_SWITCH`؛ آستانه‌ها `CHANGEOVER_*` و helper `ApplyBatteryPath` |
| `../../Rtos/Src/task_control.c` | تسک مشترک؛ `func__Measurement_GetSnapshot(&snap)` + `func__Fault_Get()` + `func__Changeover_Evaluate(&snap, faults)`؛ فلگ UI به‌صورت سراسری `BOOL__G__UiBatteryAlarmIssued` (بدون API تکراری) |
| `../../Rtos/Src/task_ui.c` | مالک فلگ `BOOL__G__UiBatteryAlarmIssued` (تعریف در `ui_led.c`، هیسترزیس 21000/21200) |
| `../../Config/Inc/app_types.h` | `app_state_t`, `measurement_snapshot_t`, `fault_mask_t` |
| `Changeover_Board_Validation.xlsx` | برگه اعتبارسنجی با 21 سناریو (invalid، fault+invalid، fault+valid، فول، 21000+flag، 20800 مستقل، <3s، 3s، reconnect <3s/3s، PB5/PB7) |

`changeover.c` را به بیلد LED اضافه نکن.

## توابع

| نام | کار |
|---|---|
| `func__Changeover_Init` | `s_state=APP_STATE_BOOT`، تایمرها inactive، `protect=false` + `batterySwitchOn=false`، هر دو پایه باتری در وضعیت خاموش امن (PB5 OFF + PB11 deasserted) |
| `func__Changeover_Evaluate(snap, faults)` | اگر `snap==NULL`/`!valid` → حفظ state و هر دو پایه باتری و reset تایمر؛ وگرنه اگر `fault!=0` → `FAULT` بدون pin؛ وگرنه ارزیابی قطع (`v<20800` مستقل و `v<21000&&flag` گیت) و وصل (`input&&v>=21200`) هر کدام 3000ms روی هر دو پایه باتری؛ زمان با `func__Rtos_MillisecondsToTicks` |
| `func__Changeover_ApplyBatteryPath` | helper داخلی: ON = PB5 ON + PB11 deasserted، OFF = PB5 OFF + PB11 asserted، shadow state همگام |
| `TaskControl` | فقط اگر Changeover یا Charger یا Jitter یک باشد ساخته می‌شود |

حالت‌ها: `BOOT`، `INPUT`، `BATTERY`، `FAULT`، `SAFE` — `IDLE` برای حالت معتبر استفاده نمی‌شود؛ پس از `BOOT` و با `snapshot` معتبر بدون cut → `BATTERY` (input false) یا `INPUT` (input true)، پس از cut → `SAFE`، پس از reconnect → `INPUT`؛ `MODULE_CHANGEOVER` در build تست برد 1 است.

## پایه‌ها

Changeover از دو سیگنال منطقی باتری استفاده می‌کند؛ پایه فیزیکی و قطبیت در BSP پنهان است؛ ماژول `board_pins.h` و HAL را include نمی‌کند.

جدول زیر فقط مرجع فیزیکی برد فعلی است:

| پایه منطقی | پایه فیزیکی | لیبل | نقش | HIGH یعنی (شماتیک) |
|---|---|---|---|---|
| `BSP_GPIO_PROTECT_BATTERY` | PB11 | `MCU_LOW_BAT` / `MCU_PROTECT_BATT` | قطع مسیر باتری Q17 | High امن و مسیر وصل (فعال Low = قطع) |
| `BSP_GPIO_BATTERY_SWITCH` | PB5 | `MCU_BAT_SWITCH` | سوئیچ اصلی مسیر باتری | High امن خاموش (فعال Low = روشن)، در BATTERY/INPUT روشن، در SAFE خاموش |
| `BSP_GPIO_RELAY` (PB7) | PB7 | `MCU_PROTECT_CHARGER` | رله ورودی ترانس | **ممنوع: در اختیار Charger** |

قطبیت از شماتیک است، روی برد اندازه نشده.

## پیش‌فرض امن

- بعد از `func__Changeover_Init`، `state=BOOT` و هر دو پایه باتری خاموش امن: PB5 OFF (High) و PB11 deasserted (High)؛ مسیر باتری از طریق PB5 قطع است.
- اگر `snapshot==NULL` یا `!valid` → `state` و هر دو پایه باتری حفظ، تایمرهای pending reset، حتی `fault!=0` هم `FAULT` نمی‌شود و زمان نامعتبر جزو 3000ms حساب نمی‌شود.
- فقط وقتی `snapshot` معتبر است و `fault!=0` → `state=FAULT` بدون تغییر هیچ pin (تایمرها reset).
- زمان‌گیری فقط با `osKernelGetTickCount()` و `func__Rtos_MillisecondsToTicks(CHANGEOVER_DURATION_MS)` و بدون فرض `tick=1ms`؛ `MillisecondsToTicks` نرخ تیک را از `osKernelGetTickFreq()` می‌گیرد.
- در BATTERY/INPUT: PB5 ON (Low) + PB11 deasserted (High) = مسیر وصل؛ در SAFE: PB5 OFF (High) + PB11 asserted (Low) = مسیر قطع دوگانه.

## درخت اتصال

صدا زده می‌شود از (وقتی فلگ ۱ شود):

```text
rtos_app.c → TaskControl → task_control.c
  measurement_snapshot_t snap; fault_mask_t faults;
  (void)func__Measurement_GetSnapshot(&snap); // valid, v_bat24_mv, input_present
  faults = func__Fault_Get();
  // UI owns flag: ui_led.c → BOOL__G__UiBatteryAlarmIssued (continuous, hysteresis 21000/21200)
  state = func__Changeover_Evaluate(&snap, faults);
    ├── snap==NULL || !valid → preserve state + PB5/PB11, reset timers, fault هم FAULT نمی‌شود (invalid not counted)
    ├── else if fault!=0 → state=FAULT, no pin change (PB5/PB11 preserved)
    ├── else if v<20800 for 3000ms → battery path OFF (PB5 OFF + PB11 asserted) → SAFE
    ├── else if v<21000 && flag==true for 3000ms → battery OFF → SAFE
    ├── else if input==true && v>=21200 for 3000ms → battery ON (PB5 ON + PB11 deasserted) → INPUT
    └── else valid without cut → INPUT (input true) یا BATTERY (input false) با battery ON
```

این ماژول صدا می‌زند:

```text
changeover.c
  ├── bsp_gpio.h → func__BspGpio_Write(PB5/PB11)
  │     ├── true  PB5 = ON (Low active) / PB11 = cut (Low active)
  │     └── false PB5 = OFF (High safe) / PB11 = reconnect (High safe)
  ├── rtos_time.h → func__Rtos_MillisecondsToTicks(CHANGEOVER_DURATION_MS) + osKernelGetTickCount()
  ├── cmsis_os2.h → osKernelGetTickCount()
  ├── ui_led.h (extern) → BOOL__G__UiBatteryAlarmIssued (read only)
  └── app_types.h → app_state_t, measurement_snapshot_t, fault_mask_t

snapshot از Measurement می‌آید، faults از Fault، flag از UI؛ Changeover آن‌ها را include نمی‌کند جز flag به‌صورت extern. PB7 و BOARD_PINS/HAL ممنوع.
```
```
