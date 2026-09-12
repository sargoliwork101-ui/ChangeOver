# معماری الان

`main.c` مال CubeMX است. منطق در `Firmware/`.

```text
App_Start
  Ui_Init + Ui_SetProfile(SELFTEST)
  Rtos_Start → فقط TaskUi هر 100 ms → Ui_Run
```

پروفایل LED همه داخل `Modules/Ui` است. Task فقط `Ui_Run` را صدا می‌زند.
