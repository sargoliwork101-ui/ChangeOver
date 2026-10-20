# معماری تسک‌های RTOS / RTOS task architecture

<!--
[EN] Companion text for Documentation/rtos_flowchart.png. Every number below is
     copied from the firmware headers, not from memory; the picture and this
     file are generated/kept next to:
       - Firmware/Config/Inc/rtos_config.h     (priorities, stacks)
       - Firmware/Config/Src/app_config.c      (task periods)
       - Firmware/Modules/Measurement/measurement.h (1 ms sample period)
       - Firmware/Modules/Ui/ui_led.h          (UI_TICK_MS)
       - Firmware/Bsp/Inc/bsp_iwdg.h           (1500 ms stale limit)
       - CubeIDE/Core/Inc/FreeRTOSConfig.h     (1 kHz tick)
     Regenerate the picture with: python3 tools/make_rtos_flowchart.py

[FA] متن همراهِ تصویر Documentation/rtos_flowchart.png. هیچ عددی اینجا از حافظه
     نوشته نشده؛ همه از هدرهای خود فرم‌ور آمده است (فهرست بالا). تصویر با
     «python3 tools/make_rtos_flowchart.py» بازتولید می‌شود.
-->

## نسخهٔ HTML / HTML version

همین معماری به شکل صفحهٔ تعاملی (فلوچارت کلیک‌شدنی، درخت کد و جدول‌ها) هم هست:
`Documentation/rtos_architecture.html` — با `python3 tools/make_process_pages.py` ساخته می‌شود.

## ۱) پنج تسک و اولویت‌ها / The five threads

اولویت در CMSIS-RTOS2 عددِ بزرگ‌تر = زودتر اجرا. منبع: `rtos_config.h`.

| تسک (thread) | تابع ورود | اولویت CMSIS | مقدار عددی | دوره | استک |
|---|---|---|---|---|---|
| حفاظت / protection | `func__TaskProtection` | `osPriorityLow3` | ۱۱ | ۵ ms | ۱۹۲ word |
| کنترل / control | `func__TaskControl` | `osPriorityLow2` | ۱۰ | ۱۰ ms | ۲۵۶ word |
| اندازه‌گیری / measurement | `func__TaskMeasurement` | `osPriorityLow2` | ۱۰ | ۱ ms (`MEASUREMENT_PERIOD_MS`) | ۱۹۲ word |
| ارتباط ESP / comm | `func__TaskComm` | `osPriorityLow1` | ۹ | ۱۰۰ ms | ۲۵۶ word |
| رابط کاربر / UI | `func__TaskUi` | `osPriorityLow` | ۸ | ۱۰ ms (`UI_TICK_MS`) | ۱۲۸ word |

نکته‌ها / notes:

- **`MODULE_PROTECTION = 0`** است، پس تسک حفاظت امروز ساخته نمی‌شود؛ کدش سر جایش است و با یک‌شدن فلگ برمی‌گردد. / protection is compiled and host-tested but not created today.
- کنترل و اندازه‌گیری هم‌اولویت‌اند (۱۰) و با time-slicing (تیک ۱ کیلوهرتز) می‌چرخند. / equal priority, time-sliced at 1 kHz.
- هیچ‌کدام از تسک‌ها `osPriorityAboveNormal` و بالاتر را نمی‌گیرند؛ وقفه‌ها (EXTI و UART/DMA) همیشه بر تسک‌ها مقدم‌اند. / no thread sits above normal; interrupts preempt everything.

## ۲) از ریست تا زمان‌بند / Boot chain

```
main()  →  func__BspIwdg_Init()  →  func__App_Start()
        →  func__App_Init()      (NVM replay, CalLut, Diag, FlashLock=off)
        →  func__Rtos_Start()    (osKernelInitialize, Fault_Init, 5× osThreadNew)
        →  osKernelStart()       (never returns)
```

ساخت هر تسک پشت همان فلگ `MODULE_*` است که در `modules_enable.h` روشن می‌شود؛ همان فلگ، مجموعهٔ موردانتظار واچ‌داگ را هم می‌سازد (`BSP_IWDG_EXPECT_MASK`)، پس «تسک ساخته‌شده» و «تسک پایش‌شده» هرگز از هم جدا نمی‌شوند.

## ۳) چه چیزی بین تسک‌ها رد می‌شود / Shared data

| داده | نویسنده | خواننده‌ها | محافظ |
|---|---|---|---|
| `measurement_snapshot_t` | تسک اندازه‌گیری | کنترل، UI، ارتباط | کپی اتمیک با `osKernelLock()` |
| `fault_mask_t` | کنترل (`Fault_Evaluate`) | ارتباط، Diag | `volatile` + `osKernelLock()` در مسیر خواندن |
| پارامترهای پنل (setterها) | تسک ارتباط (ESP) | اندازه‌گیری، شارژر، UI، عدم‌توازن، Fault | `volatile` + `osKernelLock()` |
| رکورد دیاگ | کنترل | ارتباط (برای پنل) | `volatile` |

ترتیب ثابت پاسِ کنترل / fixed control-pass order (منبع: `task_control.c`):

```
Fault_Evaluate → Jitter_Run → Charger_EvaluateTechnicalFault
→ McuPowerPath_Run → Imbalance_Evaluate → Changeover_Evaluate
→ Charger_Evaluate → Diag_* → BspIwdg_PollKick
```

`Imbalance` عمداً **قبل** از `Changeover` است تا وتوی خروجی و گیت شارژ در همان پاس دیده شوند؛ حالت چنج‌اورِ همان پاس برای «روی باتری» در پاس بعدیِ عدم‌توازن استفاده می‌شود.

## ۴) واچ‌داگ / Watchdog

- `IWDG` مستقل، LSI ÷ ۳۲، شمارش ۱۲۵۰ ≈ ۱ ثانیه.
- هر تسک با `func__BspIwdg_CheckIn(slot, nowMs)` اعلام حضور می‌کند.
- فقط تسک کنترل `func__BspIwdg_PollKick(nowMs)` را صدا می‌زند و آن هم فقط وقتی **همهٔ** شکاف‌های موردانتظار زیر ۱۵۰۰ میلی‌ثانیه تازه باشند. یک شکاف کهنه = هیچ تازه‌سازی = ریست سخت‌افزاری (عمدی).
- اگر ترکیب ماژول‌ها تسک کنترل نداشته باشد، واچ‌داگ عمداً روشن نمی‌شود (`BSP_IWDG_PUMPER_EXISTS`).

## ۵) پیش‌زمینه / Interrupts and DMA

| منبع | چه کار می‌کند | مقصد در تسک‌ها |
|---|---|---|
| `EXTI2`, `EXTI9_5` | PB2/PB6، فقط لبهٔ پایین‌روندهٔ LM393 | پرچم `Jitter` برای کنترل |
| `EXTI4` | PB4 ورود ۲۴ ولت + `McuPowerPath_OnInputIrq` | رویداد ورودی + وصل فوری باتری |
| `USART1` + `DMA1` CH4/CH5 | RX حلقهٔ ۲۵۶ و TX حلقهٔ ۱۰۲۴ بایتی | صف فریم تسک ارتباط |
| `TIM2`/`TIM3` + ADC + DMA | گیت PWM، تریگر ADC، دو فریم حلقوی | فریم کامل برای اندازه‌گیری |
| `TIM1` | تایم‌بیس ۱ میلی‌ثانیه‌ای HAL | فقط `HAL_Delay` (در مسیر تسک‌ها استفاده نمی‌شود) |

## ۶) قواعدی که بررسی شده‌اند / Rules verified in the audit

1. هیچ تسکی حلقهٔ مشغول یا `HAL_Delay` ندارد؛ همه با `osDelayUntil` / `func__Rtos_DelayMilliseconds` می‌خوابند.
2. اولویت‌ها فقط در `rtos_config.h` تعریف می‌شوند و هیچ عدد خامی در تسک‌ها نیست.
3. حافظهٔ همهٔ تسک‌ها استاتیک است (TCB + stack از پیش تعریف‌شده)؛ `malloc` وجود ندارد.
4. تسک‌ها پیش از ورود به حلقه، Init ماژول‌های خودشان را یک‌بار صدا می‌زنند (مالکیت Init با مالک تسک).
5. واچ‌داگ فقط تسک‌های ساخته‌شده را می‌پاید و «تسک مرده» و «سخت‌افزار خراب» را از هم جدا می‌کند (ADC خراب = تسک زنده و اعلام حضور، شارژر در safe-idle).

## بازتولید / Regenerate

```bash
pip install arabic-reshaper python-bidi Pillow
python3 tools/make_rtos_flowchart.py      # -> Documentation/rtos_flowchart.png
```
