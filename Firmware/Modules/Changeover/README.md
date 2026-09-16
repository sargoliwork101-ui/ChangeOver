/**
 * @file    README.md
 * @brief   [EN] Changeover module sheet: timed battery cut/reconnect with 3000ms.
 *          [FA] برگه ماژول Changeover: قطع/وصل باتری با فیلتر ۳ ثانیه.
 */

# ماژول Changeover

## وضعیت

پیاده‌سازی کامل منطق قطع/وصل با فیلتر 3000ms، اما `MODULE_CHANGEOVER = 0` باقی است (فعال‌سازی در مرحله بعد). فقط از داده‌های مجاز استفاده می‌کند: `snapshot.valid`, `snapshot.v_bat24_mv`, `snapshot.input_present`, `fault_mask`, `BOOL__G__UiBatteryAlarmIssued`. تبدیل زمان فقط با `rtos_time.h` و بدون فرض `tick=1ms`. فقط `BSP_GPIO_PROTECT_BATTERY` (PB11 منطقی) استفاده می‌شود؛ `PB5` و `PB7` ممنوع و به هیچ‌وجه تغییر نمی‌کنند. `board_pins.h`، HAL و پایه فیزیکی در ماژول ممنوع است.

منطق:
- اگر `fault_mask != FAULT_NONE` → `state=APP_STATE_FAULT`، هیچ pin تغییر نکند.
- اگر `snapshot` نامعتبر → هیچ تصمیمی، `state` حفظ، `PB11` حفظ، زمان نامعتبر جزو 3000ms محاسبه نشود.
- قطع با گیت: `v<21000` و `BOOL__G__UiBatteryAlarmIssued==true` به‌مدت پیوسته 3000ms → قطع باتری (PB11 Low-Active).
- قطع مستقل: `v<20800` به‌مدت پیوسته 3000ms → قطع باتری (بدون فلگ).
- وصل مجدد: `input_present==true` و `v≥21200` به‌مدت پیوسته 3000ms → وصل باتری (PB11 High-Safe).

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-16 | پیاده‌سازی کامل با تایمر 3000ms قابل‌حمل (rtos_time)، هندل invalid/fault، فقط BSP_GPIO_PROTECT_BATTERY، آستانه‌های 21000/20800/21200 و ساخت `Changeover_Validation.xlsx` با 20 سناریو. |
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت `func__Changeover_Init` / `func__Changeover_Evaluate` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `changeover.h` / `changeover.c` | ماشین حالت با تایمر 3000ms و فقط `BSP_GPIO_PROTECT_BATTERY`؛ آستانه‌ها `CHANGEOVER_*` |
| `../../Rtos/Src/task_control.c` | تسک مشترک؛ `func__Measurement_GetSnapshot(&snap)` + `func__Fault_Get()` + `func__Changeover_Evaluate(&snap, faults)`؛ فلگ UI به‌صورت سراسری (بدون API تکراری) |
| `../../Rtos/Src/task_ui.c` | مالک فلگ `BOOL__G__UiBatteryAlarmIssued` (تغییر در `ui_led.c`) |
| `../../Config/Inc/app_types.h` | `app_state_t`, `measurement_snapshot_t`, `fault_mask_t` |
| `Changeover_Validation.xlsx` | برگه اعتبارسنجی با 20 سناریو (invalid، فول، 21000+فلگ، 20800، <3s، 3s، reconnect، fault، PB5/PB7) |
| `Changeover_Board_Validation.xlsx` | کپی همان فایل برای سازگاری نام |

`changeover.c` را به بیلد LED اضافه نکن.

## توابع

| نام | کار |
|---|---|
| `func__Changeover_Init` | `s_state=APP_STATE_BOOT`، تایمرها inactive، `protect=false` (safe High) |
| `func__Changeover_Evaluate(snap, faults)` | اگر fault→FAULT بدون pin؛ اگر invalid→حفظ state/PB11 و reset تایمر؛ وگرنه ارزیابی قطع (21000+flag / 20800) با 3000ms و وصل (input+21200) با 3000ms فقط روی `BSP_GPIO_PROTECT_BATTERY`؛ زمان با `func__Rtos_TicksToMilliseconds` |
| `TaskControl` | فقط اگر Changeover یا Charger یا Jitter یک باشد ساخته می‌شود |

حالت‌ها: `BOOT`، `IDLE`، `INPUT`، `BATTERY`، `FAULT`، `SAFE` — در فاز فعلی `FAULT` و `SAFE`/`IDLE` پس از قطع/وصل استفاده می‌شوند؛ `MODULE_CHANGEOVER` هنوز 0 است.

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
- در `fault !=0` حالت `FAULT` و هیچ pin تغییر نمی‌کند (تایمرها reset).
- در `snapshot` نامعتبر، `state` و `PB11` حفظ و تایمرها reset می‌شوند تا زمان نامعتبر شمرده نشود.
- زمان‌گیری فقط با `osKernelGetTickCount()` و `func__Rtos_TicksToMilliseconds()` (یا `MillisecondsToTicks`) و بدون فرض `tick=1ms`.
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
    ├── fault !=0 → state=FAULT, no pin change
    ├── !snap.valid → preserve state/PB11, reset timers (invalid not counted)
    ├── v<20800 for 3000ms → BSP_GPIO_PROTECT_BATTERY = true (Low active, cut)
    ├── v<21000 && flag==true for 3000ms → cut
    └── input==true && v>=21200 for 3000ms → BSP_GPIO_PROTECT_BATTERY = false (High safe, reconnect)
```

این ماژول صدا می‌زند:

```text
changeover.c
  ├── bsp_gpio.h → func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true/false)
  ├── rtos_time.h → func__Rtos_TicksToMilliseconds() / osKernelGetTickCount()
  ├── cmsis_os2.h → osKernelGetTickCount()
  ├── ui_led.h (extern) → BOOL__G__UiBatteryAlarmIssued (read only)
  └── app_types.h → app_state_t, measurement_snapshot_t, fault_mask_t

snapshot از Measurement می‌آید، faults از Fault، flag از UI؛ Changeover آن‌ها را include نمی‌کند جز flag به‌صورت extern.
```
