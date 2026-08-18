# Approval Status / وضعیت تأیید

تاریخ: 2026-08-17

این Repository هنوز نسخه‌ی تأییدشده‌ی محصول نیست. تمام کدها Prototype، Sample یا Scaffolding هستند مگر اینکه در این جدول صریحاً Approved شده باشند.

| بخش | وضعیت فعلی | پیش‌فرض امن | شرط تأیید |
|---|---|---|---|
| STM32 ADC Measurement | Prototype | فعال برای مشاهده، بدون توان | تست ADC و کالیبراسیون |
| STM32 Protection | Prototype | فعال برای تشخیص | تست Low Battery و Over Current |
| STM32 Changeover | Not Approved | `power_stage_enabled=false` | تست مرحله‌ای بدون بار و سپس بار محدود |
| STM32 Charger PWM | Not Approved | Duty = 0 / Controller disabled | مشخص‌شدن فرکانس، Duty و الگوریتم CC/CV |
| STM32 Relay/Battery Switch | Not Approved | Safe State | تأیید active level و تست سخت‌افزاری |
| STM32 LED/Buzzer UI | Prototype | بدون کنترل توان | تست GPIO و الگوهای هشدار |
| STM32 → ESP Legacy Telemetry | Not Approved | `esp_link_enabled=false` | تست UART و Parser |
| ESP WiFi/Web Server | Sample | Feature Flag خاموش | تست Boot و Health |
| ESP Diagnostics Storage | Sample | Feature Flag خاموش | تست LittleFS و Reset Recovery |
| ESP Charts | Sample | Feature Flag خاموش | تست Heap و API محدودشده |
| ESP Auto Test | Sample | خاموش | تست Read-Only و سپس تأیید هر Step |
| ESP STM Commands | Not Approved | `ESP_FEATURE_STM_COMMANDS=0` | Handshake، Auth و تست ایمنی |
| ESP Power-Down Handshake | Design Sample | `ESP_FEATURE_HANDSHAKE=0` | Pending Queue و Replay سمت STM32 |

## Baseline Approval Gates

در نسخه‌ی پایه:

```c
APP_CONFIG.power_stage_enabled = false;
APP_CONFIG.esp_link_enabled = false;
```

و در ESP:

```cpp
ESP_FEATURE_UART = 0
ESP_FEATURE_PROTOCOL_PARSER = 0
ESP_FEATURE_HANDSHAKE = 0
ESP_FEATURE_STM_COMMANDS = 0
ESP_FEATURE_DIAGNOSTICS_STORAGE = 0
ESP_FEATURE_TELEMETRY_STORAGE = 0
ESP_FEATURE_RUNTIME_STORAGE = 0
ESP_FEATURE_AUTO_TEST = 0
```

فعال‌کردن هر مورد باید یک Task مستقل با Strategy، Approval، Test و History داشته باشد.

## Meaning of Approved

`Approved` فقط وقتی ثبت می‌شود که:

1. Strategy توسط کاربر تأیید شده باشد.
2. Build واقعی انجام شده باشد.
3. روی سخت‌افزار تست شده باشد.
4. نتیجه و شواهد در `PROJECT_HISTORY.md` ثبت شده باشد.
5. ریسک و Deviation بررسی شده باشد.
