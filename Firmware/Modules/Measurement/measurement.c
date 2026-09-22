/**
 * @file    measurement.c
 * @brief   [EN] ADC counts to engineering units (mV / mA), step by step, with
 *              the latest snapshot shared to the other tasks. Runs inside the
 *              measurement task (RTOS): only a few conversions + one GPIO read,
 *              then it yields - no HAL_Delay anywhere. Charge currents are the
 *              PWM mid-ON synchronized samples converted to mA and then passed
 *              through the switchable filter chain - median-3 plus the
 *              moving average of the last 10 samples (user order 2026-09-22,
 *              constants at the top of measurement.h); battery voltages keep
 *              their median-5 spike guard.
 *          [FA] شمارش ADC به واحد مهندسی (mV / mA)، گام‌به‌گام، با آخرین
 *              snapshot مشترک برای تسک‌های دیگر. داخل تسک اندازه‌گیری اجرا
 *              می‌شود (RTOS): فقط چند تبدیل + یک خواندن GPIO و بعد yield —
 *              هیچ‌جا HAL_Delay ندارد. جریان‌های شارژ، نمونه‌های سنکرون وسط
 *              ON پالس PWM هستند که به mA تبدیل و بعد از زنجیرهٔ فیلتر
 *              کلیددار عبور می‌کنند — مدین-۳ به‌علاوهٔ میانگین متحرک ۱۰ نمونهٔ
 *              اخیر (دستور کاربر ۲۰۲۶-۰۹-۲۲، ثابت‌ها بالای measurement.h)؛
 *              ولتاژهای باتری محافظ مدین-۵ خود را نگه می‌دارند.
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

#if (MEASUREMENT_CURRENT_MEDIAN3_ENABLE != 0u)
/* [EN] Median-of-3 history per current channel (0=Current1, 1=Current2):
 *      kills single-sample jumps of the synchronized mid-ON reading with
 *      zero added lag (user order 2026-09-22). Exists only when the filter
 *      switch is on.
 * [FA] تاریخچهٔ مدین-۳ برای هر کانال جریان: پرش‌های تک‌نمونه‌ای خوانش
 *      سنکرون وسط ON را بدون تأخیر اضافه حذف می‌کند (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۲). فقط وقتی کلید فیلتر روشن است وجود دارد. */
static uint32_t UINT32_T__G__CurrentMedianHistoryMa[2][3];
#endif

#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
/* [EN] Moving-average window per current channel (user order 2026-09-22):
 *      the last MEASUREMENT_CURRENT_AVERAGE_WINDOW converted mA samples,
 *      a fill counter for the startup ramp and the next slot index. Exists
 *      only when the filter switch is on.
 * [FA] پنجرهٔ میانگین متحرک برای هر کانال جریان (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۲): آخرین MEASUREMENT_CURRENT_AVERAGE_WINDOW نمونهٔ تبدیل‌شده
 *      به mA، شمارندهٔ پرشدن برای شیب شروع و اندیس خانهٔ بعدی. فقط وقتی
 *      کلید فیلتر روشن است وجود دارد. */
static uint32_t UINT32_T__G__CurrentAverageWindowMa[2][MEASUREMENT_CURRENT_AVERAGE_WINDOW];
static uint8_t UINT8_T__G__CurrentAverageFillCount[2];
static uint8_t UINT8_T__G__CurrentAverageNextIndex[2];
#endif

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

#if (MEASUREMENT_CURRENT_MEDIAN3_ENABLE != 0u)
/* ==================== Measurement Current Median3 ==================== */

/**
 * @brief  [EN] Shift one new converted current sample (mA) of one channel
 *              into its median-3 history and return the middle value: a
 *              single-sample jump of the synchronized mid-ON reading is
 *              discarded with zero added lag (user order 2026-09-22).
 *         [FA] یک نمونهٔ تبدیل‌شدهٔ جریان (mA) از یک کانال را در
 *              تاریخچهٔ مدین-۳ همان کانال جابه‌جا و مقدار میانی را
 *              برمی‌گرداند: پرش تک‌نمونه‌ای خوانش سنکرون وسط ON بدون
 *              تأخیر اضافه دور انداخته می‌شود (دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] Current channel 0 or 1 / کانال جریان ۰ یا ۱
 * @param  uint32_t__sampleMa [EN] New converted sample in mA / نمونهٔ جدید mA
 * @return uint32_t [EN] Median-of-3 filtered current in mA / جریان مدین‌شده mA
 */
static uint32_t func__Measurement_CurrentMedian3(uint8_t uint8_t__channelIndex,
                                                 uint32_t uint32_t__sampleMa)
{
    uint32_t *uint32_t__historyMa;
    uint32_t uint32_t__oldestMa;
    uint32_t uint32_t__middleMa;
    uint32_t uint32_t__newestMa;

    if (uint8_t__channelIndex >= 2u)
    {
        return uint32_t__sampleMa;
    }

    uint32_t__historyMa = UINT32_T__G__CurrentMedianHistoryMa[uint8_t__channelIndex];
    uint32_t__historyMa[0] = uint32_t__historyMa[1];
    uint32_t__historyMa[1] = uint32_t__historyMa[2];
    uint32_t__historyMa[2] = uint32_t__sampleMa;

    uint32_t__oldestMa = uint32_t__historyMa[0];
    uint32_t__middleMa = uint32_t__historyMa[1];
    uint32_t__newestMa = uint32_t__historyMa[2];

    /* [EN] Median of three = the value that is neither the minimum nor the
       maximum of the three history slots.
       [FA] مدین سه‌تایی = مقداری که از سه خانهٔ تاریخچه نه کمینه است و
       نه بیشینه. */
    if (((uint32_t__oldestMa >= uint32_t__middleMa) && (uint32_t__oldestMa <= uint32_t__newestMa)) ||
        ((uint32_t__oldestMa >= uint32_t__newestMa) && (uint32_t__oldestMa <= uint32_t__middleMa)))
    {
        return uint32_t__oldestMa;
    }
    if (((uint32_t__middleMa >= uint32_t__oldestMa) && (uint32_t__middleMa <= uint32_t__newestMa)) ||
        ((uint32_t__middleMa >= uint32_t__newestMa) && (uint32_t__middleMa <= uint32_t__oldestMa)))
    {
        return uint32_t__middleMa;
    }
    return uint32_t__newestMa;
}
#endif

#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
/* ==================== Measurement Current Moving Average ==================== */

/**
 * @brief  [EN] Push one new converted current sample (mA) of one channel
 *              into its moving-average window and return the average of the
 *              last MEASUREMENT_CURRENT_AVERAGE_WINDOW samples (user order
 *              2026-09-22: window of 10). Until the window fills after a
 *              restart, the average runs over the collected samples only,
 *              so the value converges without a zero-drag from empty slots.
 *         [FA] یک نمونهٔ تبدیل‌شدهٔ جریان (mA) از یک کانال را در پنجرهٔ
 *              میانگین متحرک همان کانال می‌نویسد و میانگین آخرین
 *              MEASUREMENT_CURRENT_AVERAGE_WINDOW نمونه را برمی‌گرداند (دستور
 *              کاربر ۲۰۲۶-۰۹-۲۲: پنجرهٔ ۱۰تایی). تا پرشدن پنجره بعد از
 *              ری‌استارت، میانگین فقط روی نمونه‌های جمع‌شده اجرا می‌شود تا
 *              بدون کشیده‌شدن به صفرِ خانه‌های خالی همگرا شود.
 * @param  uint8_t__channelIndex [EN] Current channel 0 or 1 / کانال جریان ۰ یا ۱
 * @param  uint32_t__sampleMa [EN] New converted sample in mA / نمونهٔ جدید mA
 * @return uint32_t [EN] Moving-average current in mA / جریان میانگین‌گرفته mA
 */
static uint32_t func__Measurement_CurrentMovingAverage(uint8_t uint8_t__channelIndex,
                                                       uint32_t uint32_t__sampleMa)
{
    uint32_t uint32_t__windowSumMa;
    uint32_t uint32_t__windowSlot;
    uint32_t uint32_t__averageMa;

    if (uint8_t__channelIndex >= 2u)
    {
        return uint32_t__sampleMa;
    }

    /* [EN] Replace the oldest slot, then advance the ring index by hand (no
       modulo, keeps the index inside the window under MISRA rules).
       [FA] قدیمی‌ترین خانه جایگزین می‌شود، بعد اندیس حلقه دستی جلو می‌رود
       (بدون باقیماندهٔ تقسیم تا اندیس طبق قواعد MISRA داخل پنجره بماند). */
    UINT32_T__G__CurrentAverageWindowMa[uint8_t__channelIndex]
        [UINT8_T__G__CurrentAverageNextIndex[uint8_t__channelIndex]] = uint32_t__sampleMa;

    UINT8_T__G__CurrentAverageNextIndex[uint8_t__channelIndex]++;
    if (UINT8_T__G__CurrentAverageNextIndex[uint8_t__channelIndex] >=
        MEASUREMENT_CURRENT_AVERAGE_WINDOW)
    {
        UINT8_T__G__CurrentAverageNextIndex[uint8_t__channelIndex] = 0u;
    }

    if (UINT8_T__G__CurrentAverageFillCount[uint8_t__channelIndex] <
        MEASUREMENT_CURRENT_AVERAGE_WINDOW)
    {
        UINT8_T__G__CurrentAverageFillCount[uint8_t__channelIndex]++;
    }

    /* [EN] Sum only the filled slots and divide once by the fill count.
       [FA] جمع فقط روی خانه‌های پر و یک تقسیم بر تعداد خانه‌های پر. */
    uint32_t__windowSumMa = 0u;
    for (uint32_t__windowSlot = 0u;
         uint32_t__windowSlot < (uint32_t)UINT8_T__G__CurrentAverageFillCount[uint8_t__channelIndex];
         uint32_t__windowSlot++)
    {
        uint32_t__windowSumMa +=
            UINT32_T__G__CurrentAverageWindowMa[uint8_t__channelIndex][uint32_t__windowSlot];
    }

    uint32_t__averageMa =
        uint32_t__windowSumMa / (uint32_t)UINT8_T__G__CurrentAverageFillCount[uint8_t__channelIndex];

    return uint32_t__averageMa;
}
#endif

/* ==================== Measurement ApplyCurrentFilters / اعمال فیلترهای جریان ==================== */

/**
 * @brief  [EN] Run the enabled current-filter chain of one channel, in the
 *              fixed order median-3 first (kill single jumps) then the
 *              moving average (smooth), exactly as the two switches
 *              MEASUREMENT_CURRENT_MEDIAN3_ENABLE and
 *              MEASUREMENT_CURRENT_AVERAGE_ENABLE select at compile time.
 *              With both switches off the sample passes through unchanged.
 *         [FA] زنجیرهٔ فیلتر جریان فعالِ یک کانال را با ترتیب ثابت اجرا
 *              می‌کند: اول مدین-۳ (حذف پرش تکی) بعد میانگین متحرک (صاف‌کردن)
 *              — دقیقاً طبق انتخاب دو کلید MEASUREMENT_CURRENT_MEDIAN3_ENABLE
 *              و MEASUREMENT_CURRENT_AVERAGE_ENABLE در زمان کامپایل. با
 *              خاموش‌بودن هر دو کلید نمونه بدون تغییر عبور می‌کند.
 * @param  uint8_t__channelIndex [EN] Current channel 0 or 1 / کانال جریان ۰ یا ۱
 * @param  uint32_t__sampleMa [EN] Converted sample in mA / نمونهٔ تبدیل‌شده mA
 * @return uint32_t [EN] Filtered current in mA / جریان فیلترشده mA
 */
static uint32_t func__Measurement_ApplyCurrentFilters(uint8_t uint8_t__channelIndex,
                                                      uint32_t uint32_t__sampleMa)
{
    (void)uint8_t__channelIndex;

#if (MEASUREMENT_CURRENT_MEDIAN3_ENABLE != 0u)
    uint32_t__sampleMa =
        func__Measurement_CurrentMedian3(uint8_t__channelIndex, uint32_t__sampleMa);
#endif

#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
    uint32_t__sampleMa =
        func__Measurement_CurrentMovingAverage(uint8_t__channelIndex, uint32_t__sampleMa);
#endif

    return uint32_t__sampleMa;
}

/**
 * @brief  [EN] Shift one new battery-channel voltage (mV) into the median-5
 *         history and return the filtered value. Filters the DERIVED low/high
 *         voltages that feed the charger state machine and the central
 *         battery-lost detector, so a short spike burst cannot fake
 *         "battery gone".
 *         [FA] نمونهٔ جدید ولتاژ نیم‌باتری (mV) را در تاریخچهٔ مدین-۵
 *         جابه‌جا و مقدار فیلترشده را برمی‌گرداند؛ روی مقادیر مشتق‌شدهٔ
 *         low/high که خوراک شارژر و آشکارساز مرکزی قطع باتری هستند اعمال
 *         می‌شود تا ترکیدگی کوتاه نتواند «باتری رفت» را جعل کند.
 * @param  uint8_t__channelIndex [EN] Battery channel 0 or 1 / کانال باتری
 * @param  uint32_t__sampleMv    [EN] New derived voltage sample in mV / ولتاژ جدید
 * @return uint32_t [EN] Median-of-5 filtered voltage in mV / ولتاژ مدین‌شده
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
#if ((MEASUREMENT_CURRENT_MEDIAN3_ENABLE != 0u) || (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u))
    uint32_t uint32_t__i;
#endif
#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
    uint32_t uint32_t__j;
#endif

    UINT8_T__G__MeasurementWarmupFrameCount = 0u;

#if (MEASUREMENT_CURRENT_MEDIAN3_ENABLE != 0u)
    /* [EN] Clear both median-3 current histories so a restart begins from a
       clean filter state.
       [FA] تاریخچهٔ مدین-۳ هر دو کانال صفر می‌شود تا ری‌استارت از وضعیت
       فیلتر تمیز شروع شود. */
    for (uint32_t__i = 0u; uint32_t__i < 2u; uint32_t__i++)
    {
        UINT32_T__G__CurrentMedianHistoryMa[uint32_t__i][0] = 0u;
        UINT32_T__G__CurrentMedianHistoryMa[uint32_t__i][1] = 0u;
        UINT32_T__G__CurrentMedianHistoryMa[uint32_t__i][2] = 0u;
    }
#endif

#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
    /* [EN] Clear both moving-average windows; the fill counter restarts the
       startup ramp from the first sample.
       [FA] پنجرهٔ میانگین هر دو کانال صفر می‌شود؛ شمارندهٔ پرشدن شیب شروع
       را از اولین نمونه از نو می‌راند. */
    for (uint32_t__i = 0u; uint32_t__i < 2u; uint32_t__i++)
    {
        for (uint32_t__j = 0u; uint32_t__j < MEASUREMENT_CURRENT_AVERAGE_WINDOW; uint32_t__j++)
        {
            UINT32_T__G__CurrentAverageWindowMa[uint32_t__i][uint32_t__j] = 0u;
        }
        UINT8_T__G__CurrentAverageFillCount[uint32_t__i] = 0u;
        UINT8_T__G__CurrentAverageNextIndex[uint32_t__i] = 0u;
    }
#endif

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
    uint32_t uint32_t__current1SampleMa;
    uint32_t uint32_t__current1Ma;
    uint32_t uint32_t__inputVoltageMv;
    uint32_t uint32_t__battery24Mv;
    uint32_t uint32_t__battery12Mv;
    uint32_t uint32_t__batteryLowMv;
    uint32_t uint32_t__batteryHighMv;
    uint32_t uint32_t__current2SampleMa;
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
    /* [EN] Currents: the board port delivers the PWM mid-ON synchronized raw
       counts in the CURRENT1/CURRENT2 frame positions; this module converts
       them to mA and then runs the switchable filter chain - median-3 to
       kill single-sample jumps, moving average of the last 10 samples to
       smooth (user order 2026-09-22, constants at the top of
       measurement.h). Voltages keep their median-5 spike guard.
       [FA] جریان‌ها: پورت برد شمارش‌های خام سنکرونِ وسط ON پالس PWM را در
       جایگاه‌های CURRENT1/2 فریم می‌گذارد؛ این ماژول آن‌ها را به mA تبدیل و
       بعد زنجیرهٔ فیلتر کلیددار را اجرا می‌کند — مدین-۳ برای حذف پرش
       تک‌نمونه‌ای و میانگین متحرک ۱۰ نمونهٔ اخیر برای صاف‌کردن (دستور کاربر
       ۲۰۲۶-۰۹-۲۲، ثابت‌ها بالای measurement.h). ولتاژها محافظ مدین-۵ خود را
       نگه می‌دارند. */
    uint32_t__current1SampleMa =
        func__Measurement_Current1CountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1]);
    uint32_t__current1Ma =
        func__Measurement_ApplyCurrentFilters(0u, uint32_t__current1SampleMa);
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

    /* [EN] Median-5 on both derived battery channel voltages, right where the
       charger and the battery-lost detector consume them: a single-frame ADC
       spike on the switching node can no longer fake a >14.8 V jump; real
       steps pass with only a few frames of lag, no moving average.
       [FA] مدین-۵ روی ولتاژ مشتق‌شدهٔ هر دو نیم‌باتری دقیقاً همان‌جایی که
       شارژر و آشکارساز قطع باتری مصرفش می‌کنند؛ اسپایک تک‌فریمی دیگر پرش
       بالای ۱۴٫۸V را جعل نمی‌کند و پله واقعی با چند فریم تأخیر عبور می‌کند. */
    uint32_t__batteryLowMv =
        func__Measurement_MedianFilterVoltageSample(0u, uint32_t__batteryLowMv);
    uint32_t__batteryHighMv =
        func__Measurement_MedianFilterVoltageSample(1u, uint32_t__batteryHighMv);
    uint32_t__current2SampleMa =
        func__Measurement_Current2CountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2]);
    uint32_t__current2Ma =
        func__Measurement_ApplyCurrentFilters(1u, uint32_t__current2SampleMa);

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
