# Debug Diagnostics Protocol

این سند جدول رسمی کدهای عیب‌یابی Firmware است. هر کد یک معنای ثابت دارد و بعد از انتشار نباید برای معنای دیگری استفاده شود.

## هدف

وقتی سیستم روی برد اجرا می‌شود، باید بتوانیم از روی یک خط Debug بفهمیم:

- Firmware در چه Stateای است.
- آخرین رویداد چه بوده است.
- رویداد اطلاعاتی، هشدار، خطا یا بحرانی است.
- مقدار اندازه‌گیری‌شده چه بوده است.
- چه Faultهایی فعال بوده‌اند.
- رویداد چند بار تکرار شده است.
- ESP8266 چه چیزی را باید روی نمایشگر نشان دهد.

## قالب خط سریال

### Telemetry

`Esp8266Service` خط Measurement زیر را تولید می‌کند:

```text
T,<input24v_mv>,<battery24v_mv>,<battery12v_mv>,<current1_ma>,<current2_ma>,<state>,<fault_mask>\r\n
```

تمام ولتاژها millivolt و جریان‌ها milliampere هستند.

### Diagnostic

ماژول Diagnostics خط زیر را تولید می‌کند:

```text
D,<code>,<severity>,<value>,<fault_mask>,<state>,<occurrence_count>\r\n
```

نمونه:

```text
D,16386,3,12450,2,7,4
```

تفسیر نمونه:

```text
code            = 16386 decimal = 0x4002 = Over Current Channel 1
severity        = 3 = ERROR
value           = 12450 mA
fault_mask      = 2 = FAULT_OVER_CURRENT
state           = 7 = FAULT
occurrence_count= 4
```

## Severity

| مقدار | نام | نمایش پیشنهادی روی ESP |
|---:|---|---|
| 1 | INFO | آبی/اطلاعاتی |
| 2 | WARNING | زرد/هشدار |
| 3 | ERROR | قرمز/خطا |
| 4 | FATAL | قرمز چشمک‌زن/توقف |

## جدول کدها

### Boot و Initialization — گروه `0x1xxx`

| کد | Hex | نام | Severity | مقدار Value |
|---:|---|---|---|---|
| 4096 | `0x1000` | `DIAG_BOOT_STARTED` | INFO | 0 |
| 4097 | `0x1001` | `DIAG_INIT_FAILED` | FATAL | مرحله‌ی ناموفق |

### ADC و Measurement — گروه `0x2xxx`

| کد | Hex | نام | Severity | مقدار Value |
|---:|---|---|---|---|
| 8193 | `0x2001` | `DIAG_ADC_DMA_START_FAILED` | ERROR | HAL status |
| 8194 | `0x2002` | `DIAG_ADC_OUT_OF_RANGE` | ERROR | Raw ADC |

### Source و Changeover — گروه `0x3xxx`

| کد | Hex | نام | Severity | مقدار Value |
|---:|---|---|---|---|
| 12289 | `0x3001` | `DIAG_INPUT_PRESENT` | INFO | Input mV |
| 12290 | `0x3002` | `DIAG_INPUT_LOST` | WARNING | Input mV |
| 12545 | `0x3101` | `DIAG_SOURCE_INPUT` | INFO | 0 |
| 12546 | `0x3102` | `DIAG_SOURCE_BATTERY` | INFO | Battery mV |

### Protection — گروه `0x4xxx`

| کد | Hex | نام | Severity | مقدار Value |
|---:|---|---|---|---|
| 16385 | `0x4001` | `DIAG_LOW_BATTERY` | WARNING | Battery mV |
| 16386 | `0x4002` | `DIAG_OVERCURRENT_CH1` | ERROR | Current mA |
| 16387 | `0x4003` | `DIAG_OVERCURRENT_CH2` | ERROR | Current mA |
| 16388 | `0x4004` | `DIAG_PROTECTION_ACTIVE` | ERROR | Fault mask |

### Jitter — گروه `0x5xxx`

| کد | Hex | نام | Severity | مقدار Value |
|---:|---|---|---|---|
| 20481 | `0x5001` | `DIAG_JITTER1_LOST` | WARNING | Timeout ms |
| 20482 | `0x5002` | `DIAG_JITTER2_LOST` | WARNING | Timeout ms |

### Communication — گروه `0x6xxx`

| کد | Hex | نام | Severity | مقدار Value |
|---:|---|---|---|---|
| 24577 | `0x6001` | `DIAG_ESP_TX_FAILED` | ERROR | HAL/status |
| 24578 | `0x6002` | `DIAG_UART_TX_FAILED` | ERROR | HAL/status |

### Fatal و System — گروه `0x7xxx`

| کد | Hex | نام | Severity | مقدار Value |
|---:|---|---|---|---|
| 28673 | `0x7001` | `DIAG_STACK_OVERFLOW` | FATAL | Task ID |
| 28674 | `0x7002` | `DIAG_SYSTEM_FAULT` | FATAL | Fault mask |

## State Codes

این مقدار همان `system_state_t` است:

| مقدار | State |
|---:|---|
| 0 | BOOT |
| 1 | SELF_TEST |
| 2 | INPUT_SOURCE |
| 3 | BATTERY_SOURCE |
| 4 | CHARGING |
| 5 | LOW_BATTERY |
| 6 | OVER_CURRENT |
| 7 | FAULT |

## Fault Mask

| Bit | نام |
|---:|---|
| 0 | `FAULT_LOW_BATTERY` |
| 1 | `FAULT_OVER_CURRENT` |
| 2 | `FAULT_ADC_OUT_OF_RANGE` |
| 3 | `FAULT_INPUT_UNDERVOLT` |
| 4 | `FAULT_JITTER_LOST` |
| 5 | `FAULT_COMMUNICATION` |
| 6 | `FAULT_HARDWARE` |

## قرارداد ESP8266

ESP8266 باید خط `D,...` را دریافت و:

1. عدد Code را به نام قابل‌خواندن تبدیل کند.
2. Severity را به رنگ/آیکون تبدیل کند.
3. Value را با واحد مناسب نشان دهد.
4. State و Fault Mask را نمایش دهد.
5. در صورت تغییر Code، رویداد جدید را در Log محلی ثبت کند.
6. در صورت تکرار یک Code، `occurrence_count` را افزایش‌یافته نشان دهد.

## قواعد توسعه‌ی جدول

- کد استفاده‌شده هرگز حذف یا reuse نمی‌شود.
- برای کد جدید، ابتدا این فایل، `diagnostics.h` و `AI_CONTEXT.md` به‌روزرسانی شوند.
- هر تغییر در Value یا Severity باید در `PROJECT_HISTORY.md` ثبت شود.
- پیام دیباگ نباید با `printf` تولید شود.
- Diagnostics نباید مسیر کنترل PWM را Block کند.

## Handshake و عدم از دست‌رفتن داده

در حالت Legacy، ESP فقط فریم‌های `T` و `D` را دریافت می‌کند. برای حالت پایدار، Feature Handshake باید فعال شود و فریم‌های زیر استفاده شوند:

```text
H,<version>,<session_id>,<sequence>,<crc16>
A,<version>,<session_id>,<sequence>,<crc16>
B,<version>,<session_id>,<sequence>,<crc16>
S,<version>,<session_id>,<sequence>,<crc16>
P,<version>,<session_id>,<sequence>,<crc16>
R,<version>,<session_id>,<sequence>,<crc16>
```

معنی حروف:

| نوع | معنی |
|---|---|
| H | Hello |
| A | Acknowledge |
| B | Heartbeat |
| S | Sync Request |
| P | Power Down Prepare |
| R | Power Down Ready |

وقتی STM32 قصد خاموش‌کردن ESP را دارد:

```text
STM32 → P
ESP    → Flush LittleFS
ESP    → R
STM32 → قطع تغذیه ESP
```

تا زمانی که صف Pending سمت STM32 و Sync بعد از Boot به‌طور کامل تست نشده‌اند، این Feature نباید در محصول نهایی فعال شود.

## Auto Test

اسکلت تست ESP در مسیرهای زیر قرار دارد:

```text
ESP8266/ESP_AutoTest.h
ESP8266/ESP_AutoTest.cpp
ESP8266/data/test.html
```

تست‌های قدرت و ارسال Command به STM32 پیش‌فرض خاموش هستند. تست Read-Only باید اول تأیید شود.
