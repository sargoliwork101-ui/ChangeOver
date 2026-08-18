# ESP8266-01 Web Debugger Sample

این پوشه نمونه‌ی مستقل Arduino برای `ESP8266-01` است. فایل اصلی برنامه:

```text
ESP8266_WebDebugger.ino
```

## وضعیت نمونه

این پروژه برای Bring-up مرحله‌ای طراحی شده است و هیچ قابلیت آن هنوز Approved محصول نیست.

Baseline نهایی عمداً تمام قابلیت‌ها را خاموش نگه می‌دارد:

```text
تمام ESP_FEATUREها = OFF
```

برای شروع Phase 0 فقط این سه مورد را موقتاً فعال کن:

```text
ESP_FEATURE_WIFI = 1
ESP_FEATURE_WEB_SERVER = 1
ESP_FEATURE_LITTLEFS = 1
```

بعد از تست و ثبت نتیجه، هر Feature بعدی جداگانه فعال می‌شود.

## ساختار

- `ESP8266_WebDebugger.ino`: setup و loop اصلی
- `ESP_LinkManager`: دریافت UART و اتصال لایه‌ها
- `ESP_ProtocolParser`: parser فریم‌های T و D و اسکلت پروتکل جدید
- `ESP_DiagnosticsStore`: ذخیره‌ی Ring Buffer خطاها روی LittleFS
- `ESP_TelemetryStore`: ذخیره‌ی نمونه‌های نمودار
- `ESP_AutoTest`: تست‌های مرحله‌ای و Read-Only
- `ESP_WebServer`: Routeها و APIها
- `data/`: صفحات HTML، CSS و JavaScript جداگانه

## صفحات

```text
/
/debug
/charts
/test
/settings
```

## نصب با Arduino IDE

1. هسته‌ی ESP8266 را در Arduino IDE نصب کن.
2. Board را روی `Generic ESP8266 Module` یا برد متناظر ESP-01 قرار بده.
3. برای اولین تست، فقط Featureهای Phase 0 را فعال کن:

```text
ESP_FEATURE_WIFI = 1
ESP_FEATURE_WEB_SERVER = 1
ESP_FEATURE_LITTLEFS = 1
```

4. پروژه را با فایل `ESP8266_WebDebugger.ino` باز کن.
5. بعد از فعال‌کردن Phase 0، SoftAP با نام زیر ایجاد می‌شود:

```text
SSID: ChangeOver-Debug
Password: changeover
```

6. اگر `ESP_FEATURE_LITTLEFS` فعال است، پوشه‌ی `data/` را با ابزار Upload LittleFS روی ESP بارگذاری کن.
7. Serial Monitor را با Baud Rate زیر باز کن:

```text
115200
```

8. بعد از اتصال به SoftAP، آدرس زیر را باز کن:

```text
http://192.168.4.1/
```

## UART0 و پروگرام ESP

ESP8266-01 برای پروگرام از UART0 استفاده می‌کند:

```text
GPIO1 = TX
GPIO3 = RX
```

طبق تصمیم پروژه، هنگام پروگرام ماژول ESP از مسیر ارتباط با STM32 جدا می‌شود. بعد از پروگرام، دوباره به UART STM32 متصل می‌شود.

در زمان اتصال به STM32:

```text
ESP TX → STM RX
ESP RX → STM TX
GND مشترک
سطح منطقی 3.3V
```

## پروتکل فعلی

نسخه‌ی اولیه‌ی STM32 این خطوط را ارسال می‌کند:

```text
T,<input24v_mv>,<battery24v_mv>,<battery12v_mv>,<current1_ma>,<current2_ma>,<state>,<fault_mask>
D,<code>,<severity>,<value>,<fault_mask>,<state>,<occurrence_count>
```

Parser فعلی از فریم‌های Legacy پشتیبانی می‌کند. Handshake و Sequence/CRC در Feature Flag جدا هستند و تا آماده‌شدن سمت STM32 فعال نمی‌شوند.

## ذخیره‌سازی

Storeها حتی با خاموش‌بودن Storage در RAM کار می‌کنند تا صفحه‌ی Debugger و Charts قابل تست باشند. وقتی Featureهای ذخیره‌سازی فعال شوند، داده‌ها روی LittleFS هم Flush می‌شوند:

```text
/diagnostics.bin
/telemetry.bin
/esp_config.txt
```

نوشتن Flash باید Batch شود تا عمر LittleFS کاهش پیدا نکند.

## تست خودکار

صفحه‌ی `/test` برای تست‌های زیر آماده شده است:

- WiFi
- Web Server
- LittleFS
- Parser
- دریافت STM32
- حافظه‌ی ESP
- UART
- Handshake
- تست‌های Read-Only

فرمان به STM32، تست Relay، Battery Switch و PWM به‌صورت پیش‌فرض خاموش هستند.

## هشدار

Arduino `.ino` در عمل C++ است. قواعد MISRA C مربوط به Firmware STM32 هستند. این بخش با Embedded Safe C++ نوشته شده و از Exception، `new/delete` در مسیر Runtime و عملیات بدون محدودیت پرهیز می‌کند.
