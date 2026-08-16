# معماری Firmware

## اصل ماژولار بودن در MISRA C

در C کلاس نداریم. هر ماژول یک context struct و یک API محدود دارد:

```c
typedef struct
{
    /* private state */
} charger_controller_t;

void charger_controller_init(charger_controller_t * controller);
void charger_controller_update(charger_controller_t * controller);
```

جزئیات private در فایل `.c` تا حد امکان `static` می‌شوند. Header فقط قرارداد موردنیاز سایر ماژول‌ها را export می‌کند.

## لایه‌ها

```text
Generated CubeMX / STM32 HAL / FreeRTOS
                    │
                    ▼
              BSP Wrappers
       GPIO / ADC / PWM / UART / Pin Map
                    │
                    ▼
             Application Services
 Measurement / Fault / Protection / Actuator
 Charger / Changeover / Jitter / Diagnostics
 User Interface / ESP8266 Service
                    │
                    ▼
                RTOS Tasks
```

## مالکیت داده و خروجی

- `MeasurementManager` تنها مالک Snapshot اندازه‌گیری است.
- `FaultManager` تنها مالک Fault EventGroup است.
- `ActuatorManager` تنها مسیر منطقی دسترسی به خروجی‌ها است.
- `ControlTask` تصمیم کنترل را اجرا می‌کند.
- `ProtectionTask` Fault را به‌روزرسانی می‌کند.
- ISR فقط timestamp/event می‌سازد.

## Taskها

| Task | مسئولیت | ارتباط |
|---|---|---|
| MeasurementTask | دریافت DMA و تبدیل ADC | Task notification از ADC ISR |
| ProtectionTask | Low Battery و Over Current | Snapshot → FaultManager |
| ControlTask | State Machine و خروجی | Snapshot + FaultManager → ActuatorManager |
| CommunicationTask | ارسال Telemetry | Snapshot + FaultManager → UART |
| UiTask | LED و Buzzer | State + Fault → ActuatorManager |

## Safe State

در زمان Boot، نبود Snapshot معتبر، Fault سخت‌افزاری، Over Current یا ADC خارج از محدوده:

```text
PWM1 = 0
PWM2 = 0
Charger Relay = OFF
Battery Switch = SAFE/OFF
```

مقدار واقعی Safe/OFF برای Battery Switch باید روی برد تأیید شود.

## Diagnostics path

```text
Module / Task
    -> diagnostics_report()
    -> last_event snapshot
    -> CommunicationTask
    -> Esp8266Service
    -> UART line: D,<code>,<severity>,<value>,<fault_mask>,<state>,<count>
    -> ESP8266 display/log
```

Diagnostics نباید مسیر PWM یا Protection را Block کند. برای Stack Overflow، مسیر `diagnostics_report_emergency()` بدون Mutex استفاده می‌شود و بلافاصله سیستم وارد مسیر Terminal Safe State می‌شود.
