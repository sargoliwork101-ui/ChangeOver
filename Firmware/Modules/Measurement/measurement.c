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
/* [EN] Median history per current channel (0=Current1, 1=Current2), window
 *      = runtime median size 1/3/5 with default 3 (user order 2026-09-22):
 *      kills single-sample jumps of the synchronized mid-ON reading with
 *      zero added lag. Exists only when the compile-time filter switch is
 *      on.
 * [FA] تاریخچهٔ مدین برای هر کانال جریان (۰=جریان ۱، ۱=جریان ۲)، پنجره
 *      = اندازهٔ مدین زمان اجرا ۱/۳/۵ با پیش‌فرض ۳ (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۲): پرش‌های تک‌نمونه‌ای خوانش سنکرون وسط ON را بدون
 *      تأخیر اضافه حذف می‌کند. فقط وقتی کلید فیلتر کامپایل روشن است
 *      وجود دارد. */
static uint32_t UINT32_T__G__CurrentMedianHistoryMa[2][MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX];
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

/* ==================== Runtime filter config / voltage offsets (ESP panel) ==================== */

/* [EN] Runtime copies of the current-filter configuration (user order
 *      2026-09-22: the ESP command panel can resize the median window and
 *      the moving-average window live; ONE size parameter per filter -
 *      size 1 means bypass, so a separate on/off switch is unnecessary).
 *      The compile-time switches above remain the capability gates: a
 *      filter compiled out can never be switched on at runtime, the median
 *      window can never exceed MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX and the
 *      average window never exceeds the compiled ring size. The measurement
 *      task detects a change and resets the filter state in its own
 *      context, so no cross-task locking is needed (written by the EspLink
 *      task, volatile).
 * [FA] نسخهٔ زمان اجرای پیکربندی فیلتر جریان (دستور کاربر ۲۰۲۶-۰۹-۲۲:
 *      پنل ESP می‌تواند پنجرهٔ مدین و پنجرهٔ میانگین متحرک را زنده تغییر
 *      دهد؛ برای هر فیلتر یک پارامتر اندازه - اندازهٔ ۱ یعنی عبور
 *      مستقیم، پس کلید روشن/خاموش جدا لازم نیست). کلیدهای کامپایل بالا
 *      ظرفیت را تعیین می‌کنند: فیلتری که کامپایل نشده هرگز روشن نمی‌شود،
 *      پنجرهٔ مدین هرچه باشد از MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX و
 *      پنجرهٔ میانگین از اندازهٔ حلقهٔ کامپایل بزرگ‌تر نمی‌شود. تسک
 *      اندازه‌گیری تغییر را می‌بیند و وضعیت فیلتر را در زمینهٔ خودش ریست
 *      می‌کند؛ پس قفل بین‌تسکی لازم نیست (نوشته از تسک EspLink، volatile). */
static volatile uint8_t UINT8_T__G__FilterMedianSize = 3u;
static volatile uint8_t UINT8_T__G__FilterAverageWindow =
    (uint8_t)MEASUREMENT_CURRENT_AVERAGE_WINDOW_DEFAULT;

/* [EN] Last configuration the measurement task applied; owned by the
 *      measurement task only (change detection).
 * [FA] آخرین پیکربندی اعمال‌شده توسط تسک اندازه‌گیری؛ فقط مالکش همین
 *      تسک است (تشخیص تغییر). */
static uint8_t UINT8_T__G__FilterMedianSizeApplied = 3u;
static uint8_t UINT8_T__G__FilterAverageWindowApplied =
    (uint8_t)MEASUREMENT_CURRENT_AVERAGE_WINDOW_DEFAULT;

/* [EN] Runtime voltage calibration offsets in mV, default 0 = today's
 *      behavior; applied AFTER the divider conversion, before the
 *      low/high derivation. RAM only - a reboot returns to 0.
 * [FA] آفست‌های کالیبراسیون ولتاژ بر حسب mV در زمان اجرا؛ پیش‌فرض ۰ یعنی
 *      رفتار فعلی؛ بعد از تبدیل مقسم و قبل از محاسبهٔ پایین/بالا اعمال
 *      می‌شوند. فقط RAM - ری‌استارت صفر برمی‌گردد. */
static volatile int32_t INT32_T__G__VoltageInOffsetMv = 0;
static volatile int32_t INT32_T__G__Voltage24OffsetMv = 0;
static volatile int32_t INT32_T__G__Voltage12OffsetMv = 0;
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
 *              into its median history and return the middle value. The
 *              window size is the runtime median size (v1.4, user order
 *              2026-09-25: ANY value 1..15, even sizes included; the ESP
 *              panel can change it live); a
 *              single-sample jump of the synchronized mid-ON reading is
 *              discarded with zero added lag. Insertion sort of a LOCAL
 *              copy keeps the live history untouched, same pattern as the
 *              voltage median-5.
 *         [FA] یک نمونهٔ تبدیل‌شدهٔ جریان (mA) از یک کانال را در
 *              تاریخچهٔ مدین همان کانال جابه‌جا و مقدار میانی را
 *              برمی‌گرداند. اندازهٔ پنجره همان اندازهٔ مدین زمان اجرا
 *              است (v1.4، دستور کاربر ۲۰۲۶-۰۹-۲۵: هر مقدار ۱..۱۵، زوج
 *              هم مجاز؛ پنل ESP زنده عوضش می‌کند)؛ پرش تک‌نمونه‌ای خوانش
 *              سنکرون وسط ON بدون تأخیر
 *              اضافه دور انداخته می‌شود. مرتب‌سازی درجی روی کپی محلی،
 *              همان الگوی مدین-۵ ولتاژ.
 * @param  uint8_t__channelIndex [EN] Current channel 0 or 1 / کانال جریان ۰ یا ۱
 * @param  uint32_t__sampleMa [EN] New converted sample in mA / نمونهٔ جدید mA
 * @param  uint8_t__medianSize [EN] Active median window (1..15) / پنجرهٔ فعال
 * @return uint32_t [EN] Median-filtered current in mA / جریان مدین‌شده mA
 */
static uint32_t func__Measurement_CurrentMedian(uint8_t uint8_t__channelIndex,
                                                uint32_t uint32_t__sampleMa,
                                                uint8_t uint8_t__medianSize)
{
    uint32_t UINT32_T__A__Sorted[MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX];
    uint32_t *uint32_t__historyMa;
    uint8_t uint8_t__index;
    uint8_t uint8_t__pass;

    if ((uint8_t__channelIndex >= 2u) ||
        (uint8_t__medianSize < 1u) ||
        (uint8_t__medianSize > (uint8_t)MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX))
    {
        return uint32_t__sampleMa;
    }

    if (uint8_t__medianSize == 1u)
    {
        /* [EN] Window of one = bypass. [FA] پنجرهٔ یک‌تایی = عبور مستقیم. */
        return uint32_t__sampleMa;
    }

    uint32_t__historyMa = UINT32_T__G__CurrentMedianHistoryMa[uint8_t__channelIndex];
    for (uint8_t__index = (uint8_t)(uint8_t__medianSize - 1u);
         uint8_t__index > 0u;
         uint8_t__index--)
    {
        uint32_t__historyMa[uint8_t__index] =
            uint32_t__historyMa[uint8_t__index - 1u];
    }
    uint32_t__historyMa[0] = uint32_t__sampleMa;

    for (uint8_t__index = 0u; uint8_t__index < uint8_t__medianSize; uint8_t__index++)
    {
        UINT32_T__A__Sorted[uint8_t__index] = uint32_t__historyMa[uint8_t__index];
    }
    for (uint8_t__pass = 1u; uint8_t__pass < uint8_t__medianSize; uint8_t__pass++)
    {
        uint32_t uint32_t__key = UINT32_T__A__Sorted[uint8_t__pass];
        int32_t int32_t__slot = (int32_t)uint8_t__pass - 1;

        while ((int32_t__slot >= 0) &&
               (UINT32_T__A__Sorted[int32_t__slot] > uint32_t__key))
        {
            UINT32_T__A__Sorted[int32_t__slot + 1] = UINT32_T__A__Sorted[int32_t__slot];
            int32_t__slot--;
        }
        UINT32_T__A__Sorted[int32_t__slot + 1] = uint32_t__key;
    }

    return UINT32_T__A__Sorted[uint8_t__medianSize / 2u];
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
        UINT8_T__G__FilterAverageWindow)
    {
        UINT8_T__G__CurrentAverageNextIndex[uint8_t__channelIndex] = 0u;
    }

    if (UINT8_T__G__CurrentAverageFillCount[uint8_t__channelIndex] <
        UINT8_T__G__FilterAverageWindow)
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
    /* [EN] Runtime window (ESP panel): the compiled switch is the
       capability, the size is the live setting (v1.4: 1..2 = bypass,
       3..15 = active, any value).
       [FA] پنجرهٔ زمان اجرا (پنل ESP): کلید کامپایل ظرفیت است و اندازه
       تنظیم زنده (v1.4: ۱..۲ = عبور مستقیم، ۳..۱۵ = فعال، هر مقدار). */
    if (UINT8_T__G__FilterMedianSize >= 3u)
    {
        uint32_t__sampleMa =
            func__Measurement_CurrentMedian(uint8_t__channelIndex,
                                            uint32_t__sampleMa,
                                            UINT8_T__G__FilterMedianSize);
    }
#endif

#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
    /* [EN] Runtime window (ESP panel): window 1 = bypass, >= 2 = active.
       [FA] پنجرهٔ زمان اجرا (پنل ESP): پنجرهٔ ۱ = عبور مستقیم، ≥۲ = فعال. */
    if (UINT8_T__G__FilterAverageWindow >= 2u)
    {
        uint32_t__sampleMa =
            func__Measurement_CurrentMovingAverage(uint8_t__channelIndex, uint32_t__sampleMa);
    }
#endif

    return uint32_t__sampleMa;
}

/* ==================== Measurement ResetCurrentFilters ==================== */

/**
 * @brief  [EN] Zero both current-filter histories and restart both average
 *              ramps. Called from Init and whenever the measurement task
 *              sees that the runtime filter configuration changed, so a
 *              window resize never mixes stale slots into the average.
 *              Only the measurement task may call this.
 *         [FA] تاریخچهٔ هر دو فیلتر جریان را صفر و شیب هر دو میانگین را
 *              از نو شروع می‌کند؛ از Init و هر بار که تسک اندازه‌گیری تغییر
 *              پیکربندی زمان اجرا را ببیند صدا زده می‌شود تا تغییر پنجره
 *              هرگز خانه‌های قدیمی را داخل میانگین نیامیزد. فقط تسک
 *              اندازه‌گیری اجازهٔ صدا زدن دارد.
 */
static void func__Measurement_ResetCurrentFilters(void)
{
#if (MEASUREMENT_CURRENT_MEDIAN3_ENABLE != 0u)
    for (uint32_t uint32_t__i = 0u; uint32_t__i < 2u; uint32_t__i++)
    {
        for (uint32_t uint32_t__j = 0u;
             uint32_t__j < (uint32_t)MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX;
             uint32_t__j++)
        {
            UINT32_T__G__CurrentMedianHistoryMa[uint32_t__i][uint32_t__j] = 0u;
        }
    }
#endif

#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
    for (uint32_t uint32_t__i = 0u; uint32_t__i < 2u; uint32_t__i++)
    {
        for (uint32_t uint32_t__j = 0u; uint32_t__j < MEASUREMENT_CURRENT_AVERAGE_WINDOW; uint32_t__j++)
        {
            UINT32_T__G__CurrentAverageWindowMa[uint32_t__i][uint32_t__j] = 0u;
        }
        UINT8_T__G__CurrentAverageFillCount[uint32_t__i] = 0u;
        UINT8_T__G__CurrentAverageNextIndex[uint32_t__i] = 0u;
    }
#endif
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
/* [EN] Live current-chain diagnostics, unfiltered single-frame values of
 *      the last frame (user order 2026-09-22: raw counts -> shunt uV ->
 *      mA before any filter, so the chain can be checked against a scope
 *      and an ammeter step by step).
 * [FA] دیاگ زندهٔ زنجیرهٔ جریان، مقادیر تک‌فریمیِ فیلترنشدهٔ آخرین فریم
 *      (دستور کاربر ۲۰۲۶-۰۹-۲۲: شمارش خام -> ولتاژ شانت uV -> mA قبل از
 *      هر فیلتر، تا زنجیره گام‌به‌گام با اسکوپ و آمپرمتر چک شود). */
volatile uint32_t UINT32_T__G__MeasCurrent1RawCounts = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent1ShuntUv = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent1MaUnfiltered = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent2RawCounts = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent2ShuntUv = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent2MaUnfiltered = 0u;
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

    /* [EN] Clean filter state and re-sync the applied-copy of the runtime
       filter configuration (a config that arrived before Init cannot
       survive into the first frame).
       [FA] وضعیت فیلتر تمیز و همگام‌سازی دوبارهٔ کپیِ اعمال‌شدهٔ پیکربندی
       زمان اجرا (پیکربندی‌ای که قبل از Init رسیده باشد به اولین فریم
       نمی‌رسد). */
    func__Measurement_ResetCurrentFilters();
    UINT8_T__G__FilterMedianSizeApplied = UINT8_T__G__FilterMedianSize;
    UINT8_T__G__FilterAverageWindowApplied = UINT8_T__G__FilterAverageWindow;

    UINT32_T__G__MeasInputVoltageMv = 0u;
    UINT32_T__G__MeasBattery24Mv = 0u;
    UINT32_T__G__MeasBattery12Mv = 0u;
    UINT32_T__G__MeasBatteryLowMv = 0u;
    UINT32_T__G__MeasBatteryHighMv = 0u;
    UINT32_T__G__MeasCurrent1Ma = 0u;
    UINT32_T__G__MeasCurrent2Ma = 0u;
    UINT32_T__G__MeasCurrent1RawCounts = 0u;
    UINT32_T__G__MeasCurrent1ShuntUv = 0u;
    UINT32_T__G__MeasCurrent1MaUnfiltered = 0u;
    UINT32_T__G__MeasCurrent2RawCounts = 0u;
    UINT32_T__G__MeasCurrent2ShuntUv = 0u;
    UINT32_T__G__MeasCurrent2MaUnfiltered = 0u;
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

/* ==================== Measurement Current Counts To Shunt Uv ==================== */

/**
 * @brief  [EN] Raw current counts to the pure-hardware shunt voltage in uV
 *              via the BSP (no zero offset, no bench trim) - live
 *              diagnostic for the current-chain review (user order
 *              2026-09-22).
 *         [FA] شمارش خام جریان به ولتاژ شانتِ فقط-سخت‌افزاری بر حسب uV از
 *              طریق BSP (بدون آفست صفر، بدون اصلاح بنچ) - دیاگ زندهٔ
 *              بررسی زنجیرهٔ جریان (دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint16_t__counts [EN] ADC count / شمارش ADC
 * @return uint32_t [EN] Shunt voltage in uV / ولتاژ شانت بر حسب uV
 */
uint32_t func__Measurement_CurrentCountsToShuntUv(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_CurrentCountsToShuntUv(uint16_t__counts);
}

/* ==================== Measurement ApplyVoltageOffsetMv ==================== */

/**
 * @brief  [EN] Saturating signed add of one runtime calibration offset to a
 *              voltage in mV; the result is never below 0 mV. The offset is
 *              clamped to +/-MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV by its
 *              setter, so the sum cannot overflow int32_t.
 *         [FA] جمع علامتدارِ اشباع‌شوندهٔ یک آفست کالیبراسیون زمان اجرا
 *              به ولتاژ بر حسب mV؛ نتیجه هرگز زیر ۰mV نمی‌رود. آفست در
 *              setter خودش به ±MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV گیره
 *              می‌شود، پس جمع در int32_t سرریز نمی‌کند.
 * @param  uint32_t__voltageMv [EN] Converted voltage, mV / ولتاژ تبدیل‌شده
 * @param  int32_t__offsetMv [EN] Runtime offset, mV / آفست زمان اجرا
 * @return uint32_t [EN] Offset voltage, mV / ولتاژ آفست‌خورده
 */
static uint32_t func__Measurement_ApplyVoltageOffsetMv(uint32_t uint32_t__voltageMv,
                                                       int32_t int32_t__offsetMv)
{
    int32_t int32_t__resultMv;

    int32_t__resultMv = (int32_t)uint32_t__voltageMv + int32_t__offsetMv;
    if (int32_t__resultMv < 0)
    {
        return 0u;
    }

    return (uint32_t)int32_t__resultMv;
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
    uint32_t uint32_t__current1ShuntUv;
    uint32_t uint32_t__inputVoltageMv;
    uint32_t uint32_t__battery24Mv;
    uint32_t uint32_t__battery12Mv;
    uint32_t uint32_t__batteryLowMv;
    uint32_t uint32_t__batteryHighMv;
    uint32_t uint32_t__current2SampleMa;
    uint32_t uint32_t__current2Ma;
    uint32_t uint32_t__current2ShuntUv;
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

    /* [EN] Runtime filter-config change detection (ESP panel, user order
 *          2026-09-22): when the median size or the average window moved,
 *          reset the filter state in this task's own context and take over
 *          the new configuration - a resize never mixes stale slots into
 *          the filters and no cross-task lock is needed.
 * [FA] تشخیص تغییر پیکربندی زندهٔ فیلتر (پنل ESP، دستور کاربر
 *      ۲۰۲۶-۰۹-۲۲): با جابه‌جاشدن اندازهٔ مدین یا پنجرهٔ میانگین، وضعیت
 *      فیلتر در زمینهٔ همین تسک ریست و پیکربندی جدید تحویل گرفته
 *      می‌شود - تغییر اندازه هرگز خانه‌های قدیمی را داخل فیلترها
 *      نمی‌آمیزد و قفل بین‌تسکی لازم نیست. */
    if ((UINT8_T__G__FilterMedianSizeApplied != UINT8_T__G__FilterMedianSize) ||
        (UINT8_T__G__FilterAverageWindowApplied != UINT8_T__G__FilterAverageWindow))
    {
        func__Measurement_ResetCurrentFilters();
        UINT8_T__G__FilterMedianSizeApplied = UINT8_T__G__FilterMedianSize;
        UINT8_T__G__FilterAverageWindowApplied = UINT8_T__G__FilterAverageWindow;
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
    uint32_t__current1ShuntUv =
        func__Measurement_CurrentCountsToShuntUv(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1]);
    uint32_t__current1Ma =
        func__Measurement_ApplyCurrentFilters(0u, uint32_t__current1SampleMa);
    uint32_t__inputVoltageMv =
        func__Measurement_V24CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_24V_IN]);
    uint32_t__battery24Mv =
        func__Measurement_V24CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_24V_BAT]);
    uint32_t__battery12Mv =
        func__Measurement_V12CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_12V_BAT]);

    /* [EN] Runtime voltage calibration offsets (ESP panel, user order
 *          2026-09-22): applied after the divider conversion and before the
 *          low/high derivation, each clamped by the setter to
 *          +/-MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV; default 0 keeps today's
 *          behavior. Saturating signed add, result never below 0 mV.
 * [FA] آفست‌های کالیبراسیون ولتاژ زمان اجرا (پنل ESP، دستور کاربر
 *      ۲۰۲۶-۰۹-۲۲): بعد از تبدیل مقسم و قبل از محاسبهٔ پایین/بالا اعمال
 *      می‌شوند؛ هر یک در setter به ±MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV
 *      گیره می‌شود و پیش‌فرض ۰ همان رفتار فعلی است. جمع علامتدارِ
 *      اشباع‌شونده؛ نتیجه هرگز زیر ۰mV نمی‌رود. */
    uint32_t__inputVoltageMv = func__Measurement_ApplyVoltageOffsetMv(
        uint32_t__inputVoltageMv, INT32_T__G__VoltageInOffsetMv);
    uint32_t__battery24Mv = func__Measurement_ApplyVoltageOffsetMv(
        uint32_t__battery24Mv, INT32_T__G__Voltage24OffsetMv);
    uint32_t__battery12Mv = func__Measurement_ApplyVoltageOffsetMv(
        uint32_t__battery12Mv, INT32_T__G__Voltage12OffsetMv);
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
    uint32_t__current2ShuntUv =
        func__Measurement_CurrentCountsToShuntUv(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2]);
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
    /* [EN] Unfiltered current-chain diagnostics of this frame: raw counts,
       pure-hardware shunt uV and pre-filter mA (user order 2026-09-22).
       [FA] دیاگ فیلترنشدهٔ زنجیرهٔ جریان همین فریم: شمارش خام، ولتاژ شانت
       فقط-سخت‌افزاری و mA قبل از فیلتر (دستور کاربر ۲۰۲۶-۰۹-۲۲). */
    UINT32_T__G__MeasCurrent1RawCounts =
        (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1];
    UINT32_T__G__MeasCurrent1ShuntUv = uint32_t__current1ShuntUv;
    UINT32_T__G__MeasCurrent1MaUnfiltered = uint32_t__current1SampleMa;
    UINT32_T__G__MeasInputVoltageMv = uint32_t__inputVoltageMv;
    UINT32_T__G__MeasBattery24Mv = uint32_t__battery24Mv;
    UINT32_T__G__MeasBattery12Mv = uint32_t__battery12Mv;
    UINT32_T__G__MeasBatteryLowMv = uint32_t__batteryLowMv;
    UINT32_T__G__MeasBatteryHighMv = uint32_t__batteryHighMv;
    UINT32_T__G__MeasCurrent2Ma = uint32_t__current2Ma;
    UINT32_T__G__MeasCurrent2RawCounts =
        (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2];
    UINT32_T__G__MeasCurrent2ShuntUv = uint32_t__current2ShuntUv;
    UINT32_T__G__MeasCurrent2MaUnfiltered = uint32_t__current2SampleMa;
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

/* ==================== Measurement runtime config API (ESP panel) ==================== */

/**
 * @brief  [EN] Set the runtime median window size of the current filter
 *              (user order 2026-09-22: ONE size parameter per filter -
 *              size 1 means bypass, so no separate on/off switch exists).
 *              Valid sizes are the odd values 1, 3 and 5; any other request
 *              is rounded DOWN to the next valid size (0/1/2 -> 1 = bypass,
 *              3/4 -> 3, >=5 -> 5). The compiled switch
 *              MEASUREMENT_CURRENT_MEDIAN3_ENABLE remains the capability
 *              gate: when it is 0 the request is clamped to 1 (bypass).
 *              RAM only - a reboot restores the default of 3.
 *         [FA] اندازهٔ پنجرهٔ مدین فیلتر جریان در زمان اجرا (دستور کاربر
 *              ۲۰۲۶-۰۹-۲۲: برای هر فیلتر یک پارامتر اندازه - اندازهٔ ۱
 *              یعنی عبور مستقیم، پس کلید جدا لازم نیست). اندازه‌های معتبر
 *              ۱ و ۳ و ۵ هستند؛ هر درخواست دیگر به پایین‌ترین اندازهٔ
 *              معتبر گرد می‌شود (۰/۱/۲ → ۱، ۳/۴ → ۳، ≥۵ → ۵). کلید کامپایل
 *              MEASUREMENT_CURRENT_MEDIAN3_ENABLE ظرفیت را تعیین می‌کند:
 *              اگر ۰ باشد درخواست به ۱ گیره می‌شود. فقط RAM - ری‌استارت
 *              پیش‌فرض ۳ را برمی‌گرداند.
 * @param  uint8_t__medianSize [EN] Requested size / اندازهٔ درخواستی
 * @return uint8_t [EN] Applied size / اندازهٔ اعمال‌شده
 */
uint8_t func__Measurement_SetFilterMedianSize(uint8_t uint8_t__medianSize)
{
#if (MEASUREMENT_CURRENT_MEDIAN3_ENABLE != 0u)
    /* [EN] v1.4 (user order 2026-09-25): ANY size 1..MAX - even sizes
       allowed, no odd rounding. 1..2 behave as bypass in the task.
       [FA] ‌v1.4 (دستور کاربر ۲۰۲۶-۰۹-۲۵): هر اندازهٔ ۱..MAX - زوج هم
       مجاز، بدون گردکردن به فرد. ۱..۲ در تسک مثل عبور مستقیم رفتار
       می‌کنند. */
    if (uint8_t__medianSize < 1u)
    {
        uint8_t__medianSize = 1u;
    }
    else if (uint8_t__medianSize > (uint8_t)MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX)
    {
        uint8_t__medianSize = (uint8_t)MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX;
    }
    else
    {
        /* [EN] Any value inside 1..MAX is taken as-is.
           [FA] هر مقدار داخل ۱..MAX همان‌طور که هست پذیرفته می‌شود. */
    }
#else
    (void)uint8_t__medianSize;
    uint8_t__medianSize = 1u;
#endif

    UINT8_T__G__FilterMedianSize = uint8_t__medianSize;
    return uint8_t__medianSize;
}

/**
 * @brief  [EN] Set the runtime moving-average window size, clamped to
 *              1..MEASUREMENT_CURRENT_AVERAGE_WINDOW (the compiled ring
 *              size is the hard ceiling). The measurement task resets the
 *              filter state on the next frame after a change.
 *         [FA] اندازهٔ پنجرهٔ میانگین متحرک در زمان اجرا، گیره در
 *              ۱..MEASUREMENT_CURRENT_AVERAGE_WINDOW (اندازهٔ حلقهٔ کامپایل
 *              سقف قطعی است). تسک اندازه‌گیری پس از تغییر در فریم بعدی
 *              وضعیت فیلتر را ریست می‌کند.
 * @param  uint8_t__windowSamples [EN] Requested window / پنجرهٔ درخواستی
 * @return uint8_t [EN] Applied window / پنجرهٔ اعمال‌شده
 */
uint8_t func__Measurement_SetFilterAverageWindow(uint8_t uint8_t__windowSamples)
{
    if (uint8_t__windowSamples < 1u)
    {
        uint8_t__windowSamples = 1u;
    }
    else if (uint8_t__windowSamples > (uint8_t)MEASUREMENT_CURRENT_AVERAGE_WINDOW)
    {
        uint8_t__windowSamples = (uint8_t)MEASUREMENT_CURRENT_AVERAGE_WINDOW;
    }
    else
    {
        /* [EN] Value already inside the window. [FA] مقدار داخل بازه است. */
    }

    UINT8_T__G__FilterAverageWindow = uint8_t__windowSamples;
    return uint8_t__windowSamples;
}

/**
 * @brief  [EN] Read the live median window size of the current filter.
 *         [FA] اندازهٔ زندهٔ پنجرهٔ مدین فیلتر جریان.
 * @return uint8_t [EN] 1, 3 or 5 / اندازهٔ فعال
 */
uint8_t func__Measurement_GetFilterMedianSize(void)
{
    return UINT8_T__G__FilterMedianSize;
}

/**
 * @brief  [EN] Read the live moving-average window size.
 *         [FA] اندازهٔ زندهٔ پنجرهٔ میانگین متحرک.
 * @return uint8_t [EN] Window in samples / پنجره بر حسب نمونه
 */
uint8_t func__Measurement_GetFilterAverageWindow(void)
{
    return UINT8_T__G__FilterAverageWindow;
}

/**
 * @brief  [EN] Set one runtime voltage calibration offset, clamped to
 *              +/-MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV. Index 0 = 24 V
 *              input, 1 = 24 V battery pack, 2 = 12 V (middle node)
 *              battery. Default 0 = today's behavior; RAM only.
 *         [FA] یک آفست کالیبراسیون ولتاژ زمان اجرا، گیره در
 *              ±MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV. اندیس ۰ = ورودی ۲۴V،
 *              ۱ = باتری ۲۴V، ۲ = باتری ۱۲V (نود میانی). پیش‌فرض ۰ همان
 *              رفتار فعلی؛ فقط RAM.
 * @param  uint8_t__channelIndex [EN] 0 = VIN, 1 = V24, 2 = V12 / اندیس
 * @param  int32_t__offsetMv [EN] Requested offset, mV / آفست درخواستی
 * @return int32_t [EN] Applied offset, mV / آفست اعمال‌شده
 */
int32_t func__Measurement_SetVoltageOffsetMv(uint8_t uint8_t__channelIndex,
                                             int32_t int32_t__offsetMv)
{
    if (int32_t__offsetMv > (int32_t)MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV)
    {
        int32_t__offsetMv = (int32_t)MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV;
    }
    else if (int32_t__offsetMv < -(int32_t)MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV)
    {
        int32_t__offsetMv = -(int32_t)MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV;
    }
    else
    {
        /* [EN] Value already inside the window. [FA] مقدار داخل بازه است. */
    }

    if (uint8_t__channelIndex == 0u)
    {
        INT32_T__G__VoltageInOffsetMv = int32_t__offsetMv;
    }
    else if (uint8_t__channelIndex == 1u)
    {
        INT32_T__G__Voltage24OffsetMv = int32_t__offsetMv;
    }
    else
    {
        INT32_T__G__Voltage12OffsetMv = int32_t__offsetMv;
    }

    return int32_t__offsetMv;
}

/**
 * @brief  [EN] Read one runtime voltage calibration offset.
 *         [FA] یک آفست کالیبراسیون ولتاژ زمان اجرا را می‌خواند.
 * @param  uint8_t__channelIndex [EN] 0 = VIN, 1 = V24, 2 = V12 / اندیس
 * @return int32_t [EN] Live offset, mV / آفست زنده
 */
int32_t func__Measurement_GetVoltageOffsetMv(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return INT32_T__G__VoltageInOffsetMv;
    }
    if (uint8_t__channelIndex == 1u)
    {
        return INT32_T__G__Voltage24OffsetMv;
    }

    return INT32_T__G__Voltage12OffsetMv;
}
