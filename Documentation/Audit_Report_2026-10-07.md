# گزارش پاس برنامه‌نویس برد — ۲۰۲۶-۱۰-۰۷
# Embedded-programmer pass — 2026-10-07

## دامنه / Scope

پاس «برنامه‌نویس برد» روی شاخهٔ `arena/ffde804c-changeover`: خواندن خط‌به‌خط
لایه‌های محصول (App، Rtos، Bsp، Config و ماژول‌های Changeover/Charger/Ui/
Measurement/EspLink)، بررسی شماتیک `Circuit/ChangeOver(24V_DC).pdf` برگِ MCU و
برگ Analog/Indicator، و رفع ایرادهای واقعی بدون دست‌زدن به منطق اصلی.

Board-programmer pass on branch `arena/ffde804c-changeover`: line-by-line read
of the product layers (App, Rtos, Bsp, Config and the Changeover/Charger/Ui/
Measurement/EspLink modules), schematic review of the MCU, Analog-Input and
Indicator&Sound sheets, and fixing real defects without touching the main
logic.

## بررسی شماتیک در برابر کد / Schematic vs code

| سیگنال شماتیک | پایهٔ MCU | board_pins.h | نتیجه |
|---|---|---|---|
| MCU_PWM1 | PA0 (TIM2_CH1) | PIN_PWM1 | ✔ |
| ADC_CURRENT1 | PA1 | PIN_ADC_CURRENT1 | ✔ |
| MCU_ADC_24_IN | PA2 | PIN_ADC_24_IN | ✔ |
| MCU_ADC_24_BAT | PA3 | PIN_ADC_24_BAT | ✔ |
| MCU_BUZZER | PA4 | PIN_BUZZER | ✔ |
| MCU_ADC_12_BAT | PA5 | PIN_ADC_12_BAT | ✔ |
| MCU_PWM2 | PA6 (TIM3_CH1) | PIN_PWM2 | ✔ |
| ADC_CURRENT2 | PA7 | PIN_ADC_CURRENT2 | ✔ |
| MCU_ESP_CHPD | PA8 | PIN_ESP_CHPD | ✔ |
| MCU_TX / MCU_RX | PA9 / PA10 | PIN_TX / PIN_RX | ✔ |
| MCU_R_LED / Y / G | PB0 / PB1 / PB10 | PIN_LED_R/Y/G | ✔ |
| MCU_JITTER1 / 2 | PB2 / PB6 (EXTI falling) | PIN_JITTER1/2 | ✔ |
| MCU_INT_24_IN | PB4 | PIN_INT_24_IN | ✔ |
| MCU_BAT_SWITCH | PB5, active-low (Q1) | PIN_BAT_SWITCH, ACTIVE_HIGH=0 | ✔ |
| MCU_PROTECT_CHARGER | PB7 (رله) | PIN_RELAY | ✔ |
| MCU_PROTECT_BATT / LOW_BAT | PB11, active-low (Q17) | PIN_PROTECT_BATT, ACTIVE_HIGH=0 | ✔ |
| SWDIO / SWCLK / SWO | PA13 / PA14 / PB3 | — (دیباگ) | ✔ |

- قطبیت‌ها از برگ Indicator&Sound تأیید شد: LED/بوق ترانزیستور NPN با بار به
  +5V → active-high؛ مسیرهای باتری Q1/Q17 → active-low؛ هر دو با
  `board_pins.h` یکسان‌اند.
- مقسم‌های آنالوگ: سری 1.2K (R11/R13/R15) + شنت 6.8K (R12/R14/R16) و تاپ
  68K/68K/33K (R46/R47/R48) روی برگ قدرت — دقیقاً همان
  `BSP_MEASUREMENT_SENSE_*` در `bsp_measurement.c`.
- ترتیب اسکن ADC در `main.c` (CH1,CH2,CH3,CH5,CH7,VREFINT) با قرارداد
  `bsp_adc.h` یکی است.

## ایرادهای یافته‌شده و رفع‌شده / Defects found and fixed

1. **دروازهٔ RTL قرمز بود** (۵ خط در `charger.c:3664` و `plink_panel.h`):
   کامنت‌های فارسیِ بدون علامت جهت، `tools/check_ai_rules.sh` را می‌شکستند
   برخلاف ادعای گزارش قبلی. با خود ابزار پروژه
   (`python3 tools/fix_rtl_comments.py`) اصلاح شد.
2. **مهر بیلد پنل کهنه شد** پس از ویرایش `plink_panel.h`: با
   `tools/stamp_panel.py` تازه شد و `panel_preview.html` بازتولید شد (دستور
   دائمی کاربر: پنل شبیه‌سازی‌شده همیشه تازه).
3. **بودجهٔ انتقال پنل ۸ بایت شکست** (۳۸۴۰۰۸ > ۳۸۴۰۰۰): علامت‌های U+200E
   اجباری مارک‌آپ را از پلهٔ ۳۷۵ کیلوبایت جلو زدند؛ سقف عمداً و مستند
   ۳۷۵→۳۷۶ کیلوبایت (۳۸۴۰۰۰→۳۸۵۰۲۴) در `plink_config.h` پله خورد — رفتار،
   ارسال تکه‌ای و ممیزی بدون تغییر.

## خروج ابزارها / Tool output

| ابزار | نتیجه |
|---|---|
| `bash tools/check_ai_rules.sh` | ALL CHECKS PASSED (شامل RTL) |
| `bash tools/check_firmware_syntax.sh` | PASSED — ESP C++ + همهٔ تست‌های هاست (Measurement 2346، CalLut 125، Charger، Imbalance، Changeover، Fault، Protection، Jitter، McuPowerPath، EspLink NVM/parser) |
| `python3 tools/audit_consistency.py` | 443 invariant، 0 finding |
| `python3 tools/fix_rtl_comments.py --check` | PASS |

## خارج از محیط / Not verifiable here

بیلد ARM واقعی (`arm-none-eabi-gcc`/CubeIDE) و تست فیزیکی برد در این محیط
نیست؛ بررسی شماتیک از متن/وکتور خود PDF استخراج شده، نه چشم‌انداز ابزار CAD.

## پاس دوم — بررسی خط‌به‌خط کل برنامه / Second pass - whole-program line-by-line

در پاس دوم همهٔ فایل‌های BSP/Rtos/Config/Core و ماژول‌ها بازخوانی شدند
(bsp_adc، bsp_measurement، bsp_flash، bsp_pwm، bsp_uart، mcu_power_path،
ui_buzzer، changeover، fault، cal_lut، بردارهای وقفه، FreeRTOSConfig و
لینکر). تقسیم‌های متغیره همگی نگهبان صفر دارند و حلقه‌ها کران‌دارند.

ایراد واقعی یافته‌شده و رفع‌شده: **۲۰۵ تگ Doxygen شکسته** — یک نویسهٔ نامرئی
LRM بین `@` و `param`/`return` (۱۲۵ `@param` و ۷۸ `@return` در ۲۶ فایل) که
تگ‌ها را عملاً متنی عادی می‌کرد؛ خود دروازهٔ قوانین هم آن را بی‌صدا نشان می‌داد
(`rtos_tasks.h: funcs=5 @param=0`). نویسه‌ها فقط از تگ‌ها حذف شدند (بدون هیچ
تغییر منطقی) و علامت‌های جهت مجازِ کامنت‌های فارسی دست‌نخورده ماندند؛ بعد از
اصلاح، `rtos_tasks.h` در خروجی دروازه `@param=5` گزارش می‌شود.

| ابزار | نتیجهٔ پس از پاس دوم |
|---|---|
| `bash tools/check_ai_rules.sh` | ALL CHECKS PASSED |
| `bash tools/check_firmware_syntax.sh` | PASSED (همهٔ تست‌های هاست و ESP) |
| `python3 tools/audit_consistency.py` | 443 invariant، 0 finding |

The second pass re-read every BSP/Rtos/Config/Core file and module
(bsp_adc, bsp_measurement, bsp_flash, bsp_pwm, bsp_uart, mcu_power_path,
ui_buzzer, changeover, fault, cal_lut, the IRQ vectors, FreeRTOSConfig and
the linker script). All variable divisors carry zero guards and all spins are
bounded.

Real defect found and fixed: **205 broken Doxygen tags** - an invisible LRM
sat between `@` and `param`/`return` (125 `@param`, 78 `@return`, 26 files),
turning the tags into plain text; the rules gate showed it silently
(`rtos_tasks.h: funcs=5 @param=0`). Only the tag characters were cleaned (no
logic touched); the legitimate RTL marks inside Persian comments remain. After
the fix the gate reports `rtos_tasks.h @param=5`.

---

## Pass 3 addendum / پیوست ۲۰۲۶-۱۰-۰۷ (مرور سوم، بدون یافتهٔ جدید) / 2026-10-07 pass-3 addendum (no new finding)

**[FA] مرور سوم خط‌به‌خط (دستور کاربر). این بار بدون هیچ تغییر کد؛ نتیجهٔ خالص: ایراد کارکردی جدیدی پیدا نشد.**
**[EN] Third line-by-line pass (user order). No code changed this pass; net result: no new functional defect.**

| حوزه / Area | نتیجه / Result |
|---|---|
| `measurement.c` — کل `func__Measurement_Run` (median-5، آفست‌ها قبل از تفاضل، گارد `V24>=V12`، تغییر فیلتر، قفل کرنل روی snapshot) | سالم / clean |
| `esp_link_nvm.c` — زنجیرهٔ static_assert (۲ رکورد در صفحه، banks روی NVM/LUTNVM/SPARE، پنجرهٔ id بازنشستهٔ ۷۲/۷۳/۷۶) | سالم / clean |
| `app_config.c` + `rtos_config.h` (۱۰/۵/۱۰۰ میلی‌ثانیه؛ اولویت‌ها؛ پشته‌ها ۱۲۸/۱۹۲/۱۹۲/۲۵۶/۲۵۶) | هم‌راستا / consistent |
| `jitter.c`، `protection.c` (FAULT_ADC زنده، نه چسبنده)، `ClampAlarms/ClampPid` شارژر (کف imax+50، سقف‌های CHG_*) | سالم / clean |
| `plink_link.h` — ماشین ارسال گام‌به‌گام LUT (v1.67 finding L1: یک فریم در هر ACK، مهلت ۶۰۰/۲۵۰۰ ms، retry=3، خطای ۱/۲) | سالم / clean |
| تقسیم متغیر در کل Firmware (grep) | فقط ۳ مورد، هر سه گارددار (قبلاً بررسی شد) / 3 guarded hits |
| جاروب `-Wconversion -Wsign-conversion` روی همهٔ `Firmware/*.c` | تنها هشدار کد خودی: `CHG_DEAD_PARAM_INDEX` در `charger.h:1752` (ریختن id u16 به u8) — به‌دلیل گارد قبلی `CHG_DEAD_PARAM_OWNS` پیچش ناممکن است؛ **عمدی، بدون ریسک** / benign, guarded, intentional |
| سیم‌کشی وقفه‌ها: EXTI2(PB2)/EXTI4(PB4)/EXTI9_5(PB6) در `stm32f1xx_it.c`، `DMA1_Channel4`/`USART1` در `bsp_uart.c`، TIM1_UP | تطبیق با نقشهٔ صفحهٔ ۴ / matches schematic page 4 |
| دروازه‌ها (اجرای تازه) / gates (fresh run) | `check_ai_rules.sh` ALL PASSED؛ `check_firmware_syntax.sh`: ۹۸ تست ESP + buzzer + UI + ۵۲ تست شارژر ALL PASSED؛ `audit_consistency.py` ۴۴۳ بررسی PASSED |

**[EN] Coverage note / یادداشت پوشش:** across the three passes every firmware source file has been read at least
once and the ESP sketch plus its shared headers have been reviewed; what no host check can cover remains the
radio/flash/board behaviour, which belongs in the module Validation Excel files. / در سه مرور هر فایل منبع
Firmware حداقل یک بار کامل خوانده شده و اسکچ ESP و هدرهای مشترکش بررسی شده‌اند؛ آنچه هیچ تست هاستی پوشش
نمی‌دهد رفتار رادیو/فلش/برد واقعی است که جای آن در فایل‌های Validation ماژول‌هاست.

---

## Pass 4 addendum / پیوست ۲۰۲۶-۱۰-۰۷ (مرور چهارم - شکار دسته‌ای) / 2026-10-07 pass-4 addendum (class-based hunt)

**[FA] مرور چهارم به‌جای خواندن ترتیبی، چهار «دستهٔ ایراد» را سیستماتیک شکار کرد. هیچ ایراد کارکردی تازه‌ای پیدا نشد؛ کد تغییر نکرد.**
**[EN] The fourth pass hunted four defect CLASSES systematically instead of reading sequentially. No new functional defect; no code changed.**

| دسته / Class | روش / Method | نتیجه / Result |
|---|---|---|
| حلقهٔ بافر و DMA ی UART | خواندن کامل `bsp_uart.c` | سالم: نوشتن اتمیک کل-فریم، گارد `-1u` پرنشدن حلقه، `TxWriteActive`، مدولوی آرتیفکت `CNDTR=0`، commit-first در پمپ / clean |
| پارسر فریم و مسیریابی پارامتر | خواندن کامل `ParseByte`/`ResyncFromByte`/`HandleFrame`/`HandleLutFrame`/`ApplyParam` | سالم: resync روی `AA AA 55` و `AA` در نسخه/طول/CRC-HI، گارد طول، `StagePoint` با بررسی اندیس / clean |
| سرریز و بی‌علامتی | grep شیب‌دار روی `* 1000`/`<< 8`/`<< 16` و تایمرهای u16؛ بررسی `yHigh - yLow` بدون‌علامت در درون‌یابی LUT | سالم: تفریق بدون‌علامت درون‌یابی با سه لایه محافظت می‌شود - `ChannelCheck` در `Commit` پیش از نوشتن، `RecordValidate` هنگام بوت/پس از نوشتن، جدول‌های کامپایل صعودی؛ تست هاست CalLut هر دو رد `ST_CHAIN`/`ST_POWER` را پوشش می‌دهد / triple-guarded, tested |
| اشتراک ISR/تسک | ممیزی همهٔ متغیرهای نوشته‌شده در وقفه | سالم: `bsp_exti.c` با `__disable_irq`/PRIMASK اتمیک، پرچم‌های `mcu_power_path` و `bsp_uart` همگی `volatile` / clean |

**[EN] Full-suite verification run this pass / اجرای کامل همهٔ سوئیت‌ها در این مرور:**
CalLut 125/0، Changeover 114/0، EspLink NVM PASS، EspLink parser 15/0، Fault 48/0، Imbalance 3805/0، Jitter 24/0،
McuPowerPath 49/0، Measurement 2346/0، Protection 61/0، ESP panel (داخل گیت) 98 تست + charger 52 تست + UI/buzzer PASS،
`audit_consistency.py` 443 بررسی PASS، `check_ai_rules.sh` ALL PASSED. / بیش از ۶٬۶۰۰ بررسی، صفر خطا.

**[FA] جمع‌بندی چهار مرور:** هر فایل منبع Firmware و اسکچ ESP حداقل یک‌بار کامل خوانده شده و چهار دستهٔ ایراد
کلاسیک امبدد (بافر/DMA، پارسر پروتکل، سرریز/بی‌علامتی، مسابقهٔ ISR) شکار سیستماتیک شده‌اند. ایراد کارکردی
بازِ شناخته‌شده‌ای در کد باقی نمانده است؛ تنها پوشش‌نداده، رفتار واقعی رادیو/فلش/برد است (فایل‌های Validation).
**[EN] Four-pass bottom line:** every firmware source file and the ESP sketch have been read in full at least once,
and the four classic embedded defect classes (ring/DMA, protocol parser, overflow/unsigned, ISR races) have been
systematically hunted. No known open functional defect remains; only real radio/flash/board behaviour stays
uncovered here and belongs in the module Validation Excel files.

---

## Professional-review pass / مرور حرفه‌ای ۲۰۲۶-۱۰-۰۷ (اصلاحات بدون تغییر منطق) / 2026-10-07 professional pass (non-logic fixes)

**[FA] مرور به سبک برنامه‌نویس حرفه‌ای: ابزارهای هشدار روی کل کد، سپس اصلاح یافته‌ها. هیچ منطقی تغییر نکرد؛ رفتار روی هدف ۳۲ بیتی بیت‌به‌بیت یکسان است.**
**[EN] Professional-style review: warning tooling over the whole tree, then the findings fixed. No logic changed; target behaviour is bit-identical.**

| # | فایل / File | اصلاح / Fix |
|---|---|---|
| 1 | `Firmware/Bsp/Src/bsp_flash.c` | تبدیل «نشانی فلش → اشاره‌گر» از `(uintptr_t)` می‌گذرد + `#include <stdint.h>`؛ هشدار ناهم‌اندازه‌بودن در ساخت هاست رفع شد (ARM32: کد یکسان) / portable address->pointer cast |
| 2 | `Firmware/Modules/EspLink/esp_link_nvm.c` | همان الگو در دو نقطه: `func__EspLink_NvmPageRecord` و حلقهٔ بازخوانیِ پس از نوشتن + `#include <stdint.h>` / two uintptr_t casts |
| 3 | `Firmware/Modules/Charger/charger.c` | تنها هشدار `-Wconversion` باقی‌ماندهٔ کد خودی (`CHG_DEAD_PARAM_INDEX` در `func__Charger_SetDeadParam`): cast صریح `(uint8_t)` در انتساب + یادداشت «گارد OWNS پیش از این، بازه را به ۰..۸ محدود کرده». سبک MISRA برای باریک‌کردنِ مقصوددار / explicit intended narrowing |
| 4 | `Firmware/Modules/Protection/protection.h` و `.c` | رفع **تناقض مستند با پیاده‌سازی**: برچسب کهنهٔ «(placeholder)/اسکلت» برداشته شد و عبارت «latch faults» اصلاح شد - پیاده‌سازی عمداً `FAULT_ADC` را زنده (بدون قفل) نگه می‌دارد؛ خلاصهٔ دقیق دوزبانه + `@note` ممیزی اضافه شد. `Init` خالی هم مستند شد (ماژول بی‌حالت است) / stale "placeholder" removed, "latch faults" wording corrected to the deliberate LIVE-not-latched design |

**[EN] Method note / یادداشت روش:** the first quick sweep falsely reported "clean" because `charger.c` never compiled
(`cmsis_os2.h` missing from the include set and the fatal error filtered out). The corrected sweep with the full
CubeIDE include set is what produced findings 1-3 - a reminder that a warning sweep must prove the files compiled. /
جاروب اول به‌غلط «پاک» گزارش کرد چون `charger.c` اصلاً کامپایل نشده بود (فقدان `cmsis_os2.h` و فیلترشدن خطای مهلک)؛
جاروب اصلاح‌شده با مجموعهٔ کامل include های CubeIDE یافته‌های ۱ تا ۳ را داد.

**[EN] Verification / اعتبارسنجی:** `check_ai_rules.sh` ALL PASSED (RTL fixer دو نشانهٔ جهت به خطوط `@note` جدید افزود)؛
`check_firmware_syntax.sh`: ۹۸ تست ESP + ۵۲ تست شارژر + UI/buzzer ALL PASSED؛ `audit_consistency.py` ۴۴۳ بررسی PASSED؛
هر ۱۱ سوئیت هاست (CalLut/Changeover/NVM/parser/Fault/Imbalance/Jitter/McuPowerPath/Measurement/Protection) PASS؛
جاروب `-Wall -Wextra` و `-Wconversion -Wsign-conversion` با include کامل: صفر هشدار کد خودی.

---

## Panel CRC-warning fix / رفع هشدار CRC پنل ۲۰۲۶-۱۰-۰۷ / 2026-10-07

**[FA] گزارش کاربر: هشدار زرد «N فریم به‌خاطر خطای CRC رد شد» در پنل، که با بستن و رفرش هم برمی‌گشت.**
**[EN] User report: the yellow "N frames rejected by CRC" panel warning, which came back after closing/refreshing.**

**[FA] ریشهٔ ایراد (نه نویز سیم، نه پروتکل):** STM32 هر ‎100ms‎ بی‌قید تله‌متری می‌فرستد و ESP بایت‌ها را فقط در
‎loop()‎ تخلیه می‌کند. هندلر صفحهٔ اصلی ‎~375KB‎ را در ‎~188‎ تکهٔ ‎2KB‎ داخل یک فراخوانی ‎handleClient‎ می‌فرستد؛
روی کلاینت کند این چند ثانیه طول می‌کشد و در این مدت هیچ‌کس حلقهٔ RX را خالی نمی‌کند. حلقهٔ ‎1024‎ بایتی فقط
‎~1s‎ حاشیه دارد، پس سرریز می‌کند، بایت‌ها وسط فریم گم می‌شوند و پارسر ESP هر فریم ناقص را با CRC رد می‌کند.
شمارندهٔ ‎ce‎ سمت ESP تجمعی و بدون ریست است، پس هشدار همیشه می‌ماند و «رفرش» خودش خطای تازه اضافه می‌کند.
**[EN] Root cause (not wire noise, not the protocol):** the STM32 streams telemetry every 100 ms unconditionally
and the ESP drained UART only in loop(). The root handler streams the ~375 KB page as ~188 x 2 KB slices inside a
single handleClient call; on slow clients that takes seconds during which nothing empties the 1024-byte RX ring
(~1 s of headroom), so it overflows, bytes are lost mid-frame and the ESP parser rejects each broken frame on CRC.
The ce counter is cumulative on the ESP, so the warning persists - and every refresh adds fresh errors.

**[FA] اصلاحات (فقط سمت ESP، پروتکل و STM32 دست‌نخورده):**
**[EN] Fixes (ESP side only; protocol and STM32 untouched):**

| فایل / File | تغییر / Change |
|---|---|
| `esp_link_panel/plink_link.h` | تابع ‎func__Esp_DrainSerial()‎ اضافه شد (تخلیهٔ بی‌بلوکهٔ UART در پارسر) / new non-blocking drain helper |
| `esp_link_panel/plink_http.h` | در حلقهٔ تکه‌های صفحه، بعد از هر ‎sendContent_P‎ تخلیه انجام می‌شود / drain after every 2 KB slice |
| `esp_link_panel/esp_link_panel.ino` | ‎loop()‎ همان تابع مشترک را صدا می‌کند (یک پیاده‌سازی، دو فراخوان) / loop() now shares the helper |
| `esp_link_panel/plink_config.h` | حلقهٔ RX از ‎1024‎ به ‎2048‎ (حاشیهٔ ‎~2s‎ برای ‎streamFile‎ و مسیرهای داخلی وب‌سرور که از بیرون تخلیه‌پذیر نیستند) / RX ring 1024->2048 |

**[EN] Verification / اعتبارسنجی:** `check_ai_rules.sh` ALL PASSED؛ `check_firmware_syntax.sh`: ۹۸ تست ESP (اسکچ واقعی
کامپایل‌شده) + ۵۲ تست شارژر + UI/buzzer ALL PASSED؛ `audit_consistency.py` ۴۴۳ بررسی PASSED؛
`make_panel_preview.py` دوباره تولید شد (محتوای پنل تغییر نکرد - فایل بایت‌به‌بایت یکسان) و سرور شبیه‌ساز restart شد.
**[FA] نکتهٔ عملی برای کاربر: شمارندهٔ قدیمی در RAM ی ESP تا ریست برق می‌ماند؛ بعد از فلش‌کردن نسخهٔ اصلاح‌شده از صفر
شروع می‌شود. اگر بعد از آن هم هشدار دیده شد، آن‌وقت واقعاً نویز سیم/زمین است.**
**[EN] Note: the old count stays in the ESP's RAM until power-cycle; after flashing the fixed sketch it starts from
zero. If the warning ever appears again afterwards, THEN it is genuine wire/ground noise.**

---

## Panel UX pass + lossless-drain proof / مرور UX پنل و اثبات بی‌اتلافی ۲۰۲۶-۱۰-۰۷ / 2026-10-07

**[EN] 1) Data-loss question answered with a test, not words.** The user asked whether the in-handler UART drain
(CRC-warning fix) itself loses data. It does not: the drain calls the exact same parser path loop() used, and
`func__Esp_HandleFrame` contains zero HTTP calls (no re-entrancy). New host-test section 16 proves it end-to-end:
five TLM frames waiting on the UART while `func__Esp_HttpRoot()` streams the panel are ALL parsed (seq reaches
204), zero new CRC errors, ring empty afterwards. Suite: 98 -> 102 checks, all pass. The pre-fix code was the
one losing data (ring overflow); reverting would restore the loss, so the fix stays.
**[FA] سؤال «آیا داده از دست می‌رود؟» با تست پاسخ داده شد نه با حرف: پنج فریم منتظر حین ارسال صفحه همگی پارس
می‌شوند (تست ۱۶، سوئیت ۹۸→۱۰۲). کد قبلی داده از دست می‌داد؛ اصلاح ماندنی است.

**[EN] 2) Panel changes by user order (plink_panel.h; stamp 96317bb -> d225ed8, preview regenerated, server
restarted):** removed the «شبیه‌ساز سناریوها» launch card and its `opensim()`; centered the fault-LED strip
(`justify-content:center`) and its title; charger-card current & duty fonts another -20% (22.4 -> 17.92px) and
the battery-voltage line (label + number) now uses that same font (unit 11 -> 12px); bench tab: manual-duty list
and SWEEP controls merged onto ONE row with duty first (note text «فهرست دستی بالا» -> «کنارش»); removed the four
card-header hint spans (ids 33..34 / 35..37 / median+average / 0..3+9..10).
**[FA] تغییرات پنل به دستور کاربر: حذف کارت شبیه‌ساز سناریوها، وسط‌چین LED های فالت، ۲۰٪ کوچک‌تر شدن فونت جریان
و duty و هم‌فونت‌شدن ولتاژ باتری، یکی‌شدن ردیف duty دستی و SWEEP در تب بنچ (duty اول)، و حذف چهار راهنمای
سربرگ کارت‌ها. مهر پنل نو شد، پیش‌نمایش بازتولید و سرور restart شد.

**[EN] Gates:** check_ai_rules ALL PASSED (RTL fixer added one direction mark); check_firmware_syntax: 102 ESP +
52 charger + UI/buzzer ALL PASSED; audit_consistency 443 checks PASSED; preview spot-checks confirm every edit.

## Calibration-card follow-up (user report, 2026-10-07)

Four user reports on the «کالیبراسیون خودکار از همین جدول» card; findings and
fixes. Panel stamp after this pass: fdfb1c3.

### 1) Numbers rendered RTL in the calibration tables — FIXED
The calibration result table (caltb), the live difference column (caldiff) and
the bench sample list (calsl) put numbers into RTL table cells with no
direction override, so decimal points and +/- signs jumped to the wrong end of
the line. Fix: every numeric fragment is now wrapped in `<span dir="ltr">` or
rendered inside a `dir="ltr"` cell — current-board values, signed diffs, clamp
notices, R² figures, residual maxima, and the duty/raw/mA/mV sample columns.

### 2) "Calibration settings are not sent to the board / do nothing" — ROOT CAUSE: simulator display gap, NOT a broken send path
Evidence the send path works: (a) host suite drives the real sketch HTTP
handler — POST /s emits a PLINK_CMD_SET_PARAM frame on the wire (tests 102);
(b) live simulator test — POST /s?id=0&v=77 then GET /t returns p[0]=77.
What was broken: the simulator telemetry ignored the calibration parameters
entirely (t[2..4] = raw simulated current, voltages uncorrected), so applying
a calibration produced no visible change anywhere on the panel — the effect
the user described. Fix in tools/panel_preview_server.js telemetry(): currents
now follow chain mA = (raw − offset) × K × gain/1000 relative to the sim's
default offset/gain (ids 0..3), and voltages take the additive offsets after
the raw-count derivation (ids 4..6), with the v24 = low + high invariants
re-imposed. Defaults leave the demo unchanged; live test: gain 1046→1200
moved 52→60 mA (+14.7%), vin offset +300 mV moved 24100→24400, pack offset
−500 mV moved vhigh −500, invariant held, defaults restored afterwards.

### 3) Calibration mathematics — VERIFIED CORRECT
calfit() is a textbook ordinary least-squares fit (slope, intercept, R²,
max residual, span all correct). The gain/offset derivation matches the
firmware chain exactly: firmware computes mA = (raw − off) × K_MA × g/1000,
the panel solves g = round(a×1000/K_MA) and o = round(−b/a) from the fit
y(DMM mA) ~ x(raw counts) — substituting reproduces the DMM line. Voltage
offsets use the additive rule off_new = off_now + mean(DMM − board), which is
exact for additive mV offsets. Bench samples r1/r2 are raw ADC counts (same
axis as firmware off), confirmed at the sample-build site. Warnings (≥4 pts,
spread ≥20 counts, R² ≥0.98, residual ≤150 mA, zero-current sanity) are sound
guardrails, not maths errors.

### 4) "V" rendered before the number — FIXED
The battery-voltage line in the panel page rendered `<b>number</b> <span>V</span>`
loosely inside an RTL span, so bidi put V ahead of the number. Fix: number and
unit are now inside ONE `<span dir="ltr">`.

Gates: check_ai_rules ALL PASSED · host syntax ALL PASSED (ESP 102, charger 52) ·
audit_consistency PASSED · preview regenerated (stamp fdfb1c3) · simulator
restarted and calibration effect verified live.

## LUT send path + calibration card UX (user report, 2026-10-07)

### "Board did not answer the send stage" - ROOT CAUSE: simulator protocol drift, FIXED
The preview's POST /lut violated the real protocol (plink_http.h
func__Esp_HttpLutPush) in three ways: it refused 0-point channels (a legal
"leave this battery alone" push), invented a CRC the panel could never match
(handshake could never complete), and answered {"_s":200} instead of
{"ok":1,...} - so every legitimate push failed with a misleading error and no
reason. The handler is now an exact mirror: strict CSV scan, same refusals
with the SAME reason codes (len/n/empty/pt/mono/crc), 0 points accepted per
channel, received CRC echoed. Live-tested: single-battery push (n1=3,n2=0)
commits, CRC round-trips, reset accepted; 8 refusal cases all return the
right code.

### Panel: reasons on every failure
POST refusals map the firmware's e-codes to Persian; the txe=1 message now
names the exact stage (LSTG[tx]) and lists the common causes (link
wire/noise, board reset mid-send, board busy).

### Buttons gated on real readiness (user order)
New lupd() (runs every poll + on data change): "ارسال جدول به برد" is
disabled until a sendable table exists AND the link is up AND no bench
capture is running; "ریست برد" is disabled until one confirmed commit
happened this session (LSNT). The reason is carried in the title attribute.
Buttons regrouped under two short labels (direct flash send / calibration.h
build output); the long explanation paragraph removed; generated-code
textarea now uses the dark skin (.calcd shares .bxw textarea).

### Single-battery answer
Yes - one battery alone has always been legal on the panel and the firmware
(0 points = channel untouched); only the simulator blocked it. Fixed above.

Panel budget: additions broke the 385024 ceiling twice (386852, 385739);
trimmed ~1850 bytes of comments/strings, final stamp c2be30d, AUDIT PASSED.
Gates: rules ALL PASSED, host syntax ALL PASSED, simulator restarted.

## REAL-BOARD root cause: "board did not answer the send stage" (user report, 2026-10-07)

The user reproduced the LUT-push failure ON THE BOARD, proving the simulator
protocol drift (fixed earlier today) was only half the story.

### Root cause (STM32, esp_link.c)
func__EspLink_SendLutAck filled the ACK's n1/n2 echo fields with
func__CalLut_Points() - the ACTIVE flash table - instead of the counts of
the transaction being staged. On a fresh board the active table is empty, so
every ACK said n1=0, n2=0. The ESP's freshness check (plink_link.h,
func__Esp_LutTxPump) deliberately matches the echoed counts against the push
it just sent (so a duplicate retry-ACK can never be mistaken for the next
step's answer); with 0 != sent-count it rejected the board's perfectly good
ACKs, retried 3 times per stage and finally reported txe=1 - "board did not
answer" - although the board had answered every frame. A push could only
succeed when the new table's point counts happened to equal the old table's.

### Fix
esp_link.c now stores the counts declared by LUT_BEGIN (LutStageN1/N2) and
echoes THOSE in every LUT_ACK of the transaction (zeroed on a failed BEGIN,
in Init and in the host reset). esp_link.h documents the semantics. The
panel/ESP side is unchanged - its strict match was the correct behaviour.

### Why no test caught it
In host mode ParseByte deliberately bypasses the dispatch graph (acceptance
counter only), so the whole LUT handler chain was gc-sectioned out of the
parser test. Fix: new host probe func__EspLink_HostTest_HandleLutFrame +
UART capture double; the CalLut host test now links esp_link.c and asserts
BEGIN/CHUNK ACKs echo the STAGED counts, using active+1 as the staged count
so a regression can never pass by coincidence. Mutation-verified: reverting
the echo to CalLut_Points makes the test fail (2 checks).

CalLut host test: 138 checks, 0 failures (was 124). Gates: host syntax ALL
PASSED, rules ALL PASSED, audit PASSED. The board must be re-flashed with
this build for the fix to take effect.
