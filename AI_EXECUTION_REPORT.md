# گزارش اجرای AI

**آخرین به‌روزرسانی:** 2026-09-23<br>
**شاخه:** `arena/01a0c744-changeover`<br>
**مالک گزارش:** Agent ارشد پروژه<br>
**قوانین مرجع:** `AI_AGENT_RULES.md`

## خلاصهٔ وضعیت

- مهاجرت رابط برنامه به CMSIS-RTOS2 در commit `20a8ca7` انجام شده است.
- مرز سخت‌افزار در commit `4995ac3` تثبیت شده و کالیبراسیون Measurement در commit `bbc7ca5` به BSP منتقل شده است.
- UI، Measurement، EspLink و Rtos دیگر `board_pins.h`، `main.h` یا headerهای STM32 را مستقیماً مصرف نمی‌کنند.
- Headerهای عمومی BSP (`bsp_gpio.h`, `bsp_adc.h`, `bsp_measurement.h`, `bsp_exti.h`, `bsp_pwm.h`, `bsp_uart.h`) HAL-free هستند.
- جزئیات HAL، هندل ADC، نگاشت پایه‌ها و کالیبراسیون مدار فقط در پورت برد/فایل‌های platform-specific باقی می‌مانند.
- Agentهای ماژول باید پیش از شروع `AI_AGENT_RULES.md` و README ماژول خود را بخوانند؛ مرز BSP قرارداد پایه است و تغییر سراسری فقط با دستور Agent ارشد انجام می‌شود.
- **ممیزی کامل برنامه ۲۰۲۶-۰۹-۲۲ انجام شد** (تک‌تک ۲۵ فایل سورس Firmware/CubeIDE + رفتار HAL چک شد): رفع race در `BspExti_TakeEvent` (بخش بحرانی PRIMASK — رویداد jitter گم‌شده دیگر تریپ JIT را قورت نمی‌داد)، حذف فراخوانی تکراری `McuPowerPath_OnInputIrq` در callback PB4، تبدیل `FAULT_ADC` در Protection به وضعیت لحظه‌ای (قبلاً latch ابدی بود و با روشن‌شدن آیندهٔ ماژول، شارژر از بوت safe-idle می‌ماند)، و اصلاح مستندات ناسازگار (docblock شارژر FLOAT/ABSORB، کامنت unfiltered قدیمی، define تکراری `MODULE_MCU_POWER_PATH`).
- جداسازی آستانهٔ قطع باتری به دستور کاربر ۲۰۲۶-۰۹-۲۲: تشخیص غیبت `FAULT_BAT_ABSENT_MV` = 6V، بازیابی همچنان `FAULT_BATTERY_BACK_MV` = 7V (هیسترزیس ۱V).
- **پروتکل ESP-Link کامل و سه‌مرحله‌ای (۲۰۲۶-۰۹-۲۲/۲۳):** UART بازنویسی‌شده با ۹۲۱۶۰۰ و DMA دوطرفه؛ فریم‌های SET_PARAM/GET_PARAMS/TLM_LIVE/PARAM_REPORT/PARAMS_BULK با payload تا ۱۱۲ بایت؛ ۲۰ پارامتر زندهٔ فقط-RAM (کالیبراسیون جریان/ولتاژ، فیلترها، بازدهی، قطع کانال، سقف و duty فیکس). قرارداد کامل در `ESP_AGENT_SPEC.md` v1.2.
- **مود تست دستی سراسری v1.2 سمت برد (۲۰۲۶-۰۹-۲۳، کامیت 03f5b8d):** پارامتر ۱۹ → تعلیق شارژر خودکار و درایو مستقیم هر کانال با ۱۶/۱۸؛ state 9 و فلگ b5؛ ددمن ۳s لینک؛ re-arm با ارسال مجدد duty؛ کف سخت‌افزاری (ورودی، JIT، ۱۵٫۰V، سقف، قطع کانال)؛ فریز `Fault_Evaluate` حین مود؛ تست Host ۲۴/۲۴.
- **پنل وب ESP ممیزی خط‌به‌خط و اصلاح شد (۲۰۲۶-۰۹-۲۳، کامیت 32098b4):** فایل `esp_link_panel/esp_link_panel.ino` در این شاخه مرجع است (چهار اصلاح: نوسازی دوره‌ای ۳۰s جدول پارامترها، کش فونت نسخه‌دار، no-store صفحهٔ اصلی، پیام خطای ورودی نامعتبر).

## وضعیت جریان جریان/نمونه‌برداری Measurement (2026-09-22)

- دورهٔ تسک Measurement `MEASUREMENT_PERIOD_MS = 1` است (دستور کاربر ۲۰۲۶-۰۹-۲۲): نمونه‌های سنکرون حداکثر ۱ms قدیمی‌اند.
- دو جایگاه جریان فریم (CURRENT1/CURRENT2) با نمونهٔ سنکرون وسط پنجرهٔ ON پالس PWM پر می‌شوند: بک‌اند خصوصی ADC2 برای هر کانال یک تبدیل regular با تریگر سخت‌افزاری انجام می‌دهد — کانال ۱ با `TIM2_CC2` و کانال ۲ با `TIM3_TRGO` (منبع OC2REF؛ CH2 داخلی هر تایمر با PWM mode 2 و `CCR2 = CCR1/2` لبه را دقیقاً وسط ON می‌گذارد). گیت پارک (compare صفر) = بدون لبه = بدون نمونه؛ fallback مقدار اسکن غیرهمزمان همان جایگاه است.
- فیلترهای جریان (دستور کاربر ۲۰۲۶-۰۹-۲۲، کلید کامپایل در `measurement.h`، پیش‌فرض روشن): **مدین-۳** (`MEASUREMENT_CURRENT_MEDIAN3_ENABLE`) و بعد **میانگین متحرک ۱۰ نمونه** (`MEASUREMENT_CURRENT_AVERAGE_ENABLE` + `MEASUREMENT_CURRENT_AVERAGE_WINDOW`، پنجرهٔ جدا per channel، شیب شروع فقط روی خانه‌های پر). EMA حذف است؛ شارژر هیچ فیلتر خودش را اضافه نمی‌کند. ولتاژها مدین-۵ دارند.
- فرمول ADC→mA در `bsp_measurement.c` مرحله‌به‌مرحله از مدار ساخته شده: آفست پر-کانال → counts به mV پایه (×3300/4095) → خنثی‌سازی تقسیم R41(1k)/R42(10k) → خنثی‌سازی گین ۱۰۱ → شانت 10mΩ به mA → گین پرمیل بنچ (۱۰۸۵). یک تقسیم نهایی روی numerator/denominator در uint64؛ نتیجه با فرمول قبلی هم‌ارز (~۰٫۸۸mA/count پیش از گین).

## وضعیت فعلی ماژول‌ها

- **Measurement**: فعال (۱ms). مسیر: ADC2 سنکرون → splice در GetRaw → تبدیل → زنجیرهٔ فیلتر → snapshot اتمیک با `osKernelLock`.
- **Charger**: فعال با `CHG_MASTER_ENABLE=1` و `CHG_TRANSFORMER_KNOWN=1`؛ دو کانال مستقل ۱۲V (Ch1→VHIGH، Ch2→VLOW) + مود تست دستی v1.2 (پارامتر ESP ۱۹، state 9). OFF→(گیت ۱۵s ثبات اتصال)→BULK(رمپ ۱٪، باند ۶۳۰..۶۵۰mA خروجی تخمینی)→ABSORB(تثبیت ۱۴٫۴V، پلهٔ ۰٫۱٪، شستشوی کل مدت + تیپر <۵۰mA/۶۰s، سقف ۱h)→FLOAT(پارک روی صفر؛ <۱۲٫۸V→BULK). JIT: نصف duty→حداکثر ۱۰٪→سومی FINAL_FAULT (رلهٔ NC باز). خطای سخت >۹۵۰mA ریست کانال (با فیلترها حداکثر ~۱۰ms تأخیر؛ JIT سخت‌افزاری حفاظت سریع است).
- **Fault**: مالک تشخیص قطع باتری — قاعدهٔ ۱: پمپ بالای ۱۴٫۸V حین شارژ فعال (۱۵۰ms)؛ قاعدهٔ ۲: هر نیم <۶V با ورودی سالم (۱s)؛ بازیابی: هر دو نیم ≥۷V بدون پمپ (۱s).
- **Jitter**: فعال؛ رویداد EXTI LM393 → صف retry شارژر (احیای تک‌تک).
- **McuPowerPath**: فعال؛ Q1/PB5 مستقل از Changeover (قطع ۵s بعد از ورودی ≥۲۲V، اتصال فوری در ISR با قطع ورودی؛ هیسترزیس ۲۲/۲۱٫۵V).
- **Changeover**: فعال؛ cut در v<20800 (یا <21000 با فلگ UI) بعد از ۳s → PB11، reconnect با ورودی + ≥21200 بعد از ۳s.
- **UI**: فعال؛ سناریوها از snapshot واقعی (اولویت: اضافه‌ولتاژ ورودی، قطع باتری، فول، شارژ فعال، InputOk، BatteryRun).
- **Protection**: خاموش (`MODULE_PROTECTION=0`)؛ `FAULT_ADC` وضعیت لحظه‌ای است (Set در نامعتبر، Clear در معتبر).
- **EspLink/Comm**: پروتکل کامل v1.2 پیاده و ممیزی‌شده (فریم‌ها، ۲۰ پارامتر، مود دستی، ددمن)؛ در build خاموش (`MODULE_ESP=0`) — فعال‌سازی فقط با همان یک خط. پنل وب سمت ESP در `esp_link_panel/` (نسخهٔ ممیزی 32098b4)؛ جزئیات در بخش «اتصال و برنامهٔ ESP».

## قرارداد BSP

- GPIO از شناسه‌های منطقی `BSP_GPIO_*` استفاده می‌کند و قطبیت active-high/active-low در پورت خصوصی اعمال می‌شود؛ PB5 و PB11 در وضعیت امن physical High هستند.
- ADC از موقعیت‌های normalized استفاده می‌کند؛ map فیزیکی فعلی PA1/PA2/PA3/PA5/PA7 + بک‌اند سنکرون ADC2 (خصوصی پورت، بدون CubeMX).
- Measurement منطق تبدیل را نگه نمی‌دارد و از `bsp_measurement` استفاده می‌کند؛ تقسیم‌های ۲۴V/۱۲V، R41/R42، گین و شانت در port هستند.
- EXTI رویدادهای منطقی `JITTER1`، `JITTER2` و `INPUT_DETECT` را با `bsp_exti_src_t` ارائه می‌کند؛ `TakeEvent` اتمیک (PRIMASK) است؛ هوک اضطراری McuPowerPath فقط یک‌بار قبل از ثبت رویداد.
- PWM: هر دو تایمر از Init پیوسته می‌چرخند با درهم‌گذاری ثابت نیم‌دوره (۱۰µs در ۵۰kHz) و استارت با دو نوشتن رجیستری پشت‌سرهم؛ خاموش = compare صفر؛ CH2 داخلی تریگر نمونه‌برداری وسط ON.
- UART با `func__BspUart_Init(void)` و جریان بایت کار می‌کند؛ USART1/PA9/PA10 و baud در public API دیده نمی‌شوند.
- در صورت تغییر MCU یا برد، فقط پورت BSP و فایل‌های platform-specific تغییر می‌کنند؛ منطق Module/App کپی نمی‌شود.

## اتصال و برنامهٔ ESP (2026-09-23)

- **لینک:** USART1 روی PA9/PA10 با ۹۲۱۶۰۰ 8N1 و DMA دوطرفه (بازنویسی `bsp_uart`، بدون بار CPU؛ audit round 2 رفع TX-stall و RX-abort). CH_PD از PA8 توسط برد کنترل می‌شود؛ برنامهٔ ESP هرگز آن را لمس نمی‌کند.
- **پروتکل v1.2:** فریم `AA 55 type len payload XOR` با payload تا ۱۱۲ بایت؛ SET_PARAM (id+u32 LE) / GET_PARAMS / TLM_LIVE (84B هر ~100ms، ۲۰ فیلد) / PARAM_REPORT / PARAMS_BULK (۲۰ پارامتر = 101B). پارامترها فقط-RAM با کلمپ در مرز تسک. مود تست دستی: پارامتر ۱۹ + ددمن ۳s + re-arm با ارسال مجدد duty + قطع ۱۵V. قرارداد کامل: `ESP_AGENT_SPEC.md` v1.2 و بخش «محدودیت‌ها و الزامات سمت ESP» در `Firmware/Modules/EspLink/README.md`.
- **پنل وب ESP:** `esp_link_panel/esp_link_panel.ino` (ESP8266/ESP32، AP «ChangeOver-ESP») — سه تب وضعیت/کالیبراسیون/تست دستی، فرمول‌های §5.3 با مقادیر زنده، دستیارهای کالیبراسیون، چارت ۳۶ ثانیه‌ای فیلتر، keepalive مستقل از مرورگر + محافظ پنل بسته (۱۰s → ID19=0). این فایل در این شاخه **ممیزی خط‌به‌خط** شده (کامیت 32098b4): سینتکس C++ هر دو مسیر ESP8266/ESP32 پاس (gcc -Wall -Wextra)، JS پاس (node --check)، فونت woff2 معتبر. عامل ESP باید همین نسخه را مبنا قرار دهد (دستور جاری در README ماژول EspLink).
- **فعال‌سازی روی برد:** فقط `MODULE_ESP=1` در `modules_enable.h` — هیچ تغییر دیگری لازم نیست.

## دستور واگذاری به Agentهای فرعی

پس از تأیید این مرحله، هر Agent ماژول باید روی branch اختصاصی خودش کار کند و این قرارداد را اجرا کند:

1. پیش از تغییر، `AI_AGENT_RULES.md`، README همان ماژول و `Firmware/Bsp/README.md` را بخواند؛ `main`، `.ioc`، `board_pins.h` و `AI_EXECUTION_REPORT.md` را تغییر ندهد.
2. فقط headerهای منطقی `Firmware/Bsp/Inc` را include کند. استفاده از پایه، `main.h`، HAL، handle، timer/channel یا ترتیب فیزیکی ADC در Module/Rtos ممنوع است.
3. از این APIها استفاده کند: GPIO با `func__BspGpio_*`، ADC با `func__BspAdc_*` و `func__BspMeasurement_*`، PWM با `func__BspPwm_*`، UART با `func__BspUart_*` و EXTI با `func__BspExti_*`.
4. قبل از هر actuator، `BOOL__G__MeasDataValid`/snapshot معتبر و policy ایمنی را بررسی کند؛ Charger بدون دادهٔ معتبر duty غیرصفر ندهد، EspLink با CH_PD خاموش شروع شود و Jitter eventهای قبلی را پاک کند.
5. تغییر mapping یا polarity را در Agent محلی انجام ندهد؛ اگر mismatch شماتیک پیدا شد، به Agent ارشد گزارش کند.
6. تغییرات را فقط در branch خودش commit کند، `bash tools/check_ai_rules.sh`، `bash tools/check_firmware_syntax.sh` و تست مربوط به ماژول را اجرا کند و hash commit، فایل‌های تغییرکرده، تست‌ها و issueهای سخت‌افزاری را گزارش دهد.

## زمان‌بندی و رفتار RTOS

- CMSIS-RTOS2 رابط عمومی برنامه است و FreeRTOS فقط backend داخلی است (tick = ۱kHz).
- Threadها با `osThreadNew` و `cb_mem`/`stack_mem` استاتیک ساخته می‌شوند (UI=128w، Measurement=192w، Control=256w؛ اولویت‌ها در `rtos_config.h`).
- تبدیل میلی‌ثانیه به tick از `osKernelGetTickFreq()` در `rtos_time` انجام می‌شود.
- از `HAL_Delay`، API مستقیم FreeRTOS در منطق محصول و تخصیص پویا استفاده نمی‌شود.
- snapshot Measurement هنگام انتشار/کپی با `osKernelLock` و `osKernelRestoreLock` محافظت می‌شود و به دستور مخصوص هستهٔ MCU وابسته نیست.
- ADC و DMA توسط backend برد اجرا می‌شوند؛ Task Measurement فقط فریم کامل را مصرف می‌کند. انتظار bounded تبدیل سنکرون (~۲۰µs به‌ازای هر کانال، وقفه‌ها فعال) بدترین حالت ~۵٪ CPU تسک اندازه‌گیری در دورهٔ ۱ms مصرف می‌کند.

## اعتبارسنجی انجام‌شده (2026-09-22)

- `bash tools/check_firmware_syntax.sh` — موفق؛ syntax سورس‌های CubeIDE/Core و Firmware با GCC سمت Host.
- `python3 Firmware/Modules/Charger/host_test_charger.py` — **۲۴/۲۴ موفق** (از v1.2: شامل قرارداد مود دستی — تست `test_manual_test_mode_v12`)
- ممیزی خط‌به‌خط پنل ESP (2026-09-23) — سینتکس C++ هر دو ESP8266/ESP32 پاس، JS پاس، فونت معتبر، نگاشت‌های پروتکل تطبیق کامل (قرارداد فیلترهای کلیددار، فرمول مرحله‌ای، تریگرهای سخت‌افزاری، آستانهٔ ۶V/۷V قطع باتری و...).
- `python3 Firmware/Modules/Ui/host_test_ui.py` — موفق.
- `bash tools/check_ai_rules.sh` — فقط **یک FAIL شناخته‌شده**: چک [17] انتظار `CHG_MASTER_ENABLE=0` دارد تا «تکمیل bring-up»؛ bring-up کامل شده و کاربر روشن‌بودن شارژر را انتخاب کرده است (تصمیم کاربر ۲۰۲۶-۰۹-۲۲: دست نزنیم).
- کامپایل جداگانهٔ هر ۴ ترکیب کلیدهای فیلتر جریان (هر دو روشن/خاموش و تک‌کلید) — موفق.
- راستی‌آزمایی رفتار فیلترها روی host (هارنس): پرش تک‌نمونه‌ای حذف، پلهٔ واقعی با تأخیر یک نمونه، میانگین فقط روی خانه‌های پر.
- بازبینی رفتار HAL: `HAL_ADCEx_Calibration_Start` روی F1 ADC را روشن (ADON) جا می‌گذارد — تریگر خارجی ADC2 بعد از کالیبراسیون کار می‌کند.
- `git diff --check` — موفق.

## محدودیت‌های اعتبارسنجی

- Build و لینک واقعی STM32، symbol/map و اندازه‌گیری RAM/Flash هنوز اجرا نشده است؛ `arm-none-eabi-gcc` و STM32CubeIDE در محیط موجود نیستند. مصرف RAM فیلترهای جریان با هر دو کلید روشن ~۱۰۸ بایت استاتیک است (از Map file نهایی تأیید شود).
- تحلیل رسمی MISRA با ابزار اختصاصی انجام نشده است.
- نمونه‌برداری سنکرون، فیلترهای جریان و مسیر شارژ کامل هنوز روی برد واقعی با اسکوپ تأیید نشده‌اند؛ نقطهٔ بنچ کانال ۱ (آفست/گین Trans1) هم هنوز ثبت نشده (فعلاً کپی موقت کانال ۲).
- تست Host/syntax جایگزین تست عملی برد نیست؛ نتایج تست واقعی باید در Workbookهای اعتبارسنجی مربوط ثبت شوند.

## مستندات مرجع

- قرارداد ESP: `ESP_AGENT_SPEC.md` (v1.2) و برنامهٔ پنل `esp_link_panel/esp_link_panel.ino`
- ماژول‌ها: `Firmware/Modules/*/README.md` (Measurement، Charger، Fault، Jitter، McuPowerPath، Changeover، UI، Protection، EspLink).
- برد و پورت‌ها: `Firmware/Bsp/README.md` و `CubeMX/README.md`.
- مرجع State/سناریو: `Documentation/System_State_Scenarios.xlsx` و `Documentation/System_State_Machine.md/.drawio` (منطق Changeover از ۲۰۲۶-۰۹-۱۶ دست‌نخورده است و با کد فعلی سازگار است).
- ورودی‌های اصلی محصول: `DOC/State.xlsx` (جدول ورودی/خروجی و UI — سند مرجع اولیه).
