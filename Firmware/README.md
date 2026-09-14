/**
 * @file    README.md
 * @brief   [EN] Firmware root. Product code lives here, not in CubeMX main.c.
 *          [FA] ریشه فرمور. منطق محصول این‌جاست، نه در main.c مکعب.
 */

# ChangeOver Firmware

`main.c` فقط کلاک، HAL، `MX_*_Init` و `App_Start()`.

**مرحله فعلی:** LED و بازر. راهنما داخل خود ماژول است:

`Modules/Ui/README.md`

هر ماژول توضیح کامل خودش را در `README.md` همان پوشه دارد. پوشهٔ `docs` جدا نیست.

| ماژول | الان |
|---|---|
| `Modules/Ui` | فعال |
| `Modules/Measurement` | اسکلت — ADC را روشن نکن |
| `Modules/Protection` | اسکلت |
| `Modules/Changeover` | اسکلت — رله را روشن نکن |
| `Modules/Charger` | اسکلت — PWM را روشن نکن |
| `Modules/Jitter` | اسکلت |
| `Modules/Fault` | اسکلت |
| `Modules/EspLink` | اسکلت — UART را روشن نکن |

کلید روشن/خاموش: `Config/Inc/modules_enable.h` (الان فقط `MODULE_UI = 1`).

قوانین دستیار: `AI_CONTEXT.md`
