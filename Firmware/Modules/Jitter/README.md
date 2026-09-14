/**
 * @file    README.md
 * @brief   [EN] Jitter module: LM393 trip flags.
 *          [FA] ماژول جیتر: پرچم تریپ LM393.
 */

# ماژول Jitter

توضیح کامل **همین ماژول** همین‌جاست.

الان **خاموش** است (`MODULE_JITTER 0`). EXTI را برای این مرحله Enable نکن. فایل‌ها را پاک نکن.

## کار ماژول

دو مقایسهٔ LM393 روی برد، نویز/جیتر مسیر قدرت را نشان می‌دهند. این ماژول لبهٔ EXTI را می‌گیرد و تریپ را **قفل** می‌کند تا یک پالس کوتاه گم نشود.

خروجی: `Jitter_ChannelTripped(1)` یا `(2)`. بعداً Fault بیت `FAULT_JITTER_1` / `FAULT_JITTER_2` را می‌گذارد و Changeover به FAULT می‌رود.

## پایه‌هایی که بعداً مال این ماژول‌اند

حالا نزن. در `board_pins.h` 5V-tolerant مشخص شده:

| پایه | نقش |
|---|---|
| PB2 | جیتر ۱ |
| PB6 | جیتر ۲ |

## فایل‌ها

| فایل | نقش |
|---|---|
| `jitter.h` / `jitter.c` | قفل تریپ |
| `../../Bsp/Src/bsp_exti.c` | پرچم نرم‌افزاری EXTI (اسکلت) |
| `../../Rtos/Src/task_control.c` | تسک مشترک |

`jitter.c` و `bsp_exti.c` را به بیلد LED اضافه نکن.

## توابع همین الان در کد

- `Jitter_Init` — `s_trip[0]` و `[1]` را false می‌کند؛ `BspExti_Init`.
- `Jitter_Run` — اگر `BspExti_TakeEvent` برای کانال ۱ یا ۲ true باشد همان کانال را true قفل می‌کند. یک‌بار Take یعنی پرچم EXTI مصرف می‌شود.
- `Jitter_ChannelTripped` — فقط `1` و `2` معتبرند؛ عدد دیگر false.

قفل است نه سطح لحظه‌ای: بعد از تریپ، تا `Jitter_Init` (یا منطق Clear بعدی) true می‌ماند.
