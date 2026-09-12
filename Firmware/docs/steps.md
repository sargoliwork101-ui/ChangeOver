# نقشه راه قدم‌به‌قدم

هر قدم را تمام کن، روی برد ببین، بعد فلگ قدم بعدی را روشن کن. از روی قدم‌ها نپر.

## قدم ۰ — پروژه CubeMX

- MCU: `STM32F103C8T6`
- Debug: **Serial Wire** (نه Full JTAG) تا PB4 آزاد شود
- HSE 8 MHz، SYSCLK پیشنهادی 72 MHz
- GPIO طبق `Config/Inc/board_pins.h`
- ADC1 Scan + DMA برای PA1, PA2, PA3, PA5, PA7 — **هنوز در کد راه ننداز**
- TIM2_CH1 = PA0 ، TIM3_CH1 = PA6 — PWM، Duty صفر
- USART1 = PA9/PA10
- EXTI: PB2, PB4, PB6
- FreeRTOS: static allocation روشن، dynamic خاموش
- در `main.c` فقط snippet فایل `CubeMX/main.c.snippet`

خروجی این قدم: پروژه بیلد می‌شود و `App_Start()` اجرا می‌شود.

## قدم ۱ — UI (الان Enable است)

هدف: بفهمی اسکلت زنده است.

- LED قرمز/زرد/سبز و بازر
- Task UI هر 100 ms
- هیچ خروجی قدرت تغییر نکند

قبول: LED سبز آرام چشمک بزند.

## قدم ۲ — Actuator در حالت امن

هدف: بعد از Reset، رله شارژر قطع، PWM صفر.

- با مولتی‌متر PB7, PA0, PA6, PB5, PB11 را بخوان
- قطبیت را در `board_pins.h` با کامنت `MEASURED` ثبت کن
- تا اندازه‌گیری، هیچ باری به OUT وصل نکن

قبول: رله کلیک نکند، MOSFET شارژر پالس نداشته باشد.

## قدم ۳ — Measurement

- `MODULE_MEASUREMENT 1`
- DMA ADC
- تبدیل خام به mV / mA فقط چاپ/LED، بدون کنترل

قبول: ولتاژ ورودی و باتری با مولتی‌متر در حد معقول یکی باشند.

## قدم ۴ — Fault + Protection

- `MODULE_FAULT` و `MODULE_PROTECTION`
- فقط ثبت خطا و Safe State
- هنوز Changeover و شارژر را از روی خطا فرمان نده مگر قطع امن

قبول: اگر ADC نیاید، سیستم در Safe بماند.

## قدم ۵ — Jitter و تشخیص ورودی

- `MODULE_JITTER 1`
- EXTI روی PB2/PB6/PB4 فقط رویداد؛ منطق در Task

قبول: لبه روی پایه، Flag در ماژول دیده شود.

## قدم ۶ — Changeover نرم‌افزاری

- `MODULE_CHANGEOVER 1`
- به‌خاطر سخت‌افزار آنالوگ، نرم‌افزار بیشتر ناظر است + `PROTECT_BATT`
- Break-before-make سخت‌افزاری هنوز کامل نیست؛ با ولتاژ پایین تست کن

قبول: سیاست حالت‌ها روی کاغذ و با LED، بدون بار واقعی.

## قدم ۷ — PWM بدون ترانس

- `MODULE_CHARGER` هنوز می‌تواند ۰ بماند
- Timer را با اسیلوسکوپ روی پایه MCU ببین، گیت MOSFET را درنیاور یا منبع را جدا کن

قبول: فرکانس و Duty همان است که خواستی؛ Duty سقف دارد.

## قدم ۸ — شارژر و رله با منبع محدود

فقط بعد از کالیبراسیون جریان و تأیید قطبیت رله.

- `MODULE_CHARGER 1`
- `APP_CONFIG.power_stage_enabled` هنوز پیش‌فرض false است؛ دستی و آگاهانه true کن

قبول: رله با فرمان MCU، جریان محدود، قطع اضطراری از Protection.

## قدم ۹ — ESP8266

- `MODULE_ESP 1`
- ابتدا فقط Telemetry یک‌طرفه
- CH_PD را از MCU کنترل کن؛ تا UART پایدار نشده فرمان نده

---

اگر وسط راه ماژول جدید لازم شد: یک پوشه در `Modules/`، یک فلگ، یک Init در `app.c`، در صورت نیاز یک Task. به `main.c` چیزی اضافه نکن.
