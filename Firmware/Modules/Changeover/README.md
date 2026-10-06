/**
 * @file    README.md
 * @brief   [EN] Changeover module sheet: timed battery cut/reconnect with 3000ms.
 *          [FA] برگه ماژول Changeover: قطع/وصل باتری با فیلتر ۳ ثانیه.
 */

# ماژول Changeover

## وضعیت

پیاده‌سازی کامل منطق قطع/وصل با فیلتر 3000ms، `MODULE_CHANGEOVER = 1` است تا منطق واقعی روی تسک کنترل اجرا و روی برد تست شود. فقط از داده‌های مجاز استفاده می‌کند: `snapshot.valid`, `snapshot.v_bat24_mv`, `snapshot.input_present`, `fault_mask` (قفل باتری کم از v1.74 داخلی است). تبدیل زمان فقط با `rtos_time.h` و بدون فرض `tick=1ms`. فقط `BSP_GPIO_PROTECT_BATTERY` (PB11 منطقی) استفاده می‌شود؛ `PB5` و `PB7` ممنوع و به هیچ‌وجه تغییر نمی‌کنند. `board_pins.h`، HAL و پایه فیزیکی در ماژول ممنوع است.

منطق (snapshot-first):
- اگر `snapshot==NULL` یا `snapshot.valid==false` → هیچ تصمیمی، `state` حفظ، `PB11` حفظ، تایمرهای pending reset، `fault` هم در این حالت `state` را تغییر نمی‌دهد، زمان نامعتبر جزو 3000ms حساب نمی‌شود.
- فقط وقتی `snapshot` معتبر است و `fault_mask != FAULT_NONE` → `state=APP_STATE_FAULT` بدون تغییر هیچ pin.
- قطع با گیت: `v_bat24_mv<21000` و قفل داخلی باتری کم (ست زیر ۲۱۰۰۰، پاک در ۲۱۲۰۰) به‌مدت پیوسته 3000ms → قطع باتری (PB11 Low-Active).
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
| `../../Rtos/Src/task_control.c` | تسک مشترک؛ `func__Measurement_GetSnapshot(&snap)` + `func__Fault_Get()` + `func__Changeover_Evaluate(&snap, faults)` (از v1.74 بدون هیچ فلگ UI) |
| `../../Rtos/Src/task_ui.c` | فقط LED و بازر؛ از v1.74 هیچ فلگ باتری کمی ندارد |
| `../../Config/Inc/app_types.h` | `app_state_t`, `measurement_snapshot_t`, `fault_mask_t` |
| `Changeover_Board_Validation.xlsx` | برگه اعتبارسنجی با 21 سناریو (invalid، fault+invalid، fault+valid، فول، 21000+flag، 20800 مستقل، <3s، 3s، reconnect <3s/3s، PB5/PB7) |

`changeover.c` را به بیلد LED اضافه نکن.

## توابع

| نام | کار |
|---|---|
| `func__Changeover_Init` | `s_state=APP_STATE_BOOT`، تایمرها inactive، `protect=false` (safe High) |
| `func__Changeover_Evaluate(snap, faults)` | اگر `snap==NULL`/`!valid` → حفظ state/PB11 و reset تایمر (fault هم FAULT نمی‌شود)؛ وگرنه اگر `fault!=0` → `FAULT` بدون pin؛ وگرنه ارزیابی قطع (`v<20800` مستقل و `v<21000&&flag` گیت) و وصل (`input&&v>=21200`) هر کدام پیوسته 3000ms فقط روی `BSP_GPIO_PROTECT_BATTERY`؛ زمان با `func__Rtos_MillisecondsToTicks` و بدون فرض `tick=1ms` |
| `TaskControl` | فقط اگر Changeover یا Charger یا Jitter یک باشد ساخته می‌شود |

حالت‌ها: `BOOT`، `INPUT`، `BATTERY`، `FAULT`، `SAFE` — `IDLE` برای حالت معتبر استفاده نمی‌شود؛ پس از `BOOT` و با `snapshot` معتبر بدون cut → `BATTERY` (input false) یا `INPUT` (input true)، پس از cut → `SAFE`، پس از reconnect → `INPUT`؛ `MODULE_CHANGEOVER` در build تست برد 1 است.

## پایه‌ها

Changeover **فقط** از سیگنال منطقی `BSP_GPIO_PROTECT_BATTERY` (PB11 فیزیکی) استفاده می‌کند. پایهٔ فیزیکی و قطبیت در BSP پنهان است؛ ماژول `board_pins.h` و HAL را include نمی‌کند.

جدول زیر فقط مرجع فیزیکی برد فعلی است:

| پایه منطقی | پایه فیزیکی | لیبل | نقش | HIGH یعنی (شماتیک) |
|---|---|---|---|---|
| `BSP_GPIO_PROTECT_BATTERY` | PB11 | `MCU_LOW_BAT` / `MCU_PROTECT_BATT` | قطع مسیر باتری Q17 | High امن و مسیر غیرفعال (فعال Low) |
| `BSP_GPIO_BATTERY_SWITCH` (PB5) | PB5 | `MCU_BAT_SWITCH` | — | **ممنوع: تغییر نمی‌کند** |
| `BSP_GPIO_RELAY` (PB7) | PB7 | `MCU_PROTECT_CHARGER` | — | **ممنوع: تغییر نمی‌کند** |

قطبیت از شماتیک است، روی برد اندازه نشده.

## پیش‌فرض امن

- بعد از `func__Changeover_Init`، `state=BOOT` و `PB11` منطقی `false` (Safe High فیزیکی)؛ `BSP` در `func__BspGpio_Init` قبلاً Safe High را اعمال کرده است.
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
  state = func__Changeover_Evaluate(&snap, faults);
    ├── snap==NULL || !valid → preserve state/PB11, reset timers, fault هم FAULT نمی‌شود (invalid not counted)
    ├── else if fault!=0 → state=FAULT, no pin change
    ├── else if v<20800 for 3000ms → BSP_GPIO_PROTECT_BATTERY = true (Low active, cut) → SAFE
    ├── else if v<21000 && قفل داخلی باتری کم (21000/21200) for 3000ms → cut → SAFE
    ├── else if input==true && v>=21200 for 3000ms → BSP_GPIO_PROTECT_BATTERY = false (High safe, reconnect) → INPUT
    └── else valid without cut → INPUT (input true) یا BATTERY (input false)
```

این ماژول صدا می‌زند:

```text
changeover.c
  ├── bsp_gpio.h → func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true/false) // true=cut/protect, false=reconnect
  ├── rtos_time.h → func__Rtos_MillisecondsToTicks(CHANGEOVER_DURATION_MS) + osKernelGetTickCount()
  ├── cmsis_os2.h → osKernelGetTickCount()
  └── app_types.h → app_state_t, measurement_snapshot_t, fault_mask_t

snapshot از Measurement می‌آید، faults از Fault، flag از UI؛ Changeover آن‌ها را include نمی‌کند جز flag به‌صورت extern. فقط BOARD_PINS/HAL ممنوع.
```
```

## تستر

`Tester/` — تست هاست ماشین حالت با ساعت و GPIO بدلی (۱۱۴ بررسی، شامل رگرسیون
قفل باتری‌ضعیف v1.81 و سرریز تیک). اجرا با
`Tester/run_host_test_changeover.sh`؛ داخل `tools/check_firmware_syntax.sh` هم
اجرا می‌شود. توضیح کامل در `Tester/README.md`.

## ماشین حالت

`Changeover_State_Machine.xlsx` — حالت‌ها و گذارهای همین ماژول، رنگی و
راست‌به‌چپ با فونت وزیرمتن. از برگهٔ «ماشین حالت» در فایل اعتبارسنجی همین
پوشه هم به آن لینک هست و خودش به نمای سیستمی
(`Documentation/System_State_Machine.xlsx`) برمی‌گردد. بازتولید:
`python3 tools/make_module_state_machines.py`.
