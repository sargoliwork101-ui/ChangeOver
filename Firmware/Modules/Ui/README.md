/**
 * @file    README.md
 * @brief   [EN] UI module sheet: LEDs and buzzer.
 *          [FA] برگه ماژول UI: ال‌ای‌دی و بازر.
 */

# ماژول UI

## وضعیت

فعال. `MODULE_UI = 1`. تنها ماژولی که الان اجرا می‌شود.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه یکدست شد |
| 2026-09 | کامنت خط‌به‌خط انگلیسی روی `Ui_Scenario1` / `Ui_Scenario2`؛ سناریو ۲ بدون متغیر اضافه |
| 2026-09 | `Ui_BoardTest` یک‌بار، بعد `UI_FLAG` یکی از دو سناریو |
| 2026-09 | الگوها داخل `ui.c`؛ تسک فقط انتخاب می‌کند |

## فایل‌ها

| فایل | نقش |
|---|---|
| `ui.h` / `ui.c` | API و الگو |
| `../../Rtos/Src/task_ui.c` | تسک؛ `UI_FLAG` |
| `../../Bsp/Src/bsp_gpio.c` | نوشتن پایه |
| `../../Config/Inc/board_pins.h` | شماره پایه |
| `../../Config/Src/app_config.c` | زمان‌ها |

## توابع

| نام | کار |
|---|---|
| `Ui_Init` | همه خروجی UI را خاموش می‌کند |
| `Ui_BoardTest` | یک‌بار قرمز، زرد، سبز، بوق؛ برمی‌گردد |
| `Ui_Scenario1` | سبز ۵۰۰ روشن / ۵۰۰ خاموش؛ قرمز خاموش؛ برنمی‌گردد |
| `Ui_Scenario2` | سبز ۵۰۰ روشن سپس ۱۰۰۰ خاموش؛ قرمز هر ۵۰۰ چشمک؛ برنمی‌گردد |
| `TaskUi` | تست برد، بعد FLAG؛ برنمی‌گردد |
| `green` (static) | PB10 |
| `red` (static) | PB0 |
| `yellow` (static) | PB1 |
| `buzzer` (static) | PA4 |
| `all_off` (static) | هر چهار تا Low |

`UI_FLAG` در `task_ui.c`: `1u` سناریو ۱، غیر از آن سناریو ۲.

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی |
|---|---|---|---|
| PB0 | `MCU_R_LED` | LED قرمز Q4 | روشن |
| PB1 | `MCU_Y_LED` | LED زرد Q5 | روشن |
| PB10 | `MCU_G_LED` | LED سبز Q6 | روشن |
| PA4 | `MCU_BUZZER` | بازر Q7 | صدا |

همه خروجی، Push-Pull، بعد Reset باید Low باشند.

## پیش‌فرض امن

`Ui_Init` هر چهار پایه را Low می‌کند. قبل از `App_Start` هم CubeMX Level = Low.
