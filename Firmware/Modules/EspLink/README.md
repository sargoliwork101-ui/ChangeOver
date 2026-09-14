/**
 * @file    README.md
 * @brief   [EN] EspLink module: ESP power and UART telemetry.
 *          [FA] ماژول EspLink: تغذیه ESP و تله‌متری UART.
 */

# ماژول EspLink

توضیح کامل **همین ماژول** همین‌جاست.

الان **خاموش** است (`MODULE_ESP 0`). UART و پایهٔ CH_PD را برای این مرحله Enable نکن. فایل‌ها را پاک نکن.

## کار ماژول

ESP8266 روی برد تله‌متری می‌فرستد. STM فرمان از ESP نمی‌گیرد تا پروتکل جدا نوشته شود.

دو کار جدا:

1. تغذیه: پایهٔ CH_PD
2. UART: فرستادن snapshot / state / faults

`EspLink_Init` باید ESP را **خاموش** بگذارد تا قبل از مرحلهٔ شبکه، ماژول بی‌جهت روشن نشود.

## پایه‌هایی که بعداً مال این ماژول‌اند

حالا نزن:

| پایه | نقش |
|---|---|
| PA8 | CH_PD — شماتیک: High = ESP روشن |
| PA9 | USART1_TX |
| PA10 | USART1_RX |

## فایل‌ها

| فایل | نقش |
|---|---|
| `esp_link.h` / `esp_link.c` | قدرت و `Run` تله‌متری |
| `../../Bsp/Src/bsp_uart.c` | UART (اسکلت) |
| `../../Rtos/Src/task_comm.c` | تسک؛ فلگ صفر = Idle |

`esp_link.c` و `bsp_uart.c` را به بیلد LED اضافه نکن.

## توابع همین الان در کد

- `EspLink_Power` — `BspGpio_Write` روی `PIN_ESP_CHPD`.
- `EspLink_Init` — `EspLink_Power(false)`.
- `EspLink_Run` — آرگومان‌ها را دور می‌ریزد؛ هنوز بایتی روی UART نمی‌رود.

کلید `APP_CONFIG.esp_link_enabled` برای بعد است؛ با `MODULE_ESP` قاطی نشود: یکی کامپایل تسک است، یکی اجازهٔ زمان اجرا.
