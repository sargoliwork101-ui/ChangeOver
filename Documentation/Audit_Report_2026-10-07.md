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
