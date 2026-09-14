/**
 * @file    README.md
 * @brief   [EN] EspLink module sheet: ESP power and UART.
 *          [FA] برگه ماژول EspLink: تغذیه ESP و UART.
 */

# ماژول EspLink

## وضعیت

اسکلت. `MODULE_ESP = 0`. UART را Enable نکن. فایل را پاک نکن.

## تاریخچه

| تاریخ | تغییر |
|---|---|
| 2026-09-14 | درخت اتصال فایل‌ها اضافه شد |
| 2026-09-14 | برگه ماژول با توابع، پایه‌ها، لیبل و تاریخچه |
| 2026-09 | اسکلت `EspLink_Init` / `Power` / `Run` |

## فایل‌ها

| فایل | نقش |
|---|---|
| `esp_link.h` / `esp_link.c` | تغذیه و تله‌متری |
| `../../Bsp/Src/bsp_uart.c` | UART — به بیلد LED اضافه نکن |
| `../../Rtos/Src/task_comm.c` | تسک |

## توابع

| نام | کار |
|---|---|
| `EspLink_Init` | ESP را خاموش می‌کند |
| `EspLink_Power` | CH_PD را High/Low می‌کند |
| `EspLink_Run` | تله‌متری؛ فعلاً بایتی نمی‌فرستد |
| `TaskComm` | تا فلگ صفر Idle |

`MODULE_ESP` ساخت تسک است. `APP_CONFIG.esp_link_enabled` اجازهٔ زمان اجرا است.

## پایه‌ها

| پایه | لیبل | نقش | HIGH یعنی |
|---|---|---|---|
| PA8 | `MCU_ESP_CHPD` | تغذیه ESP (CH_PD) | شماتیک: ESP روشن |
| PA9 | `USART1_TX` | سریال به ESP | UART |
| PA10 | `USART1_RX` | سریال از ESP | UART |

## پیش‌فرض امن

`EspLink_Init` → `EspLink_Power(false)` یعنی PA8 Low. STM از ESP فرمان نمی‌گیرد تا پروتکل جدا نوشته شود.
