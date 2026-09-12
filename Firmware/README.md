/**
 * @file    README.md
 * @brief   [EN] Firmware root. Product code lives here, not in CubeMX main.c.
 *          [FA] ریشه فرمور. منطق محصول این‌جاست، نه در main.c مکعب.
 */

# ChangeOver Firmware

`main.c` فقط کلاک، HAL، `MX_*_Init` و `App_Start()`.

**مرحله فعلی:** LED و بازر. راهنما: `docs/01-led-buzzer-cubeide.md`

قوانین دستیار: `AI_CONTEXT.md` — قبل از هر تغییر خوانده شود.

```text
main.c  →  App_Start()  →  TaskUi
                              │
                         Ui_BoardTest یک‌بار
                         بعد FLAG → Ui_Scenario1 یا Ui_Scenario2
```
