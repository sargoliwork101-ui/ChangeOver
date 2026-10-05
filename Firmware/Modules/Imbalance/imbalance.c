/**
 * @file    imbalance.c
 * @brief   [EN] Scenario 6 - battery imbalance monitor. Pure logic; see
 *              imbalance.h for the full behaviour contract.
 *          [FA] سناریوی ۶ - پایش عدم‌توازن باتری؛ قرارداد رفتار در imbalance.h.
 *
 * @note    [EN] Persistence: events / latched cycles / latch are stored in
 *              EspLink NVM as slots 200..202. A slot is written ONLY on a
 *              genuine transition (evaluate returns true after a counter or
 *              the latch moved): once per over-limit episode (each episode
 *              is >= stability ms long by construction), once per charge
 *              session while latched, once per latch, and once per automatic
 *              reset. With the defaults that means at most a handful of
 *              flash writes per DAY on a severely degraded battery - against
 *              ~10k erase cycles this is decades of wear margin, so debounced
 *              on-move persistence is justified here (no periodic write).
 *          [FA] ماندگاری: اسلات‌های ۲۰۰..۲۰۲ فقط هنگام تغییر واقعی میزبان
 *              ذخیرهٔ فلش می‌شوند (بارها در روز با باتریِ خیلی خراب)؛ با
 *              تحمل ~۱۰٬۰۰۰ بار پاک‌سازی، حاشیهٔ فرسودگی دهه‌هاست - پس حتی
 *              ذخیرهٔ فوری هم هزینه‌ای ندارد، چه رسد به دبونس.
 */

#include "imbalance.h"

#include <stddef.h>   /* NULL */

/* ==================== Compile-time invariants / ناورداهای زمان کامپایل ==================== */
_Static_assert(IMBAL_PARAM_LAST_ID >= IMBAL_PARAM_FIRST_ID, "param range");
_Static_assert(IMBAL_PARAM_TABLE_SIZE <= 32u, "table size");
_Static_assert(IMBAL_PARAM_FIRST_ID2 > IMBAL_PARAM_LAST_ID, "the two id blocks must not overlap");

/* ==================== Parameter table / جدول پارامترها ==================== */

/* [EN] Live values; boot = macro defaults, NVM replay overrides.
 * [FA] مقادیر زنده؛ بوت = پیش‌فرض ماکرو، پخش NVM بازنویسی می‌کند. */
static uint32_t UINT32_T__G__A__Param[IMBAL_PARAM_TABLE_SIZE];

/* [EN] Persisted runtime state (NVM slots 200..202).
 * [FA] وضعیت ماندگار زمان‌اجرا. */
static uint32_t UINT32_T__G__Events;        /* [EN] Episode count / شمارش رویداد */
static uint32_t UINT32_T__G__LatchedCycles; /* [EN] Cycles since latch / سیکل‌های قفل */
static uint32_t UINT32_T__G__Latched;       /* [EN] 0/1 / قفل */

/* [EN] Last outputs, refreshed every pass for TLM/UI consumers.
 * [FA] آخرین خروجی‌ها برای مصرف‌کنندگان TLM/UI. */
static imbalance_outputs_t IMBAL_OUTPUTS_T__G__Last;

/* [EN] Timing/episode bookkeeping (RAM only).
 * [FA] دفترچهٔ زمانی/اپیزود (فقط RAM). */
static bool     BOOL__G__HasChargeEnd;      /* [EN] A charge session ended at least once / حداقل یک پایان شارژ */
static uint32_t UINT32_T__G__RestAnchorMs;  /* [EN] Rest window anchor: last charge end, or boot / لنگر استراحت */
static bool     BOOL__G__ChargingEdge;      /* [EN] charging seen at startup? / تشخیص لبهٔ اول */
static uint32_t UINT32_T__G__ChargeStartMs; /* [EN] This session's start / شروع این سیکل */
static bool     BOOL__G__PrevCharging;      /* [EN] Edge memory / حافظهٔ لبه */
static bool     BOOL__G__Episode;           /* [EN] Episode in progress / اپیزود جاری */
static uint32_t UINT32_T__G__EpisodeLimitMv;/* [EN] Limit that armed the episode / حد لحظهٔ شروع اپیزود */
static uint32_t UINT32_T__G__OverSinceMs;   /* [EN] Over-limit continuous start / شروع دویش فراتر از حد */
static bool     BOOL__G__OverTiming;        /* [EN] Over-limit timing active / اندازه در جریان */
static uint32_t UINT32_T__G__AbsentSinceMs; /* [EN] Battery-absent continuous start / شروع نبود باتری */
static bool     BOOL__G__AbsentTiming;      /* [EN] Absence timer active / تایمر نبود فعال */
static uint32_t UINT32_T__G__LastBeepMs;    /* [EN] Last latch beep / آخرین بوق قفل */

/* ==================== Internal helpers / کمکی‌های داخلی ==================== */

static uint32_t func__Imbalance_ParamDefault(uint8_t uint8_t__paramId)
{
    uint32_t uint32_t__ret;

    switch (uint8_t__paramId)
    {
        case IMBAL_PARAM_REST_LIMIT_MV:        uint32_t__ret = IMBAL_DEF_REST_LIMIT_MV;        break;
        case IMBAL_PARAM_DISCHARGE_LIMIT_MV:   uint32_t__ret = IMBAL_DEF_DISCHARGE_LIMIT_MV;   break;
        case IMBAL_PARAM_POST_CHARGE_WAIT_MS:  uint32_t__ret = IMBAL_DEF_POST_CHARGE_WAIT_MS;  break;
        case IMBAL_PARAM_IN_CHARGE_WAIT_MS:    uint32_t__ret = IMBAL_DEF_IN_CHARGE_WAIT_MS;    break;
        case IMBAL_PARAM_STABILITY_MS:         uint32_t__ret = IMBAL_DEF_STABILITY_MS;         break;
        case IMBAL_PARAM_HYSTERESIS_MV:        uint32_t__ret = IMBAL_DEF_HYSTERESIS_MV;        break;
        case IMBAL_PARAM_MAX_EVENTS:           uint32_t__ret = IMBAL_DEF_MAX_EVENTS;           break;
        case IMBAL_PARAM_LATCH_BEEP_PERIOD_MS: uint32_t__ret = IMBAL_DEF_BEEP_PERIOD_MS;       break;
        case IMBAL_PARAM_LATCH_BEEP_LEN_MS:    uint32_t__ret = IMBAL_DEF_BEEP_LEN_MS;          break;
        case IMBAL_PARAM_BLOCK_OUTPUT_EN:      uint32_t__ret = IMBAL_DEF_BLOCK_OUTPUT_EN;      break;
        case IMBAL_PARAM_MAX_LATCHED_CYCLES:   uint32_t__ret = IMBAL_DEF_MAX_LATCHED_CYCLES;   break;
        case IMBAL_PARAM_LATCH_BLINK_PERIOD_MS: uint32_t__ret = IMBAL_DEF_LATCH_BLINK_PERIOD_MS; break;
        case IMBAL_PARAM_LATCH_BLINK_DUTY_PCT:  uint32_t__ret = IMBAL_DEF_LATCH_BLINK_DUTY_PCT;  break;
        default:                               uint32_t__ret = 0u;                             break;
    }

    return uint32_t__ret;
}

static uint32_t func__Imbalance_ParamClamp(uint8_t uint8_t__paramId, uint32_t uint32_t__value)
{
    uint32_t uint32_t__ret = uint32_t__value;

    switch (uint8_t__paramId)
    {
        case IMBAL_PARAM_REST_LIMIT_MV:
        case IMBAL_PARAM_DISCHARGE_LIMIT_MV:
            if (uint32_t__ret > IMBAL_MAX_LIMIT_MV) { uint32_t__ret = IMBAL_MAX_LIMIT_MV; }
            break;
        case IMBAL_PARAM_POST_CHARGE_WAIT_MS:
        case IMBAL_PARAM_IN_CHARGE_WAIT_MS:
            if (uint32_t__ret > IMBAL_MAX_WAIT_MS) { uint32_t__ret = IMBAL_MAX_WAIT_MS; }
            break;
        case IMBAL_PARAM_STABILITY_MS:
            if (uint32_t__ret < IMBAL_MIN_STABILITY_MS) { uint32_t__ret = IMBAL_MIN_STABILITY_MS; }
            if (uint32_t__ret > IMBAL_MAX_STABILITY_MS) { uint32_t__ret = IMBAL_MAX_STABILITY_MS; }
            break;
        case IMBAL_PARAM_HYSTERESIS_MV:
            if (uint32_t__ret > IMBAL_MAX_HYSTERESIS_MV) { uint32_t__ret = IMBAL_MAX_HYSTERESIS_MV; }
            break;
        case IMBAL_PARAM_MAX_EVENTS:
        case IMBAL_PARAM_MAX_LATCHED_CYCLES:
            if (uint32_t__ret < 1u) { uint32_t__ret = 1u; }
            if (uint32_t__ret > IMBAL_MAX_COUNT) { uint32_t__ret = IMBAL_MAX_COUNT; }
            break;
        case IMBAL_PARAM_LATCH_BEEP_PERIOD_MS:
            /* [EN] 0 = beep off is a legal value; clamp only the upper end.
             * [FA] صفر یعنی بوق خاموش و مجاز است؛ فقط سقف گیره می‌شود. */
            if (uint32_t__ret > IMBAL_MAX_BEEP_PERIOD_MS) { uint32_t__ret = IMBAL_MAX_BEEP_PERIOD_MS; }
            break;
        case IMBAL_PARAM_LATCH_BEEP_LEN_MS:
            if (uint32_t__ret < IMBAL_MIN_BEEP_LEN_MS) { uint32_t__ret = IMBAL_MIN_BEEP_LEN_MS; }
            if (uint32_t__ret > IMBAL_MAX_BEEP_LEN_MS) { uint32_t__ret = IMBAL_MAX_BEEP_LEN_MS; }
            break;
        case IMBAL_PARAM_BLOCK_OUTPUT_EN:
            if (uint32_t__ret > 1u) { uint32_t__ret = 1u; }
            break;
        case IMBAL_PARAM_LATCH_BLINK_PERIOD_MS:
            /* [EN] 0 = solid red, a deliberate opt-out; any other value is
             *      pulled into the visible-blink window.
             * [FA] صفر یعنی قرمز ثابت؛ بقیهٔ مقادیر داخل پنجرهٔ چشمک دیدنی. */
            if (uint32_t__ret != 0u)
            {
                if (uint32_t__ret < IMBAL_MIN_BLINK_PERIOD_MS) { uint32_t__ret = IMBAL_MIN_BLINK_PERIOD_MS; }
                if (uint32_t__ret > IMBAL_MAX_BLINK_PERIOD_MS) { uint32_t__ret = IMBAL_MAX_BLINK_PERIOD_MS; }
            }
            break;
        case IMBAL_PARAM_LATCH_BLINK_DUTY_PCT:
            if (uint32_t__ret < IMBAL_MIN_BLINK_DUTY_PCT) { uint32_t__ret = IMBAL_MIN_BLINK_DUTY_PCT; }
            if (uint32_t__ret > IMBAL_MAX_BLINK_DUTY_PCT) { uint32_t__ret = IMBAL_MAX_BLINK_DUTY_PCT; }
            break;
        default:
            break;
    }

    return uint32_t__ret;
}

static uint32_t func__Imbalance_ReadParam(uint8_t uint8_t__paramId)
{
    return UINT32_T__G__A__Param[IMBAL_PARAM_INDEX(uint8_t__paramId)];
}

/* ==================== Public API / API عمومی ==================== */

void func__Imbalance_Init(void)
{
    uint8_t uint8_t__id;

    for (uint8_t__id = IMBAL_PARAM_FIRST_ID; uint8_t__id <= IMBAL_PARAM_LAST_ID2; uint8_t__id++)
    {
        if (IMBAL_PARAM_OWNS(uint8_t__id))
        {
            UINT32_T__G__A__Param[IMBAL_PARAM_INDEX(uint8_t__id)] =
                func__Imbalance_ParamDefault(uint8_t__id);
        }
    }

    /* [EN] Persisted fields are NOT reset here: EspLink NVM replay may run
     *      before or after Init, and both orders must land on the stored
     *      values. Boot RAM starts zeroed by the runtime and replay fills it.
     * [FA] مقادیر ماندگار اینجا صفر نمی‌شوند؛ بوت = صفر زمان‌اجرا و پخش NVM
     *      پرشان می‌کند. */

    BOOL__G__HasChargeEnd     = false;
    UINT32_T__G__RestAnchorMs = 0u;      /* [EN] 0 = anchored at boot / صفر یعنی از بوت */
    BOOL__G__ChargingEdge     = false;
    UINT32_T__G__ChargeStartMs = 0u;
    BOOL__G__PrevCharging     = false;
    BOOL__G__Episode          = false;
    UINT32_T__G__EpisodeLimitMv = 0u;
    UINT32_T__G__OverSinceMs  = 0u;
    BOOL__G__OverTiming       = false;
    UINT32_T__G__AbsentSinceMs = 0u;
    BOOL__G__AbsentTiming     = false;
    /* [EN] First latch beep due exactly one period after latch; init to 0 so
     *      a latch at boot+before-first-period behaves deterministically.
     * [FA] اولین بوق دقیقاً یک دوره پس از قفل. */

    UINT32_T__G__LastBeepMs   = 0u;

    IMBAL_OUTPUTS_T__G__Last.uint32_t__imbalanceMv  = 0u;
    IMBAL_OUTPUTS_T__G__Last.bool__episode          = false;
    IMBAL_OUTPUTS_T__G__Last.uint32_t__events       = UINT32_T__G__Events;
    IMBAL_OUTPUTS_T__G__Last.bool__latched          = (UINT32_T__G__Latched != 0u);
    IMBAL_OUTPUTS_T__G__Last.uint32_t__latchedCycles = UINT32_T__G__LatchedCycles;
    IMBAL_OUTPUTS_T__G__Last.bool__blockOutput      = false;
    IMBAL_OUTPUTS_T__G__Last.bool__chargingAllowed  = true;
    IMBAL_OUTPUTS_T__G__Last.bool__beepDue          = false;
}

void func__Imbalance_GetOutputs(imbalance_outputs_t *imbalance_outputs_t__outputs)
{
    if (imbalance_outputs_t__outputs != NULL)
    {
        *imbalance_outputs_t__outputs = IMBAL_OUTPUTS_T__G__Last;
    }
}

bool func__Imbalance_Evaluate(const imbalance_inputs_t *imbalance_inputs_t__inputs,
                              uint32_t uint32_t__nowMs,
                              imbalance_outputs_t *imbalance_outputs_t__outputs)
{
    bool     bool__persistChanged;
    uint32_t uint32_t__imbalanceMv;
    bool     bool__windowOpen;
    uint32_t uint32_t__activeLimitMv;
    bool     bool__charging;
    bool     bool__justLatched;

    bool__persistChanged = false;
    bool__windowOpen     = false;
    uint32_t__activeLimitMv  = 0u;
    bool__justLatched    = false;

    if (imbalance_inputs_t__inputs == NULL)
    {
        if (imbalance_outputs_t__outputs != NULL)
        {
            *imbalance_outputs_t__outputs = IMBAL_OUTPUTS_T__G__Last;
        }
        return false;
    }

    /* ---- live imbalance (absolute, user decision "قدرمطلق") ---- */
    if (imbalance_inputs_t__inputs->uint32_t__vHighMv >= imbalance_inputs_t__inputs->uint32_t__vLowMv)
    {
        uint32_t__imbalanceMv = imbalance_inputs_t__inputs->uint32_t__vHighMv -
                                imbalance_inputs_t__inputs->uint32_t__vLowMv;
    }
    else
    {
        uint32_t__imbalanceMv = imbalance_inputs_t__inputs->uint32_t__vLowMv -
                                imbalance_inputs_t__inputs->uint32_t__vHighMv;
    }

    /* [EN] Untrustworthy measurements freeze every timer and window decision;
     *      counters keep their last values (same philosophy as Changeover:
     *      invalid snapshot = invalid time, no progress, no wrong verdict).
     * [FA] با نمونهٔ نامعتبر همهٔ تایمرها و پنجره‌ها فریز می‌شوند. */
    if (imbalance_inputs_t__inputs->bool__valid == false)
    {
        BOOL__G__OverTiming = false;
        if (imbalance_outputs_t__outputs != NULL)
        {
            *imbalance_outputs_t__outputs = IMBAL_OUTPUTS_T__G__Last;
        }
        return false;
    }

    /* ---- charge-session edges (start/end + latched cycle counting) ---- */
    bool__charging = imbalance_inputs_t__inputs->bool__charging;

    if ((bool__charging == true) && (BOOL__G__PrevCharging == false))
    {
        /* [EN] Rising edge: session start. Count it when latched - this is
         *      the "20 more cycles then no charge" budget spending.
         * [FA] لبهٔ صعودی: شروع سیکل؛ در حالت قفل به شمارندهٔ سیکل اضافه. */
        BOOL__G__ChargingEdge   = true;
        UINT32_T__G__ChargeStartMs = uint32_t__nowMs;
        if (UINT32_T__G__Latched != 0u)
        {
            if (UINT32_T__G__LatchedCycles < IMBAL_MAX_COUNT)
            {
                UINT32_T__G__LatchedCycles++;
                bool__persistChanged = true;
            }
        }
    }
    if ((bool__charging == false) && (BOOL__G__PrevCharging == true))
    {
        /* [EN] Falling edge: anchor the rest window at the charge end.
         * [FA] لبهٔ نزولی: لنگر پنجرهٔ استراحت = پایان شارژ. */
        BOOL__G__HasChargeEnd  = true;
        UINT32_T__G__RestAnchorMs = uint32_t__nowMs;
    }
    BOOL__G__PrevCharging = bool__charging;

    /* ---- automatic reset on battery absence (real battery swap) ----
     * [EN] Counters and the latch belong to the BATTERY, not to the board:
     *      when the pack physically leaves, the memory must leave with it.
     *      3000 ms continuous absence - a battery swap takes minutes.
     * [FA] شمارنده‌ها متعلق به باتری‌اند؛ با تعویض واقعی، خودکار صفر. */
    if (imbalance_inputs_t__inputs->bool__batAbsent == true)
    {
        if (BOOL__G__AbsentTiming == false)
        {
            BOOL__G__AbsentTiming    = true;
            UINT32_T__G__AbsentSinceMs = uint32_t__nowMs;
        }
        else if ((uint32_t__nowMs - UINT32_T__G__AbsentSinceMs) >= IMBAL_ABSENT_RESET_MS)
        {
            if ((UINT32_T__G__Events != 0u) || (UINT32_T__G__LatchedCycles != 0u) || (UINT32_T__G__Latched != 0u))
            {
                UINT32_T__G__Events        = 0u;
                UINT32_T__G__LatchedCycles = 0u;
                UINT32_T__G__Latched       = 0u;
                bool__persistChanged       = true;
            }
            BOOL__G__Episode    = false;
            BOOL__G__OverTiming = false;
        }
        else
        {
            /* [EN] Absent but still within the qualifier: nothing to do. */
        }
    }
    else
    {
        BOOL__G__AbsentTiming = false;
    }

    /* ---- window classification / طبقه‌بندی پنجره ----
     * [EN] discharge: output on battery (and not charging) -> immediate,
     *      discharge limit.
     *      charging: after param 111 since session start (0 = off), rest limit.
     *      rest: after param 110 since the last charge end (or since boot if
     *      none yet - a battery that sits at rest from boot is exactly what
     *      the user wants measured), rest limit; 0 = rest eval off.
     * [FA] دشارژ: فوری با حد دشارژ. حین شارژ: پس از ۱۱۱ با حد استراحت.
     *      استراحت: پس از ۱۱۰ از پایان آخرین شارژ (یا از بوت). */
    if (imbalance_inputs_t__inputs->bool__onBattery == true)
    {
        if (bool__charging == false)
        {
            bool__windowOpen    = true;
            uint32_t__activeLimitMv = func__Imbalance_ReadParam(IMBAL_PARAM_DISCHARGE_LIMIT_MV);
        }
    }

    if (bool__windowOpen == false)
    {
        if (bool__charging == true)
        {
            uint32_t uint32_t__waitMs = func__Imbalance_ReadParam(IMBAL_PARAM_IN_CHARGE_WAIT_MS);

            if ((uint32_t__waitMs != 0u) &&
                ((uint32_t__nowMs - UINT32_T__G__ChargeStartMs) >= uint32_t__waitMs))
            {
                bool__windowOpen    = true;
                uint32_t__activeLimitMv = func__Imbalance_ReadParam(IMBAL_PARAM_REST_LIMIT_MV);
            }
        }
        else
        {
            uint32_t uint32_t__waitMs = func__Imbalance_ReadParam(IMBAL_PARAM_POST_CHARGE_WAIT_MS);

            if (uint32_t__waitMs != 0u)
            {
                if (BOOL__G__HasChargeEnd == false)
                {
                    /* [EN] No charge ended yet: anchor at boot so rest is
                     *      monitored after 110 ms-time from power-up.
                     * [FA] هنوز شارژی تمام نشده: لنگر = بوت. */
                    if (uint32_t__nowMs >= uint32_t__waitMs)
                    {
                        bool__windowOpen    = true;
                        uint32_t__activeLimitMv = func__Imbalance_ReadParam(IMBAL_PARAM_REST_LIMIT_MV);
                    }
                }
                else if ((uint32_t__nowMs - UINT32_T__G__RestAnchorMs) >= uint32_t__waitMs)
                {
                    bool__windowOpen    = true;
                    uint32_t__activeLimitMv = func__Imbalance_ReadParam(IMBAL_PARAM_REST_LIMIT_MV);
                }
                else
                {
                    /* [EN] Inside the grace delay: hold. / داخل مهلت: صبر. */
                }
            }
        }
    }

    /* ---- episode detection (stability + hysteresis) ---- */
    if (bool__windowOpen == true)
    {
        if (BOOL__G__Episode == false)
        {
            if (uint32_t__imbalanceMv > uint32_t__activeLimitMv)
            {
                if (BOOL__G__OverTiming == false)
                {
                    BOOL__G__OverTiming     = true;
                    UINT32_T__G__OverSinceMs = uint32_t__nowMs;
                }
                else if ((uint32_t__nowMs - UINT32_T__G__OverSinceMs) >=
                         func__Imbalance_ReadParam(IMBAL_PARAM_STABILITY_MS))
                {
                    /* [EN] Count ONCE per episode; the hysteresis below defines
                     *      when this episode ends and the next may count.
                     * [FA] هر اپیزود یک‌بار می‌شمارد؛ پایان با هیسترزیس. */
                    if (UINT32_T__G__Events < IMBAL_MAX_COUNT)
                    {
                        UINT32_T__G__Events++;
                        bool__persistChanged = true;
                    }
                    BOOL__G__Episode          = true;
                    UINT32_T__G__EpisodeLimitMv = uint32_t__activeLimitMv;
                    BOOL__G__OverTiming       = false;
                }
                else
                {
                    /* [EN] Over the limit, still gathering stability. / در حال انباشت پایداری. */
                }
            }
            else
            {
                BOOL__G__OverTiming = false;
            }
        }
    }

    /* [EN] Episode end: below (limit - hysteresis). Evaluated in ANY window
     *      state - a charge session mid-episode must not freeze the episode
     *      open forever when the imbalance genuinely relaxed.
     * [FA] پایان اپیزود در هر وضعیت پنجره (با هیسترزیس و حدِ لحظهٔ شروع). */
    if (BOOL__G__Episode == true)
    {
        uint32_t uint32_t__clearMv = UINT32_T__G__EpisodeLimitMv;
        uint32_t uint32_t__hystMv  = func__Imbalance_ReadParam(IMBAL_PARAM_HYSTERESIS_MV);

        if (uint32_t__clearMv > uint32_t__hystMv)
        {
            uint32_t__clearMv -= uint32_t__hystMv;
        }
        else
        {
            uint32_t__clearMv = 0u;
        }

        if (uint32_t__imbalanceMv <= uint32_t__clearMv)
        {
            BOOL__G__Episode = false;
        }
    }

    /* ---- latch on budget exhausted ----
     * [EN] Checked AFTER episode counting so the verdict fires in the very
     *      same pass the budget is spent - an immediate verdict, not one
     *      evaluation period late.
     * [FA] پس از شمارش اپیزود تا قضاوت همان لحظه صادر شود. */
    if ((UINT32_T__G__Latched == 0u) &&
        (UINT32_T__G__Events >= func__Imbalance_ReadParam(IMBAL_PARAM_MAX_EVENTS)))
    {
        UINT32_T__G__Latched       = 1u;
        UINT32_T__G__LatchedCycles = 0u;   /* [EN] fresh budget from this moment / بودجهٔ تازه */
        bool__persistChanged       = true;
        bool__justLatched          = true;
    }

    /* ---- hourly beep while latched / بوق ساعتی در قفل ---- */
    IMBAL_OUTPUTS_T__G__Last.bool__beepDue = false;
    if (UINT32_T__G__Latched != 0u)
    {
        uint32_t uint32_t__periodMs = func__Imbalance_ReadParam(IMBAL_PARAM_LATCH_BEEP_PERIOD_MS);

        if ((bool__justLatched == true) && (uint32_t__periodMs != 0u))
        {
            /* [EN] Confirmation beep at the latch moment: the operator must
             *      hear the verdict immediately, then hourly.
             * [FA] بوق تأیید در لحظهٔ قفل، سپس هر یک دوره. */
            IMBAL_OUTPUTS_T__G__Last.bool__beepDue = true;
            UINT32_T__G__LastBeepMs               = uint32_t__nowMs;
        }
        else if ((uint32_t__periodMs != 0u) &&
                 ((uint32_t__nowMs - UINT32_T__G__LastBeepMs) >= uint32_t__periodMs))
        {
            IMBAL_OUTPUTS_T__G__Last.bool__beepDue = true;
            UINT32_T__G__LastBeepMs               = uint32_t__nowMs;
        }
        else
        {
            /* [EN] No beep due this pass. / این پاس بوقی نیست. */
        }
    }

    /* ---- outputs / خروجی‌ها ---- */
    IMBAL_OUTPUTS_T__G__Last.uint32_t__imbalanceMv   = uint32_t__imbalanceMv;
    IMBAL_OUTPUTS_T__G__Last.bool__episode           = BOOL__G__Episode;
    IMBAL_OUTPUTS_T__G__Last.uint32_t__events        = UINT32_T__G__Events;
    IMBAL_OUTPUTS_T__G__Last.bool__latched           = (UINT32_T__G__Latched != 0u);
    IMBAL_OUTPUTS_T__G__Last.uint32_t__latchedCycles = UINT32_T__G__LatchedCycles;
    IMBAL_OUTPUTS_T__G__Last.bool__blockOutput =
        ((UINT32_T__G__Latched != 0u) &&
         (func__Imbalance_ReadParam(IMBAL_PARAM_BLOCK_OUTPUT_EN) != 0u));
    IMBAL_OUTPUTS_T__G__Last.bool__chargingAllowed =
        ((UINT32_T__G__Latched == 0u) ||
         (UINT32_T__G__LatchedCycles < func__Imbalance_ReadParam(IMBAL_PARAM_MAX_LATCHED_CYCLES)));

    if (imbalance_outputs_t__outputs != NULL)
    {
        *imbalance_outputs_t__outputs = IMBAL_OUTPUTS_T__G__Last;
    }

    return bool__persistChanged;
}

bool func__Imbalance_SetParam(uint8_t uint8_t__paramId,
                              uint32_t uint32_t__value,
                              uint32_t *uint32_t__appliedValue)
{
    bool bool__ret = false;

    if (IMBAL_PARAM_OWNS(uint8_t__paramId))
    {
        uint32_t uint32_t__clamped = func__Imbalance_ParamClamp(uint8_t__paramId, uint32_t__value);

        UINT32_T__G__A__Param[IMBAL_PARAM_INDEX(uint8_t__paramId)] = uint32_t__clamped;
        if (uint32_t__appliedValue != NULL)
        {
            *uint32_t__appliedValue = uint32_t__clamped;
        }
        bool__ret = true;
    }
    else if ((uint8_t__paramId >= IMBAL_SLOT_FIRST_ID) && (uint8_t__paramId <= IMBAL_SLOT_LAST_ID))
    {
        /* [EN] Runtime slots: NVM boot replay only. The panel never sends
         *      these (not in XIDS/backup), values clamp to 0..255 because
         *      the counters live in a u8 budget by design.
         * [FA] اسلات‌های زمان‌اجرا: فقط پخش NVM هنگام بوت؛ گیره ۰..۲۵۵. */
        uint32_t uint32_t__clamped = (uint32_t__value > IMBAL_MAX_COUNT) ? IMBAL_MAX_COUNT : uint32_t__value;

        switch (uint8_t__paramId)
        {
            case IMBAL_SLOT_EVENTS_ID: UINT32_T__G__Events        = uint32_t__clamped; break;
            case IMBAL_SLOT_CYCLES_ID: UINT32_T__G__LatchedCycles = uint32_t__clamped; break;
            case IMBAL_SLOT_LATCH_ID:  UINT32_T__G__Latched       = (uint32_t__clamped != 0u) ? 1u : 0u; break;
            default: break;
        }
        if (uint32_t__appliedValue != NULL)
        {
            *uint32_t__appliedValue = uint32_t__clamped;
        }
        bool__ret = true;
    }
    else
    {
        /* [EN] Not ours. / مال ما نیست. */
    }

    return bool__ret;
}

bool func__Imbalance_GetParam(uint8_t uint8_t__paramId,
                              uint32_t *uint32_t__value)
{
    bool bool__ret = false;

    if (IMBAL_PARAM_OWNS(uint8_t__paramId))
    {
        if (uint32_t__value != NULL)
        {
            *uint32_t__value = func__Imbalance_ReadParam(uint8_t__paramId);
        }
        bool__ret = true;
    }
    else if ((uint8_t__paramId >= IMBAL_SLOT_FIRST_ID) && (uint8_t__paramId <= IMBAL_SLOT_LAST_ID))
    {
        if (uint32_t__value != NULL)
        {
            switch (uint8_t__paramId)
            {
                case IMBAL_SLOT_EVENTS_ID: *uint32_t__value = UINT32_T__G__Events;        break;
                case IMBAL_SLOT_CYCLES_ID: *uint32_t__value = UINT32_T__G__LatchedCycles; break;
                case IMBAL_SLOT_LATCH_ID:  *uint32_t__value = UINT32_T__G__Latched;       break;
                default: break;
            }
        }
        bool__ret = true;
    }
    else
    {
        /* [EN] Not ours. / مال ما نیست. */
    }

    return bool__ret;
}
