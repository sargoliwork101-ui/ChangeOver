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
#include "bsp_gpio.h"
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

/* ==================== Median prefilter / پیش‌فیلتر مدین ==================== */

/* [EN] Median-of-3 history per current channel (0=Current1, 1=Current2):
 *      kills single-frame ADC spikes with ZERO added lag, so the EMA below
 *      does not need to be strengthened further (user bench directive).
 * [FA] تاریخچه مدین-۳ برای هر کانال جریان: spikeهای تکی را بدون تأخیر
 *      اضافه حذف می‌کند تا نیازی به قوی‌ترکردن فیلتر میانگین نباشد. */
static uint32_t UINT32_T__G__CurrentMedianHistoryMa[2][3];

/* [EN] Median-of-5 history per battery channel voltage (0=v_bat_low,
   1=v_bat_high): raised 2026-09-19 from median-3 because a false buzzer
   still fired WHILE the charger genuinely pumped (absorb mode) - that
   means glitch bursts of two consecutive frames also cross 14.8 V, and
   median-3 passes a 2-frame burst straight through. Median-of-5 only
   outputs the middle sample, so any burst shorter than 3 frames is dead
   while real voltage steps pass with ~20 ms of lag at the 10 ms frame
   cadence - nothing the control loop can notice.
   [FA] تاریخچهٔ مدین-۵ ولتاژ دو نیم‌باتری: چون بوق فیک حین ابزوربِ
   واقعی هم زده شد، پرش‌های دو-فریمی پشت‌سر از مدین-۳ رد می‌شدند؛ مدین-۵
   هر ترکیدگی کوتاه‌تر از ۳ فریم را نابود می‌کند و پلهٔ واقعی با حدود دو
   فریم تأخیر عبور می‌کند. */
static uint32_t UINT32_T__G__BatVoltageMedianHistoryMv[2][5];

static uint32_t func__Measurement_Median3(uint32_t uint32_t__aMa,
                                          uint32_t uint32_t__bMa,
                                          uint32_t uint32_t__cMa)
{
    if (((uint32_t__aMa >= uint32_t__bMa) && (uint32_t__aMa <= uint32_t__cMa)) ||
        ((uint32_t__aMa >= uint32_t__cMa) && (uint32_t__aMa <= uint32_t__bMa)))
    {
        return uint32_t__aMa;
    }
    if (((uint32_t__bMa >= uint32_t__aMa) && (uint32_t__bMa <= uint32_t__cMa)) ||
        ((uint32_t__bMa >= uint32_t__cMa) && (uint32_t__bMa <= uint32_t__aMa)))
    {
        return uint32_t__bMa;
    }
    return uint32_t__cMa;
}

/* [EN] Median-of-5 for the battery voltage channel prefilter: plain
   insertion sort of a LOCAL copy keeps the live history untouched; the
   middle element is the spike-immune sample. Five-element sorting network
   avoided on purpose - clarity beats cycle-pinching at a 10 ms cadence.
   [FA] مدین-۵ برای پیش‌فیلتر ولتاژ: مرتب‌سازی درجی روی کپی محلی است تا
   تاریخچهٔ زنده دست‌نخورده بماند و عنصر میانی برگردد. */
static uint32_t func__Measurement_Median5(uint32_t *uint32_t__samples)
{
    uint32_t uint32_t__sorted[5u];
    uint8_t  uint8_t__index;
    uint8_t  uint8_t__pass;

    for (uint8_t__index = 0u; uint8_t__index < 5u; uint8_t__index++)
    {
        uint32_t__sorted[uint8_t__index] = uint32_t__samples[uint8_t__index];
    }
    for (uint8_t__pass = 1u; uint8_t__pass < 5u; uint8_t__pass++)
    {
        uint32_t uint32_t__key = uint32_t__sorted[uint8_t__pass];
        int32_t  int32_t__slot = (int32_t)uint8_t__pass - 1;

        while ((int32_t__slot >= 0) &&
               (uint32_t__sorted[int32_t__slot] > uint32_t__key))
        {
            uint32_t__sorted[int32_t__slot + 1] = uint32_t__sorted[int32_t__slot];
            int32_t__slot--;
        }
        uint32_t__sorted[int32_t__slot + 1] = uint32_t__key;
    }
    return uint32_t__sorted[2u];
}

static uint32_t func__Measurement_MedianFilterSample(uint8_t uint8_t__channelIndex,
                                                     uint32_t uint32_t__sampleMa)
{
    uint32_t *uint32_t__historyMa;

    if (uint8_t__channelIndex >= 2u)
    {
        return uint32_t__sampleMa;
    }

    uint32_t__historyMa = UINT32_T__G__CurrentMedianHistoryMa[uint8_t__channelIndex];
    uint32_t__historyMa[0] = uint32_t__historyMa[1];
    uint32_t__historyMa[1] = uint32_t__historyMa[2];
    uint32_t__historyMa[2] = uint32_t__sampleMa;

    return func__Measurement_Median3(uint32_t__historyMa[0],
                                     uint32_t__historyMa[1],
                                     uint32_t__historyMa[2]);
}

/**
 * @brief  [EN] Shift one new battery-channel voltage (mV) into the median-5
 *         history and return the filtered value. Voltage-burst-safe version
 *         of the current median stage: filters the DERIVED low/high voltages that
 *         feed the charger state machine and the central battery-lost
 *         detector, so a single-frame spike cannot fake "battery gone".
 *         [FA] نمونهٔ جدید ولتاژ نیم‌باتری (mV) را در تاریخچهٔ مدین-۳
 *         جابه‌جا و مقدار فیلترشده را برمی‌گرداند؛ روی مقادیر مشتق‌شدهٔ
 *         low/high که خوراک شارژر و آشکارساز مرکزی قطع باتری هستند اعمال
 *         می‌شود تا اسپایک تک‌فریمی نتواند «باتری رفت» را جعل کند.
 * @param  uint8_t__channelIndex [EN] Battery channel 0 or 1 / کانال باتری
 * @param  uint32_t__sampleMv    [EN] New derived voltage sample in mV / ولتاژ جدید
 * @return uint32_t [EN] Median-of-3 filtered voltage in mV / ولتاژ مدین‌شده
 */
static uint32_t func__Measurement_MedianFilterVoltageSample(uint8_t uint8_t__channelIndex,
                                                            uint32_t uint32_t__sampleMv)
{
    uint32_t *uint32_t__historyMv;

    if (uint8_t__channelIndex >= 2u)
    {
        return uint32_t__sampleMv;
    }

    uint32_t__historyMv = UINT32_T__G__BatVoltageMedianHistoryMv[uint8_t__channelIndex];
    uint32_t__historyMv[0] = uint32_t__historyMv[1];
    uint32_t__historyMv[1] = uint32_t__historyMv[2];
    uint32_t__historyMv[2] = uint32_t__historyMv[3];
    uint32_t__historyMv[3] = uint32_t__historyMv[4];
    uint32_t__historyMv[4] = uint32_t__sampleMv;

    return func__Measurement_Median5(uint32_t__historyMv);
}

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

    /* [EN] First-order low-pass per 10 ms frame: prev +/- (diff+15)/16, so a
       step reaches ~63% in ~160 ms (alpha 1/16). Strengthened twice on user
       bench feedback (1/4 -> 1/8 -> 1/16) - the published current showed
       visible oscillation.
       [FA] فیلتر مرتبه اول روی هر فریم ۱۰ms با آلفای ۱/۱۶ (ثابت زمانی
       ~۱۶۰ms)؛ دو مرحله تقویت شد تا نوسان خوانش جریان از بین برود. */
    if (uint32_t__sampleMa >= uint32_t__previousMa)
    {
        uint32_t__differenceMa = uint32_t__sampleMa - uint32_t__previousMa;
        return uint32_t__previousMa + ((uint32_t__differenceMa + 15u) / 16u);
    }

    uint32_t__differenceMa = uint32_t__previousMa - uint32_t__sampleMa;
    return uint32_t__previousMa - ((uint32_t__differenceMa + 15u) / 16u);
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
/* ==================== Measurement Current1 Counts To Ma ==================== */

/**
 * @brief  [EN] Channel-1 raw counts to mA via the BSP per-channel
 *              calibration (Shunt1 / Trans1 chain).
 *         [FA] تبدیل شمارش کانال ۱ به mA با کالیبراسیون مستقل BSP.
 * @param  uint16_t__counts [EN] ADC count / شمارش ADC
 * @return uint32_t [EN] Current in mA / جریان mA
 */
uint32_t func__Measurement_Current1CountsToMa(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_Current1CountsToMa(uint16_t__counts);
}

/* ==================== Measurement Current2 Counts To Ma ==================== */

/**
 * @brief  [EN] Channel-2 raw counts to mA via the BSP per-channel
 *              calibration (Shunt2 / Trans2 chain).
 *         [FA] تبدیل شمارش کانال ۲ به mA با کالیبراسیون مستقل BSP.
 * @param  uint16_t__counts [EN] ADC count / شمارش ADC
 * @return uint32_t [EN] Current in mA / جریان mA
 */
uint32_t func__Measurement_Current2CountsToMa(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_Current2CountsToMa(uint16_t__counts);
}

/* ==================== Measurement Current Counts To Ma (legacy) ==================== */

uint32_t func__Measurement_CurrentCountsToMa(uint16_t uint16_t__counts)
{
    return func__Measurement_Current2CountsToMa(uint16_t__counts);
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
            func__Measurement_MedianFilterSample(
                0u,
                func__Measurement_Current1CountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1])));
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

    /* [EN] Median-3 on both derived battery channel voltages, right where the
       charger and the battery-lost detector consume them: a single-frame ADC
       spike on the switching node can no longer fake a >14.8 V jump; real
       steps pass with at most one extra frame of history, no moving average.
       [FA] مدین-۳ روی ولتاژ مشتق‌شدهٔ هر دو نیم‌باتری دقیقاً همان‌جایی که
       شارژر و آشکارساز قطع باتری مصرفش می‌کنند؛ اسپایک تک‌فریمی دیگر پرش
       بالای ۱۴٫۸V را جعل نمی‌کند و پله واقعی بدون تأخیر عبور می‌کند. */
    uint32_t__batteryLowMv =
        func__Measurement_MedianFilterVoltageSample(0u, uint32_t__batteryLowMv);
    uint32_t__batteryHighMv =
        func__Measurement_MedianFilterVoltageSample(1u, uint32_t__batteryHighMv);
    UINT32_T__G__Current2FilteredMa =
        func__Measurement_FilterCurrent(
            UINT32_T__G__Current2FilteredMa,
            func__Measurement_MedianFilterSample(
                1u,
                func__Measurement_Current2CountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2])));
    uint32_t__current2Ma = UINT32_T__G__Current2FilteredMa;

    /* [EN] The BSP exposes the board input-detect signal as a logical GPIO;
       polarity and physical pin mapping remain inside the board port.
       [FA] BSP سیگنال تشخیص ورودی برد را به‌صورت GPIO منطقی ارائه می‌کند؛
       قطبیت و نگاشت پایهٔ فیزیکی داخل پورت برد می‌ماند. */
    bool__inputPresent =
        func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT);

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
