/**
 * @file    imbalance.c
 * @brief   [EN] Scenario 5 - battery imbalance monitor. Pure logic; see
 *              imbalance.h for the full behaviour contract.
 *          [FA] سناریوی ۵ - پایش عدم‌توازن باتری؛ قرارداد رفتار در imbalance.h.
 *
 * @note    [EN] Persistence: events / latched cycles / latch are stored in
 *              EspLink NVM as slots 200..202. A slot is written ONLY on a
 *              genuine transition (evaluate returns true after a committed
 *              full-cycle event, latch, post-latch cycle, or automatic reset).
 *              Stable partial-charge candidates never write NVM. With the
 *              defaults that means at most a handful of flash writes per DAY
 *              on a severely degraded battery - against ~10k erase cycles
 *              this is decades of wear margin, so on-move persistence is
 *              justified here (no periodic write).
 *          [FA] ماندگاری: اسلات‌های ۲۰۰..۲۰۲ فقط هنگام تغییر واقعی میزبان
 *              ذخیرهٔ فلش می‌شوند (بارها در روز با باتریِ خیلی خراب)؛ با
 *              تحمل ~۱۰٬۰۰۰ بار پاک‌سازی، حاشیهٔ فرسودگی دهه‌هاست - پس حتی
 *              ذخیرهٔ فوری هم هزینه‌ای ندارد، چه رسد به دبونس.
 */

#include "imbalance.h"

#include <stddef.h>   /* NULL */

#if defined(__has_include)
#if __has_include("cmsis_os2.h")
#include "cmsis_os2.h"
#define IMBAL_HAS_RTOS_LOCK 1
#endif
#endif

/* ==================== Compile-time invariants / ناورداهای زمان کامپایل ==================== */
_Static_assert(IMBAL_PARAM_LAST_ID >= IMBAL_PARAM_FIRST_ID, "param range");
_Static_assert(IMBAL_PARAM_TABLE_SIZE <= 32u, "table size");
_Static_assert(IMBAL_PARAM_FIRST_ID2 > IMBAL_PARAM_LAST_ID, "the two id blocks must not overlap");
_Static_assert(IMBAL_PARAM_FIRST_ID3 > IMBAL_PARAM_LAST_ID2, "the three id blocks must not overlap");
_Static_assert(IMBAL_PARAM_FIRST_ID4 > IMBAL_PARAM_LAST_ID3, "the four id blocks must not overlap");

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
 * [FA] آخرین خروجی‌ها برای مصرف‌کنندگان ‎TLM/UI‎. */
static imbalance_outputs_t IMBAL_OUTPUTS_T__G__Last;

/* [EN] Timing/episode bookkeeping (RAM only).
 * [FA] دفترچهٔ زمانی/اپیزود (فقط RAM). */
static bool     BOOL__G__HasChargeEnd;      /* [EN] A charge session ended at least once / حداقل یک پایان شارژ */
static uint32_t UINT32_T__G__RestAnchorMs;  /* [EN] Rest window anchor: last charge end, or boot / لنگر استراحت */
static bool     BOOL__G__ChargingEdge;      /* [EN] charging seen at startup? / تشخیص لبهٔ اول */
static uint32_t UINT32_T__G__ChargeStartMs; /* [EN] This session's start / شروع این سیکل */
static bool     BOOL__G__PrevCharging;      /* [EN] Edge memory / حافظهٔ لبه */
static bool     BOOL__G__PrevThermalRest;   /* [EN] Thermal-rest edge memory / حافظهٔ لبهٔ استراحت حرارتی */
static bool     BOOL__G__CycleFullSeen;      /* [EN] This cycle reached FLOAT / این سیکل به فول رسید */
static bool     BOOL__G__RequireFreshFullCycle; /* [EN] Ignore stale FLOAT after a battery swap / فول تازه لازم است */
static bool     BOOL__G__CycleHasImbalance;  /* [EN] Deferred candidate / نامزدِ معوق */
static bool     BOOL__G__CycleEventRecorded; /* [EN] One event already committed / رویداد ثبت‌شدهٔ همین سیکل */
static uint8_t  UINT8_T__G__CleanFullCycles; /* [EN] Consecutive full cycles without event / سیکل‌های کامل پاک */
static bool     BOOL__G__Episode;           /* [EN] Episode in progress / اپیزود جاری */
static uint32_t UINT32_T__G__EpisodeLimitMv;/* [EN] Limit that armed the episode / حد لحظهٔ شروع اپیزود */
static uint32_t UINT32_T__G__OverSinceMs;   /* [EN] Over-limit continuous start / شروع دویش فراتر از حد */
static bool     BOOL__G__OverTiming;        /* [EN] Over-limit timing active / اندازه در جریان */
static uint32_t UINT32_T__G__AbsentSinceMs; /* [EN] Battery-absent continuous start / شروع نبود باتری */
static bool     BOOL__G__AbsentTiming;      /* [EN] Absence timer active / تایمر نبود فعال */
static uint32_t UINT32_T__G__LastBeepMs;    /* [EN] Last latch beep / آخرین بوق قفل */
static bool     BOOL__G__ManualResetPending;/* [EN] Manual reset pending nowMs anchor / ریست دستی در انتظار تیک زمان */

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
        case IMBAL_PARAM_LATCH_BEEP_COUNT:      uint32_t__ret = IMBAL_DEF_LATCH_BEEP_COUNT;      break;
        case IMBAL_PARAM_LATCH_BEEP_GAP_MS:     uint32_t__ret = IMBAL_DEF_LATCH_BEEP_GAP_MS;     break;
        case IMBAL_PARAM_CLEAN_FULL_CYCLES:     uint32_t__ret = IMBAL_DEF_CLEAN_FULL_CYCLES;  break;
        default:                               uint32_t__ret = 0u;                             break;
    }

    return uint32_t__ret;
}

/* ==================== Imbalance_ParamClamp ==================== */

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
        case IMBAL_PARAM_LATCH_BEEP_COUNT:
            if (uint32_t__ret < 1u) { uint32_t__ret = 1u; }
            if (uint32_t__ret > IMBAL_MAX_BEEP_COUNT) { uint32_t__ret = IMBAL_MAX_BEEP_COUNT; }
            break;
        case IMBAL_PARAM_LATCH_BEEP_GAP_MS:
            if (uint32_t__ret > IMBAL_MAX_BEEP_GAP_MS) { uint32_t__ret = IMBAL_MAX_BEEP_GAP_MS; }
            break;
        case IMBAL_PARAM_CLEAN_FULL_CYCLES:
            if (uint32_t__ret < 1u) { uint32_t__ret = 1u; }
            if (uint32_t__ret > IMBAL_MAX_COUNT) { uint32_t__ret = IMBAL_MAX_COUNT; }
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

/* [EN] Audit fix 2026-10-06: seed the compiled defaults exactly once, on the
 *      first touch of the table. func__EspLink_NvmInit() replays the stored
 *      panel values before the scheduler starts, while func__Imbalance_Init()
 *      runs later inside the control thread; the old unconditional default
 *      loop therefore overwrote every restored scenario-5 setting on each
 *      power-up. Seeding once keeps the documented rule "a module Init never
 *      undoes a panel/flash value" and changes nothing for a fresh board.
 * [FA] اصلاح ممیزی ۲۰۲۶-۱۰-۰۶: پیش‌فرض‌های کامپایل فقط یک‌بار و در نخستین
 *      دسترسی به جدول نوشته می‌شوند. ‎func__EspLink_NvmInit()‎ مقادیر ذخیره‌شدهٔ
 *      پنل را پیش از شروع ‎scheduler‎ بازپخش می‌کند، ولی ‎func__Imbalance_Init()‎
 *      بعداً داخل نخ کنترل اجرا می‌شود؛ پس حلقهٔ پیش‌فرضِ بی‌قیدِ قبلی هر بار
 *      با روشن‌شدن، تنظیم‌های بازیابی‌شدهٔ سناریو ۵ را پاک می‌کرد. یک‌بار
 *      مقداردهی، قانون مستند «‎Init‎ ماژول هیچ‌وقت مقدار پنل/فلش را برنمی‌گرداند»
 *      را حفظ می‌کند و برای برد نو هیچ تغییری ندارد. */
static bool BOOL__G__ParamsSeeded;

/* ==================== Imbalance_SeedDefaultsOnce ==================== */

static void func__Imbalance_SeedDefaultsOnce(void)
{
    uint8_t uint8_t__id;

    if (BOOL__G__ParamsSeeded != false)
    {
        return;
    }

    BOOL__G__ParamsSeeded = true;

    for (uint8_t__id = IMBAL_PARAM_FIRST_ID; uint8_t__id <= IMBAL_PARAM_LAST_ID4; uint8_t__id++)
    {
        if (IMBAL_PARAM_OWNS(uint8_t__id))
        {
            UINT32_T__G__A__Param[IMBAL_PARAM_INDEX(uint8_t__id)] =
                func__Imbalance_ParamDefault(uint8_t__id);
        }
    }
}

/* ==================== Imbalance_ReadParam ==================== */

static uint32_t func__Imbalance_ReadParam(uint8_t uint8_t__paramId)
{
    func__Imbalance_SeedDefaultsOnce();
    return UINT32_T__G__A__Param[IMBAL_PARAM_INDEX(uint8_t__paramId)];
}

/* ==================== Public API / API عمومی ==================== */

void func__Imbalance_Init(void)
{
    /* [EN] Defaults are seeded once (see func__Imbalance_SeedDefaultsOnce);
     *      a value already applied by the panel or the NVM replay survives.
     * [FA] پیش‌فرض‌ها یک‌بار نوشته می‌شوند (به ‎func__Imbalance_SeedDefaultsOnce‎
     *      نگاه کنید)؛ مقداری که پنل یا بازپخش ‎NVM‎ اعمال کرده باقی می‌ماند. */
    func__Imbalance_SeedDefaultsOnce();

    /* [EN] Persisted fields are NOT reset here: EspLink NVM replay may run
     *      before or after Init, and both orders must land on the stored
     *      values. Boot RAM starts zeroed by the runtime and replay fills it.
     * [FA] مقادیر ماندگار اینجا صفر نمی‌شوند؛ بوت = صفر زمان‌اجرا و پخش NVM
     *      پرشان می‌کند. */

    BOOL__G__HasChargeEnd     = false;
    UINT32_T__G__RestAnchorMs = 0u;      /* [EN] 0 = anchored at boot / صفر یعنی از بوت */
    BOOL__G__ChargingEdge      = false;
    UINT32_T__G__ChargeStartMs = 0u;
    BOOL__G__PrevCharging      = false;
    BOOL__G__PrevThermalRest   = false;
    BOOL__G__CycleFullSeen     = false;
    BOOL__G__RequireFreshFullCycle = false;
    BOOL__G__CycleHasImbalance = false;
    BOOL__G__CycleEventRecorded = false;
    UINT8_T__G__CleanFullCycles = 0u;
    BOOL__G__Episode           = false;
    UINT32_T__G__EpisodeLimitMv = 0u;
    UINT32_T__G__OverSinceMs  = 0u;
    BOOL__G__OverTiming       = false;
    UINT32_T__G__AbsentSinceMs = 0u;
    BOOL__G__AbsentTiming     = false;
    /* [EN] First latch beep due exactly one period after latch; init to 0 so
     *      a latch at boot+before-first-period behaves deterministically.
     * [FA] اولین بوق دقیقاً یک دوره پس از قفل. */

    UINT32_T__G__LastBeepMs   = 0u;
    BOOL__G__ManualResetPending = false;

    IMBAL_OUTPUTS_T__G__Last.uint32_t__imbalanceMv  = 0u;
    IMBAL_OUTPUTS_T__G__Last.bool__episode          = false;
    IMBAL_OUTPUTS_T__G__Last.uint32_t__events       = UINT32_T__G__Events;
    IMBAL_OUTPUTS_T__G__Last.bool__latched          = (UINT32_T__G__Latched != 0u);
    IMBAL_OUTPUTS_T__G__Last.uint32_t__latchedCycles = UINT32_T__G__LatchedCycles;
    IMBAL_OUTPUTS_T__G__Last.bool__blockOutput      =
        ((UINT32_T__G__Latched != 0u) &&
         (func__Imbalance_ReadParam(IMBAL_PARAM_BLOCK_OUTPUT_EN) != 0u));
    IMBAL_OUTPUTS_T__G__Last.bool__chargingAllowed  =
        ((UINT32_T__G__Latched == 0u) ||
         (UINT32_T__G__LatchedCycles <
          func__Imbalance_ReadParam(IMBAL_PARAM_MAX_LATCHED_CYCLES)));
    IMBAL_OUTPUTS_T__G__Last.bool__beepDue          = false;
    IMBAL_OUTPUTS_T__G__Last.bool__halvesMismatch   = false;
}

/* ==================== Imbalance_GetOutputs ==================== */

void func__Imbalance_GetOutputs(imbalance_outputs_t *imbalance_outputs_t__outputs)
{
    if (imbalance_outputs_t__outputs != NULL)
    {
        *imbalance_outputs_t__outputs = IMBAL_OUTPUTS_T__G__Last;
    }
}

/**
 * @brief  [EN] Drop every RAM-only charge-cycle/episode value after a battery
 *              absence signal. A new charging edge is required before a stale
 *              FLOAT indication can qualify a cycle again.
 *         [FA] همهٔ وضعیت‌های موقت چرخه/رویداد را پس از نبود باتری پاک می‌کند؛
 *              پیش از پذیرش FLOAT، شروع یک شارژ تازه لازم است.
 * @param  uint32_t__nowMs [EN] Current monotonic time in ms / زمان یکنواخت فعلی، میلی‌ثانیه
 * @return void [EN] No return value / خروجی ندارد
 */
static void func__Imbalance_ResetCycleState(uint32_t uint32_t__nowMs)
{
    BOOL__G__HasChargeEnd       = false;
    UINT32_T__G__RestAnchorMs   = uint32_t__nowMs;
    BOOL__G__ChargingEdge       = false;
    UINT32_T__G__ChargeStartMs  = uint32_t__nowMs;
    BOOL__G__PrevCharging       = false;
    BOOL__G__PrevThermalRest    = false;
    BOOL__G__CycleFullSeen      = false;
    BOOL__G__RequireFreshFullCycle = true;
    BOOL__G__CycleHasImbalance  = false;
    BOOL__G__CycleEventRecorded = false;
    UINT8_T__G__CleanFullCycles = 0u;
    BOOL__G__Episode            = false;
    UINT32_T__G__EpisodeLimitMv = 0u;
    UINT32_T__G__OverSinceMs    = 0u;
    BOOL__G__OverTiming         = false;
    UINT32_T__G__LastBeepMs     = uint32_t__nowMs;
}

/**
 * @brief  [EN] Clear battery-owned persistent counters/latch after a
 *              qualified battery swap.
 *         [FA] شمارنده‌ها و قفلِ متعلق به باتری را پس از تأیید تعویض پاک می‌کند.
 * @‎return bool [EN] true iff an NVM-backed value changed‎ / اگر مقدار ماندگار عوض شد true
 */
static bool func__Imbalance_ClearBatteryState(void)
{
    bool bool__changed = ((UINT32_T__G__Events != 0u) ||
                          (UINT32_T__G__LatchedCycles != 0u) ||
                          (UINT32_T__G__Latched != 0u));

    UINT32_T__G__Events        = 0u;
    UINT32_T__G__LatchedCycles = 0u;
    UINT32_T__G__Latched       = 0u;

    return bool__changed;
}

/**
 * @brief  [EN] Publish the current live and persisted Imbalance outputs.
 *         [FA] خروجی زنده و ماندگارِ عدم‌توازن را برای مصرف‌کننده منتشر می‌کند.
 * @‎param  uint32_t__imbalanceMv [EN] Absolute half-voltage difference in mV‎ / اختلاف دو نیم، میلی‌ولت
 * @‎param  bool__sameState [EN] true when both charger channels match / true‎ اگر دو کانال هم‌حالت‌اند
 * @‎param  bool__beepDue [EN] One-pass beep pulse‎ / پالس بوق همین پاس
 * @param  imbalance_outputs_t__outputs [EN] Optional output copy / کپی خروجی، ممکن است NULL باشد
 * @return void [EN] No return value / خروجی ندارد
 */
static void func__Imbalance_PublishOutputs(uint32_t uint32_t__imbalanceMv,
                                           bool bool__sameState,
                                           bool bool__beepDue,
                                           imbalance_outputs_t *imbalance_outputs_t__outputs)
{
    IMBAL_OUTPUTS_T__G__Last.bool__halvesMismatch = (bool__sameState == false);
    IMBAL_OUTPUTS_T__G__Last.uint32_t__imbalanceMv = uint32_t__imbalanceMv;
    IMBAL_OUTPUTS_T__G__Last.bool__episode = BOOL__G__Episode;
    IMBAL_OUTPUTS_T__G__Last.uint32_t__events = UINT32_T__G__Events;
    IMBAL_OUTPUTS_T__G__Last.bool__latched = (UINT32_T__G__Latched != 0u);
    IMBAL_OUTPUTS_T__G__Last.uint32_t__latchedCycles = UINT32_T__G__LatchedCycles;
    IMBAL_OUTPUTS_T__G__Last.bool__blockOutput =
        ((UINT32_T__G__Latched != 0u) &&
         (func__Imbalance_ReadParam(IMBAL_PARAM_BLOCK_OUTPUT_EN) != 0u));
    IMBAL_OUTPUTS_T__G__Last.bool__chargingAllowed =
        ((UINT32_T__G__Latched == 0u) ||
         (UINT32_T__G__LatchedCycles < func__Imbalance_ReadParam(IMBAL_PARAM_MAX_LATCHED_CYCLES)));
    IMBAL_OUTPUTS_T__G__Last.bool__beepDue = bool__beepDue;

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
    bool     bool__chargeComplete;
    bool     bool__sameState;
    bool     bool__justLatched;
    bool     bool__beepDue;
    bool     bool__absenceElapsed;

    bool__persistChanged = false;
    bool__windowOpen     = false;
    uint32_t__activeLimitMv  = 0u;
    bool__justLatched    = false;
    bool__beepDue        = false;
    bool__absenceElapsed = false;

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
        /* [EN] Invalid data breaks the continuous-absence proof; never count
           its elapsed time toward a battery-swap reset.
           [FA] نمونهٔ نامعتبر پیوستگی نبود را می‌شکند؛ زمانش برای ریست شمرده نشود. */
        BOOL__G__AbsentTiming = false;
        UINT32_T__G__AbsentSinceMs = 0u;
        if (imbalance_outputs_t__outputs != NULL)
        {
            *imbalance_outputs_t__outputs = IMBAL_OUTPUTS_T__G__Last;
        }
        return false;
    }

    /* ---- charge-session edges and full-charge qualification ---- */
    bool__charging      = imbalance_inputs_t__inputs->bool__charging;
    if (BOOL__G__ManualResetPending != false)
    {
        /* [EN] Manual reset from the panel clears all Scenario 5 timers and
           anchors the current charge/rest clock to nowMs so counting restarts
           cleanly from zero.
           [FA] ریست دستی از پنل همهٔ تایمرهای سناریوی ۵ را صفر می‌کند و مبدأ
           زمان شارژ و استراحت را روی لحظهٔ فعلی می‌گذارد تا از اول بشمارد. */
        BOOL__G__ManualResetPending    = false;
        BOOL__G__HasChargeEnd          = false;
        UINT32_T__G__RestAnchorMs      = uint32_t__nowMs;
        BOOL__G__ChargingEdge          = bool__charging;
        UINT32_T__G__ChargeStartMs     = uint32_t__nowMs;
        BOOL__G__PrevCharging          = bool__charging;
        BOOL__G__PrevThermalRest       = imbalance_inputs_t__inputs->bool__thermalRest;
        BOOL__G__CycleFullSeen         = false;
        BOOL__G__RequireFreshFullCycle = (bool__charging == false);
        BOOL__G__CycleHasImbalance     = false;
        BOOL__G__CycleEventRecorded    = false;
        UINT8_T__G__CleanFullCycles    = 0u;
        BOOL__G__Episode               = false;
        UINT32_T__G__EpisodeLimitMv    = 0u;
        UINT32_T__G__OverSinceMs       = 0u;
        BOOL__G__OverTiming            = false;
        UINT32_T__G__LastBeepMs        = uint32_t__nowMs;
    }
    if ((imbalance_inputs_t__inputs->bool__thermalRest == true) ||
        (BOOL__G__PrevThermalRest == true))
    {
        /* [EN] During the charger's 1-hour thermal rest (and at the pass it
           exits), reset the imbalance timers and current-cycle candidate (user
           order 2026-10-22: if the user added higher-capacity batteries, long
           charging with thermal rests must reset the imbalance and dead-battery
           timers) without double-counting post-latch cycles when BULK resumes.
           [FA] در زمان استراحت حرارتی ۱ ساعتهٔ شارژر (و لحظهٔ خروج از آن)،
           تایمرها و نامزد عدم‌توازن ریست می‌شوند (دستور کاربر ۲۰۲۶-۱۰-۲۲: در
           صورت افزایش ظرفیت باتری‌ها توسط کاربر و طولانی‌شدن زمان شارژ، تایمرها
           در استراحت ۱ ساعته ریست شوند) و سیکل قفل هنگام ادامهٔ بالک دوباره
           شمرده نمی‌شود. */
        UINT32_T__G__ChargeStartMs  = uint32_t__nowMs;
        UINT32_T__G__RestAnchorMs   = uint32_t__nowMs;
        BOOL__G__OverTiming         = false;
        UINT32_T__G__OverSinceMs    = 0u;
        BOOL__G__Episode            = false;
        UINT32_T__G__EpisodeLimitMv = 0u;
        BOOL__G__CycleFullSeen      = false;
        BOOL__G__CycleHasImbalance  = false;
        BOOL__G__CycleEventRecorded = false;
        if (imbalance_inputs_t__inputs->bool__thermalRest == true)
        {
            bool__charging = BOOL__G__PrevCharging;
        }
    }
    BOOL__G__PrevThermalRest = imbalance_inputs_t__inputs->bool__thermalRest;
    bool__chargeComplete = imbalance_inputs_t__inputs->bool__chargeComplete;

    /* [EN] v1.72 (user order): comparable only when both halves are in the
           same state. One charging and one resting is a charger-made
           difference, never an imbalance verdict.
       [FA] فقط وقتی قابل مقایسه‌اند که هر دو نیم هم‌حالت باشند. */
    bool__sameState = (imbalance_inputs_t__inputs->bool__chargingCh1 ==
                       imbalance_inputs_t__inputs->bool__chargingCh2);

    /* ---- battery-removal priority: quarantine, then full reset ---- */
    if (imbalance_inputs_t__inputs->bool__batAbsent == true)
    {
        if (BOOL__G__AbsentTiming == false)
        {
            BOOL__G__AbsentTiming     = true;
            UINT32_T__G__AbsentSinceMs = uint32_t__nowMs;
        }
        bool__absenceElapsed =
            ((uint32_t)(uint32_t__nowMs - UINT32_T__G__AbsentSinceMs) >=
             IMBAL_ABSENT_RESET_MS);

        /* [EN] While the battery is absent, discard any in-flight candidate
           and skip cycle/window evaluation. This prevents the same pass that
           clears NVM slots from immediately recreating an event or latch.
           [FA] هنگام نبود باتری، نامزد و چرخهٔ نیمه‌کاره کنار گذاشته می‌شود؛
           این دور پس از ریست وارد ارزیابی عادی نمی‌شود. */
        func__Imbalance_ResetCycleState(uint32_t__nowMs);
        if (bool__absenceElapsed != false)
        {
            bool__persistChanged = func__Imbalance_ClearBatteryState();
        }
        func__Imbalance_PublishOutputs(0u, bool__sameState, false,
                                       imbalance_outputs_t__outputs);
        return bool__persistChanged;
    }

    /* [EN] Also honor the full 3 s if the first present sample arrives just
       after the threshold; a reconnect must not erase an already-qualified
       absence interval. [FA] اگر نمونهٔ اتصال پس از مرز ۳s آمد، همان‌جا ریست کن. */
    if (BOOL__G__AbsentTiming != false)
    {
        bool__absenceElapsed =
            ((uint32_t)(uint32_t__nowMs - UINT32_T__G__AbsentSinceMs) >=
             IMBAL_ABSENT_RESET_MS);
        BOOL__G__AbsentTiming = false;
        UINT32_T__G__AbsentSinceMs = 0u;
        if (bool__absenceElapsed != false)
        {
            bool__persistChanged = func__Imbalance_ClearBatteryState();
            func__Imbalance_ResetCycleState(uint32_t__nowMs);
            func__Imbalance_PublishOutputs(0u, bool__sameState, false,
                                           imbalance_outputs_t__outputs);
            return bool__persistChanged;
        }
    }

    if ((bool__charging == true) && (BOOL__G__PrevCharging == false))
    {
        /* [EN] A new cycle starts here. Only a PREVIOUS cycle that reached
           FLOAT can be clean; an interrupted charge is neither clean nor an
           imbalance event. The configured number of consecutive full cycles
           (param 136, default three) with no committed event clears only the
           stored imbalance counter.
           [FA] سیکل جدید از اینجا شروع می‌شود. فقط سیکلی که قبلاً به FLOAT
           رسیده «پاک» است؛ شارژ نیمه‌کاره نه پاک است نه رویداد. تعداد تنظیم‌شده
           (پارامتر ۱۳۶، پیش‌فرض سه) سیکل کامل بدون رویداد فقط شمارندهٔ
           ماندگار عدم‌توازن را پاک می‌کند. */
        if (BOOL__G__CycleFullSeen == true)
        {
            if (BOOL__G__CycleEventRecorded == true)
            {
                UINT8_T__G__CleanFullCycles = 0u;
            }
            else if (UINT8_T__G__CleanFullCycles <
                     (uint8_t)func__Imbalance_ReadParam(IMBAL_PARAM_CLEAN_FULL_CYCLES))
            {
                UINT8_T__G__CleanFullCycles++;
            }

            if (UINT8_T__G__CleanFullCycles >=
                (uint8_t)func__Imbalance_ReadParam(IMBAL_PARAM_CLEAN_FULL_CYCLES))
            {
                /* [EN] The three-clean-cycle rule clears only the imbalance
                   event counter. The existing latch and its post-latch-cycle
                   budget are deliberately preserved; battery absence remains
                   the reset path for those values.
                   [FA] قانون سه سیکل پاک فقط شمارندهٔ عدم‌توازن را پاک می‌کند؛
                   قفل و بودجهٔ سیکلِ قفل طبق قرارداد قبلی حفظ می‌شوند و مسیر
                   ریست آن‌ها نبود واقعی باتری است. */
                if (UINT32_T__G__Events != 0u)
                {
                    UINT32_T__G__Events  = 0u;
                    bool__persistChanged = true;
                }
                UINT8_T__G__CleanFullCycles = 0u;
            }
            BOOL__G__CycleFullSeen      = false;
            BOOL__G__CycleHasImbalance  = false;
            BOOL__G__CycleEventRecorded = false;
        }
        else
        {
            /* [EN] Preserve a discharge candidate across the rising edge of the
               charge that follows it so the full charge + rest can verify it.
               [FA] نامزد دشارژ هنگام شروع شارژِ بعدی حفظ می‌شود تا پس از شارژ
               کامل و استراحت بررسی شود. */
            BOOL__G__CycleEventRecorded = false;
        }
        BOOL__G__OverTiming         = false;
        BOOL__G__ChargingEdge       = true;
        BOOL__G__RequireFreshFullCycle = false;
        UINT32_T__G__ChargeStartMs  = uint32_t__nowMs;

        /* [EN] Count a post-latch charging cycle only while within the
           ‎configured post-latch cycle budget. [FA]‎ شمارش سیکل قفل فقط تا سقف
           تنظیم‌شدهٔ سیکل‌های مجاز پس از قفل. */
        if (UINT32_T__G__Latched != 0u)
        {
            if (UINT32_T__G__LatchedCycles <
                func__Imbalance_ReadParam(IMBAL_PARAM_MAX_LATCHED_CYCLES))
            {
                UINT32_T__G__LatchedCycles++;
                bool__persistChanged = true;
            }
        }
    }
    if ((bool__charging == false) && (BOOL__G__PrevCharging == true))
    {
        /* [EN] Falling edge: anchor the rest window and require a fresh
           stability proof in post-charge rest before committing an event.
           If FLOAT was not reached, discard the candidate and do not register
           an event.
           [FA] لبهٔ نزولی: لنگر استراحت و بازآغاز تایمر پایداری در استراحت پس از
           شارژ؛ اگر به FLOAT نرسیده، نامزد دور ریخته می‌شود. */
        BOOL__G__HasChargeEnd     = true;
        UINT32_T__G__RestAnchorMs = uint32_t__nowMs;
        BOOL__G__OverTiming       = false;
        BOOL__G__Episode          = false;
        if (bool__chargeComplete == false)
        {
            BOOL__G__CycleFullSeen      = false;
            BOOL__G__CycleHasImbalance  = false;
            BOOL__G__CycleEventRecorded = false;
        }
    }

    /* [EN] IsChargeComplete is sampled after the charger has parked in FLOAT.
       It qualifies this cycle, but it does not by itself create an event.
       [FA] کامل‌شدن فقط سیکل را معتبر می‌کند و به‌تنهایی رویداد نمی‌سازد. */
    if ((bool__chargeComplete == true) &&
        (imbalance_inputs_t__inputs->bool__onBattery == false) &&
        (BOOL__G__RequireFreshFullCycle == false))
    {
        BOOL__G__CycleFullSeen = true;
    }
    BOOL__G__PrevCharging = bool__charging;

    /* ---- window classification / طبقه‌بندی پنجره ----
     * [EN] discharge: output on battery (and not charging) -> immediate,
     *      discharge limit.
     *      charging: after param 111 since session start (0 = off), rest limit.
     *      rest: after param 110 since the last charge end (or since boot if
     *      none yet - a battery that sits at rest from boot is exactly what
     *      the user wants measured), rest limit; 0 = rest eval off.
     * [FA] دشارژ: فوری با حد دشارژ. حین شارژ: پس از ۱۱۱ با حد استراحت.
     *      استراحت: پس از ۱۱۰ از پایان آخرین شارژ (یا از بوت). */
    if ((imbalance_inputs_t__inputs->bool__onBattery == true) &&
        (bool__charging == false))
    {
        if (BOOL__G__CycleEventRecorded == false)
        {
            bool__windowOpen    = true;
            uint32_t__activeLimitMv = func__Imbalance_ReadParam(IMBAL_PARAM_DISCHARGE_LIMIT_MV);
        }
    }

    if ((bool__windowOpen == false) &&
        (imbalance_inputs_t__inputs->bool__thermalRest == false))
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
        else if (BOOL__G__CycleFullSeen == true)
        {
            uint32_t uint32_t__waitMs = func__Imbalance_ReadParam(IMBAL_PARAM_POST_CHARGE_WAIT_MS);

            if (uint32_t__waitMs != 0u)
            {
                if (BOOL__G__HasChargeEnd == false)
                {
                    /* [EN] No charge ended yet: anchor at boot (or latest reset)
                     *      so rest is monitored after 110 ms-time from anchor.
                     * [FA] هنوز شارژی تمام نشده: لنگر = بوت یا آخرین ریست. */
                    if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__RestAnchorMs) >= uint32_t__waitMs)
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

    /* [EN] v1.72: halves out of step or charger in thermal rest - shut the
           window and drop the stability timer, so nothing counts while they
           are incomparable or paused in mid-charge thermal rest.
       [FA] دو نیم هم‌حالت نیستند یا شارژر در استراحت حرارتی است: پنجره بسته و
           تایمر پایداری صفر می‌شود. */
    if ((bool__sameState == false) ||
        (imbalance_inputs_t__inputs->bool__thermalRest == true))
    {
        bool__windowOpen = false;
        BOOL__G__OverTiming = false;
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
                    /* [EN] A stable condition is only a candidate until the
                       cycle reaches FLOAT and post-charge rest confirms it.
                       [FA] شرط پایدار تا رسیدن سیکل به FLOAT و تأیید در حالت
                       استراحت فقط نامزد است. */
                    BOOL__G__CycleHasImbalance = true;
                    BOOL__G__Episode          = true;
                    UINT32_T__G__EpisodeLimitMv = uint32_t__activeLimitMv;
                    BOOL__G__OverTiming       = false;
                    if ((imbalance_inputs_t__inputs->bool__onBattery == true) &&
                        (bool__charging == false))
                    {
                        BOOL__G__CycleFullSeen         = false;
                        BOOL__G__RequireFreshFullCycle = true;
                    }
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

    /* [EN] Commit at most one event for this cycle, and only after FLOAT
       once the post-charge rest delay (param 110) has elapsed and the rest
       voltage difference has stayed above param 108 for the stability duration
       (param 112, proven by BOOL__G__Episode in the rest window).
       [FA] حداکثر یک رویداد برای هر سیکل، فقط بعد از FLOAT و گذشت زمان انتظار
       استراحت (۱۱۰)، در صورتی که در حالت استراحت اختلاف بیش از حد (۱۰۸) به مدت
       پایداری (۱۱۲) برقرار مانده باشد. */
    if ((BOOL__G__CycleFullSeen == true) &&
        (bool__charging == false) &&
        (imbalance_inputs_t__inputs->bool__onBattery == false) &&
        (bool__sameState == true) &&
        (BOOL__G__CycleHasImbalance == true) &&
        (BOOL__G__Episode == true) &&
        (BOOL__G__CycleEventRecorded == false))
    {
        uint32_t uint32_t__restWaitMs = func__Imbalance_ReadParam(IMBAL_PARAM_POST_CHARGE_WAIT_MS);
        bool     bool__restReady      = ((uint32_t)(uint32_t__nowMs - UINT32_T__G__RestAnchorMs) >=
                                         uint32_t__restWaitMs);

        if ((bool__restReady == true) &&
            (uint32_t__imbalanceMv > func__Imbalance_ReadParam(IMBAL_PARAM_REST_LIMIT_MV)))
        {
            if (UINT32_T__G__Events < IMBAL_MAX_COUNT)
            {
                UINT32_T__G__Events++;
                bool__persistChanged = true;
            }
            BOOL__G__CycleEventRecorded = true;
            UINT8_T__G__CleanFullCycles = 0u;
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
    if (UINT32_T__G__Latched != 0u)
    {
        uint32_t uint32_t__periodMs = func__Imbalance_ReadParam(IMBAL_PARAM_LATCH_BEEP_PERIOD_MS);

        if ((bool__justLatched == true) && (uint32_t__periodMs != 0u))
        {
            /* [EN] Confirmation beep at the latch moment: the operator must
             *      hear the verdict immediately, then hourly.
             * [FA] بوق تأیید در لحظهٔ قفل، سپس هر یک دوره. */
            bool__beepDue = true;
            UINT32_T__G__LastBeepMs = uint32_t__nowMs;
        }
        else if ((uint32_t__periodMs != 0u) &&
                 ((uint32_t__nowMs - UINT32_T__G__LastBeepMs) >= uint32_t__periodMs))
        {
            bool__beepDue = true;
            UINT32_T__G__LastBeepMs = uint32_t__nowMs;
        }
        else
        {
            /* [EN] No beep due this pass. / این پاس بوقی نیست. */
        }
    }

    func__Imbalance_PublishOutputs(uint32_t__imbalanceMv, bool__sameState,
                                   bool__beepDue, imbalance_outputs_t__outputs);

    return bool__persistChanged;
}

bool func__Imbalance_SetParam(uint8_t uint8_t__paramId,
                              uint32_t uint32_t__value,
                              uint32_t *uint32_t__appliedValue)
{
    bool bool__ret = false;
#if defined(IMBAL_HAS_RTOS_LOCK)
    int32_t int32_t__savedKernelLock = osKernelLock();
#endif

    /* [EN] Make sure defaults exist before any panel/NVM write or read.
     * [FA] پیش از هر نوشتن/خواندن پنل یا ‎NVM‎، وجود پیش‌فرض‌ها تضمین می‌شود. */
    func__Imbalance_SeedDefaultsOnce();

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
        /* [EN] Runtime slots: NVM boot replay plus the dedicated ESP latch-reset
         *      action (not ordinary XIDS/backup edits); clamp to the u8 budget.
         * [FA] اسلات‌های زمان‌اجرا: پخش NVM و مسیر مستقل ریست قفل ESP؛ نه
         *      ویرایش معمولی پنل/پشتیبان. گیره مطابق بودجهٔ ۸بیتی است. */
        uint32_t uint32_t__clamped = (uint32_t__value > IMBAL_MAX_COUNT) ? IMBAL_MAX_COUNT : uint32_t__value;

        switch (uint8_t__paramId)
        {
            case IMBAL_SLOT_EVENTS_ID:
                UINT32_T__G__Events = uint32_t__clamped;
                if (uint32_t__clamped == 0u)
                {
                    BOOL__G__HasChargeEnd          = false;
                    BOOL__G__CycleFullSeen         = false;
                    BOOL__G__RequireFreshFullCycle = true;
                    BOOL__G__Episode               = false;
                    UINT32_T__G__EpisodeLimitMv    = 0u;
                    UINT32_T__G__OverSinceMs       = 0u;
                    BOOL__G__OverTiming            = false;
                    BOOL__G__CycleHasImbalance     = false;
                    BOOL__G__CycleEventRecorded    = false;
                    UINT8_T__G__CleanFullCycles    = 0u;
                    BOOL__G__ManualResetPending    = true;
                }
                else
                {
                    BOOL__G__ManualResetPending = false;
                }
                break;
            case IMBAL_SLOT_CYCLES_ID: UINT32_T__G__LatchedCycles = uint32_t__clamped; break;
            case IMBAL_SLOT_LATCH_ID:
                UINT32_T__G__Latched = (uint32_t__clamped != 0u) ? 1u : 0u;
                if ((uint32_t__clamped == 0u) && (UINT32_T__G__Events == 0u))
                {
                    BOOL__G__HasChargeEnd          = false;
                    BOOL__G__CycleFullSeen         = false;
                    BOOL__G__RequireFreshFullCycle = true;
                    BOOL__G__Episode               = false;
                    UINT32_T__G__EpisodeLimitMv    = 0u;
                    UINT32_T__G__OverSinceMs       = 0u;
                    BOOL__G__OverTiming            = false;
                    BOOL__G__CycleHasImbalance     = false;
                    BOOL__G__CycleEventRecorded    = false;
                    UINT8_T__G__CleanFullCycles    = 0u;
                    BOOL__G__ManualResetPending    = true;
                }
                break;
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

    if (bool__ret == true)
    {
        IMBAL_OUTPUTS_T__G__Last.bool__episode = BOOL__G__Episode;
        IMBAL_OUTPUTS_T__G__Last.uint32_t__events = UINT32_T__G__Events;
        IMBAL_OUTPUTS_T__G__Last.bool__latched = (UINT32_T__G__Latched != 0u);
        IMBAL_OUTPUTS_T__G__Last.uint32_t__latchedCycles = UINT32_T__G__LatchedCycles;
        IMBAL_OUTPUTS_T__G__Last.bool__blockOutput =
            ((UINT32_T__G__Latched != 0u) &&
             (func__Imbalance_ReadParam(IMBAL_PARAM_BLOCK_OUTPUT_EN) != 0u));
        IMBAL_OUTPUTS_T__G__Last.bool__chargingAllowed =
            ((UINT32_T__G__Latched == 0u) ||
             (UINT32_T__G__LatchedCycles <
              func__Imbalance_ReadParam(IMBAL_PARAM_MAX_LATCHED_CYCLES)));
    }

#if defined(IMBAL_HAS_RTOS_LOCK)
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
#endif

    return bool__ret;
}

bool func__Imbalance_GetParam(uint8_t uint8_t__paramId,
                              uint32_t *uint32_t__value)
{
    bool bool__ret = false;

    if (uint32_t__value == NULL)
    {
        return false;
    }

    /* [EN] Make sure defaults exist before any panel/NVM write or read.
     * [FA] پیش از هر نوشتن/خواندن پنل یا ‎NVM‎، وجود پیش‌فرض‌ها تضمین می‌شود. */
    func__Imbalance_SeedDefaultsOnce();

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
