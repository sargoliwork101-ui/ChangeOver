/**
 * @file    measurement.c
 * @brief   [EN] ADC counts to engineering units (mV / mA), step by step;
 *              latest snapshot shared with the other tasks. Runs in the
 *              measurement task: a few conversions + one GPIO read, then
 *              yield - no HAL_Delay anywhere. Charge currents = the PWM
 *              mid-ON synchronized samples, filtered on the RAW counts
 *              first (median + moving average), converted to mA after;
 *              battery voltages keep their median-5 spike guard on the
 *              raw counts.
 *          [FA] شمارش ADC به واحد مهندسی (‎mV/mA)‎ گام‌به‌گام؛ آخرین snapshot
 *              مشترک با تسک‌های دیگر. فقط چند تبدیل + یک خواندن GPIO و بعد
 *              yield - بدون HAL_Delay. جریان شارژ = نمونهٔ سنکرون وسط ON که
 *              اول روی شمارش خام فیلتر می‌شود (مدین + میانگین متحرک) و بعد
 *              تبدیل؛ ولتاژ باتری محافظ مدین-۵ روی شمارش خام دارد.
 * @note    [EN] Divider/gain constants are private to bsp_measurement.c;
 *              converted values are exposed as UINT32_T__G__Meas* /
 *              BOOL__G__Meas* globals - written only by this task,
 *              readable by any module (and the debugger via Live
 *              Expressions).
 *          [FA] ثابت‌های تقسیم/گین خصوصی bsp_measurement.c است؛ مقادیر
 *              تبدیل‌شده در گلوبال‌های Meas* فقط با نوشتن همین تسک در
 *              دسترس همه‌اند (از جمله دیباگ با Live Expressions).
 */

#include "measurement.h"
#include "calibration.h"
#include "cal_lut.h"
#include "bsp_adc.h"
#include "bsp_measurement.h"
#include "bsp_gpio.h"
#include "cmsis_os2.h"
#include <stddef.h>

/* ==================== Static State ==================== */

/* [EN] Shared snapshot for the other tasks (UI / protection / comm).
 *      Written only by the measurement task, read by GetSnapshot.
 *      [FA] snapshot مشترک برای تسک‌های دیگر (‎UI / protection / comm)‎.
 *      فقط توسط تسک measurement نوشته و با GetSnapshot خوانده می‌شود. */
static volatile measurement_snapshot_t MEASUREMENT_SNAPSHOT_T__G__Snap;

/* [EN] Number of completed stable normalized ADC frames collected during
 *      startup warm-up. Unit: completed ADC frame.
 * [FA] تعداد فریم‌های کامل و پایدار ADC استانداردشده در ‎warm-up‎ شروع.
 *      واحد: فریم کامل ADC. */
static uint8_t UINT8_T__G__MeasurementWarmupFrameCount;

/* [EN] Median-of-5 history of the RAW counts of the two battery ADC
   channels (0=V24_BAT, 1=V12_BAT; user order 2026-09-27: filters run on
   the raw ADC data, conversion/derivation happen after): raised
   2026-09-19 from median-3 because a false buzzer
   still fired WHILE the charger genuinely pumped (absorb mode) - that
   means glitch bursts of two consecutive frames also cross 14.8 V, and
   median-3 passes a 2-frame burst straight through. Median-of-5 only
   outputs the middle sample, so any burst shorter than 3 frames is dead
   while real voltage steps pass with ~20 ms of lag at the 10 ms frame
   cadence - nothing the control loop can notice.
   [FA] تاریخچهٔ مدین-۵ شمارش خام دو کانال ADC باتری: چون بوق فیک حین ابزوربِ
   واقعی هم زده شد، پرش‌های دو-فریمی پشت‌سر از مدین-۳ رد می‌شدند؛ مدین-۵
   هر ترکیدگی کوتاه‌تر از ۳ فریم را نابود می‌کند و پلهٔ واقعی با حدود دو
   فریم تأخیر عبور می‌کند. */
static uint32_t UINT32_T__G__BatVoltageMedianHistoryCounts[2][5];

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
static uint32_t UINT32_T__G__CurrentMedianHistoryCounts[2][MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX];
#endif

#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
/* [EN] Moving-average window per current channel (user order 2026-09-22):
 *      the last MEASUREMENT_CURRENT_AVERAGE_WINDOW raw count samples (user
 *      order 2026-09-27: filters run on the raw ADC data), a fill counter
 *      for the startup ramp and the next slot index. Exists only when the
 *      filter switch is on.
 * [FA] پنجرهٔ میانگین متحرک برای هر کانال جریان (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۲): آخرین MEASUREMENT_CURRENT_AVERAGE_WINDOW نمونهٔ شمارش
 *      خام (دستور کاربر ۲۰۲۶-۰۹-۲۷: فیلتر روی دادهٔ خام ADC)، شمارندهٔ
 *      پرشدن برای شیب شروع و اندیس خانهٔ بعدی. فقط وقتی کلید فیلتر روشن
 *      است وجود دارد. */
static uint32_t UINT32_T__G__CurrentAverageWindowCounts[2][MEASUREMENT_CURRENT_AVERAGE_WINDOW];
static uint16_t UINT16_T__G__CurrentAverageFillCount[2];
static uint16_t UINT16_T__G__CurrentAverageNextIndex[2];
#endif

/* ==================== Runtime filter config / voltage offsets (ESP panel) ==================== */

/* [EN] Full-program audit 2026-10-05: this whole group used to sit INSIDE
 *      the MEASUREMENT_CURRENT_AVERAGE_ENABLE block above, although none of
 *      it belongs to the moving-average filter - the runtime median size and
 *      the three voltage offsets are read unconditionally by
 *      func__Measurement_Run, func__Measurement_Init and the public
 *      setters and getters. Setting that switch to 0u therefore broke the
 *      build with eight "undeclared" errors. The
 *      declarations are unconditional now; behaviour with the switch at 1u
 *      is bit-for-bit identical.
 * [FA] ممیزی کل برنامه ۲۰۲۶-۱۰-۰۵: این گروه قبلاً داخل بلوک
 *      MEASUREMENT_CURRENT_AVERAGE_ENABLE بود، در حالی‌که هیچ‌کدامشان به
 *      فیلتر میانگین متحرک ربط ندارند؛ اندازهٔ مدین زمان اجرا و سه آفست
 *      ولتاژ بدون شرط خوانده می‌شوند. پس صفرکردن آن کلید بیلد را با هشت
 *      خطای undeclared می‌شکست. حالا بدون شرط‌اند و با کلید ۱ رفتار دقیقاً
 *      همان است. */

/* [EN] Runtime copies of the current-filter configuration (resizable
 *      live from the ESP panel; size 1 = bypass, no separate on/off
 *      switch). The compile-time switches stay the capability gates; the
 *      measurement task detects a change and resets the filter state in
 *      its own context, so no cross-task locking (written by EspLink,
 *      volatile).
 * [FA] نسخهٔ زمان اجرای پیکربندی فیلتر جریان (قابل تغییر زنده از پنل؛
 *      اندازهٔ ۱ = عبور مستقیم). کلیدهای کامپایل ظرفیت را تعیین می‌کنند و
 *      تسک اندازه‌گیری تغییر را در زمینهٔ خودش ریست می‌کند - بدون قفل
 *      بین‌تسکی (نوشته از تسک EspLink، volatile). */
static volatile uint8_t UINT8_T__G__FilterMedianSize = 3u;
static volatile uint16_t UINT16_T__G__FilterAverageWindow =
    (uint16_t)MEASUREMENT_CURRENT_AVERAGE_WINDOW_DEFAULT;

/* [EN] Last configuration the measurement task applied; owned by the
 *      measurement task only (change detection).
 * [FA] آخرین پیکربندی اعمال‌شده توسط تسک اندازه‌گیری؛ فقط مالکش همین
 *      تسک است (تشخیص تغییر). */
static uint8_t UINT8_T__G__FilterMedianSizeApplied = 3u;
static uint16_t UINT16_T__G__FilterAverageWindowApplied =
    (uint16_t)MEASUREMENT_CURRENT_AVERAGE_WINDOW_DEFAULT;

/* [EN] Runtime voltage calibration offsets in mV, default 0 = today's
 *      behavior; applied AFTER the divider conversion, before the
 *      low/high derivation. Flash-persisted since v1.14 (NVM ids 4/5/6).
 * [FA] آفست‌های کالیبراسیون ولتاژ بر حسب mV در زمان اجرا؛ پیش‌فرض ۰ یعنی
 *      رفتار فعلی؛ بعد از تبدیل مقسم و قبل از محاسبهٔ پایین/بالا اعمال
 *      می‌شوند. روی فلش می‌مانند (NVM نسخهٔ ۱.۱۴، شناسه‌های ۴/۵/۶). */
static volatile int32_t INT32_T__G__VoltageInOffsetMv = 0;
static volatile int32_t INT32_T__G__Voltage24OffsetMv = 0;
static volatile int32_t INT32_T__G__Voltage12OffsetMv = 0;

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
 * @brief  [EN] Shift one new raw current count sample into the channel's
 *              median history and return the middle value. Window size =
 *              the runtime median size (any 1..15, even sizes too); a
 *              single-sample jump of the synchronized reading is discarded
 *              with zero added lag. Insertion sort of a LOCAL copy keeps
 *              the live history untouched (same pattern as the voltage
 *              median-5).
 *         [FA] نمونهٔ خام جدید را در تاریخچهٔ مدین کانال جابه‌جا و مقدار
 *              میانی را برمی‌گرداند. اندازهٔ پنجره = اندازهٔ مدین زمان اجرا
 *              (هر ۱..۱۵، زوج هم)؛ پرش تک‌نمونه‌ای بدون تأخیر اضافه دور
 *              داده می‌شود. مرتب‌سازی درجی روی کپی محلی، تاریخچهٔ زنده دست
 *              نمی‌خورد (الگوی مدین-۵ ولتاژ).
 * @param  uint8_t__channelIndex [EN] Current channel 0 or 1 / کانال جریان ۰ یا ۱
 * @param  uint32_t__sampleCounts [EN] New raw count sample / نمونهٔ شمارش خام جدید
 * @param  uint8_t__medianSize [EN] Active median window (1..15)‎ / پنجرهٔ فعال
 * @return uint32_t [EN] Median-filtered counts‎ / شمارش مدین‌شده
 */
static uint32_t func__Measurement_CurrentMedian(uint8_t uint8_t__channelIndex,
                                                uint32_t uint32_t__sampleCounts,
                                                uint8_t uint8_t__medianSize)
{
    uint32_t UINT32_T__A__Sorted[MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX];
    uint32_t *uint32_t__historyCounts;
    uint8_t uint8_t__index;
    uint8_t uint8_t__pass;

    if ((uint8_t__channelIndex >= 2u) ||
        (uint8_t__medianSize < 1u) ||
        (uint8_t__medianSize > (uint8_t)MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX))
    {
        return uint32_t__sampleCounts;
    }

    if (uint8_t__medianSize == 1u)
    {
        /* [‎EN] Window of one = bypass. [FA]‎ پنجرهٔ یک‌تایی = عبور مستقیم. */
        return uint32_t__sampleCounts;
    }

    uint32_t__historyCounts = UINT32_T__G__CurrentMedianHistoryCounts[uint8_t__channelIndex];
    for (uint8_t__index = (uint8_t)(uint8_t__medianSize - 1u);
         uint8_t__index > 0u;
         uint8_t__index--)
    {
        uint32_t__historyCounts[uint8_t__index] =
            uint32_t__historyCounts[uint8_t__index - 1u];
    }
    uint32_t__historyCounts[0] = uint32_t__sampleCounts;

    for (uint8_t__index = 0u; uint8_t__index < uint8_t__medianSize; uint8_t__index++)
    {
        UINT32_T__A__Sorted[uint8_t__index] = uint32_t__historyCounts[uint8_t__index];
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
 * @brief  [EN] Push one new raw current count sample into the channel's
 *              moving-average window and return the average. Until the
 *              window fills after a restart, the average runs over the
 *              collected samples only (no zero-drag from empty slots).
 *         [FA] نمونهٔ خام جدید را در پنجرهٔ میانگین متحرک کانال می‌نویسد
 *              و میانگین را برمی‌گرداند. تا پرشدن پنجره، میانگین فقط روی
 *              نمونه‌های جمع‌شده اجرا می‌شود (بدون کشیده‌شدن به صفرِ
 *              خانه‌های خالی).
 * @param  uint8_t__channelIndex [EN] Current channel 0 or 1 / کانال جریان ۰ یا ۱
 * @param  uint32_t__sampleCounts [EN] New raw count sample / نمونهٔ شمارش خام جدید
 * @return uint32_t [EN] Moving-average counts‎ / شمارش میانگین‌گرفته
 */
static uint32_t func__Measurement_CurrentMovingAverage(uint8_t uint8_t__channelIndex,
                                                       uint32_t uint32_t__sampleCounts)
{
    uint32_t uint32_t__windowSumCounts;
    uint32_t uint32_t__windowSlot;
    uint32_t uint32_t__averageCounts;

    if (uint8_t__channelIndex >= 2u)
    {
        return uint32_t__sampleCounts;
    }

    /* [EN] Replace the oldest slot, then advance the ring index by hand (no
       modulo, keeps the index inside the window under MISRA rules).
       [FA] قدیمی‌ترین خانه جایگزین می‌شود، بعد اندیس حلقه دستی جلو می‌رود
       (بدون باقیماندهٔ تقسیم تا اندیس طبق قواعد MISRA داخل پنجره بماند). */
    UINT32_T__G__CurrentAverageWindowCounts[uint8_t__channelIndex]
        [UINT16_T__G__CurrentAverageNextIndex[uint8_t__channelIndex]] = uint32_t__sampleCounts;

    UINT16_T__G__CurrentAverageNextIndex[uint8_t__channelIndex]++;
    if (UINT16_T__G__CurrentAverageNextIndex[uint8_t__channelIndex] >=
        UINT16_T__G__FilterAverageWindow)
    {
        UINT16_T__G__CurrentAverageNextIndex[uint8_t__channelIndex] = 0u;
    }

    if (UINT16_T__G__CurrentAverageFillCount[uint8_t__channelIndex] <
        UINT16_T__G__FilterAverageWindow)
    {
        UINT16_T__G__CurrentAverageFillCount[uint8_t__channelIndex]++;
    }

    /* [EN] Sum only the filled slots and divide once by the fill count.
       [FA] جمع فقط روی خانه‌های پر و یک تقسیم بر تعداد خانه‌های پر. */
    uint32_t__windowSumCounts = 0u;
    for (uint32_t__windowSlot = 0u;
         uint32_t__windowSlot < (uint32_t)UINT16_T__G__CurrentAverageFillCount[uint8_t__channelIndex];
         uint32_t__windowSlot++)
    {
        uint32_t__windowSumCounts +=
            UINT32_T__G__CurrentAverageWindowCounts[uint8_t__channelIndex][uint32_t__windowSlot];
    }

    uint32_t__averageCounts =
        uint32_t__windowSumCounts / (uint32_t)UINT16_T__G__CurrentAverageFillCount[uint8_t__channelIndex];

    return uint32_t__averageCounts;
}
#endif

/* ==================== Measurement ApplyCurrentFilters / اعمال فیلترهای جریان ==================== */

/**
 * @brief  [EN] Run the enabled current-filter chain of one channel in the
 *              fixed order: median first (kill single jumps), then the
 *              moving average (smooth) - exactly as the two compile-time
 *              switches select. Both off = pass-through unchanged.
 *         [FA] زنجیرهٔ فیلتر جریان فعال کانال را با ترتیب ثابت اجرا
 *              می‌کند: اول مدین (حذف پرش تکی) بعد میانگین متحرک (صاف‌کردن)
 *              - دقیقاً طبق دو کلید کامپایل. هر دو خاموش = عبور بدون تغییر.
 * @param  uint8_t__channelIndex [EN] Current channel 0 or 1 / کانال جریان ۰ یا ۱
 * @param  uint32_t__sampleCounts [EN] Raw count sample / نمونهٔ شمارش خام
 * @return uint32_t [EN] Filtered counts / شمارش فیلترشده
 */
static uint32_t func__Measurement_ApplyCurrentFilters(uint8_t uint8_t__channelIndex,
                                                      uint32_t uint32_t__sampleCounts)
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
        uint32_t__sampleCounts =
            func__Measurement_CurrentMedian(uint8_t__channelIndex,
                                            uint32_t__sampleCounts,
                                            UINT8_T__G__FilterMedianSize);
    }
#endif

#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
    /* [EN] Runtime window (ESP panel): window 1 = bypass, >= 2 = active.
       [FA] پنجرهٔ زمان اجرا (پنل ESP): پنجرهٔ ۱ = عبور مستقیم، ≥۲ = فعال. */
    if (UINT16_T__G__FilterAverageWindow >= 2u)
    {
        uint32_t__sampleCounts =
            func__Measurement_CurrentMovingAverage(uint8_t__channelIndex, uint32_t__sampleCounts);
    }
#endif

    return uint32_t__sampleCounts;
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
            UINT32_T__G__CurrentMedianHistoryCounts[uint32_t__i][uint32_t__j] = 0u;
        }
    }
#endif

#if (MEASUREMENT_CURRENT_AVERAGE_ENABLE != 0u)
    for (uint32_t uint32_t__i = 0u; uint32_t__i < 2u; uint32_t__i++)
    {
        for (uint32_t uint32_t__j = 0u; uint32_t__j < MEASUREMENT_CURRENT_AVERAGE_WINDOW; uint32_t__j++)
        {
            UINT32_T__G__CurrentAverageWindowCounts[uint32_t__i][uint32_t__j] = 0u;
        }
        UINT16_T__G__CurrentAverageFillCount[uint32_t__i] = 0u;
        UINT16_T__G__CurrentAverageNextIndex[uint32_t__i] = 0u;
    }
#endif
}

/**
 * @brief  [EN] Shift one new RAW count sample of a battery ADC channel
 *         into the median-5 history and return the filtered counts. The
 *         charger state machine and the battery-lost detector consume the
 *         voltages DERIVED from these filtered conversions, so a short
 *         spike burst cannot fake "battery gone".
 *         [FA] نمونهٔ خام کانال ADC باتری را در تاریخچهٔ مدین-۵ جابه‌جا و
 *         شمارش فیلترشده را برمی‌گرداند. شارژر و آشکارساز قطع باتری از
 *         ولتاژهای مشتق‌شده از همین تبدیل‌ها استفاده می‌کنند تا ترکیدگی
 *         کوتاه «باتری رفت» را جعل نکند.
 * @param  uint8_t__channelIndex [EN] 0 = V24_BAT raw, 1 = V12_BAT raw‎ / شمارش خام
 * @param  uint32_t__sampleCounts [EN] New raw count sample / شمارش خام جدید
 * @return uint32_t [EN] Median-of-5 filtered counts‎ / شمارش مدین‌شده
 */
static uint32_t func__Measurement_MedianFilterVoltageSample(uint8_t uint8_t__channelIndex,
                                                            uint32_t uint32_t__sampleCounts)
{
    uint32_t *uint32_t__historyCounts;

    if (uint8_t__channelIndex >= 2u)
    {
        return uint32_t__sampleCounts;
    }

    uint32_t__historyCounts = UINT32_T__G__BatVoltageMedianHistoryCounts[uint8_t__channelIndex];
    uint32_t__historyCounts[0] = uint32_t__historyCounts[1];
    uint32_t__historyCounts[1] = uint32_t__historyCounts[2];
    uint32_t__historyCounts[2] = uint32_t__historyCounts[3];
    uint32_t__historyCounts[3] = uint32_t__historyCounts[4];
    uint32_t__historyCounts[4] = uint32_t__sampleCounts;

    return func__Measurement_Median5(uint32_t__historyCounts);
}


/* ==================== Global Shared Values ==================== */
/* [EN] The engineering values of the newest frame, shared to all tasks.
 *      Written ONLY by the measurement task (Run/Init); any module reads
 *      them after including measurement.h. Meas* prefix avoids a link
 *      collision with the UI manual test globals (task_ui.c).
 * [FA] مقادیر مهندسی آخرین فریم، مشترک برای همهٔ تسک‌ها. فقط تسک
 *      ‎measurement (Run/Init)‎ می‌نویسد؛ هر ماژول بعد از include کردن
 *      measurement.h می‌خواند. پیشوند Meas* از تداخل لینک با متغیرهای
 *      تست دستی UI (task_ui.c) جلوگیری می‌کند. */
volatile uint32_t UINT32_T__G__MeasInputVoltageMv = 0u;
volatile uint32_t UINT32_T__G__MeasBattery24Mv = 0u;
volatile uint32_t UINT32_T__G__MeasBattery12Mv = 0u;
volatile uint32_t UINT32_T__G__MeasBatteryLowMv = 0u;
volatile uint32_t UINT32_T__G__MeasBatteryHighMv = 0u;
/* [EN] Measured ADC reference (VDDA) in mV, from the internal 1.20 V channel.
 *      Published even while CAL_VDDA_TRACKING_ENABLE is 0, because the whole
 *      point is to LOOK at it first: compare this against a DMM on the 3.3 V
 *      rail, and the gap is the global scale error every channel carries.
 *      0 = not available / reading outside a plausible 3.0..3.6 V.
 * [FA] مرجع ADC اندازه‌گیری‌شده بر حسب mV از کانال داخلی ۱٫۲۰ ولت. حتی وقتی
 *      ردیابی خاموش است منتشر می‌شود، چون هدف همین است که اول ببینیدش: این را
 *      با مولتی‌متر روی ریل ۳٫۳ ولت مقایسه کنید؛ اختلاف، همان خطای مقیاس
 *      سراسری است که همهٔ کانال‌ها حمل می‌کنند. صفر یعنی در دسترس نیست. */
volatile uint32_t UINT32_T__G__MeasVddaMv = 0u;
/* [EN] Raw ADC counts of the voltage channels. WHY THEY ARE ALWAYS ON rather
 *      than behind a "calibration mode": a mode that neutralises gains and
 *      offsets would also neutralise them for the CONTROL path, so the
 *      over-voltage cut and the current fault would be judging uncalibrated
 *      numbers for as long as the mode is left on - and a mode left on by
 *      accident is a real failure mode on a bench. Publishing the raw counts
 *      alongside costs nothing, can never mis-protect, and is strictly MORE
 *      informative: from counts plus a DMM every coefficient can be rebuilt
 *      from scratch, including ones a bypass mode would still have applied.
 * [FA] چرا همیشه روشن‌اند نه پشت «مود کالیبره»: مودی که ضرایب و آفست‌ها را
 *      خنثی کند، آن‌ها را برای مسیر کنترل هم خنثی می‌کند، پس تا وقتی آن مود
 *      روشن است قطع اضافه‌ولتاژ و خطای جریان دارند روی اعداد کالیبره‌نشده قضاوت
 *      می‌کنند - و مودی که سهواً روشن بماند سر بنچ یک حالت خرابی واقعی است.
 *      انتشار شمارش خام در کنارش هیچ هزینه‌ای ندارد، هرگز نمی‌تواند حفاظت را
 *      خراب کند، و اطلاعات بیشتری می‌دهد: از شمارش به‌اضافهٔ مولتی‌متر هر ضریبی
 *      از صفر بازساخته می‌شود، حتی ضرایبی که مود بایپس همچنان اعمالشان می‌کرد. */
volatile uint32_t UINT32_T__G__MeasVinRawCounts = 0u;
volatile uint32_t UINT32_T__G__MeasV24RawCounts = 0u;
volatile uint32_t UINT32_T__G__MeasV12RawCounts = 0u;
volatile uint32_t UINT32_T__G__MeasVrefintRawCounts = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent1Ma = 0u;
volatile uint32_t UINT32_T__G__MeasCurrent2Ma = 0u;
/* [EN] Live current-chain diagnostics, unfiltered single-frame values of
 *      the last frame (user order 2026-09-22: raw counts -> shunt uV ->
 *      mA before any filter, so the chain can be checked against a scope
 *      and an ammeter step by step).
 * [FA] دیاگ زندهٔ زنجیرهٔ جریان، مقادیر تک‌فریمیِ فیلترنشدهٔ آخرین فریم
 *      (دستور کاربر ۲۰۲۶-۰۹-۲۲: شمارش خام -> ولتاژ شانت ‎uV -> mA‎ قبل از
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
 *         [FA] آخرین نمونه را صفر می‌کند (‎valid = false)‎.
 */
void func__Measurement_Init(void)
{
    /* [EN] Zero the warm-up counter, shared globals and snapshot; nothing is
       valid until the required number of stable frames is collected.
       [FA] شمارندهٔ ‎warm-up‎، گلوبال‌های مشترک و snapshot را صفر می‌کند؛
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
    UINT16_T__G__FilterAverageWindowApplied = UINT16_T__G__FilterAverageWindow;

    UINT32_T__G__MeasInputVoltageMv = 0u;
    UINT32_T__G__MeasBattery24Mv = 0u;
    UINT32_T__G__MeasBattery12Mv = 0u;
    UINT32_T__G__MeasBatteryLowMv = 0u;
    UINT32_T__G__MeasBatteryHighMv = 0u;
    UINT32_T__G__MeasVddaMv = 0u;
    UINT32_T__G__MeasVinRawCounts = 0u;
    UINT32_T__G__MeasV24RawCounts = 0u;
    UINT32_T__G__MeasV12RawCounts = 0u;
    UINT32_T__G__MeasVrefintRawCounts = 0u;
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

/* ==================== Battery24 Counts To Mv ==================== */

/**
 * @brief  [EN] Convert the battery-PACK 24 V channel through the USER divider
 *              factor (2026-09-25): the pack sense path attenuates
 *              6.8k/69.2k to the pin, NOT the input net's 6.8k/76k.
 *         [FA] کانال باتری‌پک ۲۴ ولت را با ضریب مقسم «کاربر» تبدیل می‌کند
 *              (۲۰۲۶-۰۹-۲۵): مسیر سنس پک تا پایه ‎6.8k/69.2k‎ تضعیف دارد،
 *              نه ‎6.8k/76k‎ مثل نت ورودی.
 * @param  uint16_t__counts [EN] Normalized ADC count / شمارش استاندارد ADC
 * @return uint32_t [EN] Pack voltage in mV / ولتاژ پک mV
 */
uint32_t func__Measurement_Battery24CountsToMv(uint16_t uint16_t__counts)
{
    return func__BspMeasurement_Battery24CountsToMv(uint16_t__counts);
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

/* ==================== Measurement Current1 Counts To Ma (LUT, user order 2026-09-27) ==================== */

#if (CAL_CURRENT1_LUT_ENABLE != 0u)
/* [EN] Live battery-1 terminal voltage cache for the ch1 power LUT (v1.19,
 *      user order 2026-09-27): vhigh = V24 - V12, written AFTER the median-5
 *      voltage filter each pass, read by func__Measurement_Current1CountsToMa
 *      one pass later (1 ms stale - negligible vs the battery time
 *      constant). Clamped to 8.0..15.0 V so a missing/garbage voltage can
 *      never blow up the division; boot default 12.0 V.
 * [FA] کش ولتاژ زندهٔ ترمینال باتری ۱ برای LUT توانیِ کانال ۱ (v1.19):
 *      ‎vhigh = V24‎ − V12، بعد از فیلتر مدین-۵ هر پاس نوشته می‌شود و یک پاس
 *      بعدتر خوانده می‌شود (۱ms کهنگی - ناچیز مقابل ثابت زمانی باتری).
 *      گیرهٔ ۸..۱۵V تا ولتاژ گم/خراب تقسیم را منفجر نکند؛ پیش‌فرض بوت
 *      ۱۲٫۰V. */
static uint32_t UINT32_T__G__Battery1VoltageMv = 12000u;

/* [EN] The tail slope indexes POINTS-1/POINTS-2: fail the build if the
   table ever shrinks below 2 points.
   Full-program audit 2026-10-05: the two asserts below used to sit AFTER
   the #endif of this guard, so they referenced CAL_CURRENT1_LUT_POINTS and
   the ch1 axes even when CAL_CURRENT1_LUT_ENABLE was 0u - which broke the
   build. They now live inside the guard, exactly like the channel-2 pair.
   [FA] شیب دنباله ‎POINTS-1/POINTS-2‎ را می‌خواند: اگر جدول روزی زیر ۲ نقطه
   رفت، بیلد بشکند. ممیزی ۲۰۲۶-۱۰-۰۵: این دو assert قبلاً بعد از endif این
   گارد بودند و با خاموش‌بودن کلید کانال ۱ بیلد را می‌شکستند؛ حالا مثل جفتِ
   کانال ۲ داخل گاردند. */
_Static_assert(CAL_CURRENT1_LUT_POINTS >= 2u, "ch1 LUT needs >= 2 points");
/* [EN] v1.63 (user question: "can the table have more or fewer points -
   does it break anything?"). The point COUNT is free: every loop here is
   driven by CAL_CURRENT1_LUT_POINTS, which is sizeof-derived. The one way
   a re-fitted table can still break the build silently is pasting two axes
   of DIFFERENT length - the interpolation would then read past the end of
   the shorter one. That is now a compile error instead of a field fault.
   [FA] تعداد نقاط آزاد است چون همهٔ حلقه‌ها از روی sizeof حساب می‌شوند.
   تنها خطای خاموش ممکن این بود که دو محور با طول متفاوت کپی شوند؛ حالا
   خطای زمان کامپایل است، نه خرابی در میدان. */
_Static_assert(sizeof(CAL_Current1LutChainMa) ==
               sizeof(CAL_Current1LutBatteryMw),
               "ch1 LUT axes must hold the same number of points");
#endif

/* ==================== Measurement bench LUT interpolation (v1.66) ==================== */

#if ((CAL_CURRENT1_LUT_ENABLE != 0u) || (CAL_CURRENT2_LUT_ENABLE != 0u))
/* [EN] Exact (a*b)/div with saturation and NEAREST-ROUNDING, no compiler u64
   helper. The product is accumulated in two u32 words and divided with
   restoring binary division; the final remainder decides the last unit.
   This keeps the firmware flash diet while preventing the old u32 wrap in
   both interpolation and power-to-current conversion, and keeps those two
   conversions on the same rounding rule as the panel that fitted them.
   [FA] محاسبهٔ دقیق و اشباع‌شوندهٔ ‎(a*b)/div‎ با رُند به نزدیک و بدون
   helper شانزده‌بیتی کامپایلر: حاصل‌ضرب در دو کلمهٔ u32 جمع و با تقسیم دودویی
   restoring تقسیم می‌شود و باقی‌ماندهٔ پایانی رقم آخر را تعیین می‌کند. رژیم
   فلش حفظ می‌شود، wrap قدیمی در interpolation و توان‌به‌جریان از بین می‌رود و
   این دو تبدیل همان قاعدهٔ رُندِ پنلی را دارند که آنها را برازش کرده است. */
static uint32_t func__Measurement_MulDivU32Saturating(uint32_t uint32_t__a,
                                                      uint32_t uint32_t__b,
                                                      uint32_t uint32_t__divisor)
{
    uint32_t uint32_t__productLow = 0u;
    uint32_t uint32_t__productHigh = 0u;
    uint32_t uint32_t__addLow = uint32_t__a;
    uint32_t uint32_t__addHigh = 0u;
    uint32_t uint32_t__multiplier = uint32_t__b;
    uint32_t uint32_t__remainder = 0u;
    uint32_t uint32_t__quotient = 0u;
    uint8_t uint8_t__bit;

    if ((uint32_t__divisor == 0u) || (uint32_t__a == 0u) ||
        (uint32_t__b == 0u))
    {
        return 0u;
    }

    /* Shift-and-add multiplication into a 64-bit value represented as
       high/low u32 words. The mathematical product of two u32 values fits. */
    for (uint8_t__bit = 0u; uint8_t__bit < 32u; uint8_t__bit++)
    {
        if ((uint32_t__multiplier & 1u) != 0u)
        {
            uint32_t uint32_t__oldLow = uint32_t__productLow;
            uint32_t__productLow += uint32_t__addLow;
            uint32_t__productHigh += uint32_t__addHigh;
            if (uint32_t__productLow < uint32_t__oldLow)
            {
                uint32_t__productHigh++;
            }
        }
        uint32_t__addHigh = (uint32_t)((uint32_t__addHigh << 1) |
                                       (uint32_t__addLow >> 31));
        uint32_t__addLow <<= 1;
        uint32_t__multiplier >>= 1;
    }

    /* Divide the 64-bit product from MSB to LSB. A quotient bit above bit 31
       proves the public u32 result must saturate. */
    for (uint8_t__bit = 64u; uint8_t__bit > 0u; uint8_t__bit--)
    {
        uint8_t uint8_t__productBitIndex = (uint8_t)(uint8_t__bit - 1u);
        uint32_t uint32_t__productBit =
            (uint8_t__productBitIndex >= 32u)
                ? ((uint32_t__productHigh >>
                    (uint8_t__productBitIndex - 32u)) & 1u)
                : ((uint32_t__productLow >> uint8_t__productBitIndex) & 1u);
        uint32_t uint32_t__complement = uint32_t__divisor -
                                         uint32_t__remainder;
        bool bool__subtract = false;

        /* Compare 2*remainder+bit with divisor without overflowing u32. */
        if (uint32_t__remainder >= uint32_t__complement)
        {
            uint32_t__remainder -= uint32_t__complement;
            if (uint32_t__productBit != 0u)
            {
                uint32_t__remainder++;
            }
            bool__subtract = true;
        }
        else if ((uint32_t__productBit != 0u) &&
                 (uint32_t__remainder == (uint32_t__complement - 1u)))
        {
            uint32_t__remainder = 0u;
            bool__subtract = true;
        }
        else
        {
            uint32_t__remainder = (uint32_t)((uint32_t__remainder << 1) |
                                              uint32_t__productBit);
        }

        if (bool__subtract != false)
        {
            if (uint8_t__productBitIndex >= 32u)
            {
                return UINT32_MAX;
            }
            uint32_t__quotient = (uint32_t)((uint32_t__quotient << 1) | 1u);
        }
        else if (uint8_t__productBitIndex < 32u)
        {
            uint32_t__quotient <<= 1;
        }
    }

    /* [EN] NEAREST-ROUNDING (user order 2026-10-08): every caller of this
       helper converts a MEASURED quantity - a bench LUT interpolation or a
       battery power divided by the live battery voltage - and the panel, the
       bench anchors and the BSP current chain all round to the nearest
       integer. Truncating here instead is a one-sided UNDER-read of the
       battery current that the charger's limit and the taper decision then
       inherit (and under-reading a charge current is the unsafe direction).
       The exact remainder is already sitting in uint32_t__remainder, so the
       decision is ONE comparison and never forms 2x remainder (the
       subtraction cannot underflow: remainder < divisor always holds after
       restoring division, and divisor == 0 returned above).
       [FA] رُند به نزدیک (دستور کاربر ۲۰۲۶-۱۰-۰۸): هر صداکنندهٔ این helper
       یک کمیت اندازه‌گیری‌شده را تبدیل می‌کند - درون‌یابی جدول بنچ یا تقسیم
       توان باتری بر ولتاژ زندهٔ باتری - و پنل، لنگرهای بنچ و زنجیرهٔ جریان
       BSP همگی به نزدیک‌ترین عدد صحیح رُند می‌کنند. برش در اینجا کم‌خوانی
       یک‌طرفهٔ جریان باتری است که حد جریان و تشخیص شیبِ شارژر آن را ارث
       می‌برند (و کم‌خوانی جریان شارژ جهت ناامن است). باقی‌ماندهٔ دقیق همین
       حالا در uint32_t__remainder هست، پس تصمیم فقط یک مقایسه است و هرگز
       ‎۲×r‎ ساخته نمی‌شود (تفریق سرریز نمی‌کند: بعد از تقسیم restoring
       همیشه ‎r < divisor‎ و divisor صفر بالاتر برگشته است). */
    if ((uint32_t__remainder != 0u) &&
        (uint32_t__remainder >= (uint32_t__divisor - uint32_t__remainder)))
    {
        if (uint32_t__quotient == UINT32_MAX)
        {
            return UINT32_MAX;
        }
        uint32_t__quotient++;
    }

    return uint32_t__quotient;
}

static uint32_t func__Measurement_InterpU32Increasing(uint32_t uint32_t__yLow,
                                                      uint32_t uint32_t__deltaX,
                                                      uint32_t uint32_t__deltaY,
                                                      uint32_t uint32_t__spanX)
{
    uint32_t uint32_t__deltaYScaled;
    uint32_t uint32_t__result;

    if (uint32_t__spanX == 0u)
    {
        return uint32_t__yLow;
    }
    uint32_t__deltaYScaled = func__Measurement_MulDivU32Saturating(
        uint32_t__deltaX, uint32_t__deltaY, uint32_t__spanX);
    uint32_t__result = uint32_t__yLow + uint32_t__deltaYScaled;
    if (uint32_t__result < uint32_t__yLow)
    {
        return UINT32_MAX;
    }
    return uint32_t__result;
}

static uint32_t func__Measurement_PowerMwToMa(uint32_t uint32_t__powerMw,
                                               uint32_t uint32_t__voltageMv)
{
    return func__Measurement_MulDivU32Saturating(uint32_t__powerMw,
                                                  1000u,
                                                  uint32_t__voltageMv);
}

#ifdef MEASUREMENT_HOST_TEST
uint32_t func__Measurement_HostTest_MulDivU32(uint32_t uint32_t__a,
                                              uint32_t uint32_t__b,
                                              uint32_t uint32_t__divisor)
{
    return func__Measurement_MulDivU32Saturating(uint32_t__a,
                                                  uint32_t__b,
                                                  uint32_t__divisor);
}
#endif

/**
 * @brief  [EN] Piecewise-linear interpolation over ONE bench table: chain
 *              mA -> battery POWER mW. Both channels and BOTH sources (the
 *              compile-time table in calibration.h and the flash table a
 *              panel push stored, v1.66) go through this one function, so
 *              a pushed table can never behave differently from a pasted
 *              one - the only thing that changes is which array is read.
 *              Linear between anchors, the last slope extends above the
 *              last anchor, and a degenerate (equal-x) segment returns the
 *              point value instead of dividing by zero.
 *         [FA] درون‌یابی خطی-تکه‌ای روی یک جدول بنچ: mA زنجیره ← توان
 *              باتری. هر دو کانال و «هر دو منبع» (جدول کامپایل‌تایم در
 *              calibration.h و جدول فلش که ارسال پنل ذخیره کرده - v1.66)
 *              از همین یک تابع می‌گذرند، پس جدول ارسال‌شده هرگز نمی‌تواند
 *              رفتار متفاوتی از جدول چسبانده‌شده داشته باشد؛ تنها تفاوت
 *              این است که کدام آرایه خوانده می‌شود.
 * @param  UINT32_T__A__ChainMa [EN] Chain axis / محور زنجیره
 * @param  UINT32_T__A__PowerMw [EN] Power axis / محور توان
 * @param  uint32_t__points [EN] Point count, >= 2 / تعداد نقاط
 * @param  uint32_t__chainMa [EN] Chain current / جریان زنجیره
 * @return uint32_t [EN] Battery power in mW / توان باتری بر حسب mW
 */
static uint32_t func__Measurement_BenchLutInterp(
    const uint32_t *UINT32_T__A__ChainMa,
    const uint32_t *UINT32_T__A__PowerMw,
    uint32_t uint32_t__points,
    uint32_t uint32_t__chainMa)
{
    uint32_t uint32_t__index;

    /* [EN] Audit guard 2026-10-06: the contract is points >= 2 and every
     *      caller enforces it (CAL_LUT_POINTS_MIN), but the tail branch
     *      below indexes [points - 2]; an unsigned underflow there would
     *      read far outside the table. Refusing a degenerate table keeps
     *      valid inputs bit-identical.
     * [FA] نگهبان ممیزی ۲۰۲۶-۱۰-۰۶: قرارداد «حداقل دو نقطه» است و همهٔ
     *      فراخوان‌ها آن را رعایت می‌کنند، ولی شاخهٔ انتهایی ‎[points - 2]‎ را
     *      می‌خواند و کم‌ریزی بدون‌علامت آنجا بیرون از جدول را می‌خواند.
     *      رد کردن جدول ناقص، خروجی ورودی‌های معتبر را عوض نمی‌کند. */
    if (uint32_t__points < 2u)
    {
        return 0u;
    }

    for (uint32_t__index = 1u; uint32_t__index < uint32_t__points;
         uint32_t__index++)
    {
        uint32_t uint32_t__xHigh = UINT32_T__A__ChainMa[uint32_t__index];

        if (uint32_t__chainMa <= uint32_t__xHigh)
        {
            uint32_t uint32_t__xLow = UINT32_T__A__ChainMa[uint32_t__index - 1u];
            uint32_t uint32_t__yLow = UINT32_T__A__PowerMw[uint32_t__index - 1u];
            uint32_t uint32_t__yHigh = UINT32_T__A__PowerMw[uint32_t__index];

            /* [EN] Degenerate-segment guard (tables are hand-edited and now
               also uploaded): equal anchors would divide by zero.
               [FA] گارد بازهٔ تباه‌شده: لنگرهای برابر تقسیم‌برصفر می‌کردند. */
            if (uint32_t__xHigh == uint32_t__xLow)
            {
                return uint32_t__yHigh;
            }
            return func__Measurement_InterpU32Increasing(
                uint32_t__yLow,
                uint32_t__chainMa - uint32_t__xLow,
                uint32_t__yHigh - uint32_t__yLow,
                uint32_t__xHigh - uint32_t__xLow);
        }
    }

    /* [EN] Above the last anchor: extend the last segment's slope.
       [FA] بالای آخرین لنگر: شیب آخرین بازه ادامه می‌یابد. */
    if (UINT32_T__A__ChainMa[uint32_t__points - 1u] ==
        UINT32_T__A__ChainMa[uint32_t__points - 2u])
    {
        return UINT32_T__A__PowerMw[uint32_t__points - 1u];
    }
    return func__Measurement_InterpU32Increasing(
        UINT32_T__A__PowerMw[uint32_t__points - 1u],
        uint32_t__chainMa - UINT32_T__A__ChainMa[uint32_t__points - 1u],
        UINT32_T__A__PowerMw[uint32_t__points - 1u] -
            UINT32_T__A__PowerMw[uint32_t__points - 2u],
        UINT32_T__A__ChainMa[uint32_t__points - 1u] -
            UINT32_T__A__ChainMa[uint32_t__points - 2u]);
}
#endif

#if (CAL_CURRENT1_LUT_ENABLE != 0u)
/**
 * @brief  [EN] Piecewise-linear bench correction: ADC chain mA of channel
 *              1 -> battery-1 POWER in mW (the DCM energy per cycle is
 *              battery-voltage independent; current = P/Vbat, so the table
 *              carries POWER and the caller divides by the live battery
 *              voltage). Linear interpolation between anchors; the last
 *              slope extends above the last anchor; 0 maps to 0.
 *         [FA] اصلاح خطی-تکه‌ای بنچ: mA زنجیرهٔ ADC کانال ۱ → «توان باتری
 *              ۱» بر حسب mW (انرژی هر سایکل DCM مستقل از ولتاژ باتری است؛
 *              جریان = ‎P/Vbat‎، پس جدول توان را می‌دهد و صداکننده بر ولتاژ
 *              زنده تقسیم می‌کند). بین لنگرها درون‌یابی خطی؛ بالای آخرین
 *              لنگر شیب آخر ادامه می‌یابد؛ صفر به صفر.
 * @param  uint32_t__chainMa [EN] ADC chain output in mA / خروجی زنجیرهٔ ADC بر حسب mA
 * @return uint32_t [EN] Battery-1 power in mW‎ / توان باتری ۱ بر حسب mW
 */
static uint32_t func__Measurement_Current1BenchLut(uint32_t uint32_t__chainMa)
{
    /* [EN] v1.66 (user order 2026-10-05): a table pushed from the panel and
       stored in its own flash block WINS over the compiled table; a fresh
       board, a corrupt record or a format bump falls straight back to
       calibration.h, so the board always measures with something sane.
       [FA] v1.66 (دستور کاربر): جدولی که از پنل فرستاده و در بلوک فلش خودش
       ذخیره شده بر جدول کامپایل‌شده مقدم است؛ برد نو، رکورد خراب یا تغییر
       قالب مستقیماً به calibration.h برمی‌گردد تا برد همیشه با چیزی سالم
       اندازه بگیرد. */
    if (func__CalLut_Active((uint8_t)CAL_LUT_CHANNEL_1) != false)
    {
        return func__Measurement_BenchLutInterp(
            func__CalLut_ChainMa((uint8_t)CAL_LUT_CHANNEL_1),
            func__CalLut_PowerMw((uint8_t)CAL_LUT_CHANNEL_1),
            func__CalLut_Points((uint8_t)CAL_LUT_CHANNEL_1),
            uint32_t__chainMa);
    }

    return func__Measurement_BenchLutInterp(CAL_Current1LutChainMa,
                                            CAL_Current1LutBatteryMw,
                                            CAL_CURRENT1_LUT_POINTS,
                                            uint32_t__chainMa);
}
#endif

/* ==================== Measurement Current1 Counts To Ma ==================== */

/**
 * @brief  [EN] Channel-1 raw counts to battery mA: the BSP per-channel
 *              linear calibration, then - when the bench LUT is enabled -
 *              the piecewise-linear bench correction (the linear chain
 *              alone reads an S-curve vs the DMM). The LUT sits on the OLD
 *              chain output, so unfiltered, filtered and iest all become
 *              true battery mA; raw counts and shunt uV are untouched.
 *         [FA] شمارش خام کانال ۱ به mA باتری: کالیبراسیون خطی BSP و بعد -
 *              با فعال بودن جدول بنچ - اصلاح خطی-تکه‌ای (زنجیرهٔ خطیِ تنها،
 *              Sشکل برابر DMM می‌خواند). جدول روی خروجی زنجیرهٔ قدیم
 *              می‌نشیند پس بدون فیلتر، فیلترشده و iest هر سه به mA واقعی
 *              باتری تبدیل می‌شوند؛ شمارش خام و uV شانت دست نمی‌خورند.
 * @param  uint16_t__counts [EN] ADC count / شمارش ADC
 * @return uint32_t [EN] Corrected current in mA / جریان اصلاح‌شده mA
 */
uint32_t func__Measurement_Current1CountsToMa(uint16_t uint16_t__counts)
{
#if (CAL_CURRENT1_LUT_ENABLE != 0u)
    /* [EN] The LUT maps the ADC chain current to battery-1 POWER (the DCM
       invariant); dividing by the LIVE battery-1 terminal voltage (vhigh,
       previous 1 ms pass, clamped 8.0..15.0 V) yields the CURRENT. The
       multiply/divide uses exact split-word u32 arithmetic because panel LUT
       data is runtime; the public result saturates to u32.
       [FA] جدول جریان زنجیرهٔ ADC را به «توان باتری ۱» می‌برد (ناوردای
       DCM)؛ تقسیم بر ولتاژ زندهٔ ترمینال باتری ۱ (vhigh، پاس ۱ms قبل،
       گیرهٔ ۸..۱۵V) جریان باتری را می‌دهد. ضرب/تقسیم با arithmetic دقیق
       ‎split-word‎ انجام می‌شود چون LUT پنل دادهٔ زمان اجراست و خروجی عمومی
       به u32 اشباع می‌شود. */
    uint32_t uint32_t__batteryPowerMw = func__Measurement_Current1BenchLut(
        func__BspMeasurement_Current1CountsToMa(uint16_t__counts));
    return func__Measurement_PowerMwToMa(uint32_t__batteryPowerMw,
                                          UINT32_T__G__Battery1VoltageMv);
#else
    return func__BspMeasurement_Current1CountsToMa(uint16_t__counts);
#endif
}

/* ==================== Measurement Current2 Bench LUT (user order 2026-09-25) ==================== */

/* [EN] Bench runs proved the channel-2 chain is strongly non-linear vs
        the true battery current: ~2x too high at 5% duty, 0.85x too low at
        15..17% - no single gain or gain+offset line covers both ends, so a
        piecewise-linear table on the chain output is the honest correction.
        Physical cause: the mid-ON synchronized sample of the primary ramp
        vs the ~duty^2 energy transfer, with a DCM->CCM kink near 12% duty.
   [FA] اجرای بنچ نشان داد زنجیرهٔ کانال ۲ نسبت به جریان واقعی باتری
        به‌شدت غیرخطی است: ~۲ برابر زیاد در دیوتی ۵٪ و ۰٫۸۵ برابر کم در
        ۱۵..۱۷٪ - هیچ خط تک‌گین/گین+آفستی دو سر را نمی‌پوشاند، پس جدول
        خطی-تکه‌ای روی خروجی زنجیره اصلاح درست است. علت فیزیکی: نمونهٔ
        سنکرون وسط ON از رمپ اولیه در برابر انتقال انرژی ~duty²، با شکست
        DCM→CCM نزدیک دیوتی ~۱۲٪. */

#if (CAL_CURRENT2_LUT_ENABLE != 0u)
/**
 * @brief  [EN] Piecewise-linear bench correction: ADC chain mA of channel
 *              2 -> battery-2 POWER in mW (same DCM POWER architecture as
 *              channel 1; the caller divides by the live battery voltage).
 *              Linear interpolation between anchors; the last slope
 *              extends above the last anchor; 0 maps to 0.
 *         [FA] اصلاح خطی-تکه‌ای بنچ: mA زنجیرهٔ ADC کانال ۲ → «توان باتری
 *              ۲» بر حسب mW (همان معماری توان کانال ۱؛ صداکننده بر ولتاژ
 *              زنده تقسیم می‌کند). بین لنگرها درون‌یابی خطی؛ بالای آخرین
 *              لنگر شیب آخر ادامه می‌یابد؛ صفر به صفر.
 * @param  uint32_t__chainMa [EN] ADC chain output in mA / خروجی زنجیرهٔ ADC بر حسب mA
 * @return uint32_t [EN] Battery-2 power in mW‎ / توان باتری ۲ بر حسب mW
 */
/* [EN] Live battery-2 terminal voltage cache for the ch2 power LUT (v1.13,
 *      user order 2026-09-25): written AFTER the median-5 voltage filter each
 *      pass, read by func__Measurement_Current2CountsToMa one pass later
 *      (1 ms stale - negligible vs the battery time constant). Clamped to
 *      8.0..15.0 V so a missing/garbage voltage can never blow up the
 *      division; boot default 12.0 V.
 * [FA] کش ولتاژ زندهٔ ترمینال باتری ۲ برای LUT توانیِ کانال ۲ (v1.13):
 *      بعد از فیلتر مدین-۵ هر پاس نوشته می‌شود و یک پاس بعدتر خوانده
 *      می‌شود (۱ms کهنگی - ناچیز مقابل ثابت زمانی باتری). گیرهٔ ۸..۱۵V
 *      تا ولتاژ گم/خراب تقسیم را منفجر نکند؛ پیش‌فرض بوت ۱۲٫۰V. */
#if (CAL_CURRENT2_LUT_ENABLE != 0u)
static uint32_t UINT32_T__G__Battery2VoltageMv = 12000u;
#endif

/* [EN] The tail slope indexes POINTS-1/POINTS-2: fail the build if the
   table ever shrinks below 2 points.
   [FA] شیب دنباله ‎POINTS-1/POINTS-2‎ را می‌خواند: اگر جدول روزی زیر ۲ نقطه
   رفت، بیلد بشکند. */
_Static_assert(CAL_CURRENT2_LUT_POINTS >= 2u, "ch2 LUT needs >= 2 points");
/* [EN] v1.63: same length guard for channel 2. [FA] همان گارد برای کانال ۲. */
_Static_assert(sizeof(CAL_Current2LutChainMa) ==
               sizeof(CAL_Current2LutBatteryMw),
               "ch2 LUT axes must hold the same number of points");

static uint32_t func__Measurement_Current2BenchLut(uint32_t uint32_t__chainMa)
{
    /* [EN] v1.66 (user order 2026-10-05): a table pushed from the panel and
       stored in its own flash block WINS over the compiled table; a fresh
       board, a corrupt record or a format bump falls straight back to
       calibration.h, so the board always measures with something sane.
       [FA] v1.66 (دستور کاربر): جدولی که از پنل فرستاده و در بلوک فلش خودش
       ذخیره شده بر جدول کامپایل‌شده مقدم است؛ برد نو، رکورد خراب یا تغییر
       قالب مستقیماً به calibration.h برمی‌گردد تا برد همیشه با چیزی سالم
       اندازه بگیرد. */
    if (func__CalLut_Active((uint8_t)CAL_LUT_CHANNEL_2) != false)
    {
        return func__Measurement_BenchLutInterp(
            func__CalLut_ChainMa((uint8_t)CAL_LUT_CHANNEL_2),
            func__CalLut_PowerMw((uint8_t)CAL_LUT_CHANNEL_2),
            func__CalLut_Points((uint8_t)CAL_LUT_CHANNEL_2),
            uint32_t__chainMa);
    }

    return func__Measurement_BenchLutInterp(CAL_Current2LutChainMa,
                                            CAL_Current2LutBatteryMw,
                                            CAL_CURRENT2_LUT_POINTS,
                                            uint32_t__chainMa);
}
#endif

/**
 * @brief  [EN] Channel-2 raw counts to battery mA: the BSP per-channel
 *              linear calibration, then - when the bench LUT is enabled -
 *              the piecewise-linear bench correction. The LUT sits on the
 *              OLD chain output, so unfiltered, filtered and iest all
 *              become true battery mA; raw counts and shunt uV untouched.
 *         [FA] شمارش خام کانال ۲ به mA باتری: کالیبراسیون خطی BSP و بعد -
 *              با فعال بودن جدول بنچ - اصلاح خطی-تکه‌ای. جدول روی خروجی
 *              زنجیرهٔ قدیم می‌نشیند، پس بدون فیلتر، فیلترشده و iest هر سه
 *              به mA واقعی باتری تبدیل می‌شوند؛ شمارش خام و uV شانت
 *              دست نمی‌خورند.
 * @param  uint16_t__counts [EN] ADC count / شمارش ADC
 * @return uint32_t [EN] Corrected current in mA / جریان اصلاح‌شده mA
 */
uint32_t func__Measurement_Current2CountsToMa(uint16_t uint16_t__counts)
{
#if (CAL_CURRENT2_LUT_ENABLE != 0u)
    /* [EN] The LUT maps the ADC chain current to battery-2 POWER (the DCM
       invariant); dividing by the LIVE battery-2 terminal voltage (previous
       1 ms pass, clamped 8.0..15.0 V) yields the battery CURRENT. The exact
       split-word u32 helper prevents runtime-table multiplication from
       wrapping and saturates only when the public result cannot fit.
       [FA] جدول جریان زنجیرهٔ ADC را به «توان باتری ۲» می‌برد (ناوردای
       DCM)؛ تقسیم بر ولتاژ زندهٔ ترمینال باتری ۲ (پاس ۱ms قبل، گیرهٔ
       ۸..۱۵V) جریان باتری را می‌دهد. helper دقیق ‎split-word‎ از wrap ضرب
       جدول زمان اجرا جلوگیری می‌کند و فقط در خروجی خارج از u32 اشباع می‌شود. */
    uint32_t uint32_t__batteryPowerMw = func__Measurement_Current2BenchLut(
        func__BspMeasurement_Current2CountsToMa(uint16_t__counts));
    /* [EN] Runtime LUT values are not limited to the compiled table's small
       range; the split-word conversion helper prevents wrap and saturates.
       [FA] مقادیر LUT زمان اجرا به بازهٔ کوچک جدول کامپایل محدود نیستند؛
       کمک‌کنندهٔ ‎split-word‎ از wrap جلوگیری و اشباع می‌کند. */
    return func__Measurement_PowerMwToMa(uint32_t__batteryPowerMw,
                                          UINT32_T__G__Battery2VoltageMv);
#else
    return func__BspMeasurement_Current2CountsToMa(uint16_t__counts);
#endif
}

bool func__Measurement_CurrentIsBatteryCalibrated(uint8_t uint8_t__channelIndex)
{
    /* [EN] The public CurrentNCountsToMa() result is battery-side whenever
       the corresponding compile-time LUT is enabled. A valid runtime flash
       record overrides the anchors, while an absent/invalid record uses the
       compiled fallback; both are the same battery-power -> live-voltage
       architecture. Keep this query beside the conversion so Charger cannot
       accidentally duplicate that conversion as the LUT evolves.
       [FA] خروجی عمومی CurrentNCountsToMa وقتی LUT متناظر فعال است متعلق به
       سمت باتری است. رکورد معتبر فلش فقط لنگرها را جایگزین می‌کند و در نبود
       رکورد، fallback کامپایل‌شده مصرف می‌شود؛ هر دو همان معماری توان باتری
       تقسیم بر ولتاژ زنده‌اند. این query کنار تبدیل می‌ماند تا Charger با
       تغییر LUT نتواند تبدیل را دوباره انجام دهد. */
    if (uint8_t__channelIndex == 0u)
    {
#if (CAL_CURRENT1_LUT_ENABLE != 0u)
        return true;
#else
        return false;
#endif
    }

    if (uint8_t__channelIndex == 1u)
    {
#if (CAL_CURRENT2_LUT_ENABLE != 0u)
        return true;
#else
        return false;
#endif
    }

    return false;
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

/* ==================== Measurement Battery12 Bench Compensation (user order 2026-09-25) ==================== */

#if (CAL_BATTERY12_BENCH_COMP_ENABLE != 0u)
/* [EN] Bench DMM runs measured the V12 channel against the battery
        terminals: a static divider error plus a current-proportional
        charge-path wire drop (the board sense point sits above the battery
        terminal while charging). This compensation subtracts both so the
        panel - and the charger's own decisions on Vlow - work on the TRUE
        battery-2 terminal voltage; Vhigh = V24 - V12 shifts up by the same
        amount (the physically correct direction). The I2 input is the
        post-LUT corrected current; re-derive the constants if the bench
        wiring changes.
   [FA] بنچ با مولتی‌متر روی ترمینال باتری نشان داد کانال V12 خطای ثابت
        مقسم + افت مسیر شارژ متناسب جریان دارد (نقطهٔ سنس برد حین شارژ
        بالاتر از ترمینال باتری است). این جبران هر دو را کم می‌کند تا پنل
        و تصمیم‌های شارژر روی Vlow با ولتاژ واقعی ترمینال باتری ۲ کار
        کنند؛ ‎Vhigh = V24‎ − V12 به همان اندازه بالا می‌رود (جهت فیزیکی
        درست). ورودی I2 جریان اصلاح‌شدهٔ بعد از جدول است؛ با تغییر
        سیم‌بندی، ثابت‌ها دوباره ساخته شوند. */
/* [EN] Refit from the dense 2026-09-25T18:14 run (10 DMM points,
        0..764 mA): LSQ static 149.8 mV + 472.5 mOhm - rounded to 150/470.
        Residual vs DMM within +/-28 mV (0.23 percent) across the range.
        (Full-program audit 2026-09-27: the two constants live ONLY in
        calibration.h - the identical local redefinition here is deleted.)
   [FA] برازش دوباره از اجرای متراکم ۲۰۲۶-۰۹-۲۵T18:14 (۱۰ نقطهٔ DMM،
        ‎0..764mA)‎: کمینهٔ مربعات ‎149.8mV + 472.5mOhm‎ - گرد به ‎150/470‎.
        خطای باقی‌مانده در کل بازه ±۲۸mV (۰٫۲۳٪).
        (ممیزی کل برنامه: این دو ثابت فقط در calibration.h هستند -
        تعریف تکراری محلی اینجا حذف شد.) */

/**
 * @brief  [EN] V12 true-battery compensation: subtract the static channel
 *              error and the I2 x R charge-path wire drop, never below 0 mV.
 *         [FA] جبران V12 به باتری واقعی: کم‌کردن خطای ثابت کانال و افت
 *              مسیر I2×R؛ هرگز زیر 0mV نمی‌رود.
 * @param  uint32_t__v12Mv      [EN] Measured V12 in mV / V12‎ اندازه‌گیری‌شده mV
 * @param  uint32_t__current2Ma [EN] Corrected channel-2 current in mA‎ / جریان اصلاح‌شدهٔ کانال ۲ mA
 * @return uint32_t [EN] Compensated battery-low voltage in mV‎ / ولتاژ جبران‌شدهٔ باتری پایین mV
 */
static uint32_t func__Measurement_Battery12BenchCompensate(uint32_t uint32_t__v12Mv,
                                                           uint32_t uint32_t__current2Ma)
{
    uint32_t uint32_t__dropMv = CAL_BATTERY12_BENCH_STATIC_MV +
        ((uint32_t__current2Ma * CAL_BATTERY12_BENCH_PATH_MOHM) / 1000u);

    if (uint32_t__v12Mv > uint32_t__dropMv)
    {
        return uint32_t__v12Mv - uint32_t__dropMv;
    }
    return 0u;
}
#endif

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
    uint16_t uint16_t__battery24CountsFiltered;
    uint16_t uint16_t__battery12CountsFiltered;
    uint32_t uint32_t__current1CountsFiltered;
    uint32_t uint32_t__current2CountsFiltered;
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
           [FA] نبود فریم پایدار ‎warm-up‎ را از نو شروع و نتیجهٔ ADC را
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
        (UINT16_T__G__FilterAverageWindowApplied != UINT16_T__G__FilterAverageWindow))
    {
        func__Measurement_ResetCurrentFilters();
        UINT8_T__G__FilterMedianSizeApplied = UINT8_T__G__FilterMedianSize;
        UINT16_T__G__FilterAverageWindowApplied = UINT16_T__G__FilterAverageWindow;
    }

    /* [EN] Convert into locals first so other tasks never observe a partly
       updated measurement set.
       [FA] ابتدا در متغیرهای محلی تبدیل می‌کند تا تسک‌های دیگر مجموعهٔ
       اندازه‌گیری نیمه‌به‌روزشده نبینند. */
    /* [EN] Currents: the board port delivers the PWM mid-ON synchronized
       raw counts in the CURRENT1/CURRENT2 frame positions; this module
       runs the switchable filter chain on the RAW counts first (median to
       kill single-sample jumps, moving average to smooth) and converts the
       filtered counts to mA after. Voltages keep their median-5 spike
       guard, likewise on the raw counts.
       [FA] جریان‌ها: پورت برد شمارش‌های خام سنکرونِ وسط ON را در جایگاه‌های
       ‎CURRENT1/2‎ فریم می‌گذارد؛ این ماژول اول زنجیرهٔ فیلتر کلیددار را
       روی شمارش خام اجرا می‌کند (مدین برای حذف پرش، میانگین متحرک برای
       صاف‌کردن) و بعد شمارش فیلترشده را به mA تبدیل می‌کند. ولتاژها هم
       محافظ مدین-۵ خود را روی شمارش خام نگه می‌دارند. */

/* [EN] Filters run on the RAW ADC counts (user order 2026-09-27):
       median/average of 12-bit counts cannot exceed 4095, so the u16 casts
       at the conversion calls are airtight. The unfiltered converted
       samples stay for telemetry (unf) and diagnostics.
       [FA] فیلترها روی شمارش خام ADC اجرا می‌شوند (دستور کاربر ۲۰۲۶-۰۹-۲۷):
       مدین/میانگین شمارش ۱۲بیتی از ۴۰۹۵ بیشتر نمی‌شود پس castهای u16 سر
       تبدیل‌ها قطعی‌اند. نمونه‌های تبدیل‌شدهٔ بدون فیلتر برای تله‌متری
       (unf) می‌مانند. */
    uint32_t__current1CountsFiltered =
        func__Measurement_ApplyCurrentFilters(0u,
            (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1]);
    uint32_t__current1SampleMa =
        func__Measurement_Current1CountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1]);
    uint32_t__current1ShuntUv =
        func__Measurement_CurrentCountsToShuntUv(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1]);
    uint32_t__current1Ma =
        func__Measurement_Current1CountsToMa((uint16_t)uint32_t__current1CountsFiltered);
    uint32_t__inputVoltageMv =
        func__Measurement_V24CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_24V_IN]);
    /* [EN] Median-5 on the raw counts of the two battery ADC channels
       (Vin keeps no filter, exactly as before).
       [FA] مدین-۵ روی شمارش خام دو کانال ADC باتری (ورودی مثل قبل بدون
       فیلتر می‌ماند). */
    uint16_t__battery24CountsFiltered =
        (uint16_t)func__Measurement_MedianFilterVoltageSample(0u,
            (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_24V_BAT]);
    uint16_t__battery12CountsFiltered =
        (uint16_t)func__Measurement_MedianFilterVoltageSample(1u,
            (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_12V_BAT]);
    uint32_t__battery24Mv =
        func__Measurement_Battery24CountsToMv(uint16_t__battery24CountsFiltered);
    uint32_t__battery12Mv =
        func__Measurement_V12CountsToMv(uint16_t__battery12CountsFiltered);

    /* [EN] Channel-2 sample conversion moved BEFORE the voltage chain (user
            order 2026-09-25): the V12 bench compensation needs the corrected
            channel-2 current. Pure function of the raw frame - no state, so
            the earlier position changes nothing else.
       [FA] تبدیل نمونهٔ کانال ۲ به قبل از زنجیرهٔ ولتاژ منتقل شد (دستور
            کاربر ۲۰۲۶-۰۹-۲۵): جبران بنچ V12 به جریان اصلاح‌شدهٔ کانال ۲
            نیاز دارد. تابع خالصِ فریم خام است - بدون وضعیت - پس جابه‌جایی
            چیز دیگری را عوض نمی‌کند. */
    uint32_t__current2SampleMa =
        func__Measurement_Current2CountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2]);
    uint32_t__current2CountsFiltered =
        func__Measurement_ApplyCurrentFilters(1u,
            (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2]);
    uint32_t__current2ShuntUv =
        func__Measurement_CurrentCountsToShuntUv(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2]);

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
#if (CAL_BATTERY12_BENCH_COMP_ENABLE != 0u)
    /* [EN] Bench compensation of the battery-low channel (user order
            2026-09-25): static error + I2 wire drop, so Vlow and the derived
            Vhigh describe the true battery terminals.
       [FA] جبران بنچ کانال باتری پایین (دستور کاربر ۲۰۲۶-۰۹-۲۵): خطای
            ثابت + افت مسیر I2 تا Vlow و Vhigh مشتق‌شده، ترمینال واقعی
            باتری‌ها را توصیف کنند. */
    uint32_t__battery12Mv = func__Measurement_Battery12BenchCompensate(
        uint32_t__battery12Mv, uint32_t__current2SampleMa);
#endif
    uint32_t__batteryLowMv = uint32_t__battery12Mv;
    if (uint32_t__battery24Mv >= uint32_t__battery12Mv)
    {
        uint32_t__batteryHighMv = uint32_t__battery24Mv - uint32_t__battery12Mv;
    }
    else
    {
        uint32_t__batteryHighMv = 0u;
    }

    /* [EN] No median here anymore: the median-5 ran on the raw V24/V12
       counts above (user order 2026-09-27), so a single-frame ADC spike on
       the switching node is already dead before the conversion; low/high
       derive from the filtered conversions. Real steps pass with only a
       few frames of lag, no moving average.
       [FA] مدین اینجا دیگر نیست: مدین-۵ روی شمارش خام ‎V24/V12‎ بالا اجرا شد
       پس اسپایک تک‌فریمی پیش از تبدیل مرده است؛ ‎low/high‎ از تبدیل‌های
       فیلترشده مشتق می‌شوند. */

#if (CAL_CURRENT2_LUT_ENABLE != 0u)
    /* [EN] Feed the ch2 power-LUT voltage cache (v1.13): the filtered TRUE
       battery-2 terminal voltage, clamped 8.0..15.0 V.
       [FA] خوراک کشِ ولتاژ LUT توانی کانال ۲: ولتاژ فیلترشدهٔ واقعی
            ترمینال باتری ۲، گیرهٔ ۸٫۰..۱۵٫۰V. */
    if (uint32_t__batteryLowMv < 8000u)
    {
        UINT32_T__G__Battery2VoltageMv = 8000u;
    }
    else if (uint32_t__batteryLowMv > 15000u)
    {
        UINT32_T__G__Battery2VoltageMv = 15000u;
    }
    else
    {
        UINT32_T__G__Battery2VoltageMv = uint32_t__batteryLowMv;
    }
#endif
#if (CAL_CURRENT1_LUT_ENABLE != 0u)
    /* [EN] Feed the ch1 power-LUT voltage cache (v1.19): the filtered TRUE
       battery-1 terminal voltage (vhigh = V24 - V12), clamped 8.0..15.0 V.
       [FA] خوراک کشِ ولتاژ LUT توانی کانال ۱: ولتاژ فیلترشدهٔ واقعی
            ترمینال باتری ۱ (‎vhigh = V24‎ − V12)، گیرهٔ ۸٫۰..۱۵٫۰V. */
    if (uint32_t__batteryHighMv < 8000u)
    {
        UINT32_T__G__Battery1VoltageMv = 8000u;
    }
    else if (uint32_t__batteryHighMv > 15000u)
    {
        UINT32_T__G__Battery1VoltageMv = 15000u;
    }
    else
    {
        UINT32_T__G__Battery1VoltageMv = uint32_t__batteryHighMv;
    }
#endif
    /* [EN] Convert the filtered counts AFTER the LUT voltage cache above
       is refreshed, so the ch2 power correction divides by this pass's
       battery-2 voltage.
       [FA] تبدیل شمارش فیلترشده بعد از تازه‌شدن کش ولتاژ LUT تا تصحیح توان
       کانال ۲ بر ولتاژ همین پاس تقسیم شود. */
    uint32_t__current2Ma =
        func__Measurement_Current2CountsToMa((uint16_t)uint32_t__current2CountsFiltered);

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
       وابسته به MCU از ‎CMSIS-RTOS2‎ استفاده می‌کند. */
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
    UINT32_T__G__MeasVddaMv = func__BspMeasurement_VddaMv(
        uint16_t__raw[BSP_ADC_CHANNEL_VREFINT], CAL_VREFINT_MV);
    UINT32_T__G__MeasVinRawCounts =
        (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_24V_IN];
    /* [EN] Audit fix 2026-10-03: the V24/V12 slots used to publish the
       median+average filtered counts under a "RawCounts" name, so the
       calibration-ground-truth channel lied during sweeps (v1.25 esp_link
       comment: counts BEFORE any processing). Now all three voltage slots
       carry the true DMA frame counts, matching Vin; the filtered copy still
       feeds the mV maths above and the control loops, which are untouched.
       [FA] اصلاح ممیزی ۲۰۲۶-۱۰-۰۳: قبلاً جای‌های ‎V24/V12‎ شمارشِ
       فیلترشدهٔ median+میانگین را با نام «RawCounts» منتشر می‌کردند، پس
       کانال مبنای کالیبراسیون حین سوییپ دروغ می‌گفت. حالا هر سه جای ولتاژ
       شمارش واقعی فریم DMA را می‌دهند (مثل Vin)؛ کپی فیلترشده همچنان به
       ریاضیات mV بالا و حلقه‌های کنترل می‌خورد و دست نخورده است. */
    UINT32_T__G__MeasV24RawCounts =
        (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_24V_BAT];
    UINT32_T__G__MeasV12RawCounts =
        (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_12V_BAT];
    UINT32_T__G__MeasVrefintRawCounts =
        (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_VREFINT];
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
       [FA] اعتبار ADC فقط به شمارندهٔ ‎warm-up‎ وابسته است، نه ولتاژ یا حضور
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
       ‎CMSIS-RTOS2‎ این بخش را از دستورهای هستهٔ MCU مستقل نگه می‌دارد. */
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
 *              (one size parameter per filter; 1 = bypass, so no separate
 *              on/off switch). ANY size 1..MAX is valid (even sizes too,
 *              no odd rounding; 1..2 behave as bypass). The compiled
 *              switch stays the capability gate: 0 clamps the request to
 *              1. Flash-persisted (NVM id 7).
 *         [FA] اندازهٔ پنجرهٔ مدین فیلتر جریان در زمان اجرا (یک پارامتر
 *              اندازه؛ ۱ = عبور مستقیم). هر اندازهٔ ۱..MAX مجاز است (زوج
 *              هم، بدون گرد به فرد؛ ۱..۲ عبور مستقیم‌اند). کلید کامپایل
 *              ظرفیت را تعیین می‌کند: ۰ یعنی درخواست به ۱ گیره می‌شود. روی
 *              فلش می‌ماند (شناسهٔ NVM ۷).
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
 * @param  uint32_t__windowSamples [EN] Requested window / پنجرهٔ درخواستی
 * @return uint16_t [EN] Applied window / پنجرهٔ اعمال‌شده
 */
uint16_t func__Measurement_SetFilterAverageWindow(uint32_t uint32_t__windowSamples)
{
    uint16_t uint16_t__appliedWindow;

    /* [EN] Clamp-then-narrow (full-program audit 2026-09-26): the old u8
       path sliced 256..300 to 0..44 AND clamped the ceiling to (u8)300 =
       44, so windows above 44 were unreachable. The wire/u32 value is
       clamped first, then narrowed - the storage/index/counters are u16.
       [FA] اول گیره بعد باریک‌کردن (ممیزی کل برنامه): مسیر u8 قدیم
       ۲۵۶..۳۰۰ را به ۰..۴۴ می‌برید و سقف را هم روی (u8)300=۴۴ می‌گذاشت،
       پس پنجره‌های بالای ۴۴ دست‌نیافتنی بودند. مقدار u32 اول گیره
       می‌خورد بعد باریک می‌شود. */
    if (uint32_t__windowSamples < 1u)
    {
        uint16_t__appliedWindow = 1u;
    }
    else if (uint32_t__windowSamples > (uint32_t)MEASUREMENT_CURRENT_AVERAGE_WINDOW)
    {
        uint16_t__appliedWindow = (uint16_t)MEASUREMENT_CURRENT_AVERAGE_WINDOW;
    }
    else
    {
        uint16_t__appliedWindow = (uint16_t)uint32_t__windowSamples;
    }

    UINT16_T__G__FilterAverageWindow = uint16_t__appliedWindow;
    return uint16_t__appliedWindow;
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
 * @return uint16_t [EN] Window in samples / پنجره بر حسب نمونه
 */
uint16_t func__Measurement_GetFilterAverageWindow(void)
{
    return UINT16_T__G__FilterAverageWindow;
}

/**
 * @brief  [EN] Set one runtime voltage calibration offset, clamped to
 *              +/-MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV. Index 0 = 24 V
 *              input, 1 = 24 V battery pack, 2 = 12 V (middle node)
 *              battery. Default 0 = today's behavior; flash-persisted
 *              since v1.14 (NVM ids 4/5/6).
 *         [FA] یک آفست کالیبراسیون ولتاژ زمان اجرا، گیره در
 *              ±MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV. اندیس ۰ = ورودی ۲۴V،
 *              ۱ = باتری ۲۴V، ۲ = باتری ۱۲V (نود میانی). پیش‌فرض ۰ همان
 *              رفتار فعلی؛ روی فلش می‌ماند (NVM نسخهٔ ۱.۱۴).
 * @param  uint8_t__channelIndex [EN] 0 = VIN, 1 = V24, 2 = V12‎ / اندیس
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
 * @param  uint8_t__channelIndex [EN] 0 = VIN, 1 = V24, 2 = V12‎ / اندیس
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
