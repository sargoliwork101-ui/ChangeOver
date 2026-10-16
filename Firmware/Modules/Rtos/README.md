/**
 * @file    README.md
 * @brief   [EN] Rtos module sheet: tasks, scheduling, watchdog.
 *          [FA] برگهٔ ماژول Rtos: تسک‌ها، زمان‌بندی، واچ‌داگ.
 */

# ماژول Rtos

## وضعیت

**فعال.** تسک‌های CMSIS-RTOS2 با تخصیص استاتیک، بدون malloc/free، بدون HAL_Delay. ترتیب اجرای ماژول‌ها در `task_control` ثابت است.

## فایل‌ها

| فایل | نقش |
|---|---|
| `../../Rtos/Inc/rtos_app.h` / `rtos_config.h` | قرارداد تسک‌ها، اندازه استک، اولویت |
| `../../Rtos/Src/rtos_app.c` | ساخت threadها با cb_mem و stack_mem استاتیک |
| `../../Rtos/Src/task_measurement.c` | تسک اندازه‌گیری: Init+Start، Run هر 1ms |
| `../../Rtos/Src/task_control.c` | تسک کنترل: snapshot zero کامل 9 فیلد قبل از GetSnapshot، ترتیب imbalance(prev)->fault->charger technical fault->mcu_power_path->imbalance evaluate->changeover->charger evaluate->watchdog poll |
| `../../Rtos/Src/task_comm.c` | تسک ارتباط: snapshot zero + EspLink |
| `../../Rtos/Src/task_ui.c` | تسک UI: snapshot zero + Ui_Tick |
| `../../Rtos/Src/task_protection.c` | تسک حفاظت: snapshot zero + Protection_Run |
| `../../Rtos/Src/rtos_utils.c` | MillisecondsToTicks و Delay helpers |
| `Rtos_Validation.xlsx` | برگه اعتبارسنجی با 11 سناریو (RT-001..RT-011): snapshot zeroing، watchdog، ترتیب اجرا، Enter/Esc |

## توابع

| نام | کار |
|---|---|
| `func__Rtos_Init` | threadها را با cb_mem/stack_mem استاتیک می‌سازد |
| `func__Rtos_MillisecondsToTicks` | ms به tick با `osKernelGetTickFreq()` بدون فرض 1kHz |
| `func__Rtos_DelayUntil` | تأخیر تا زمان مطلق با جبران drift |
| `TaskMeasurement` | Init+Start ADC و loop 1ms با check-in watchdog |
| `TaskControl` | snapshot zero کامل 9 فیلد، Fault_Evaluate، Jitter، McuPowerPath، Imbalance، Changeover، Charger، PollKick |
| `TaskComm` | snapshot zero + Fault_Get + EspLink_Run |
| `TaskUi` | snapshot zero + Ui_Tick |
| `TaskProtection` | snapshot zero + Protection_Run |

## پایه‌ها

Rtos پایهٔ فیزیکی ندارد. فقط `BSP_IWDG` را از طریق `bsp_iwdg.h` مصرف می‌کند (check-in در هر تسک، kick فقط در control). HAL و board_pins در Rtos ممنوع.

| سیگنال منطقی | نقش |
|---|---|
| `BSP_IWDG_SLOT_MEASUREMENT` | check-in تسک measurement |
| `BSP_IWDG_SLOT_CONTROL` | check-in تسک control |
| `BSP_IWDG_SLOT_COMM` | check-in تسک comm |
| `BSP_IWDG_SLOT_UI` | check-in تسک ui |
| `BSP_IWDG_SLOT_PROTECTION` | check-in تسک protection |

## پیش‌فرض امن

- بعد از `Init` همه threadها با استک استاتیک ساخته می‌شوند، بدون malloc/free، بدون HAL_Delay.
- هر تسک قبل از `Measurement_GetSnapshot` ساختار `measurement_snapshot_t__snap` را کامل صفر می‌کند: `v_in=0 v_bat24=0 v_bat12=0 v_low=0 v_high=0 i1=0 i2=0 input_present=false valid=false` تا از indeterminate stack جلوگیری شود (RT-001..RT-004 / CO-023).
- `BSP_IWDG` فقط در `task_control` با `PollKick` کیک می‌زند وقتی همه slotها تازه‌اند؛ اگر یک تسک stale شود kick انجام نمی‌شود.
- زمان‌گیری فقط با `osKernelGetTickCount()` و `MillisecondsToTicks` بدون فرض tick=1ms.
- ترتیب اجرای ماژول‌ها در `task_control` ثابت است: imbalance(prev) -> fault -> charger technical fault -> mcu_power_path -> imbalance evaluate -> changeover -> charger evaluate -> watchdog poll.

## درخت اتصال

صدا زده می‌شود از:

```text
main.c → func__Rtos_Init → osKernelStart
  ├── TaskMeasurement → Measurement_Init/Run
  ├── TaskControl → snapshot zero → Fault_Evaluate → McuPowerPath → Changeover → Charger → PollKick
  ├── TaskComm → snapshot zero → EspLink_Run
  ├── TaskUi → snapshot zero → Ui_Tick
  └── TaskProtection → snapshot zero → Protection_Run
```

این ماژول صدا می‌زند:

```text
rtos_app.c
  ├── cmsis_os2.h → osThreadNew با cb_mem/stack_mem استاتیک، osDelayUntil
  ├── bsp_iwdg.h → BspIwdg_CheckIn(SLOT_*) در هر تسک، BspIwdg_PollKick در control
  └── rtos_time.h → MillisecondsToTicks با osKernelGetTickFreq()
task_control.c
  ├── measurement.h → GetSnapshot (بعد از zero کامل)
  ├── fault.h → Fault_Evaluate / Fault_Get
  ├── changeover.h / charger.h / mcu_power_path.h / imbalance.h / jitter.h
  └── protection.h (از طریق task_protection)
```

## تاریخچه

تاریخچهٔ تغییرات در `git log` همین مخزن نگهداری می‌شود. مهم‌ترین نقاط:

- v1.87 (2026-10-12): snapshot zero کامل در `task_control.c` — هر 9 فیلد صفر قبل از GetSnapshot مانند task_comm/ui/protection (CO-023 / RT-001). commit 62f3782.
- v1.86: panel dialog #pmod با queue PMQ و Enter=تأیید Esc=لغو و استایل تیره #121a2c/#35507f.
- v1.84: ترتیب ثابت اجرا در task_control با کامنت صریح.
- v1.80: static allocation کامل، حذف heap_4، بدون HAL_Delay.

## تست

`Rtos_Validation.xlsx` — شامل RT-001..RT-011 (RT-011: تبدیل تیک به ms در چک‌این‌های IWDG، کامیت 004fbf1، تست برد در انتظار)، تاریخ پایه 2026-10-12، کامیت 62f3782.

