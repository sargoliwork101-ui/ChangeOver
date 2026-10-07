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
