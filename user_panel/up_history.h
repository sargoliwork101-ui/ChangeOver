/**
 * @file    up_history.h
 * @brief   [EN] The part that turns a stream of telemetry into a story: charge
 *              sessions with real durations, input cuts, battery runs, fault
 *              appearances and clears, board restarts - plus the day and
 *              lifetime counters those events feed. Everything here reads the
 *              board; nothing here writes to it.
 *          [FA] بخشی که جریان تلمتری را به یک روایت تبدیل می‌کند: شارژهایی با
 *              مدت واقعی، قطع ورودی، کار روی باتری، آمدن و رفتن خطاها و
 *              ری‌استارت برد - به‌همراه شمارنده‌های روزانه و کل که همین رویدادها
 *              تغذیه می‌کنند. همهٔ این ماژول از برد می‌خواند؛ هیچ‌جای آن به برد
 *              نمی‌نویسد.
 *
 * @note    [EN] A charge is recorded as ONE row when it ends, carrying the
 *              duration: an hour of charging is a single line a person can
 *              read, not 240 start/end pairs to add up. Sessions shorter than
 *              UP_HIST_MIN_SESSION_S are treated as contact noise and dropped.
 *          [FA] هر شارژ موقع تمام شدن یک ردیف ثبت می‌شود که مدت را همراه دارد:
 *              یک ساعت شارژ یک خط خواناست، نه ۲۴۰ جفت شروع/پایان که باید جمع
 *              زده شوند. شارژهای کوتاه‌تر از UP_HIST_MIN_SESSION_S نویز تماس
 *              شمرده و حذف می‌شوند.
 */

#ifndef UP_HISTORY_H
#define UP_HISTORY_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "up_config.h"
#include "up_state.h"
#include "up_store.h"

/* ==================== Constants / ثابت‌ها ==================== */
#define UP_HIST_TICK_MIN_MS      200u    /* [EN] ignore faster re-entries / [FA] ورودی‌های سریع‌تر نادیده */
#define UP_HIST_TICK_MAX_MS      10000u  /* [EN] cap one accounting step / [FA] سقف هر گام حسابداری */
#define UP_HIST_MIN_SESSION_S    5u      /* [EN] shorter = contact noise / [FA] کوتاه‌تر = نویز تماس */
#define UP_HIST_CHANNELS         2u
#define UP_HIST_PRESSURE_MS      30000u  /* [EN] filesystem check cadence / [FA] بازهٔ بررسی فایل‌سیستم */

/* [EN] Energy accumulator unit: 0.0001 Wh. One unit is 0.36 J = 360 mW·ms, so
   the remainder below (mW·ms) converts by dividing by UP_HIST_MWMS_PER_E4.
   [FA] یکای انبار انرژی: ۰٫۰۰۰۱ وات‌ساعت. هر واحد ۰٫۳۶ ژول = ۳۶۰ میلی‌وات‌میلی‌ثانیه،
   پس باقی‌ماندهٔ زیر (mW·ms) با تقسیم بر UP_HIST_MWMS_PER_E4 تبدیل می‌شود. */
#define UP_HIST_MWMS_PER_E4      360000u

/* ==================== Types / تایپ‌ها ==================== */
typedef struct
{
    bool     bool__open;
    uint8_t  uint8_t__channel;         /* [EN] 1 or 2 / [FA] ۱ یا ۲ */
    uint32_t uint32_t__startAgeS;
    uint32_t uint32_t__startEpoch;
    uint32_t uint32_t__chargeMs;       /* [EN] time inside a charging state / [FA] زمان داخل حالت شارژ */
    uint32_t uint32_t__energyE4;       /* [EN] 0.0001 Wh units / [FA] یکای ۰٫۰۰۰۱ وات‌ساعت */
    uint32_t uint32_t__energyRemainder;
} up_charge_session_t;

/* ==================== State / وضعیت ==================== */
static up_charge_session_t UP_CHARGE_T__A__Session[UP_HIST_CHANNELS];
static uint8_t  UINT8_T__G__PrevState[UP_HIST_CHANNELS] = {0xFFu, 0xFFu};
static uint8_t  UINT8_T__G__PrevFlags = 0u;
static uint16_t UINT16_T__G__PrevFaultMask = 0u;
static uint16_t UINT16_T__G__PrevSeq = 0u;
static uint32_t UINT32_T__G__PrevImbEvents = 0u;
static bool     BOOL__G__PrevValid = false;
static uint32_t UINT32_T__G__LastTickMs = 0u;
static uint32_t UINT32_T__G__LastSampleMs = 0u;
static uint32_t UINT32_T__G__LastPressureMs = 0u;
static bool     BOOL__G__ForceSample = false;
static uint32_t UINT32_T__G__RunMsRemainder = 0u;
static uint32_t UINT32_T__G__RunCurrentMs = 0u;
static uint32_t UINT32_T__G__OutageStartAgeS = 0u;

/* ==================== Monotonic time / زمان یکنوا ==================== */
/**
 * @brief  [EN] Seconds since the panel's FIRST boot: the persisted base plus
 *              the current uptime. Records are stamped with this instead of the
 *              browser clock, so the history stays ordered and continuous even
 *              across reboots and even when no browser ever set the clock.
 *         [FA] ثانیه از اولین بوت پنل: مبنای ذخیره‌شده به‌علاوهٔ کارکرد فعلی.
 *              رکوردها با این مهر می‌خورند نه با ساعت مرورگر، تا تاریخچه حتی
 *              بعد از ری‌استارت و حتی وقتی هیچ مرورگری ساعت را نداده، مرتب و
 *              پیوسته بماند.
 * @return [EN] monotonic seconds / [FA] ثانیهٔ یکنوا
 */
static uint32_t func__UpHistory_AbsoluteS(void)
{
    uint32_t uint32_t__baseS = UPPANEL_STATE_T__G__State.uint32_t__clockBaseAtBootS;
    uint32_t uint32_t__totalS = uint32_t__baseS + func__UpState_UptimeS();

    return uint32_t__totalS;
}

/* ==================== Event writer / نویسندهٔ رویداد ==================== */
/**
 * @brief  [EN] Write one closed event into the event ring and arm a forced
 *              sample so the charts carry the step the event describes.
 *         [FA] نوشتن یک رویداد بسته در حلقهٔ رویداد و مسلح‌کردن یک نمونهٔ
 *              اجباری تا نمودارها همان پله‌ای را داشته باشند که رویداد توصیف
 *              می‌کند.
 * @param  uint8_t__code [EN] up_event_code_t / [FA] کد رویداد
 * @param  uint8_t__channel [EN] 1, 2 or 0 for the whole unit / [FA] ۱، ۲ یا ۰ برای کل دستگاه
 * @param  uint8_t__severity [EN] UP_SEV_* / [FA] شدت
 * @param  uint16_t__valueA [EN] first value, meaning by code / [FA] مقدار اول
 * @param  uint32_t__durationS [EN] length in seconds, 0 = instant / [FA] مدت به ثانیه
 * @param  uint16_t__valueB [EN] second value / [FA] مقدار دوم
 * @return [EN] true when written / [FA] در صورت نوشتن true
 */
static bool func__UpHistory_WriteEvent(uint8_t uint8_t__code, uint8_t uint8_t__channel, uint8_t uint8_t__severity,
                                       uint16_t uint16_t__valueA, uint32_t uint32_t__durationS, uint16_t uint16_t__valueB)
{
    up_event_t up_event_t__event;

    memset(&up_event_t__event, 0, sizeof(up_event_t__event));
    up_event_t__event.uint32_t__absS = func__UpHistory_AbsoluteS();
    up_event_t__event.uint32_t__epoch = func__UpState_NowEpochS();
    up_event_t__event.uint32_t__durS = uint32_t__durationS;
    up_event_t__event.uint16_t__valueA = uint16_t__valueA;
    up_event_t__event.uint16_t__valueB = uint16_t__valueB;
    up_event_t__event.uint8_t__code = uint8_t__code;
    up_event_t__event.uint8_t__channel = uint8_t__channel;
    up_event_t__event.uint8_t__severity = uint8_t__severity;
    up_event_t__event.uint8_t__flags = 0u;

    BOOL__G__ForceSample = true;
    return func__UpStore_AppendEvent(&up_event_t__event);
}

/* ==================== Small steps / گام‌های کوچک ==================== */
/**
 * @brief  [EN] Clamp a seconds count into the 16-bit event field: a value
 *              longer than 18 hours is stored as the ceiling, not wrapped to a
 *              small lie.
 *         [FA] کوتاه‌کردن ثانیه‌ها به میدان ۱۶ بیتی رویداد: مقدار بیش از ۱۸
 *              ساعت به سقف بسته می‌شود، نه اینکه به دروغ کوچکی بپیچد.
 * @param  uint32_t__value [EN] seconds / [FA] ثانیه
 * @return [EN] clamped value / [FA] مقدار کوتاه‌شده
 */
static uint16_t func__UpHistory_Clamp16(uint32_t uint32_t__value)
{
    if (uint32_t__value > 0xFFFFu)
    {
        return 0xFFFFu;
    }
    return (uint16_t)uint32_t__value;
}

/**
 * @brief  [EN] Severity of a fault bit (drives the colour of the diagnostics
 *              page; the Persian text lives in the web layer).
 *         [FA] شدت یک بیت خطا (رنگ صفحهٔ دیاگ را تعیین می‌کند؛ متن فارسی در
 *              لایهٔ وب است).
 * @param  uint16_t__bit [EN] single fault bit / [FA] یک بیت خطا
 * @return [EN] UP_SEV_* / [FA] شدت
 */
static uint8_t func__UpHistory_FaultSeverity(uint16_t uint16_t__bit)
{
    if ((uint16_t__bit == UP_FAULT_ADC) || (uint16_t__bit == UP_FAULT_OVERCURRENT_1) || (uint16_t__bit == UP_FAULT_OVERCURRENT_2))
    {
        return UP_SEV_CRIT;
    }

    return UP_SEV_WARN;
}

/**
 * @brief  [EN] Duration histogram bucket for a finished charge.
 *         [FA] سبد هیستوگرام مدت برای یک شارژ تمام‌شده.
 * @param  uint32_t__durationS [EN] seconds / [FA] ثانیه
 * @return [EN] bucket 0..UP_EVENT_HIST_BUCKETS-1 / [FA] سبد
 */
static uint8_t func__UpHistory_HistogramBucket(uint32_t uint32_t__durationS)
{
    static const uint32_t UINT32_T__A__EdgesS[UP_EVENT_HIST_BUCKETS - 1u] =
    {
        60u, 120u, 300u, 600u, 900u, 1200u, 1800u, 2700u,
        3600u, 5400u, 7200u, 10800u, 14400u, 21600u, 28800u
    };

    for (uint8_t uint8_t__i = 0u; uint8_t__i < (UP_EVENT_HIST_BUCKETS - 1u); uint8_t__i++)
    {
        if (uint32_t__durationS < UINT32_T__A__EdgesS[uint8_t__i])
        {
            return uint8_t__i;
        }
    }

    return (uint8_t)(UP_EVENT_HIST_BUCKETS - 1u);
}

/**
 * @brief  [EN] Add one tick of battery energy to a session, in named steps:
 *              current(mA) × voltage(mV) / 1000 = milliwatts; × ms = mW·ms;
 *              then convert into 0.0001 Wh units while keeping the remainder,
 *              so slow charges are not rounded away to zero.
 *         [FA] افزودن یک گام انرژی باتری به نشست، با گام‌های نام‌دار:
 *              جریان(mA) × ولتاژ(mV) / ۱۰۰۰ = میلی‌وات؛ × میلی‌ثانیه = mW·ms؛
 *              بعد تبدیل به یکای ۰٫۰۰۰۱ وات‌ساعت با نگه‌داشتن باقی‌مانده، تا
 *              شارژهای کم‌جریان به صفر گرد نشوند.
 * @param  up_charge_session_t__session [EN] session / [FA] نشست
 * @param  uint32_t__iMa [EN] current, mA / [FA] جریان
 * @param  uint32_t__vMv [EN] voltage, mV / [FA] ولتاژ
 * @param  uint32_t__dtMs [EN] elapsed milliseconds / [FA] میلی‌ثانیهٔ سپری‌شده
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHistory_EnergyAdd(up_charge_session_t *up_charge_session_t__session,
                                      uint32_t uint32_t__iMa, uint32_t uint32_t__vMv, uint32_t uint32_t__dtMs)
{
    uint32_t uint32_t__powerMw;
    uint32_t uint32_t__stepMwms;
    uint32_t uint32_t__total;
    uint32_t uint32_t__units;

    if (uint32_t__dtMs > UP_HIST_TICK_MAX_MS)
    {
        uint32_t__dtMs = UP_HIST_TICK_MAX_MS;
    }

    uint32_t__powerMw = (uint32_t__iMa * uint32_t__vMv) / 1000u;
    uint32_t__stepMwms = uint32_t__powerMw * uint32_t__dtMs;
    uint32_t__total = up_charge_session_t__session->uint32_t__energyRemainder + uint32_t__stepMwms;
    uint32_t__units = uint32_t__total / UP_HIST_MWMS_PER_E4;

    up_charge_session_t__session->uint32_t__energyRemainder = uint32_t__total % UP_HIST_MWMS_PER_E4;
    up_charge_session_t__session->uint32_t__energyE4 += uint32_t__units;
}

/* ==================== Charge sessions / نشست‌های شارژ ==================== */
/**
 * @brief  [EN] Open a charge session for a channel and announce the start.
 *         [FA] باز کردن نشست شارژ یک کانال و اعلام شروع.
 * @param  uint8_t__channel [EN] 1 or 2 / [FA] ۱ یا ۲
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHistory_SessionOpen(uint8_t uint8_t__channel)
{
    up_charge_session_t *up_charge_session_t__session = &UP_CHARGE_T__A__Session[uint8_t__channel - 1u];

    memset(up_charge_session_t__session, 0, sizeof(up_charge_session_t));
    up_charge_session_t__session->bool__open = true;
    up_charge_session_t__session->uint8_t__channel = uint8_t__channel;
    up_charge_session_t__session->uint32_t__startAgeS = func__UpState_UptimeS();
    up_charge_session_t__session->uint32_t__startEpoch = func__UpState_NowEpochS();

    (void)func__UpHistory_WriteEvent(UP_EV_CHARGE_START, uint8_t__channel, UP_SEV_INFO, 0u, 0u, 0u);

    /* [EN] The statistics page answers "at what time of day does it charge?" -
       one bucket per hour, kept in today's row. Counted only when the clock is
       known: without a clock every start would land in hour zero and the heat
       map would invent a midnight rush that never happened. Saturating at 255
       is deliberate - a day with more than 255 charges in one hour is a broken
       sensor, not a number worth wrapping to zero.
       [FA] صفحهٔ آمار به «چه ساعتی از شبانه‌روز شارژ می‌کند؟» جواب می‌دهد - یک
       سبد برای هر ساعت، در ردیف امروز. فقط وقتی ساعت معلوم است شمرده می‌شود:
       بدون ساعت هر شروع در ساعت صفر می‌افتاد و نقشهٔ حرارتی از خودش هجومی
       نیمه‌شبی می‌ساخت که هرگز رخ نداده. اشباع در ۲۵۵ عمدی است - روزی با بیش
       از ۲۵۵ شارژ در یک ساعت یعنی سنسور خراب، نه عددی که ارزش دور زدن به صفر
       داشته باشد. */
    if (func__UpState_NowEpochS() != 0u)
    {
        uint32_t uint32_t__hour = func__UpState_ClockHour();
        up_daily_t *up_daily_t__day = func__UpStore_DailyToday();

        if (uint32_t__hour < UP_DAY_HOURS)
        {
            if (up_daily_t__day->uint8_t__hourCharges[uint32_t__hour] < 255u)
            {
                up_daily_t__day->uint8_t__hourCharges[uint32_t__hour]++;
            }
            func__UpStore_DailyMarkDirty();
        }
    }
}

/**
 * @brief  [EN] Close a charge session: duration, energy, statistics, one row.
 *         [FA] بستن نشست شارژ: مدت، انرژی، آمار، یک ردیف.
 * @param  uint8_t__channel [EN] 1 or 2 / [FA] ۱ یا ۲
 * @param  bool__completed [EN] true when it ended in FLOAT / [FA] اگر با FLOAT تمام شد true
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHistory_SessionClose(uint8_t uint8_t__channel, bool bool__completed)
{
    up_charge_session_t *up_charge_session_t__session = &UP_CHARGE_T__A__Session[uint8_t__channel - 1u];
    up_totals_t *up_totals_t__totals = func__UpStore_Totals();
    up_daily_t *up_daily_t__day = func__UpStore_DailyToday();
    uint32_t uint32_t__durationS;
    uint32_t uint32_t__energyWh100;
    uint8_t  uint8_t__bucket;

    if (!up_charge_session_t__session->bool__open)
    {
        return;
    }

    uint32_t__durationS = up_charge_session_t__session->uint32_t__chargeMs / 1000u;
    if (uint32_t__durationS == 0u)
    {
        uint32_t__durationS = 1u;
    }
    uint32_t__energyWh100 = up_charge_session_t__session->uint32_t__energyE4 / 100u;
    uint8_t__bucket = func__UpHistory_HistogramBucket(uint32_t__durationS);

    up_charge_session_t__session->bool__open = false;

    if (uint32_t__durationS < UP_HIST_MIN_SESSION_S)
    {
        return;   /* [EN] contact noise, not a charge / [FA] نویز تماس، نه شارژ */
    }

    if (bool__completed)
    {
        (void)func__UpHistory_WriteEvent(UP_EV_CHARGE_DONE, uint8_t__channel, UP_SEV_INFO,
                                         func__UpHistory_Clamp16(uint32_t__durationS), uint32_t__durationS,
                                         func__UpHistory_Clamp16(uint32_t__energyWh100));
        up_totals_t__totals->uint32_t__charges++;
        up_daily_t__day->uint16_t__charges++;
        up_totals_t__totals->uint16_t__hist[uint8_t__bucket]++;
    }
    else
    {
        (void)func__UpHistory_WriteEvent(UP_EV_CHARGE_INCOMPLETE, uint8_t__channel, UP_SEV_WARN,
                                         func__UpHistory_Clamp16(uint32_t__durationS), uint32_t__durationS,
                                         func__UpHistory_Clamp16(uint32_t__energyWh100));
        up_totals_t__totals->uint32_t__incomplete++;
        up_daily_t__day->uint16_t__incomplete++;
    }

    up_totals_t__totals->uint32_t__sumChargeS += uint32_t__durationS;
    if ((up_totals_t__totals->uint32_t__minChargeS == 0u) || (uint32_t__durationS < up_totals_t__totals->uint32_t__minChargeS))
    {
        up_totals_t__totals->uint32_t__minChargeS = uint32_t__durationS;
    }
    if (uint32_t__durationS > up_totals_t__totals->uint32_t__maxChargeS)
    {
        up_totals_t__totals->uint32_t__maxChargeS = uint32_t__durationS;
    }

    up_totals_t__totals->uint32_t__chargeWh100 += uint32_t__energyWh100;
    up_daily_t__day->uint32_t__energyWh100 += uint32_t__energyWh100;
    up_daily_t__day->uint32_t__chargeSeconds += uint32_t__durationS;

    BOOL__G__TotalsDirty = true;
    func__UpStore_DailyMarkDirty();
}

/* ==================== Trackers / ردیاب‌ها ==================== */
/**
 * @brief  [EN] Follow both charger states: open a session when one starts
 *              charging, close it on FLOAT (completed) or on any other exit
 *              (incomplete), and feed the energy accumulator meanwhile.
 *         [FA] پیگیری حالت هر دو شارژر: با شروع شارژ نشست باز می‌کند، با FLOAT
 *              (کامل) یا هر خروج دیگر (ناتمام) می‌بندد و در این فاصله انبار
 *              انرژی را تغذیه می‌کند.
 * @param  uint32_t__dtMs [EN] elapsed milliseconds / [FA] میلی‌ثانیهٔ سپری‌شده
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHistory_TrackChargers(uint32_t uint32_t__dtMs)
{
    for (uint8_t uint8_t__c = 0u; uint8_t__c < UP_HIST_CHANNELS; uint8_t__c++)
    {
        uint8_t uint8_t__channel = (uint8_t)(uint8_t__c + 1u);
        uint8_t uint8_t__state = (uint8_t)func__UpState_TlmWord(uint8_t__c == 0u ? UP_TLM_STATE1 : UP_TLM_STATE2);
        uint8_t uint8_t__previous = UINT8_T__G__PrevState[uint8_t__c];
        bool bool__charging = (uint8_t__state == UP_ST_BULK) || (uint8_t__state == UP_ST_ABSORB) || (uint8_t__state == UP_ST_BRINGUP);
        bool bool__started = bool__charging && (uint8_t__previous != UP_ST_BULK) && (uint8_t__previous != UP_ST_ABSORB) && (uint8_t__previous != UP_ST_BRINGUP);
        bool bool__finishedFloat = (uint8_t__state == UP_ST_FLOAT) &&
                                   ((uint8_t__previous == UP_ST_BULK) || (uint8_t__previous == UP_ST_ABSORB) || (uint8_t__previous == UP_ST_BRINGUP));
        bool bool__aborted = (!bool__charging) && (uint8_t__state != UP_ST_FLOAT) &&
                             ((uint8_t__previous == UP_ST_BULK) || (uint8_t__previous == UP_ST_ABSORB) || (uint8_t__previous == UP_ST_BRINGUP));

        if (UP_CHARGE_T__A__Session[uint8_t__c].bool__open)
        {
            uint32_t uint32_t__iMa = func__UpState_TlmWord(uint8_t__c == 0u ? UP_TLM_MA1_IEST : UP_TLM_MA2_IEST);
            uint32_t uint32_t__vMv = (uint8_t__c == 0u) ? func__UpState_TlmWord(UP_TLM_VHIGH) : func__UpState_TlmWord(UP_TLM_VLOW);

            if (bool__charging)
            {
                UP_CHARGE_T__A__Session[uint8_t__c].uint32_t__chargeMs += uint32_t__dtMs;
                func__UpHistory_EnergyAdd(&UP_CHARGE_T__A__Session[uint8_t__c], uint32_t__iMa, uint32_t__vMv, uint32_t__dtMs);
            }
            else if (bool__finishedFloat)
            {
                func__UpHistory_SessionClose(uint8_t__channel, true);
            }
            else if (bool__aborted)
            {
                func__UpHistory_SessionClose(uint8_t__channel, false);
            }
        }
        else if (bool__started)
        {
            func__UpHistory_SessionOpen(uint8_t__channel);
        }

        UINT8_T__G__PrevState[uint8_t__c] = uint8_t__state;
    }
}

/**
 * @brief  [EN] Follow the 24 V input: open/close the battery-run episode, count
 *              outages and feed the run/input seconds into day and totals.
 *         [FA] پیگیری ورودی ۲۴ ولت: باز/بسته کردن باری روی باتری، شمردن قطع‌ها
 *              و افزودن ثانیه‌های باتری/ورودی به روز و جمع کل.
 * @param  uint32_t__dtMs [EN] elapsed milliseconds / [FA] میلی‌ثانیهٔ سپری‌شده
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHistory_TrackInput(uint32_t uint32_t__dtMs)
{
    up_totals_t *up_totals_t__totals = func__UpStore_Totals();
    up_daily_t *up_daily_t__day = func__UpStore_DailyToday();
    bool bool__inputNow = (func__UpState_InputPresent() != 0u);
    bool bool__inputBefore = ((UINT8_T__G__PrevFlags & 0x02u) != 0u);
    uint32_t uint32_t__wholeSeconds;

    if (bool__inputNow == bool__inputBefore)
    {
        /* [EN] Same state: account the seconds and move on. The millisecond
                remainder is kept so a stream of short ticks still adds up to
                whole seconds instead of rounding each tick to zero.
           [FA] همان حالت: ثانیه‌ها را بشمار و برو. باقی‌ماندهٔ میلی‌ثانیه نگه
                داشته می‌شود تا گام‌های کوتاه به‌جای گرد شدن هر کدام به صفر،
                روی هم ثانیه‌های کامل بسازند. */
        uint32_t__wholeSeconds = uint32_t__dtMs / 1000u;

        if (!bool__inputNow)
        {
            UINT32_T__G__RunCurrentMs += uint32_t__dtMs;
            UINT32_T__G__RunMsRemainder += (uint32_t__dtMs % 1000u);
            uint32_t__wholeSeconds += UINT32_T__G__RunMsRemainder / 1000u;
            UINT32_T__G__RunMsRemainder %= 1000u;
            up_totals_t__totals->uint32_t__runS += uint32_t__wholeSeconds;
            up_daily_t__day->uint32_t__runSeconds += uint32_t__wholeSeconds;
        }
        else
        {
            up_daily_t__day->uint32_t__inputSeconds += uint32_t__wholeSeconds;
            up_totals_t__totals->uint32_t__inputS += uint32_t__wholeSeconds;
        }

        BOOL__G__TotalsDirty = true;
        func__UpStore_DailyMarkDirty();
        return;
    }

    if (!bool__inputNow)
    {
        (void)func__UpHistory_WriteEvent(UP_EV_INPUT_LOST, 0u, UP_SEV_WARN, 0u, 0u, 0u);
        (void)func__UpHistory_WriteEvent(UP_EV_RUN_START, 0u, UP_SEV_INFO, 0u, 0u, 0u);
        UINT32_T__G__OutageStartAgeS = func__UpState_UptimeS();
        UINT32_T__G__RunCurrentMs = 0u;
        up_totals_t__totals->uint32_t__outages++;
        up_daily_t__day->uint16_t__outages++;
        BOOL__G__TotalsDirty = true;
        func__UpStore_DailyMarkDirty();
    }
    else
    {
        uint32_t uint32_t__runS = UINT32_T__G__RunCurrentMs / 1000u;
        uint32_t uint32_t__outageS = func__UpState_UptimeS() - UINT32_T__G__OutageStartAgeS;

        (void)func__UpHistory_WriteEvent(UP_EV_INPUT_BACK, 0u, UP_SEV_INFO, 0u, uint32_t__outageS, 0u);
        (void)func__UpHistory_WriteEvent(UP_EV_RUN_END, 0u, UP_SEV_INFO, 0u, uint32_t__runS, 0u);
        if (uint32_t__runS > up_totals_t__totals->uint32_t__maxRunS)
        {
            up_totals_t__totals->uint32_t__maxRunS = uint32_t__runS;
        }
        up_totals_t__totals->uint32_t__outageS += uint32_t__outageS;
        UINT32_T__G__RunCurrentMs = 0u;
        BOOL__G__TotalsDirty = true;
        func__UpStore_DailyMarkDirty();
    }
}

/**
 * @brief  [EN] Follow the fault mask: report bits appearing and clearing, one
 *              event per bit so the Persian diagnostics page can name them.
 *         [FA] پیگیری ماسک خطا: گزارش آمدن و رفتن بیت‌ها، یک رویداد برای هر بیت
 *              تا صفحهٔ دیاگ فارسی بتواند نامشان را بگوید.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHistory_TrackFaults(void)
{
    uint16_t uint16_t__mask = (uint16_t)func__UpState_TlmWord(UP_TLM_FAULTS);
    uint16_t uint16_t__appeared = (uint16_t)(uint16_t__mask & (uint16_t)~UINT16_T__G__PrevFaultMask);
    uint16_t uint16_t__cleared = (uint16_t)(UINT16_T__G__PrevFaultMask & (uint16_t)~uint16_t__mask);

    for (uint8_t uint8_t__b = 0u; uint8_t__b < 8u; uint8_t__b++)
    {
        uint16_t uint16_t__bit = (uint16_t)(1u << uint8_t__b);

        if ((uint16_t__appeared & uint16_t__bit) != 0u)
        {
            (void)func__UpHistory_WriteEvent(UP_EV_FAULT_SET, 0u, func__UpHistory_FaultSeverity(uint16_t__bit), uint16_t__bit, 0u, 0u);
        }
        if ((uint16_t__cleared & uint16_t__bit) != 0u)
        {
            (void)func__UpHistory_WriteEvent(UP_EV_FAULT_CLEAR, 0u, UP_SEV_INFO, uint16_t__bit, 0u, 0u);
        }
    }

    UINT16_T__G__PrevFaultMask = uint16_t__mask;
}

/**
 * @brief  [EN] Follow things that happen rarely: a board restart (telemetry
 *              sequence jumps backwards), an imbalance episode, and the
 *              engineering manual mode being switched on.
 *         [FA] پیگیری چیزهایی که کم پیش می‌آیند: ری‌استارت برد (عقب‌رفتن
 *              شمارندهٔ تلمتری)، رویداد عدم‌توازن و روشن‌شدن مود دستی مهندسی.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHistory_TrackRareEvents(void)
{
    up_totals_t *up_totals_t__totals = func__UpStore_Totals();
    up_daily_t *up_daily_t__day = func__UpStore_DailyToday();
    uint16_t uint16_t__seq = UPPANEL_STATE_T__G__State.up_telemetry_t__tlm.uint16_t__seq;
    int16_t  int16_t__seqDiff;
    uint32_t uint32_t__imbEvents = func__UpState_TlmWord(UP_TLM_IMB_EVENTS);
    uint8_t  uint8_t__state1 = (uint8_t)func__UpState_TlmWord(UP_TLM_STATE1);
    uint8_t  uint8_t__state2 = (uint8_t)func__UpState_TlmWord(UP_TLM_STATE2);
    bool bool__manualNow = (uint8_t__state1 == UP_ST_MANUAL) || (uint8_t__state2 == UP_ST_MANUAL);
    bool bool__manualBefore = (UINT8_T__G__PrevFlags & 0x20u) != 0u;

    int16_t__seqDiff = (int16_t)(uint16_t)(uint16_t__seq - UINT16_T__G__PrevSeq);
    if (int16_t__seqDiff < -8)
    {
        (void)func__UpHistory_WriteEvent(UP_EV_BOARD_BOOT, 0u, UP_SEV_WARN, uint16_t__seq, 0u, 0u);
        up_totals_t__totals->uint32_t__boots++;
        up_daily_t__day->uint16_t__boots++;
        BOOL__G__TotalsDirty = true;
        func__UpStore_DailyMarkDirty();
    }
    UINT16_T__G__PrevSeq = uint16_t__seq;

    if ((uint32_t__imbEvents > UINT32_T__G__PrevImbEvents) && (BOOL__G__PrevValid))
    {
        (void)func__UpHistory_WriteEvent(UP_EV_IMBALANCE, 0u, UP_SEV_WARN,
                                         (uint16_t)(uint32_t__imbEvents - UINT32_T__G__PrevImbEvents), 0u,
                                         (uint16_t)func__UpState_TlmWord(UP_TLM_IMB_MV));
        up_totals_t__totals->uint32_t__imbEvents += (uint32_t__imbEvents - UINT32_T__G__PrevImbEvents);
        BOOL__G__TotalsDirty = true;
    }
    UINT32_T__G__PrevImbEvents = uint32_t__imbEvents;

    if (bool__manualNow && !bool__manualBefore)
    {
        (void)func__UpHistory_WriteEvent(UP_EV_MANUAL_ON, 0u, UP_SEV_WARN, 0u, 0u, 0u);
    }
}

/**
 * @brief  [EN] Merge the peaks-only-window statistics from `/m` into the
 *              lifetime maxima. Called when a new window sample arrives.
 *         [FA] ادغام آمار پنجرهٔ اوج‌ها (از `/m`) در بیشینه‌های کل. با رسیدن
 *              نمونهٔ تازهٔ پنجره صدا زده می‌شود.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHistory_MergePeaks(void)
{
    up_totals_t *up_totals_t__totals = func__UpStore_Totals();
    up_peaks_t *up_peaks_t__peaks = &UPPANEL_STATE_T__G__State.up_peaks_t__peaks;

    if (up_peaks_t__peaks->uint32_t__peakI1 > up_totals_t__totals->uint32_t__peakI1)
    {
        up_totals_t__totals->uint32_t__peakI1 = up_peaks_t__peaks->uint32_t__peakI1;
    }
    if (up_peaks_t__peaks->uint32_t__peakI2 > up_totals_t__totals->uint32_t__peakI2)
    {
        up_totals_t__totals->uint32_t__peakI2 = up_peaks_t__peaks->uint32_t__peakI2;
    }
    if (up_peaks_t__peaks->uint32_t__peakDuty1 > up_totals_t__totals->uint32_t__peakDuty1)
    {
        up_totals_t__totals->uint32_t__peakDuty1 = up_peaks_t__peaks->uint32_t__peakDuty1;
    }
    if (up_peaks_t__peaks->uint32_t__peakDuty2 > up_totals_t__totals->uint32_t__peakDuty2)
    {
        up_totals_t__totals->uint32_t__peakDuty2 = up_peaks_t__peaks->uint32_t__peakDuty2;
    }

    BOOL__G__TotalsDirty = true;
}

/**
 * @brief  [EN] Feed voltages and instantaneous peaks seen in the live stream
 *              into the lifetime envelope (the /m window only sees frames while
 *              a browser watches the bench page; this sees every frame).
 *         [FA] افزودن ولتاژها و اوج‌های لحظه‌ای جریان زنده به پوشش کل (پنجرهٔ
 *              /m فقط وقتی صفحهٔ بنچ باز است فریم می‌بیند؛ این همهٔ فریم‌ها را
 *              می‌بیند).
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHistory_TrackEnvelope(void)
{
    up_totals_t *up_totals_t__totals = func__UpStore_Totals();
    up_daily_t *up_daily_t__day = func__UpStore_DailyToday();
    uint32_t uint32_t__v24 = func__UpState_TlmWord(UP_TLM_V24);
    uint32_t uint32_t__i1 = func__UpState_TlmWord(UP_TLM_MA1_IEST);
    uint32_t uint32_t__i2 = func__UpState_TlmWord(UP_TLM_MA2_IEST);
    uint32_t uint32_t__d1 = func__UpState_TlmWord(UP_TLM_DUTY1);
    uint32_t uint32_t__d2 = func__UpState_TlmWord(UP_TLM_DUTY2);
    bool bool__changed = false;

    if (uint32_t__v24 > 0u)
    {
        if ((up_totals_t__totals->uint16_t__maxV == 0u) || (uint32_t__v24 > up_totals_t__totals->uint16_t__maxV))
        {
            up_totals_t__totals->uint16_t__maxV = func__UpHistory_Clamp16(uint32_t__v24);
            bool__changed = true;
        }
        if ((up_totals_t__totals->uint16_t__minV == 0u) || (uint32_t__v24 < up_totals_t__totals->uint16_t__minV))
        {
            up_totals_t__totals->uint16_t__minV = func__UpHistory_Clamp16(uint32_t__v24);
            bool__changed = true;
        }
        if ((up_daily_t__day->uint16_t__maxV == 0u) || (uint32_t__v24 > up_daily_t__day->uint16_t__maxV))
        {
            up_daily_t__day->uint16_t__maxV = func__UpHistory_Clamp16(uint32_t__v24);
            func__UpStore_DailyMarkDirty();
        }
        if ((up_daily_t__day->uint16_t__minV == 0u) || (uint32_t__v24 < up_daily_t__day->uint16_t__minV))
        {
            up_daily_t__day->uint16_t__minV = func__UpHistory_Clamp16(uint32_t__v24);
            func__UpStore_DailyMarkDirty();
        }
    }

    if (uint32_t__i1 > up_totals_t__totals->uint32_t__peakI1)
    {
        up_totals_t__totals->uint32_t__peakI1 = uint32_t__i1;
        bool__changed = true;
    }
    if (uint32_t__i2 > up_totals_t__totals->uint32_t__peakI2)
    {
        up_totals_t__totals->uint32_t__peakI2 = uint32_t__i2;
        bool__changed = true;
    }
    if (uint32_t__d1 > up_totals_t__totals->uint32_t__peakDuty1)
    {
        up_totals_t__totals->uint32_t__peakDuty1 = uint32_t__d1;
        bool__changed = true;
    }
    if (uint32_t__d2 > up_totals_t__totals->uint32_t__peakDuty2)
    {
        up_totals_t__totals->uint32_t__peakDuty2 = uint32_t__d2;
        bool__changed = true;
    }

    if (bool__changed)
    {
        BOOL__G__TotalsDirty = true;
    }
}

/* ==================== Entry points / نقاط ورود ==================== */
/**
 * @brief  [EN] Take one time step: account seconds, run every tracker, and let
 *              the store watch the filesystem now and then.
 *         [FA] یک گام زمانی: شمردن ثانیه‌ها، اجرای همهٔ ردیاب‌ها و سپردن پایش
 *              فایل‌سیستم به انبار هر چند وقت یک‌بار.
 * @param  uint32_t__nowMs [EN] millis() of this call / [FA] زمان این فراخوانی
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHistory_Tick(uint32_t uint32_t__nowMs)
{
    /* [EN] Fold whatever the peaks window last reported into the lifetime
            maxima. Done here, once per tick, so the two modules stay unaware
            of each other: the link only fills the window, history owns totals.
       [FA] هر چه پنجرهٔ اوج‌ها آخرین بار گزارش کرده در بیشینه‌های کل ادغام
            می‌شود. اینجا و یک‌بار در هر تیک، تا دو ماژول از هم بی‌خبر بمانند:
            لینک فقط پنجره را پر می‌کند، تاریخچه مالک جمع کل است. */
    func__UpHistory_MergePeaks();

    uint32_t uint32_t__dtMs;
    bool bool__linkOnline = func__UpState_LinkOnline();

    uint32_t__dtMs = uint32_t__nowMs - UINT32_T__G__LastTickMs;
    if (uint32_t__dtMs < UP_HIST_TICK_MIN_MS)
    {
        return;
    }
    if (uint32_t__dtMs > UP_HIST_TICK_MAX_MS)
    {
        uint32_t__dtMs = UP_HIST_TICK_MAX_MS;
    }
    UINT32_T__G__LastTickMs = uint32_t__nowMs;

    if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LastPressureMs) >= UP_HIST_PRESSURE_MS)
    {
        UINT32_T__G__LastPressureMs = uint32_t__nowMs;
        (void)func__UpStore_HandlePressure();
    }

    if (!bool__linkOnline)
    {
        /* [EN] No trustworthy data: count nothing, claim nothing.
           [FA] داده‌ای که قابل اعتماد باشد نیست: نه چیزی می‌شماریم، نه ادعایی می‌کنیم. */
        return;
    }

    if (!BOOL__G__PrevValid)
    {
        /* [EN] First frame after boot: adopt the current world as the baseline
                so a restart does not invent an input cut or a charge start.
           [FA] اولین فریم بعد از بوت: وضعیت فعلی را خط پایه می‌گیریم تا
                ری‌استارت، قطع ورودی یا شروع شارژ جعلی نسازد. */
        UINT8_T__G__PrevState[0] = (uint8_t)func__UpState_TlmWord(UP_TLM_STATE1);
        UINT8_T__G__PrevState[1] = (uint8_t)func__UpState_TlmWord(UP_TLM_STATE2);
        UINT8_T__G__PrevFlags = UPPANEL_STATE_T__G__State.up_telemetry_t__tlm.uint8_t__flags;
        UINT16_T__G__PrevFaultMask = (uint16_t)func__UpState_TlmWord(UP_TLM_FAULTS);
        UINT16_T__G__PrevSeq = UPPANEL_STATE_T__G__State.up_telemetry_t__tlm.uint16_t__seq;
        UINT32_T__G__PrevImbEvents = func__UpState_TlmWord(UP_TLM_IMB_EVENTS);
        BOOL__G__PrevValid = true;

        if (!func__UpState_InputPresent())
        {
            UINT32_T__G__OutageStartAgeS = func__UpState_UptimeS();
            UINT32_T__G__RunCurrentMs = 0u;
        }
    }

    UPPANEL_STATE_T__G__State.up_totals_t__totals.uint32_t__coverageS += uint32_t__dtMs / 1000u;

    func__UpHistory_TrackInput(uint32_t__dtMs);
    func__UpHistory_TrackChargers(uint32_t__dtMs);
    func__UpHistory_TrackFaults();
    func__UpHistory_TrackRareEvents();
    func__UpHistory_TrackEnvelope();

    UINT8_T__G__PrevFlags = UPPANEL_STATE_T__G__State.up_telemetry_t__tlm.uint8_t__flags;
}

/**
 * @brief  [EN] Store one sample when the cadence is due, when an event asked
 *              for one, or when the link is about to look dead (so a chart
 *              shows the gap rather than a straight line across it).
 *         [FA] ذخیرهٔ یک نمونه وقتی بازه رسیده، وقتی رویدادی درخواست کرده یا
 *              وقتی لینک در آستانهٔ قطع است (تا نمودار شکاف را نشان دهد، نه
 *              خط مستقیمی که از رویش رد شود).
 * @param  uint32_t__nowMs [EN] millis() of this call / [FA] زمان این فراخوانی
 * @return [EN] true when a sample was written / [FA] در صورت نوشتن true
 */
static bool func__UpHistory_SampleIfDue(uint32_t uint32_t__nowMs)
{
    up_sample_t up_sample_t__sample;
    bool bool__due;

    if (!func__UpState_LinkOnline())
    {
        return false;
    }

    bool__due = BOOL__G__ForceSample ||
                ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LastSampleMs) >= UP_SAMPLE_STORE_MS);
    if (!bool__due)
    {
        return false;
    }

    memset(&up_sample_t__sample, 0, sizeof(up_sample_t__sample));
    up_sample_t__sample.uint32_t__absS = func__UpHistory_AbsoluteS();
    up_sample_t__sample.uint16_t__vinMv = (uint16_t)func__UpHistory_Clamp16(func__UpState_TlmWord(UP_TLM_VIN));
    up_sample_t__sample.uint16_t__v24Mv = (uint16_t)func__UpHistory_Clamp16(func__UpState_TlmWord(UP_TLM_V24));
    up_sample_t__sample.uint16_t__v12Mv = (uint16_t)func__UpHistory_Clamp16(func__UpState_TlmWord(UP_TLM_V12));
    up_sample_t__sample.uint16_t__i1Ma = (uint16_t)func__UpHistory_Clamp16(func__UpState_TlmWord(UP_TLM_MA1_IEST));
    up_sample_t__sample.uint16_t__i2Ma = (uint16_t)func__UpHistory_Clamp16(func__UpState_TlmWord(UP_TLM_MA2_IEST));
    up_sample_t__sample.uint16_t__duty1 = (uint16_t)func__UpHistory_Clamp16(func__UpState_TlmWord(UP_TLM_DUTY1));
    up_sample_t__sample.uint16_t__duty2 = (uint16_t)func__UpHistory_Clamp16(func__UpState_TlmWord(UP_TLM_DUTY2));
    up_sample_t__sample.uint8_t__state1 = (uint8_t)func__UpState_TlmWord(UP_TLM_STATE1);
    up_sample_t__sample.uint8_t__state2 = (uint8_t)func__UpState_TlmWord(UP_TLM_STATE2);
    up_sample_t__sample.uint8_t__flags = UPPANEL_STATE_T__G__State.up_telemetry_t__tlm.uint8_t__flags;
    up_sample_t__sample.uint8_t__flags2 = UPPANEL_STATE_T__G__State.up_telemetry_t__tlm.uint8_t__flags2;

    UINT32_T__G__LastSampleMs = uint32_t__nowMs;
    BOOL__G__ForceSample = false;

    return func__UpStore_AppendSample(&up_sample_t__sample);
}

/**
 * @brief  [EN] Prepare the history layer after boot: adopt a baseline and put a
 *              "panel on" marker in the event log.
 *         [FA] آماده‌سازی لایهٔ تاریخچه بعد از بوت: گرفتن خط پایه و گذاشتن
 *              نشان «روشن شدن پنل» در گزارش رویدادها.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHistory_Begin(void)
{
    UPPANEL_STATE_T__G__State.uint32_t__clockBaseAtBootS =
        UPPANEL_STATE_T__G__State.up_totals_t__totals.uint32_t__clockBaseS;

    memset(UP_CHARGE_T__A__Session, 0, sizeof(UP_CHARGE_T__A__Session));
    UINT8_T__G__PrevState[0] = 0xFFu;
    UINT8_T__G__PrevState[1] = 0xFFu;
    UINT8_T__G__PrevFlags = 0u;
    UINT16_T__G__PrevFaultMask = 0u;
    UINT16_T__G__PrevSeq = 0u;
    UINT32_T__G__PrevImbEvents = 0u;
    BOOL__G__PrevValid = false;
    UINT32_T__G__LastTickMs = (uint32_t)millis();
    UINT32_T__G__LastSampleMs = (uint32_t)millis();
    UINT32_T__G__LastPressureMs = (uint32_t)millis();
    UINT32_T__G__RunMsRemainder = 0u;
    UINT32_T__G__RunCurrentMs = 0u;
    UINT32_T__G__OutageStartAgeS = func__UpState_UptimeS();
    BOOL__G__ForceSample = false;

    (void)func__UpHistory_WriteEvent(UP_EV_PANEL_BOOT, 0u, UP_SEV_INFO, 0u, 0u, 0u);
}

#endif /* UP_HISTORY_H */
