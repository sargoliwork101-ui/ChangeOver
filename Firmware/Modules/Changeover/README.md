/**
 * @file    README.md
 * @brief   [EN] Changeover module sheet: valid-snapshot battery protection and logical path control.
 *          [FA] برگه ماژول Changeover: حفاظت باتری بر پایهٔ snapshot معتبر و کنترل مسیر منطقی.
 */

# ماژول Changeover

## وضعیت

پیاده‌سازی شده اما `MODULE_CHANGEOVER = 0` باقی می‌ماند. فعال‌سازی محصول فقط بعد از تست سخت‌افزاری و تصمیم نهایی انجام می‌شود. این ماژول فقط سیگنال منطقی `BSP_GPIO_PROTECT_BATTERY` را مصرف می‌کند.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-16 | پیاده‌سازی حفاظت باتری با snapshot معتبر، آستانه‌های ۲۱/۲۰.۸/۲۱.۲ ولت، پایداری ۳۰۰۰ms و مصرف فلگ آلارم UI. |
| 2026-09-14 | درخت اتصال و قرارداد اولیهٔ state machine اضافه شد. |

## فایل‌ها

| فایل | نقش |
|---|---|
| `changeover.h` / `changeover.c` | ماشین حالت، زمان‌سنجی قطع/وصل و کنترل منطقی حفاظت باتری |
| `../../Rtos/Src/task_control.c` | انتقال snapshot، fault mask و فلگ UI به Changeover |
| `../../Config/Inc/app_types.h` | `app_state_t`، `measurement_snapshot_t` و `fault_mask_t` |
| `../../Bsp/Inc/bsp_gpio.h` | API منطقی حفاظت باتری؛ mapping فیزیکی خارج از Module است |
| `Changeover_Board_Validation.xlsx` | برنامهٔ تست سناریوهای قطع، وصل، fault و snapshot نامعتبر |

## توابع

| نام | کار |
|---|---|
| `func__Changeover_Init` | state را روی `APP_STATE_BOOT` می‌گذارد و پایهٔ BSP را تحریک نمی‌کند |
| `func__Changeover_Evaluate` | snapshot معتبر، fault، فلگ UI و شرایط سه‌ثانیه‌ای را ارزیابی می‌کند |
| `TaskControl` | فقط در صورت فعال‌شدن Module، ورودی‌های واقعی را به Changeover می‌دهد |

## قرارداد عملکرد

- `snapshot == NULL` یا `snapshot.valid == false` هیچ تصمیمی ایجاد نمی‌کند؛ state و پایه حفظ می‌شوند و زمان در انتظار پاک می‌شود.
- `fault_mask != FAULT_NONE` state را `APP_STATE_FAULT` می‌کند و پایه را تغییر نمی‌دهد.
- قطع با `v_bat24_mv < 21000mV` و `BOOL__G__UiBatteryAlarmIssued == true` پس از ۳۰۰۰ms پیوسته انجام می‌شود.
- قطع مستقل با `v_bat24_mv < 20800mV` پس از ۳۰۰۰ms پیوسته انجام می‌شود.
- وصل مجدد فقط با `input_present == true` و `v_bat24_mv >= 21200mV` پس از ۳۰۰۰ms پیوسته انجام می‌شود.
- نبود ورودی همراه باتری معتبر و فول، حالت عادی `APP_STATE_BATTERY` است و خطای ADC یا قطع حفاظتی ایجاد نمی‌کند.
- زمان از `rtos_time.h` تبدیل می‌شود و فرض tick برابر ۱ms وجود ندارد.

## پایه‌ها

Changeover پایهٔ فیزیکی را نمی‌شناسد و فقط این سیگنال منطقی را مصرف می‌کند:

```text
BSP_GPIO_PROTECT_BATTERY
```

`PB11` فقط در BSP به این شناسه نگاشت می‌شود. `PB5` و `PB7` در Changeover ممنوع هستند. مقدار منطقی `true` یعنی asserted/protect و مقدار منطقی `false` یعنی deasserted/reconnect؛ قطبیت فیزیکی داخل BSP پنهان است.

## پیش‌فرض امن

`func__Changeover_Init` هیچ پایه‌ای را تغییر نمی‌دهد و safe startup بر عهدهٔ BSP است. تا وقتی `MODULE_CHANGEOVER` صفر است، TaskControl و این state machine در محصول اجرا نمی‌شوند.

## درخت اتصال

```text
Measurement
  → measurement_snapshot_t.valid / v_bat24_mv / input_present
Fault
  → fault_mask_t
UI
  → BOOL__G__UiBatteryAlarmIssued

rtos_app.c → TaskControl → func__Changeover_Evaluate(...)
                         → BSP_GPIO_PROTECT_BATTERY
```

## برگهٔ اعتبارسنجی

`Changeover_Board_Validation.xlsx` هم‌زمان با منطق نرم‌افزار ایجاد می‌شود و موارد زیر را ثبت می‌کند:

- snapshot نامعتبر و حفظ state/pin
- ورودی قطع و باتری فول
- قطع UI-assisted در ۲۱.۰V
- قطع مستقل در ۲۰.۸V
- افت کوتاه‌تر از ۳۰۰۰ms
- افت پیوستهٔ ۳۰۰۰ms
- reconnect در ۲۱.۲V با input_present
- قطع‌شدن شرط reconnect قبل از ۳۰۰۰ms
- fault mask غیرصفر بدون تغییر pin
- عدم تغییر PB5 و PB7
