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
| 2026-09-14 | اجرای AI: تکمیل بخش اجباری «درخت اتصال» طبق قالب ۷ بخشی AI_CONTEXT؛ چک قوانین پاس شد |
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

## درخت اتصال

صدا زده می‌شود از (وقتی فلگ ۱ شود):

```text
rtos_app.c → TaskComm → task_comm.c
  EspLink_Init (از App_Init در آینده)
  EspLink_Run(&snap, state, faults)
  EspLink_Power(on/off)
```

این ماژول صدا می‌زند:

```text
esp_link.c
  bsp_gpio.h / bsp_gpio.c     BspGpio_Write  → PIN_ESP_CHPD
  bsp_uart.h / bsp_uart.c     BspUart_Write (فعلاً false) / BspUart_Init
  board_pins.h                PIN_ESP_CHPD_* ، PIN_TX/RX
  app_config.h / app_config.c APP_CONFIG.esp_link_enabled ، comm_period_ms
  app_types.h                 measurement_snapshot_t ، app_state_t ، fault_mask_t
```

UART و CH_PD تا فعال‌سازی مرحله بعد خاموش می‌مانند (MODULE_ESP=0).
