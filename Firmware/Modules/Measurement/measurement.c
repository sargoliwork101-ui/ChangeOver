/**
 * @file    measurement.c
 * @brief   [EN] ADC counts to engineering units (mV / mA), step by step, with
 *              the latest snapshot shared to the other tasks. Runs inside the
 *              measurement task (RTOS): only a few conversions plus one
 *              hysteresis update, then it yields - no HAL_Delay anywhere.
 *          [FA] شمارش ADC به واحد مهندسی (mV / mA)، گام‌به‌گام، با آخرین
 *              snapshot مشترک برای تسک‌های دیگر. داخل تسک اندازه‌گیری اجرا
 *              می‌شود (RTOS): فقط چند تبدیل و یک به‌روزرسانی هیسترزیس و بعد
 *              yield — هیچ‌جا HAL_Delay ندارد.
 *
 * @note    [EN] Divider/gain values come from the schematic and are private
 *              board calibration constants in bsp_measurement.c. The
 *              converted values are exposed as globals (UINT32_T__G__Meas*,
 *              BOOL__G__Meas*), written only by this task, readable from
 *              any module - that is how the other tasks (and the debugger
 *              via Live Expressions) use them.
 *          [FA] مقادیر تقسیم/گین از شماتیک می‌آید و ثابت خصوصی
 *              bsp_measurement.c است (MISRA: عدد جادویی وسط منطق ممنوع).
 *              مقادیر تبدیل‌شده به‌صورت
 *              گلوبال (UINT32_T__G__Meas*, BOOL__G__Meas*) در دسترس‌اند —
 *              فقط این تسک می‌نویسد و هر ماژولی می‌تواند بخواند (از جمله
 *              دیباگر با Live Expressions).
 */

/* ==================== Includes ==================== */
#include "measurement.h"
#include "bsp_adc.h"
#include "bsp_measurement.h"
#include "cmsis_os2.h"
#include <stddef.h>

/* ==================== Static State ==================== */

/* [EN] Shared snapshot for the other tasks (UI / protection / comm).
 *      Written only by the measurement task, read by GetSnapshot.
 *      [FA] snapshot مشترک برای تسک‌های دیگر (UI / protection / comm).
 *      فقط توسط تسک measurement نوشته و با GetSnapshot خوانده می‌شود. */
static volatile measurement_snapshot_t MEASUREMENT_SNAPSHOT_T__G__Snap;

/* [EN] Number of completed stable normalized ADC frames collected during
 *      startup warm-up. Unit: completed ADC frame.
 * [FA] تعداد فریم‌های کامل و پایدار ADC استانداردشده در warm-up شروع.
 *      واحد: فریم کامل ADC. */
static uint8_t UINT8_T__G__MeasurementWarmupFrameCount;
static uint32_t UINT32_T__G__Current1FilteredMa;
static uint32_t UINT32_T__G__Current2FilteredMa;

/* [EN] Latched input-presence state built from the measured input voltage with
 *      hysteresis (MEASUREMENT_INPUT_PRESENT_ON_MV / _OFF_MV). Written only by
 *      the measurement task inside Run.
 * [FA] وضعیت قفل‌شدهٔ حضور ورودی که از ولتاژ اندازه‌گیری‌شدهٔ ورودی با هیسترزیس
 *      (MEASUREMENT_INPUT_PRESENT_ON_MV / _OFF_MV) ساخته می‌شود. فقط توسط تسک
 *      measurement داخل Run نوشته می‌شود. */
static bool BOOL__G__InputPresentDerived;

/* ==================== Measurement_FilterCurrent / فیلتر جریان ==================== */

/**
 * @brief  [EN] Apply a small integer low-pass filter to one LM358 current
 *              channel. This is software filtering of ADC/DMA frames, not a
 *              substitute for offset/gain calibration.
 *         [FA] روی یک کانال جریان LM358 فیلتر پایین‌گذر صحیح اعمال می‌کند.
 *              این فیلتر نرم‌افزاری فریم‌های ADC/DMA است و جای کالیبراسیون
 *              offset/gain را نمی‌گیرد.
 * @param  uint32_t__previousMa [EN] Previous filtered current / جریان فیلترشده قبلی
 * @param  uint32_t__sampleMa [EN] New calibrated sample / نمونه کالیبره جدید
 * @return uint32_t [EN] Filtered current / جریان فیلترشده
 */
static uint32_t func__Measurement_FilterCurrent(uint32_t uint32_t__previousMa,
                                                uint32_t uint32_t__sampleMa)
{
    uint32_t uint32_t__differenceMa;

    if (uint32_t__sampleMa >= uint32_t__previousMa)
    {
        uint32_t__differenceMa = uint32_t__sampleMa - uint32_t__previousMa;
        return uint32_t__previousMa + ((uint32_t__differenceMa + 3u) / 4u);
    }

    uint32_t__differenceMa = uint32_t__previousMa - uint32_t__sampleMa;
    return uint32_t__previousMa - ((uint32_t__differenceMa + 3u) / 4u);
}

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
volatile uint32_t UINT32_T__G__MeasBatteryLowMv = 0u;
volatile uint32_t UINT32_T__G__MeasBatteryHighMv = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent1Ma = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent2Ma = 0u;
volatile bool BOOL__G__MeasInputPresent = false;
volatile bool BOOL__G__MeasDataValid = false;

/* ==================== Measurement Update Input Presence ==================== */

/**
 * @brief  [EN] Latch input presence from the converted input voltage with a
 *              21 V / 20 V hysteresis band and return the latched state. The
 *              board PB4 level is intentionally not used: behind the 68K/6.8K
 *              divider it sits in the STM32 undefined input band for roughly
 *              9 V..23 V of input, so it chatters exactly where the changeover
 *              and UI decisions are made.
 *         [FA] حضور ورودی را از ولتاژ تبدیل‌شدهٔ ورودی با هیسترزیس 21V/20V قفل
 *              می‌کند و وضعیت قفل‌شده را برمی‌گرداند. عمداً از سطح PB4 استفاده
 *              نمی‌شود: پشت تقسیم 68K/6.8K، برای ورودی حدود 9V تا 23V داخل
 *              بازهٔ تعریف‌نشدهٔ ورودی STM32 می‌افتد و دقیقاً جایی پرپر می‌زند
 *              که تصمیم‌های changeover و UI گرفته می‌شود.
 * @param  uint32_t__inputVoltageMv [EN] Converted input voltage in mV,
 *                                    0..40000 / ولتاژ تبدیل‌شدهٔ ورودی بر حسب mV
 * @return bool [EN] Latched input presence / حضور قفل‌شدهٔ ورودی
 */
static bool func__Measurement_UpdateInputPresence(uint32_t uint32_t__inputVoltageMv)
{
    if ((BOOL__G__InputPresentDerived == false) &&
        (uint32_t__inputVoltageMv >= MEASUREMENT_INPUT_PRESENT_ON_MV))
    {
        BOOL__G__InputPresentDerived = true;
    }
    else if ((BOOL__G__InputPresentDerived == true) &&
             (uint32_t__inputVoltageMv <= MEASUREMENT_INPUT_PRESENT_OFF_MV))
    {
        BOOL__G__InputPresentDerived = false;
    }
    else
    {
        /* [EN] Inside the hysteresis band the previous state is kept.
           [FA] داخل بازهٔ هیسترزیس وضعیت قبلی حفظ می‌شود. */
    }

    return BOOL__G__InputPresentDerived;
}

/* ==================== Measurement Init ==================== */

/**
 * @brief  [EN] Zero the last snapshot (valid = false).
 *         [FA] آخرین نمونه را صفر می‌کند (valid = false).
 */
void func__Measurement_Init(void)
{
    /* [EN] Zero the warm-up counter, shared globals and snapshot; nothing is
       valid until the required number of stable frames is collected.
       [FA] شمارندهٔ warm-up، گلوبال‌های مشترک و snapshot را صفر می‌کند؛
       تا جمع‌شدن تعداد لازم فریم‌های پایدار چیزی معتبر نیست. */
    UINT8_T__G__MeasurementWarmupFrameCount = 0u;
    UINT32_T__G__Current1FilteredMa = 0u;
    UINT32_T__G__Current2FilteredMa = 0u;
    BOOL__G__InputPresentDerived = false;

    UINT32_T__G__MeasInputVoltageMv = 0u;
    UINT32_T__G__MeasBattery24Mv = 0u;
    UINT32_T__G__MeasBattery12Mv = 0u;
    UINT32_T__G__MeasBatteryLowMv = 0u;
    UINT32_T__G__MeasBatteryHighMv = 0u;
    UINT32_T__G__MeasCurrent1Ma = 0u;
    UINT32_T__G__MeasCurrent2Ma = 0u;
    BOOL__G__MeasInputPresent = false;
    BOOL__G__MeasDataValid = false;

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch1_ma = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch2_ma = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
}

/* ==================== Counts To Mv ==================== */

/**
 * @brief  [EN] Convert normalized ADC counts through the board calibration port.
 *         [FA] شمارش استاندارد ADC را از طریق پورت کالیبراسیون برد تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Voltage in mV / ولتاژ بر حسب mV
 */
uint32_t func__Measurement_CountsToMv(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_CountsToMv(uint16_t__counts);
}

/* ==================== V24 Counts To Mv ==================== */

/**
 * @brief  [EN] Convert a normalized 24 V channel through board calibration.
 *         [FA] کانال استاندارد ۲۴ ولت را از طریق کالیبراسیون برد تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Source voltage in mV / ولتاژ منبع mV
 */
uint32_t func__Measurement_V24CountsToMv(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_V24CountsToMv(uint16_t__counts);
}

/* ==================== V12 Counts To Mv ==================== */

/**
 * @brief  [EN] Convert the normalized 12 V channel through board calibration.
 *         [FA] کانال استاندارد ۱۲ ولت را از طریق کالیبراسیون برد تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Source voltage in mV / ولتاژ منبع mV
 */
uint32_t func__Measurement_V12CountsToMv(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_V12CountsToMv(uint16_t__counts);
}

/* ==================== Current Counts To Ma ==================== */

/**
 * @brief  [EN] Convert a normalized current channel through board calibration.
 *         [FA] کانال استاندارد جریان را از طریق کالیبراسیون برد تبدیل می‌کند.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Current in mA / جریان بر حسب mA
 */
uint32_t func__Measurement_CurrentCountsToMa(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_CurrentCountsToMa(uint16_t__counts);
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
    uint32_t uint32_t__batteryLowMv;
    uint32_t uint32_t__batteryHighMv;
    uint32_t uint32_t__current2Ma;
    bool bool__frameCopied;
    bool bool__inputPresent;
    int32_t int32_t__savedKernelLock;

    /* [EN] GetRaw copies only a completed DMA half-frame; no ADC register
       polling is performed here.
       [FA] GetRaw فقط یک نیم‌فریم کامل DMA را کپی می‌کند؛ اینجا رجیستر ADC
       پالت نمی‌شود. */
    bool__frameCopied = func__BspAdc_GetRaw(uint16_t__raw);

    if (bool__frameCopied == false)
    {
        /* [EN] A missing stable frame restarts warm-up and invalidates the
           ADC result; input presence is not evaluated in this path.
           [FA] نبود فریم پایدار warm-up را از نو شروع و نتیجهٔ ADC را
           نامعتبر می‌کند؛ در این مسیر حضور ورودی ارزیابی نمی‌شود. */
        UINT8_T__G__MeasurementWarmupFrameCount = 0u;

        int32_t__savedKernelLock = osKernelLock();
        if (int32_t__savedKernelLock >= 0)
        {
            BOOL__G__MeasDataValid = false;
            MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
            (void)osKernelRestoreLock(int32_t__savedKernelLock);
        }
        else
        {
            BOOL__G__MeasDataValid = false;
            MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
        }
        return;
    }

    /* [EN] Count only completed stable frames; saturation keeps the small
       warm-up counter from wrapping after validity is reached.
       [FA] فقط فریم‌های کامل و پایدار شمرده می‌شوند؛ اشباع شمارندهٔ کوچک
       از سرریز پس از معتبرشدن داده جلوگیری می‌کند. */
    if (UINT8_T__G__MeasurementWarmupFrameCount < MEASUREMENT_WARMUP_FRAME_COUNT)
    {
        UINT8_T__G__MeasurementWarmupFrameCount++;
    }

    /* [EN] Convert into locals first so other tasks never observe a partly
       updated measurement set.
       [FA] ابتدا در متغیرهای محلی تبدیل می‌کند تا تسک‌های دیگر مجموعهٔ
       اندازه‌گیری نیمه‌به‌روزشده نبینند. */
    UINT32_T__G__Current1FilteredMa =
        func__Measurement_FilterCurrent(
            UINT32_T__G__Current1FilteredMa,
            func__Measurement_CurrentCountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1]));
    uint32_t__current1Ma = UINT32_T__G__Current1FilteredMa;
    uint32_t__inputVoltageMv =
        func__Measurement_V24CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_24V_IN]);
    uint32_t__battery24Mv =
        func__Measurement_V24CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_24V_BAT]);
    uint32_t__battery12Mv =
        func__Measurement_V12CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_12V_BAT]);
    uint32_t__batteryLowMv = uint32_t__battery12Mv;
    if (uint32_t__battery24Mv >= uint32_t__battery12Mv)
    {
        uint32_t__batteryHighMv = uint32_t__battery24Mv - uint32_t__battery12Mv;
    }
    else
    {
        uint32_t__batteryHighMv = 0u;
    }
    UINT32_T__G__Current2FilteredMa =
        func__Measurement_FilterCurrent(
            UINT32_T__G__Current2FilteredMa,
            func__Measurement_CurrentCountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2]));
    uint32_t__current2Ma = UINT32_T__G__Current2FilteredMa;

    /* [EN] Input presence is latched from the converted input voltage with the
       documented hysteresis band, not from the PB4 level: on this board PB4
       sits behind the 68K/6.8K divider and enters the STM32 undefined input
       band while the input is between about 9 V and 23 V, which is exactly the
       sag region the changeover and UI decisions care about. PB4 remains
       available as an EXTI edge event through the BSP.
       [FA] حضور ورودی از ولتاژ تبدیل‌شدهٔ ورودی با بازهٔ هیسترزیس مستند قفل
       می‌شود، نه از سطح PB4: روی این برد PB4 پشت تقسیم 68K/6.8K است و وقتی
       ورودی بین حدود 9V تا 23V است داخل بازهٔ تعریف‌نشدهٔ ورودی STM32
       می‌افتد؛ دقیقاً همان ناحیهٔ افتی که تصمیم‌های changeover و UI به آن
       وابسته‌اند. PB4 به‌عنوان رویداد لبهٔ EXTI از طریق BSP در دسترس می‌ماند. */
    bool__inputPresent =
        func__Measurement_UpdateInputPresence(uint32_t__inputVoltageMv);

    /* [EN] Publish globals and snapshot while the RTOS scheduler is locked.
       The snapshot valid bit is written last; this uses CMSIS-RTOS2 rather
       than an MCU-specific interrupt instruction.
       [FA] گلوبال‌ها و snapshot را هنگام قفل بودن scheduler منتشر می‌کند.
       بیت معتبر بودن snapshot در آخر نوشته می‌شود؛ این کار به‌جای دستور
       وابسته به MCU از CMSIS-RTOS2 استفاده می‌کند. */
    int32_t__savedKernelLock = osKernelLock();
    if (int32_t__savedKernelLock < 0)
    {
        BOOL__G__MeasDataValid = false;
        MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
        return;
    }

    UINT32_T__G__MeasCurrent1Ma = uint32_t__current1Ma;
    UINT32_T__G__MeasInputVoltageMv = uint32_t__inputVoltageMv;
    UINT32_T__G__MeasBattery24Mv = uint32_t__battery24Mv;
    UINT32_T__G__MeasBattery12Mv = uint32_t__battery12Mv;
    UINT32_T__G__MeasBatteryLowMv = uint32_t__batteryLowMv;
    UINT32_T__G__MeasBatteryHighMv = uint32_t__batteryHighMv;
    UINT32_T__G__MeasCurrent2Ma = uint32_t__current2Ma;
    BOOL__G__MeasInputPresent = bool__inputPresent;

    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch1_ma = uint32_t__current1Ma;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv = uint32_t__inputVoltageMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = uint32_t__battery24Mv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv = uint32_t__battery12Mv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = uint32_t__batteryLowMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv = uint32_t__batteryHighMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch2_ma = uint32_t__current2Ma;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = bool__inputPresent;

    /* [EN] ADC validity depends only on the warm-up count, never on input
       voltage or input presence. Write the public flag before snapshot.valid.
       [FA] اعتبار ADC فقط به شمارندهٔ warm-up وابسته است، نه ولتاژ یا حضور
       ورودی. پرچم عمومی پیش از snapshot.valid نوشته می‌شود. */
    if (UINT8_T__G__MeasurementWarmupFrameCount >= MEASUREMENT_WARMUP_FRAME_COUNT)
    {
        BOOL__G__MeasDataValid = true;
    }
    else
    {
        BOOL__G__MeasDataValid = false;
    }

    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = BOOL__G__MeasDataValid;

    (void)osKernelRestoreLock(int32_t__savedKernelLock);
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
    int32_t int32_t__savedKernelLock;
    bool bool__snapshotValid;

    if (measurement_snapshot_t__out == NULL)
    {
        return false;
    }

    /* [EN] Prevent a task switch while copying the multi-field snapshot.
       CMSIS-RTOS2 keeps this independent of the MCU core instructions.
       [FA] هنگام کپی snapshot چندفیلدی، تعویض تسک را متوقف می‌کند.
       CMSIS-RTOS2 این بخش را از دستورهای هستهٔ MCU مستقل نگه می‌دارد. */
    int32_t__savedKernelLock = osKernelLock();
    if (int32_t__savedKernelLock < 0)
    {
        return false;
    }

    *measurement_snapshot_t__out = MEASUREMENT_SNAPSHOT_T__G__Snap;
    bool__snapshotValid = MEASUREMENT_SNAPSHOT_T__G__Snap.valid;
    (void)osKernelRestoreLock(int32_t__savedKernelLock);

    return bool__snapshotValid;
}
