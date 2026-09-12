# ChangeOver Firmware

`main.c` فقط کلاک، HAL، `MX_*_Init` و `App_Start()` است.

**مرحله فعلی:** LED و بازر. راهنما: `docs/01-led-buzzer-cubeide.md`

```text
main.c  →  App_Start()  →  TaskUi  →  Ui_Run()
                              │
                         Modules/Ui   الگوهای چشمک
                         Bsp/         HAL_GPIO
```
