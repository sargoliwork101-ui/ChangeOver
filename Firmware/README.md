# ChangeOver Firmware — اسکلت خالی ماژولار

این پوشه یک **پترن خالی** برای برنامه‌نویسی برد `ChangeOver(24V_DC)` است.

- به پروژه‌ی قبلی `ChangeOver_FreeRTOS` وصل نیست و از آن کپی نشده است.
- هنوز الگوریتم، PWM توان، شارژ و Changeover پیاده نشده‌اند.
- هدف این مرحله فقط **ساختار** است تا قدم‌به‌قدم جلو برویم و چیزی از قلم نیفتد.

---

## پیشنهاد معماری

`main.c` که CubeMX می‌سازد باید لاغر بماند:

1. کلاک و HAL
2. اولیه‌سازی peripheralها (`MX_*_Init`)
3. یک فراخوانی `App_Start()`
4. FreeRTOS scheduler

هیچ منطق محصول داخل `main.c` نیست. هر قابلیت یک «کتابخانه»ی جدا است (`module.h` / `module.c`). برای کم‌وزیاد کردن قابلیت، فلگ همان ماژول را در `Config/Inc/modules_enable.h` عوض می‌کنید یا پوشه‌اش را کنار می‌گذارید.

```
main.c  (CubeMX)          فقط MCU + start RTOS
   │
   ▼
App_Start()               سیم‌کشی ماژول‌ها، حالت امن، ساخت Task
   │
   ├── Bsp/               حرف زدن با سخت‌افزار (HAL اینجاست، فقط اینجا)
   ├── Modules/           منطق محصول (HAL نمی‌بیند)
   └── Rtos/              Taskها؛ فقط ماژول را صدا می‌زنند
```

قانون لایه‌ها:

| لایه | اجازه دارد | اجازه ندارد |
|---|---|---|
| `main.c` | HAL init، `App_Start()` | if/else محصول، ADC، PWM |
| `Rtos/` | دوره، صف، اولویت | HAL، سیاست شارژ/تغییر منبع |
| `Modules/` | تصمیم و state | `HAL_*` مستقیم |
| `Bsp/` | `HAL_*` | سیاست محصول |
| `Actuator` | تنها نویسنده‌ی خروجی قدرت | — |

خروجی‌های خطرناک (`Relay`، `BAT_SWITCH`، `PROTECT_BATT`، PWM شارژر) فقط از `Actuator` تغییر می‌کنند. اگر بعداً ماژول شارژر را حذف کنید، رله و PWM همچنان در حالت امن می‌مانند.

---

## الان کدام ماژول روشن است؟

در `modules_enable.h` فعلاً فقط اسکلت و UI روشن است. بقیه صفرند تا قدم‌به‌قدم فعال شوند.

```text
Actuator     همیشه هست (حالت امن)
Fault        روشن
Ui           روشن   ← قدم ۱: LED
Measurement  خاموش
Protection   خاموش
Changeover   خاموش
Charger      خاموش
Jitter       خاموش
EspLink      خاموش
```

---

## درخت پوشه

```text
Firmware/
├── README.md
├── docs/                    architecture.md ، steps.md
├── CubeMX/                  راهنمای Cube + snippet برای main.c
├── Config/                  پین، فلگ ماژول، آستانه، اولویت Task
├── Bsp/                     GPIO ADC PWM UART EXTI  (تنها لایهٔ HAL)
├── Modules/
│   ├── Actuator/            تنها نویسنده‌ی خروجی قدرت
│   ├── Ui/                  LED و بازر
│   ├── Fault/
│   ├── Measurement/
│   ├── Protection/
│   ├── Changeover/
│   ├── Charger/
│   ├── Jitter/
│   └── EspLink/
├── App/                     App_Init / App_Start
└── Rtos/                    Task استاتیک
```

---

## شروع کار (خلاصه)

1. در CubeIDE پروژه‌ی `STM32F103C8T6` بساز (راهنما: `CubeMX/README.md`).
2. این پوشه‌ی `Firmware/` را به پروژه Add کن.
3. Include pathها را مطابق همان راهنما بگذار.
4. انتهای `main.c` بعد از `MX_*_Init()` فقط `App_Start();` بگذار.
5. بیلد بگیر. اگر LED چشمک زد، قدم ۱ تمام است.

جزئیات هر قدم در `docs/steps.md` است. **تا قدم ۳ هیچ PWM و رله‌ای وصل نکن.**

---

## ایمنی

- PWM در این اسکلت Duty صفر است و ساخته نمی‌شود تا خودت در Config بازش کنی.
- قطبیت واقعی سخت‌افزار در `board_pins.h` و `actuator.c` کامنت شده؛ مقدار را حدس نزن، روی برد اندازه بگیر.
- Changeover بار روی برد تا حد زیادی آنالوگ است؛ نرم‌افزار فقط `PROTECT_BATT` و سوییچ تغذیه MCU را در دست دارد.
- فیوز روی شماتیک نیست. تست توان بدون منبع محدود ممنوع است.
