/**
 * @file    README.md
 * @brief   [EN] ChangeOver project front page: overview and file connections.
 *          [FA] صفحه اول پروژه ChangeOver: معرفی و درخت اتصال فایل‌ها.
 */

# ChangeOver

تغییر مسیر تغذیه ۲۴ ولت DC: ورودی یا باتری. MCU: `STM32F103C8T6`.

**مرحله فعلی:** فقط LED و بازر. ADC، PWM، UART، رله خاموش.

شماتیک: `Circuit/ChangeOver(24V_DC).pdf`

قوانین دستیار: `Firmware/AI_CONTEXT.md`

## پوشه‌ها

| پوشه | چیست |
|---|---|
| `Circuit/` | شماتیک |
| `Firmware/` | کد محصول (App, Bsp, Config, Modules, Rtos) |
| `CubeMX/` | فایل `.ioc` |
| `CubeIDE/` | پروژه STM32CubeIDE بعد از Generate |

`main.c` فقط کلاک، HAL، `MX_*_Init`، بعد `App_Start()`. منطق محصول در `Firmware/`.

کلید ماژول‌ها: `Firmware/Config/Inc/modules_enable.h` — الان فقط `MODULE_UI = 1`.

## اجرا الان

```text
CubeIDE/Core/.../main.c
  App_Start()                    Firmware/App/Src/app.c
    Ui_Init()                    Firmware/Modules/Ui/ui.c
    Rtos_Start()                 Firmware/Rtos/Src/rtos_app.c
      TaskUi                     Firmware/Rtos/Src/task_ui.c
        Ui_BoardTest()           یک‌بار تست سیم‌کشی
        حلقه هر ۱۰ms:
          Ui_Indicate(ورودی, درصد باتری)   Firmware/Modules/Ui/ui.c
            UI_INPUT_OK   سبز ثابت
            UI_BATTERY_RUN  سبز چشمک (روشن برابر درصد باتری)
            UI_BATTERY_LOW  زرد چشمک + بوق هر ۳۰ ثانیه
          (ورودی‌ها فعلاً متغیر تست دستی در task_ui.c)
          BspGpio_Write()        Firmware/Bsp/Src/bsp_gpio.c
          PIN_*                  Firmware/Config/Inc/board_pins.h
          APP_CONFIG             Firmware/Config/Src/app_config.c
```

تسک‌های measurement / protection / control / comm فایل دارند؛ با فلگ صفر ساخته نمی‌شوند.

## درخت اتصال کل پروژه

```text
ChangeOver
├── README.md                          ← همین صفحه
├── Circuit/
│   └── ChangeOver(24V_DC).pdf
├── CubeMX/
│   └── ChangeOver.ioc                 ← تنظیمات مکعب (بعد از Generate کپی شود)
├── CubeIDE/                           ← HAL، main.c، FreeRTOS مکعب
│   └── Core/Src/main.c ──#include──► Firmware/App/Inc/app.h
└── Firmware/
    ├── AI_CONTEXT.md
    ├── App/
    │   app.c ──► ui.h
    │         ──► rtos_app.h
    ├── Config/
    │   board_pins.h
    │   app_config.c / app_config.h
    │   app_types.h
    │   modules_enable.h
    │   rtos_config.h
    ├── Bsp/
    │   bsp_gpio.c ◄── Ui ، EspLink
    │   bsp_adc.c  ◄── Measurement (اسکلت)
    │   bsp_pwm.c  ◄── Charger (اسکلت)
    │   bsp_uart.c ◄── EspLink (اسکلت)
    │   bsp_exti.c ◄── Jitter (اسکلت)
    ├── Rtos/
    │   rtos_app.c ──► task_ui.c          (MODULE_UI)
    │              ──► task_measurement.c (فلگ ۰)
    │              ──► task_protection.c  (فلگ ۰)
    │              ──► task_control.c     (فلگ ۰)
    │              ──► task_comm.c        (فلگ ۰)
    │   freertos_hooks.c
    └── Modules/
        Ui          ◄── فعال
        Measurement     اسکلت
        Protection      اسکلت
        Changeover      اسکلت
        Charger         اسکلت
        Jitter          اسکلت
        Fault           اسکلت
        EspLink         اسکلت
```

برگهٔ هر ماژول (توابع، پایه‌ها، درخت همان ماژول): `Firmware/Modules/<نام>/README.md`

## تاریخچه این صفحه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | اسکلت‌های Bsp ADC/UART با تایپ ناقص (opaque) بدون فعال‌کردن درایور کامپایل می‌شوند؛ همه فایل‌های Firmware در Build هستند |
| 2026-09-14 | سناریوهای UI مبتنی بر وضعیت با `Ui_Indicate` (ورودی/درصد باتری)؛ حذف Scenario1/2 از درخت اجرا |
| 2026-09-14 | اصلاح Build پروژه CubeIDE: لینک نسبی Firmware، مسیرهای Include، Exclude اسکلت ADC/UART |
| 2026-09-14 | صفحه اول + درخت کل پروژه؛ پوشه CubeMX و CubeIDE |
