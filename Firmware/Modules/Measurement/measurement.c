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
 *              BOOL__G__Meas*), written only by this task, readable from
 *              any module - that is how the other tasks (and the debugger
 *              via Live Expressions) use them.
 *          [FA] مقادیر تقسیم/گین از شماتیک می‌آید و ثابت measurement.h است
 *              (MISRA: عدد جادویی وسط منطق ممنوع). مقادیر تبدیل‌شده به‌صورت
 *              گلوبال (UINT32_T__G__Meas*, BOOL__G__Meas*) در دسترس‌اند —
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
static volatile measurement_snapshot_t MEASUREMENT_SNAPSHOT_T__G__Snap;

/* ==================== Global Shared Values ==================== */
/* [EN] The engineering values of the newest frame, shared to all tasks.
 *      Written ONLY by the measurement task (Run/Init); any module reads
 *      them after including measurement.h. Meas* prefix avoids a link
 *      collision with the UI manual test globals (task_ui.c).
 * [FA] مقادیر مهندسی آخرین فریم، مشترک برای همهٔ تسک‌ها. فقط تسک
 *      measurement (Run/Init) می‌نویسد؛ هر ماژول بعد از include کردن
 *      measurement.h می‌خواند. پیشوند Meas* از تداخل لینک با متغیرهای
 *      تست دستی UI (task_ui.c) جلوگیری می‌کند. */
volatile uint32_t UINT32_T__G__MeasInputVoltageMv = 0u;
volatile uint32_t UINT32_T__G__MeasBattery24Mv = 0u;
volatile uint32_t UINT32_T__G__MeasBattery12Mv = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent1Ma = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent2Ma = 0u;
volatile bool BOOL__G__MeasInputPresent = false;
volatile bool BOOL__G__MeasDataValid = false;

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
    BOOL__G__MeasInputPresent = false;
    BOOL__G__MeasDataValid = false;

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
       Multiply before the division (max 3300 * 69200 fits in uint32_t).
       [FA] گام ۳: برگردان تقسیم: V_source = V_pin * R_total / R_bottom.
       اول ضرب و بعد تقسیم (حداکثر 3300 * 69200 در uint32_t جا می‌شود). */
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
 * @brief  [EN] Convert raw ADC counts to charge current in milliamps. The
 *              calculation keeps the ADC, amplifier-gain and shunt factors
 *              in one 64-bit numerator/denominator path to avoid the coarse
 *              100 mA quantization caused by early integer division.
 *         [FA] شمارش خام ADC را به جریان شارژ بر حسب میلی‌آمپر تبدیل می‌کند.
 *              عوامل ADC، گین تقویت‌کننده و شانت در مسیر صورت/مخرج ۶۴ بیتی
 *              نگه داشته می‌شوند تا تقسیم زودهنگام و پلهٔ خشن ۱۰۰mA ایجاد نشود.
 * @param  uint16_t__counts [EN] Raw ADC count, 0..4095 / شمارش خام ADC
 * @return uint32_t [EN] Current in mA, approximately 0..3300 / جریان mA
 */
uint32_t func__Measurement_CurrentCountsToMa(uint16_t uint16_t__counts)
{
    uint64_t uint64_t__adcVoltageNumerator;
    uint64_t uint64_t__currentNumerator;
    uint64_t uint64_t__currentDenominator;
    uint32_t uint32_t__currentMa;

    /* [EN] Step 1: keep the ADC scale in the numerator before division.
       [FA] گام ۱: مقیاس ADC پیش از تقسیم در صورت نگه داشته می‌شود. */
    uint64_t__adcVoltageNumerator =
        (uint64_t)uint16_t__counts * MEASUREMENT_VREF_MV;

    /* [EN] Step 2: convert the shunt-voltage ratio to milliamps.
       [FA] گام ۲: نسبت افت شانت را به میلی‌آمپر تبدیل می‌کند. */
    uint64_t__currentNumerator =
        uint64_t__adcVoltageNumerator * MEASUREMENT_CURRENT_MA_SCALE;

    /* [EN] Step 3: amplifier gain and shunt resistance form the denominator:
       counts * Vref * 1000 / (4095 * gain * shunt_mOhm).
       [FA] گام ۳: گین تقویت‌کننده و مقاومت شانت مخرج را می‌سازند:
       counts * Vref * 1000 / (4095 * gain * shunt_mOhm). */
    uint64_t__currentDenominator =
        (uint64_t)MEASUREMENT_ADC_FULL_SCALE *
        MEASUREMENT_AMP_GAIN *
        MEASUREMENT_SHUNT_MOHMS;

    uint32_t__currentMa =
        (uint32_t)(uint64_t__currentNumerator / uint64_t__currentDenominator);

    return uint32_t__currentMa;
}

/* ==================== Measurement Run ==================== */

/**
 * @brief  [EN] Pull one stable raw frame from the BSP, convert all ADC
 *              channels and publish one coherent shared snapshot.
 *         [FA] یک فریم خام پایدار را از BSP می‌گیرد، همهٔ کانال‌های ADC را
 *              تبدیل می‌کند و یک snapshot مشترک منسجم منتشر می‌کند.
 */
void func__Measurement_Run(void)
{
    uint16_t uint16_t__raw[BSP_ADC_CHANNEL_COUNT];
    uint32_t uint32_t__current1Ma;
    uint32_t uint32_t__inputVoltageMv;
    uint32_t uint32_t__battery24Mv;
    uint32_t uint32_t__battery12Mv;
    uint32_t uint32_t__current2Ma;
    bool bool__frameCopied;
    bool bool__inputPresent;
    uint32_t uint32_t__savedPrimask;

    /* [EN] GetRaw copies only a completed DMA half-frame; no ADC register
       polling is performed here.
       [FA] GetRaw فقط یک نیم‌فریم کامل DMA را کپی می‌کند؛ اینجا رجیستر ADC
       پالت نمی‌شود. */
    bool__frameCopied = func__BspAdc_GetRaw(uint16_t__raw);

    if (bool__frameCopied == false)
    {
        uint32_t__savedPrimask = __get_PRIMASK();
        __disable_irq();
        BOOL__G__MeasDataValid = false;
        MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
        __set_PRIMASK(uint32_t__savedPrimask);
        return;
    }

    /* [EN] Convert into locals first so other tasks never observe a partly
       updated measurement set.
       [FA] ابتدا در متغیرهای محلی تبدیل می‌کند تا تسک‌های دیگر مجموعهٔ
       اندازه‌گیری نیمه‌به‌روزشده نبینند. */
    uint32_t__current1Ma =
        func__Measurement_CurrentCountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1]);
    uint32_t__inputVoltageMv =
        func__Measurement_V24CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_24V_IN]);
    uint32_t__battery24Mv =
        func__Measurement_V24CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_24V_BAT]);
    uint32_t__battery12Mv =
        func__Measurement_V12CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_12V_BAT]);
    uint32_t__current2Ma =
        func__Measurement_CurrentCountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2]);

    /* [EN] PB4 = MCU_INT_24_IN (schematic): HIGH means input present. This
       polarity is from the schematic and still needs board measurement.
       [FA] PB4 = MCU_INT_24_IN (شماتیک): HIGH یعنی ورودی حاضر است. این
       قطبیت از شماتیک است و هنوز باید روی برد اندازه‌گیری شود. */
    bool__inputPresent =
        func__BspGpio_Read(PIN_INT_24_IN_PORT, PIN_INT_24_IN_PIN);

    /* [EN] Publish globals and snapshot as one short critical section. The
       snapshot valid bit is written last.
       [FA] گلوبال‌ها و snapshot را در یک critical section کوتاه منتشر می‌کند.
       بیت معتبر بودن snapshot در آخر نوشته می‌شود. */
    uint32_t__savedPrimask = __get_PRIMASK();
    __disable_irq();

    UINT32_T__G__MeasCurrent1Ma = uint32_t__current1Ma;
    UINT32_T__G__MeasInputVoltageMv = uint32_t__inputVoltageMv;
    UINT32_T__G__MeasBattery24Mv = uint32_t__battery24Mv;
    UINT32_T__G__MeasBattery12Mv = uint32_t__battery12Mv;
    UINT32_T__G__MeasCurrent2Ma = uint32_t__current2Ma;
    BOOL__G__MeasInputPresent = bool__inputPresent;

    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch1_ma = uint32_t__current1Ma;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv = uint32_t__inputVoltageMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = uint32_t__battery24Mv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv = uint32_t__battery12Mv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch2_ma = uint32_t__current2Ma;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = bool__inputPresent;

    BOOL__G__MeasDataValid = true;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = true;

    __set_PRIMASK(uint32_t__savedPrimask);
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
    uint32_t uint32_t__savedPrimask;
    bool bool__snapshotValid;

    if (measurement_snapshot_t__out == NULL)
    {
        return false;
    }

    /* [EN] Prevent a task switch while copying the multi-field snapshot.
       [FA] هنگام کپی snapshot چندفیلدی، تعویض تسک را متوقف می‌کند. */
    uint32_t__savedPrimask = __get_PRIMASK();
    __disable_irq();
    *measurement_snapshot_t__out = MEASUREMENT_SNAPSHOT_T__G__Snap;
    bool__snapshotValid = MEASUREMENT_SNAPSHOT_T__G__Snap.valid;
    __set_PRIMASK(uint32_t__savedPrimask);

    return bool__snapshotValid;
}
