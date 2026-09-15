/**
 * @file    measurement.c
 * @brief   [EN] ADC counts to engineering units (mV / mA), step by step, with
 *              the latest snapshot shared to the other tasks. Runs inside the
 *              measurement task (RTOS): only a few conversions + one GPIO read,
 *              then it yields - no HAL_Delay anywhere.
 *          [FA] شمارش ADC به واحد مهندسی (mV / mA)، گام‌به‌گام، با آخرین
 *              snapshot مشترک برای تسک‌های دیگر. داخل تسک اندازه‌گیری اجرا
 *              می‌شود (RTOS): فقط چند تبدیل + یک خواندن GPIO و بعد yield —
 *              هیچ‌جا HAL_Delay ندارد.
 *
 * @note    [EN] Divider/gain values come from the schematic and are constants
 *              in measurement.h (MISRA: no magic numbers in logic). The
 *              converted values are exposed as globals (UINT32_T__G__Meas*,
 *              BOOL_T__G__Meas*), written only by this task, readable from
 *              any module - that is how the other tasks (and the debugger
 *              via Live Expressions) use them.
 *          [FA] مقادیر تقسیم/گین از شماتیک می‌آید و ثابت measurement.h است
 *              (MISRA: عدد جادویی وسط منطق ممنوع). مقادیر تبدیل‌شده به‌صورت
 *              گلوبال (UINT32_T__G__Meas*, BOOL_T__G__Meas*) در دسترس‌اند —
 *              فقط این تسک می‌نویسد و هر ماژولی می‌تواند بخواند (از جمله
 *              دیباگر با Live Expressions).
 */

/* ==================== Includes ==================== */
#include "measurement.h"
#include "bsp_adc.h"
#include "board_pins.h"
#include <stddef.h>

/* ==================== Static State ==================== */

/* [EN] Shared snapshot for the other tasks (UI / protection / comm).
 *      Written only by the measurement task, read by GetSnapshot.
 *      [FA] snapshot مشترک برای تسک‌های دیگر (UI / protection / comm).
 *      فقط توسط تسک measurement نوشته و با GetSnapshot خوانده می‌شود. */
static measurement_snapshot_t MEASUREMENT_SNAPSHOT_T__G__Snap;

/* ==================== Global Shared Values ==================== */
/* [EN] The engineering values of the newest frame, shared to all tasks.
 *      Written ONLY by the measurement task (Run/Init); any module reads
 *      them after including measurement.h. Meas* prefix avoids a link
 *      collision with the UI manual test globals (task_ui.c).
 * [FA] مقادیر مهندسی آخرین فریم، مشترک برای همهٔ تسک‌ها. فقط تسک
 *      measurement (Run/Init) می‌نویسد؛ هر ماژول بعد از include کردن
 *      measurement.h می‌خواند. پیشوند Meas* از تداخل لینک با متغیرهای
 *      تست دستی UI (task_ui.c) جلوگیری می‌کند. */
uint32_t UINT32_T__G__MeasInputVoltageMv = 0u;
uint32_t UINT32_T__G__MeasBattery24Mv = 0u;
uint32_t UINT32_T__G__MeasBattery12Mv = 0u;
uint32_t UINT32_T__G__MeasCurrent1Ma = 0u;
uint32_t UINT32_T__G__MeasCurrent2Ma = 0u;
bool BOOL_T__G__MeasInputPresent = false;
bool BOOL_T__G__MeasDataValid = false;

/* ==================== Measurement Init ==================== */

/**
 * @brief  [EN] Zero the last snapshot (valid = false).
 *         [FA] آخرین نمونه را صفر می‌کند (valid = false).
 */
void func__Measurement_Init(void)
{
    /* [EN] Zero the shared globals and the snapshot; nothing is valid yet.
       [FA] گلوبال‌های مشترک و snapshot را صفر می‌کند؛ هنوز چیزی معتبر نیست. */
    UINT32_T__G__MeasInputVoltageMv = 0u;
    UINT32_T__G__MeasBattery24Mv = 0u;
    UINT32_T__G__MeasBattery12Mv = 0u;
    UINT32_T__G__MeasCurrent1Ma = 0u;
    UINT32_T__G__MeasCurrent2Ma = 0u;
    BOOL_T__G__MeasInputPresent = false;
    BOOL_T__G__MeasDataValid = false;

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch1_ma = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch2_ma = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
}

/* ==================== Counts To Mv ==================== */

/**
 * @brief  [EN] Convert raw 12-bit ADC counts to millivolts at the ADC pin.
 *         [FA] شمارش خام ۱۲ بیتی ADC را به میلی‌ولت در پایهٔ ADC تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Raw ADC count, 0..4095 / شمارش خام ADC
 * @return uint32_t [EN] Voltage in mV, 0..3300 / ولتاژ بر حسب mV
 */
uint32_t func__Measurement_CountsToMv(uint16_t uint16_t__counts)
{
    uint32_t uint32_t__countsScaled;
    uint32_t uint32_t__voltageMv;

    /* [EN] Step 1: scale the count to the 3.3 V reference. Multiply before
       the division to keep the integer precision (max 4095 * 3300 fits in
       uint32_t).
       [FA] گام ۱: مقیاس‌بندی شمارش به مرجع 3.3V. اول ضرب و بعد تقسیم تا
       دقت صحیح حفظ شود (حداکثر 4095 * 3300 در uint32_t جا می‌شود). */
    uint32_t__countsScaled = (uint32_t)uint16_t__counts * MEASUREMENT_VREF_MV;
    uint32_t__voltageMv = uint32_t__countsScaled / MEASUREMENT_ADC_FULL_SCALE;

    return uint32_t__voltageMv;
}

/* ==================== V24 Counts To Mv ==================== */

/**
 * @brief  [EN] Raw counts of the 24 V channels to source voltage (mV).
 *         [FA] شمارش خام کانال‌های ۲۴ ولت به ولتاژ منبع (mV).
 * @param  uint16_t__counts [EN] Raw ADC count, 0..4095 / شمارش خام ADC
 * @return uint32_t [EN] Source voltage in mV, 0..~37000 / ولتاژ منبع mV
 */
uint32_t func__Measurement_V24CountsToMv(uint16_t uint16_t__counts)
{
    uint32_t uint32_t__adcPinMv;
    uint32_t uint32_t__dividerTotalOhms;
    uint32_t uint32_t__scaledMv;
    uint32_t uint32_t__sourceMv;

    /* [EN] Step 1: voltage at the ADC pin (mV).
       [FA] گام ۱: ولتاژ روی پایهٔ ADC (mV). */
    uint32_t__adcPinMv = func__Measurement_CountsToMv(uint16_t__counts);

    /* [EN] Step 2: total divider resistance (top + bottom, ohms).
       [FA] گام ۲: مقاومت کل تقسیم‌کننده (بالا + پایین، اهم). */
    uint32_t__dividerTotalOhms =
        MEASUREMENT_DIV24_TOP_OHMS + MEASUREMENT_DIV24_BOTTOM_OHMS;

    /* [EN] Step 3: undo the divider: V_source = V_pin * R_total / R_bottom.
       Multiply before the division (max 3300 * 82800 fits in uint32_t).
       [FA] گام ۳: برگردان تقسیم: V_source = V_pin * R_total / R_bottom.
       اول ضرب و بعد تقسیم (حداکثر 3300 * 82800 در uint32_t جا می‌شود). */
    uint32_t__scaledMv = uint32_t__adcPinMv * uint32_t__dividerTotalOhms;
    uint32_t__sourceMv = uint32_t__scaledMv / MEASUREMENT_DIV24_BOTTOM_OHMS;

    return uint32_t__sourceMv;
}

/* ==================== V12 Counts To Mv ==================== */

/**
 * @brief  [EN] Raw counts of the 12 V battery channel to source voltage (mV).
 *         [FA] شمارش خام کانال باتری ۱۲ ولت به ولتاژ منبع (mV).
 * @param  uint16_t__counts [EN] Raw ADC count, 0..4095 / شمارش خام ADC
 * @return uint32_t [EN] Source voltage in mV, 0..~20000 / ولتاژ منبع mV
 */
uint32_t func__Measurement_V12CountsToMv(uint16_t uint16_t__counts)
{
    uint32_t uint32_t__adcPinMv;
    uint32_t uint32_t__dividerTotalOhms;
    uint32_t uint32_t__scaledMv;
    uint32_t uint32_t__sourceMv;

    /* [EN] Step 1: voltage at the ADC pin (mV).
       [FA] گام ۱: ولتاژ روی پایهٔ ADC (mV). */
    uint32_t__adcPinMv = func__Measurement_CountsToMv(uint16_t__counts);

    /* [EN] Step 2: total divider resistance (top + bottom, ohms).
       [FA] گام ۲: مقاومت کل تقسیم‌کننده (بالا + پایین، اهم). */
    uint32_t__dividerTotalOhms =
        MEASUREMENT_DIV12_TOP_OHMS + MEASUREMENT_DIV12_BOTTOM_OHMS;

    /* [EN] Step 3: undo the divider: V_source = V_pin * R_total / R_bottom.
       [FA] گام ۳: برگردان تقسیم: V_source = V_pin * R_total / R_bottom. */
    uint32_t__scaledMv = uint32_t__adcPinMv * uint32_t__dividerTotalOhms;
    uint32_t__sourceMv = uint32_t__scaledMv / MEASUREMENT_DIV12_BOTTOM_OHMS;

    return uint32_t__sourceMv;
}

/* ==================== Current Counts To Ma ==================== */

/**
 * @brief  [EN] Raw counts of a current channel to charge current (mA).
 *         [FA] شمارش خام کانال جریان به جریان شارژ (mA).
 * @param  uint16_t__counts [EN] Raw ADC count, 0..4095 / شمارش خام ADC
 * @return uint32_t [EN] Current in mA, 0..~3300 / جریان بر حسب mA
 */
uint32_t func__Measurement_CurrentCountsToMa(uint16_t uint16_t__counts)
{
    uint32_t uint32_t__ampOutMv;
    uint32_t uint32_t__shuntDropMv;
    uint32_t uint32_t__shuntDropScaled;
    uint32_t uint32_t__currentMa;

    /* [EN] Step 1: voltage at the ADC pin = amplifier output (mV).
       [FA] گام ۱: ولتاژ روی پایهٔ ADC = خروجی تقویت‌کننده (mV). */
    uint32_t__ampOutMv = func__Measurement_CountsToMv(uint16_t__counts);

    /* [EN] Step 2: divide by the LM358 gain (101) -> drop across the shunt
       (mV).
       [FA] گام ۲: تقسیم بر گین LM358 (101) -> افت ولتاژ روی شانت (mV). */
    uint32_t__shuntDropMv = uint32_t__ampOutMv / MEASUREMENT_AMP_GAIN;

    /* [EN] Step 3: drop (mV) over the shunt (mOhm) -> current (mA).
       mV / mOhm = 1000 * mA, so multiply by 1000 before dividing.
       [FA] گام ۳: افت (mV) تقسیم بر شانت (mΩ) -> جریان (mA).
       چون mV / mΩ = ۱۰۰۰ * mA، اول در ۱۰۰۰ ضرب و بعد تقسیم می‌شود. */
    uint32_t__shuntDropScaled = uint32_t__shuntDropMv * 1000u;
    uint32_t__currentMa = uint32_t__shuntDropScaled / MEASUREMENT_SHUNT_MOHMS;

    return uint32_t__currentMa;
}

/* ==================== Measurement Run ==================== */

/**
 * @brief  [EN] Pull one raw frame from the bsp and convert every channel into
 *              the shared snapshot.
 *         [FA] یک فریم خام از bsp می‌گیرد و همهٔ کانال‌ها را در snapshot
 *              مشترک تبدیل می‌کند.
 */
void func__Measurement_Run(void)
{
    uint16_t uint16_t__raw[BSP_ADC_CHANNEL_COUNT];
    bool bool__frameCopied;

    /* [EN] The raw frame is already in RAM (hardware-filled DMA buffer);
       GetRaw only copies 5 halfwords.
       [FA] فریم خام از قبل در RAM است (بفر پرشدهٔ سخت‌افزاری DMA)؛
       GetRaw فقط ۵ نصف‌واژه کپی می‌کند. */
    bool__frameCopied = func__BspAdc_GetRaw(uint16_t__raw);

    if (bool__frameCopied == false)
    {
        BOOL_T__G__MeasDataValid = false;
        MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
        return;
    }

    /* [EN] Convert every channel into the shared globals (single write pass;
       index order = bsp_adc.h channel table). The globals are the values the
       other tasks read.
       [FA] تبدیل همهٔ کانال‌ها در گلوبال‌های مشترک (یک بار نوشتن؛ ترتیب
       اندیس = جدول کانال bsp_adc.h). همین گلوبال‌هایی هستند که تسک‌های
       دیگر می‌خوانند. */
    UINT32_T__G__MeasCurrent1Ma =
        func__Measurement_CurrentCountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1]);
    UINT32_T__G__MeasInputVoltageMv =
        func__Measurement_V24CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_24V_IN]);
    UINT32_T__G__MeasBattery24Mv =
        func__Measurement_V24CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_24V_BAT]);
    UINT32_T__G__MeasBattery12Mv =
        func__Measurement_V12CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_12V_BAT]);
    UINT32_T__G__MeasCurrent2Ma =
        func__Measurement_CurrentCountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2]);

    /* [EN] PB4 = MCU_INT_24_IN (schematic): driven by the 24 V input through
       R46 + R10 - HIGH (~2.5 V) when the input is present, 0 V otherwise.
       Polarity is SCHEMATIC, not measured on the board yet.
       [FA] PB4 = MCU_INT_24_IN (شماتیک): از ورودی ۲۴ با R46 + R10 می‌آید —
       وقتی ورودی وصل است HIGH (~2.5V)، در غیر این صورت 0V.
       قطبیت از روی شماتیک است، هنوز روی برد اندازه‌گیری نشده. */
    BOOL_T__G__MeasInputPresent =
        (HAL_GPIO_ReadPin(PIN_INT_24_IN_PORT, PIN_INT_24_IN_PIN) == GPIO_PIN_SET);

    /* [EN] Mirror the globals into the snapshot (same data + valid flag);
       GetSnapshot keeps working for modules that prefer a one-copy read.
       [FA] آینه‌سازی گلوبال‌ها در snapshot (همان داده + فلگ valid)؛
       GetSnapshot برای ماژولهایی که ترجیح می‌دهند یک‌بار کپی کنند سالم می‌ماند. */
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch1_ma = UINT32_T__G__MeasCurrent1Ma;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv = UINT32_T__G__MeasInputVoltageMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = UINT32_T__G__MeasBattery24Mv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv = UINT32_T__G__MeasBattery12Mv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch2_ma = UINT32_T__G__MeasCurrent2Ma;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = BOOL_T__G__MeasInputPresent;

    BOOL_T__G__MeasDataValid = true;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = true;
}

/* ==================== Measurement Get Snapshot ==================== */

/**
 * @brief  [EN] Copy the last snapshot. Returns false if pointer is NULL or
 *              data is not valid yet.
 *         [FA] آخرین نمونه را کپی می‌کند. اگر اشاره‌گر NULL یا داده نامعتبر
 *              باشد false برمی‌گرداند.
 * @param  measurement_snapshot_t__out [EN] Output pointer, must not be NULL /
 *                                          اشاره‌گر خروجی
 * @return bool [EN] true if a valid snapshot was copied / اگر نمونهٔ معتبر
 *                   کپی شد true
 */
bool func__Measurement_GetSnapshot(measurement_snapshot_t *measurement_snapshot_t__out)
{
    if (measurement_snapshot_t__out == NULL)
    {
        return false;
    }

    *measurement_snapshot_t__out = MEASUREMENT_SNAPSHOT_T__G__Snap;
    return MEASUREMENT_SNAPSHOT_T__G__Snap.valid;
}
