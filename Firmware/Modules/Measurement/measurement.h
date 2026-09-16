/**
 * @file    measurement.h
 * @brief   [EN] Converts ADC counts to engineering units (mV / mA) and keeps
 *              the latest snapshot for the other tasks. Pure software: the
 *              ADC+DMA hardware fills the raw buffer autonomously (bsp_adc),
 *              this module only converts.
 *          [FA] شمارش ADC را به واحد مهندسی (mV / mA) تبدیل می‌کند و آخرین
 *              نمونه را برای تسک‌های دیگر نگه می‌دارد. کاملاً نرم‌افزاری:
 *              سخت‌افزار ADC+DMA بافر خام را به‌طور مستقل پر می‌کند
 *              (bsp_adc) و این ماژول فقط تبدیل می‌کند.
 */

#ifndef MEASUREMENT_H
#define MEASUREMENT_H

/* ==================== Includes ==================== */
#include "app_types.h"

/* ==================== Defines ==================== */

/* [EN] MEASUREMENT TASK PERIOD. CHANGE HERE to change the sample rate.
 *      10 ms = 100 readings/s - far below the hardware frame rate (~26 kHz),
 *      so the snapshot is always a fresh frame.
 * [FA] دورهٔ تسک اندازه‌گیری. برای تغییر نرخ نمونه‌برداری همین‌جا عوض شود.
 *      ۱۰ms = ۱۰۰ نمونه/ثانیه — بسیار کمتر از نرخ فریم سخت‌افزاری
 *      (~26kHz)، پس snapshot همیشه یک فریم تازه است. */
#define MEASUREMENT_PERIOD_MS      10u

/* [EN] Delay after BspAdc_Start before the first GetRaw. One full DMA frame
 *      (10 conversions) is ready ~0.1 ms after Start at 9 MHz; 1 ms is margin.
 * [FA] تأخیر بعد از BspAdc_Start تا اولین GetRaw. یک فریم کامل DMA (۱۰
 *      تبدیل) در 9MHz حدود 0.1ms بعد از Start آماده است؛ ۱ms حاشیه است. */
#define MEASUREMENT_SETTLE_MS      1u

/* [EN] ADC scale (STM32F103C8T6: 12-bit, Vref = VDDA = 3.3 V).
 * [FA] مقیاس ADC (STM32F103C8T6: ۱۲ بیتی، Vref = VDDA = 3.3V). */
#define MEASUREMENT_VREF_MV        3300u
#define MEASUREMENT_ADC_FULL_SCALE 4095u

/* [EN] Voltage divider chains from the schematic (1% resistors).
 *      24V input: R46 68K + R11 1.2K on top, R12 6.8K to GND -> PA2
 *      24V batt : R47 68K + R13 1.2K on top, R14 6.8K to GND -> PA3
 *      12V batt : R48 33K + R15 1.2K on top, R16 6.8K to GND -> PA5
 * [FA] زنجیره‌های تقسیم ولتاژ از شماتیک (مقاومت ۱٪).
 *      ورودی ۲۴: R46 68K + R11 1.2K بالا، R12 6.8K به GND -> PA2
 *      باتری ۲۴ : R47 68K + R13 1.2K بالا، R14 6.8K به GND -> PA3
 *      باتری ۱۲ : R48 33K + R15 1.2K بالا، R16 6.8K به GND -> PA5 */
#define MEASUREMENT_DIV24_TOP_OHMS    76000u  /* 68K + 1.2K / 68K + 1.2K */
#define MEASUREMENT_DIV24_BOTTOM_OHMS 6800u   /* 6.8K / 6.8K */
#define MEASUREMENT_DIV12_TOP_OHMS    41000u  /* 33K + 1.2K / 33K + 1.2K */
#define MEASUREMENT_DIV12_BOTTOM_OHMS 6800u   /* 6.8K / 6.8K */

/* [EN] Current sense (Charger_12VX2.SchDoc, Current Sense block):
 *      10 mOhm shunt (R64/R68, 0.01 ohm) + LM358 non-inverting amplifier,
 *      gain = 1 + R74/R73 = 1 + 100K/1K = 101. So 1 A -> 10 mV drop on the
 *      shunt -> 1.01 V at the ADC input.
 * [FA] اندازه‌گیری جریان (Charger_12VX2.SchDoc، بلوک Current Sense):
 *      شانت ۱۰mΩ (R64/R68، 0.01Ω) + تقویت‌کنندهٔ غیرمعاکس‌کنندهٔ LM358 با
 *      گین = ۱ + R74/R73 = ۱ + 100K/1K = 101. پس ۱A -> افت 10mV روی شانت
 *      -> 1.01V در ورودی ADC. */
#define MEASUREMENT_SHUNT_MOHMS    10u
#define MEASUREMENT_AMP_GAIN       101u

/* ==================== Globals (shared values) ==================== */
/* [EN] Shared engineering values, written ONLY by the measurement task
 *      (Run). Any module/task can read them: #include "measurement.h" and
 *      use. Check BOOL__G__MeasDataValid before trusting the numbers.
 * [FA] مقادیر مهندسی مشترک، فقط توسط تسک measurement (Run) نوشته
 *      می‌شوند. هر ماژول/تسک می‌تواند بخواند: کافی است measurement.h را
 *      include کنید. قبل از اعتماد به اعداد، BOOL__G__MeasDataValid را
 *      چک کنید.
 * @note [EN] Named Meas* on purpose: the UI test globals in task_ui.c use
 *          InputVoltageMv/BatteryVoltageMv, so Meas* avoids a link
 *          collision. When the UI switches to the real values, the manual
 *          test globals are deleted in that stage.
 *      [FA] عمداً با پیشوند Meas*: متغیرهای تست UI در task_ui.c از
 *          InputVoltageMv/BatteryVoltageMv استفاده می‌کنند و Meas* از
 *          تداخل لینک جلوگیری می‌کند. وقتی UI به مقدار واقعی وصل شد،
 *          متغیرهای تست دستی در همان مرحله حذف می‌شوند. */
extern uint32_t UINT32_T__G__MeasInputVoltageMv;   /* [EN] 24 V main input, mV (PA2) / ولتاژ ورودی ۲۴, mV */
extern uint32_t UINT32_T__G__MeasBattery24Mv;      /* [EN] 24 V battery, mV (PA3) / ولتاژ باتری ۲۴, mV */
extern uint32_t UINT32_T__G__MeasBattery12Mv;      /* [EN] 12 V battery, mV (PA5) / ولتاژ باتری ۱۲, mV */
extern uint32_t UINT32_T__G__MeasCurrent1Ma;       /* [EN] 24 V ch.1 charge current, mA (PA1) / جریان کانال ۱, mA */
extern uint32_t UINT32_T__G__MeasCurrent2Ma;       /* [EN] 12 V ch.2 charge current, mA (PA7) / جریان کانال ۲, mA */
extern bool BOOL__G__MeasInputPresent;           /* [EN] PB4 HIGH = 24 V input present (schematic) / ورودی ۲۴ وصل است */
extern bool BOOL__G__MeasDataValid;              /* [EN] true once the first frame is converted / اولین فریم تبدیل شده */

/* ==================== Measurement Init ==================== */

/**
 * @brief  [EN] Zero the last snapshot (valid = false).
 *         [FA] آخرین نمونه را صفر می‌کند (valid = false).
 */
void func__Measurement_Init(void);

/* ==================== Measurement Run ==================== */

/**
 * @brief  [EN] Pull one raw frame from the bsp and convert every channel into
 *              the shared snapshot (mV / mA + input_present + valid).
 *         [FA] یک فریم خام از bsp می‌گیرد و همهٔ کانال‌ها را در snapshot
 *              مشترک تبدیل می‌کند (mV / mA + input_present + valid).
 */
void func__Measurement_Run(void);

/* ==================== Measurement Get Snapshot ==================== */

/**
 * @brief  [EN] Copy the last snapshot. Returns false if not valid yet.
 *         [FA] آخرین نمونه را کپی می‌کند. اگر هنوز معتبر نباشد false.
 * @param  measurement_snapshot_t__out [EN] Output pointer, must not be NULL /
 *                                          اشاره‌گر خروجی
 * @return bool [EN] true if a valid snapshot was copied / اگر نمونهٔ معتبر
 *                   کپی شد true
 */
bool func__Measurement_GetSnapshot(measurement_snapshot_t *measurement_snapshot_t__out);

/* ==================== Counts To Mv ==================== */

/**
 * @brief  [EN] Convert raw 12-bit ADC counts to millivolts at the ADC pin:
 *              mV = counts * 3300 / 4095.
 *         [FA] شمارش خام ۱۲ بیتی ADC را به میلی‌ولت در پایهٔ ADC تبدیل
 *              می‌کند: mV = counts * 3300 / 4095.
 * @param  uint16_t__counts [EN] Raw ADC count, 0..4095 / شمارش خام ADC
 * @return uint32_t [EN] Voltage in mV, 0..3300 / ولتاژ بر حسب mV
 */
uint32_t func__Measurement_CountsToMv(uint16_t uint16_t__counts);

/* ==================== V24 Counts To Mv ==================== */

/**
 * @brief  [EN] Convert raw counts of the 24 V channels (24V input, 24V battery)
 *              to source volts-mV, undoing the R46/R47 + R11..R14 dividers.
 *              At 24 V the ADC pin sees ~2.15 V (full range ~37 V).
 *         [FA] شمارش خام کانال‌های ۲۴ ولت (ورودی، باتری) را به میلی‌ولت
 *              منبع تبدیل می‌کند (برگردان تقسیم R46/R47 + R11..R14).
 *              در ۲۴V، پایهٔ ADC حدود 2.15V می‌بیند (سقف ~37V).
 * @param  uint16_t__counts [EN] Raw ADC count, 0..4095 / شمارش خام ADC
 * @return uint32_t [EN] Source voltage in mV, 0..~37000 / ولتاژ منبع mV
 */
uint32_t func__Measurement_V24CountsToMv(uint16_t uint16_t__counts);

/* ==================== V12 Counts To Mv ==================== */

/**
 * @brief  [EN] Convert raw counts of the 12 V battery channel to source
 *              volts-mV, undoing the R48 + R15/R16 divider.
 *              At 12 V the ADC pin sees ~2.0 V (full range ~20 V).
 *         [FA] شمارش خام کانال باتری ۱۲ ولت را به میلی‌ولت منبع تبدیل
 *              می‌کند (برگردان تقسیم R48 + R15/R16).
 *              در ۱۲V، پایهٔ ADC حدود 2.0V می‌بیند (سقف ~20V).
 * @param  uint16_t__counts [EN] Raw ADC count, 0..4095 / شمارش خام ADC
 * @return uint32_t [EN] Source voltage in mV, 0..~20000 / ولتاژ منبع mV
 */
uint32_t func__Measurement_V12CountsToMv(uint16_t uint16_t__counts);

/* ==================== Current Counts To Ma ==================== */

/**
 * @brief  [EN] Convert raw counts of a current channel (Current1/Current2) to
 *              milliamps: divide by the LM358 gain (101) to get the shunt
 *              drop, then divide by the 10 mOhm shunt. 1 A -> ~1010 mV out.
 *         [FA] شمارش خام کانال جریان (Current1/Current2) را به میلی‌آمپر
 *              تبدیل می‌کند: ابتدا تقسیم بر گین LM358 (101) برای افت شانت،
 *              سپس تقسیم بر شانت ۱۰mΩ. ۱A -> خروجی ~1010mV.
 * @param  uint16_t__counts [EN] Raw ADC count, 0..4095 / شمارش خام ADC
 * @return uint32_t [EN] Current in mA, 0..~3300 / جریان بر حسب mA
 */
uint32_t func__Measurement_CurrentCountsToMa(uint16_t uint16_t__counts);

#endif /* MEASUREMENT_H */
