/**
 * @file    README.md
 * @brief   [EN] Jitter module sheet: LM393 trip flags.
 *          [FA] برگه ماژول Jitter: پرچم تریپ LM393.
 */

# ماژول Jitter

## وضعیت

اسکلت. `MODULE_JITTER = 0`. EXTI را Enable نکن. فایل را پاک نکن.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت `func__Jitter_Init` / `Run` / `ChannelTripped` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `jitter.h` / `jitter.c` | قفل تریپ دو کانال |
| `../../Bsp/Src/bsp_exti.c` | پرچم EXTI — به بیلد LED اضافه نکن |
| `../../Rtos/Src/task_control.c` | تسک مشترک |

## توابع

| نام | کار |
|---|---|
| `func__Jitter_Init` | هر دو تریپ false؛ `BspExti_Init` |
| `func__Jitter_Run` | اگر رویداد EXTI آمده باشد همان کانال را true قفل می‌کند |
| `func__Jitter_ChannelTripped` | کانال `1` یا `2`؛ عدد دیگر false |
| `TaskControl` | مشترک با Changeover / Charger |

تریپ latch است؛ با یک پالس true می‌ماند تا Init/Clear بعدی.

## پایه‌ها

ورودی، 5V-tolerant در شماتیک.

| پایه | لیبل | نقش | HIGH یعنی |
|---|---|---|---|
| PB2 | `MCU_JITTER1` | LM393 کانال ۱ | لبه/سطح تریپ؛ قطبیت شماتیک، اندازه نشده |
| PB6 | `MCU_JITTER2` | LM393 کانال ۲ | همان |

## پیش‌فرض امن

بعد از Init هر دو `s_trip` برابر false است. خروجی قدرت ندارد.

## درخت اتصال

صدا زده می‌شود از (وقتی فلگ ۱ شود):

```text
rtos_app.c → TaskControl → task_control.c
  func__Jitter_Init / func__Jitter_Run / func__Jitter_ChannelTripped
```

این ماژول صدا می‌زند:

```text
jitter.c
  bsp_exti.h / bsp_exti.c    BspExti_Init ، BspExti_TakeEvent
```
