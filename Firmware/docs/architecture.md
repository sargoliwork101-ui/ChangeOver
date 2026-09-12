/**
 * @file    architecture.md
 * @brief   [EN] Current firmware shape for the LED/buzzer stage.
 *          [FA] شکل فعلی فرمور در مرحله LED و بازر.
 */

# معماری الان

`main.c` مال CubeMX است. منطق در `Firmware/`.

```text
App_Start
  Ui_Init
  Rtos_Start → TaskUi
                 Ui_BoardTest() یک‌بار
                 UI_FLAG 1 → Ui_Scenario1()
                 وگرنه     → Ui_Scenario2()
```

الگوی LED داخل `Modules/Ui` است. فایل Task فقط FLAG را می‌خواند.

تسک‌های `task_measurement` / `protection` / `control` / `comm` اسکلت‌اند و پاک نمی‌شوند.
