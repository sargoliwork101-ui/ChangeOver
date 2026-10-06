/**
 * @file    imbalance.h
 * @brief   [EN] Scenario 5 - two-half battery imbalance (unnam) monitor.
 *              Watches |v_bat_high - v_bat_low| (the absolute difference of
 *              the two 12 V halves of the 24 V pack) against rest and
 *              discharge limits. A stable over-limit episode is a candidate
 *              during a partial charge, but it becomes one persisted event
 *              only after that charge reaches FLOAT; each completed charge
 *              cycle can add at most one event. Three consecutive complete
 *              cycles without a new event clear the persisted imbalance event
 *              counter; the existing latch and its cycle budget remain intact.
 *              The episode budget then latches the verdict:
 *              red LED + hourly short beep + (checkbox) battery never
 *              switched onto the output. All persisted state survives power
 *              loss; battery absence remains an immediate full reset path.
 *
 *              [EN] Evaluation time-gating (user spec): thresholds apply
 *              only (a) at rest, after param 110 from the end of a full
 *              charge cycle, and (b) during an ongoing charge, after param
 *              111 from charge start (0 disables).
 *              During charge the REST threshold is used - no separate
 *              charge threshold exists by design.
 *              In discharge (output running on battery) the discharge
 *              threshold (param 109) applies immediately, but the event is
 *              still committed only for a cycle that has reached FLOAT.
 *              The post-charge wait and in-charge wait accept up to 5 hours.
 *
 *          [FA] سناریوی ۵ - پایش عدم‌توازن (آنبالانس) دو نیم‌باتری.
 *              قدرمطلق اختلاف دو نیم به شارژ ۱۲ ولتی پک ۲۴ ولتی
 *              (|‎v_high - v_low|)‎ با حد استراحت و حد دشارژ سنجیده می‌شود،
 *              شرط فراتر از حد با زمان پایداری و هیسترزیس به نامزد تبدیل
 *              می‌شود و پس از FLOAT واقعی ثبت می‌گردد (هر سیکل حداکثر یک‌بار).
 *              پس از پر شدن سقف رویدادها قفل صادر می‌شود: LED قرمز چشمک‌زن
 *              + بوق کوتاه ساعتی + (با تیک کاربر) ممنوعیت سوئیچ شدن باتری
 *              روی خروجی.
 *              شارژ در حالت قفل آزاد می‌ماند تا بوق ساعتی کار کند، ولی پس
 *              از تعداد قابل‌تنظیم سیکل شارژ در حالت قفل، شارژ هم بسته
 *              می‌شود. سه سیکل کامل بدون رویداد جدید، فقط شمارندهٔ عدم‌توازن
 *              را صفر می‌کند؛ قفل و بودجهٔ آن حفظ می‌شوند. نبود باتری
 *              (تعویض واقعی) مسیر ریست فوری همهٔ این مقادیر است.
 *
 *              گیت زمانی ارزیابی: آستانه‌ها فقط (الف) در استراحت و پس از
 *              زمان پارامتر ۱۱۰ از پایان سیکل کامل و (ب) در حین شارژ پس از
 *              زمان پارامتر ۱۱۱ اعمال می‌شوند؛ هر دو زمان تا ۵ ساعت قابل
 *              تنظیم‌اند. حین شارژ همان آستانهٔ استراحت استفاده می‌شود.
 *              در دشارژ حد پارامتر ۱۰۹ بلافاصله دیده می‌شود، اما ثبت رویداد
 *              همچنان به کامل‌شدن سیکل و رسیدن به FLOAT وابسته است.
 *
 * @note    [EN] Pure logic: Evaluate() receives every input and the current
 *              ms time; no BSP, no RTOS, no globals from other modules.
 *              Consume func__Imbalance_GetOutputs() after each call.
 *          [FA] منطق خالص: Evaluate همهٔ ورودی‌ها و زمان فعلی را می‌گیرد؛
 *              بی‌وابستگی به ‎BSP/RTOS‎. خروجی‌ها را پس از هر فراخوانی بخوان.
 */

#ifndef IMBALANCE_H
#define IMBALANCE_H

/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Parameter ids (108..118) / شناسه‌های پارامتر ==================== */

/** [EN] Rest imbalance limit (mV, absolute difference), default 300 mV.
 *  [FA] حد عدم‌توازن در استراحت (میلی‌ولت، قدرمطلق)، پیش‌فرض ۳۰۰. */
#define IMBAL_PARAM_REST_LIMIT_MV            108u

/** [EN] Discharge imbalance limit (mV), default 500 mV.
 *  [FA] حد عدم‌توازن در دشارژ (میلی‌ولت)، پیش‌فرض ۵۰۰. */
#define IMBAL_PARAM_DISCHARGE_LIMIT_MV       109u

/** [EN] Wait after a full charge session end before rest evaluation (ms),
 *  default 600000 (10 min), max 18000000 (5 h); 0 disables rest evaluation.
 *  [FA] مکث پس از پایان سیکل کامل تا ارزیابی در استراحت (ms)، پیش‌فرض
 *  ۶۰۰۰۰۰، سقف ۱۸۰۰۰۰۰۰ (۵ ساعت)؛ صفر یعنی ارزیابی استراحت خاموش. */
#define IMBAL_PARAM_POST_CHARGE_WAIT_MS      110u

/** [EN] Wait after charge start before during-charge evaluation (ms),
 *  default 600000 (10 min), max 18000000 (5 h); 0 disables it.
 *  [FA] مکث پس از شروع شارژ تا ارزیابی حین شارژ (ms)، پیش‌فرض ۶۰۰۰۰۰،
 *  سقف ۱۸۰۰۰۰۰۰ (۵ ساعت)؛ صفر یعنی خاموش. */
#define IMBAL_PARAM_IN_CHARGE_WAIT_MS        111u

/** [EN] Episode stability time (ms): imbalance must stay over the limit
 *  continuously for this long before the episode counts ONCE,
 *  default 30000 (30 s).
 *  [FA] زمان پایداری رویداد (ms)، پیش‌فرض ۳۰۰۰۰. */
#define IMBAL_PARAM_STABILITY_MS             112u

/** [EN] Episode hysteresis (mV): an active episode ends only when the
 *  imbalance falls at or below (limit - hysteresis), default 100 mV.
 *  [FA] هیسترزیس رویداد (mV)، پیش‌فرض ۱۰۰. */
#define IMBAL_PARAM_HYSTERESIS_MV            113u

/** [EN] Episode budget: latched verdict after this many episodes,
 *  default 10.
 *  [FA] سقف رویدادها تا قفل، پیش‌فرض ۱۰. */
#define IMBAL_PARAM_MAX_EVENTS               114u

/** [EN] Beep period while latched (ms), default 3600000 (1 hour);
 *  0 disables the periodic beep.
 *  [FA] دورهٔ بوق در حالت قفل (ms)، پیش‌فرض یک ساعت؛ صفر یعنی بدون بوق. */
#define IMBAL_PARAM_LATCH_BEEP_PERIOD_MS     115u

/** [EN] Latched beep length (ms), default 200.
 *  [FA] طول بوق کوتاه در حالت قفل (ms)، پیش‌فرض ۲۰۰. */
#define IMBAL_PARAM_LATCH_BEEP_LEN_MS        116u

/** [EN] Block-output checkbox (0/1), default 1: while latched, the battery
 *  must never be switched onto the output (user bears the risk when 0).
 *  [FA] تیک مسدودی خروجی، پیش‌فرض ۱. */
#define IMBAL_PARAM_BLOCK_OUTPUT_EN          117u

/** [EN] Charge cycles allowed after latch before charging is blocked too,
 *  default 20.
 *  [FA] سیکل‌های شارژ مجاز پس از قفل تا مسدودی شارژ، پیش‌فرض ۲۰. */
#define IMBAL_PARAM_MAX_LATCHED_CYCLES       118u

/** [EN] v1.68 (user order 2026-10-05: "the red lamp must BLINK in the
 *  imbalance state"): cadence of the latched red lamp. Period in ms
 *  (default 1000) and ON share in percent (default 50) - the same
 *  period/duty shape every other scenario uses, so one pair of knobs
 *  describes the whole face. Period 0 is still accepted and means the old
 *  solid red, for anyone who wants it back without a rebuild.
 *  [FA] آهنگ چشمک چراغ قرمز در حالت قفل عدم‌توازن (دستور کاربر): دوره بر
 *  حسب میلی‌ثانیه (پیش‌فرض ۱۰۰۰) و سهم روشنی بر حسب درصد (پیش‌فرض ۵۰) -
 *  همان شکل دوره و duty بقیهٔ سناریوها. دورهٔ صفر یعنی همان قرمز ثابت قدیمی. */
#define IMBAL_PARAM_LATCH_BLINK_PERIOD_MS    123u
#define IMBAL_PARAM_LATCH_BLINK_DUTY_PCT     124u

/** [EN] v1.81: independent per-pattern beep count and inter-beep gap for the
 *  latched imbalance alarm. Defaults preserve the existing one-beep shape.
 *  [FA] نسخهٔ ۱٫۸۱: تعداد بوق و گپ مستقل الگوی بوق قفل عدم‌توازن؛ پیش‌فرض‌ها
 *  شکل قبلیِ یک بوق را حفظ می‌کنند. */
#define IMBAL_PARAM_LATCH_BEEP_COUNT         132u
#define IMBAL_PARAM_LATCH_BEEP_GAP_MS        133u

/** [EN] Number of complete FLOAT-qualified charge cycles without a new
 *  imbalance event before the event counter is cleared; default 3.
 *  [FA] تعداد سیکل‌های شارژ کامل و تأییدشده در FLOAT، بدون رویداد جدید،
 *  برای صفرکردن شمارندهٔ عدم‌توازن؛ پیش‌فرض ۳. */
#define IMBAL_PARAM_CLEAN_FULL_CYCLES        136u

/* [EN] The module owns FOUR id ranges: 108..118, 123..124, 132..133 and
 *      136. Gaps belong to other modules.
 * [FA] ماژول چهار بازهٔ شناسه دارد: ۱۰۸..۱۱۸، ۱۲۳..۱۲۴، ۱۳۲..۱۳۳ و ۱۳۶؛
 *      فاصله‌ها متعلق به ماژول‌های دیگرند. */
#define IMBAL_PARAM_FIRST_ID                 IMBAL_PARAM_REST_LIMIT_MV   /* 108 */
#define IMBAL_PARAM_LAST_ID                  IMBAL_PARAM_MAX_LATCHED_CYCLES /* 118 */
#define IMBAL_PARAM_FIRST_ID2                IMBAL_PARAM_LATCH_BLINK_PERIOD_MS /* 123 */
#define IMBAL_PARAM_LAST_ID2                 IMBAL_PARAM_LATCH_BLINK_DUTY_PCT  /* 124 */
#define IMBAL_PARAM_FIRST_ID3                IMBAL_PARAM_LATCH_BEEP_COUNT /* 132 */
#define IMBAL_PARAM_LAST_ID3                 IMBAL_PARAM_LATCH_BEEP_GAP_MS /* 133 */
#define IMBAL_PARAM_FIRST_ID4                IMBAL_PARAM_CLEAN_FULL_CYCLES /* 136 */
#define IMBAL_PARAM_LAST_ID4                 IMBAL_PARAM_CLEAN_FULL_CYCLES /* 136 */

#define IMBAL_PARAM_BLOCK1_COUNT \
    ((uint32_t)IMBAL_PARAM_LAST_ID - (uint32_t)IMBAL_PARAM_FIRST_ID + 1u)
#define IMBAL_PARAM_BLOCK2_COUNT \
    ((uint32_t)IMBAL_PARAM_LAST_ID2 - (uint32_t)IMBAL_PARAM_FIRST_ID2 + 1u)
#define IMBAL_PARAM_BLOCK3_COUNT \
    ((uint32_t)IMBAL_PARAM_LAST_ID3 - (uint32_t)IMBAL_PARAM_FIRST_ID3 + 1u)
#define IMBAL_PARAM_BLOCK4_COUNT \
    ((uint32_t)IMBAL_PARAM_LAST_ID4 - (uint32_t)IMBAL_PARAM_FIRST_ID4 + 1u)
#define IMBAL_PARAM_TABLE_SIZE \
    (IMBAL_PARAM_BLOCK1_COUNT + IMBAL_PARAM_BLOCK2_COUNT + \
     IMBAL_PARAM_BLOCK3_COUNT + IMBAL_PARAM_BLOCK4_COUNT)

/** [EN] Is this id a parameter of the imbalance module? / [FA] آیا این شناسه مال این ماژول است؟ */
#define IMBAL_PARAM_OWNS(id) \
    (((((uint32_t)(id)) >= (uint32_t)IMBAL_PARAM_FIRST_ID) && \
      (((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID)) || \
     ((((uint32_t)(id)) >= (uint32_t)IMBAL_PARAM_FIRST_ID2) && \
      (((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID2)) || \
     ((((uint32_t)(id)) >= (uint32_t)IMBAL_PARAM_FIRST_ID3) && \
      (((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID3)) || \
     ((((uint32_t)(id)) >= (uint32_t)IMBAL_PARAM_FIRST_ID4) && \
      (((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID4)))

/** [EN] Table index of an owned id (undefined for ids the module does not own).
 *  [FA] اندیس جدول برای شناسهٔ متعلق به ماژول. */
#define IMBAL_PARAM_INDEX(id) \
    ((((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID) \
        ? (((uint32_t)(id)) - (uint32_t)IMBAL_PARAM_FIRST_ID) \
        : ((((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID2) \
            ? (IMBAL_PARAM_BLOCK1_COUNT + (((uint32_t)(id)) - (uint32_t)IMBAL_PARAM_FIRST_ID2)) \
            : ((((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID3) \
                ? (IMBAL_PARAM_BLOCK1_COUNT + IMBAL_PARAM_BLOCK2_COUNT + \
                   (((uint32_t)(id)) - (uint32_t)IMBAL_PARAM_FIRST_ID3)) \
                : (IMBAL_PARAM_BLOCK1_COUNT + IMBAL_PARAM_BLOCK2_COUNT + \
                   IMBAL_PARAM_BLOCK3_COUNT + (((uint32_t)(id)) - \
                   (uint32_t)IMBAL_PARAM_FIRST_ID4)))))

/* [EN] NVM-only runtime slots (ids 200..202): persisted through the same
 *      EspLink NVM machinery as parameters but never drawn on the panel and
 *      never included in a backup. Written by the module itself.
 * [FA] اسلات‌های زمان‌اجرا (۲۰۰..۲۰۲): فقط NVM، روی پنل کشیده نمی‌شوند. */
#define IMBAL_SLOT_EVENTS_ID                 200u  /* [EN] u8 episodes / شمارنده رویداد */
#define IMBAL_SLOT_CYCLES_ID                 201u  /* [EN] u8 charge cycles since latch / سیکل‌ها */
#define IMBAL_SLOT_LATCH_ID                  202u  /* [EN] 0/1 latch / پرچم قفل */
#define IMBAL_SLOT_FIRST_ID                  IMBAL_SLOT_EVENTS_ID
#define IMBAL_SLOT_LAST_ID                   IMBAL_SLOT_LATCH_ID

/* ==================== Defaults, bounds / پیش‌فرض‌ها و مرزها ==================== */

#define IMBAL_DEF_REST_LIMIT_MV              300u
#define IMBAL_DEF_DISCHARGE_LIMIT_MV         500u
#define IMBAL_DEF_POST_CHARGE_WAIT_MS        600000u
#define IMBAL_DEF_IN_CHARGE_WAIT_MS          600000u
#define IMBAL_DEF_STABILITY_MS               30000u
#define IMBAL_DEF_HYSTERESIS_MV              100u
#define IMBAL_DEF_MAX_EVENTS                 10u
#define IMBAL_DEF_BEEP_PERIOD_MS             3600000u
#define IMBAL_DEF_BEEP_LEN_MS                200u
#define IMBAL_DEF_BLOCK_OUTPUT_EN            1u
#define IMBAL_DEF_MAX_LATCHED_CYCLES         20u
#define IMBAL_DEF_LATCH_BLINK_PERIOD_MS      1000u
#define IMBAL_DEF_LATCH_BLINK_DUTY_PCT       50u
#define IMBAL_DEF_LATCH_BEEP_COUNT           1u
#define IMBAL_DEF_LATCH_BEEP_GAP_MS          0u
#define IMBAL_DEF_CLEAN_FULL_CYCLES          3u

/* [EN] Clamp windows (min..max per id, applied by Set): wide enough for a
 *      workshop, tight enough that a typo cannot create a dead monitor.
 * [FA] پنجره‌های گیره برای هر شناسه. */
#define IMBAL_MAX_LIMIT_MV                   2000u     /* [EN] ids 108/109 / برای ۱۰۸/۱۰۹ */
#define IMBAL_MAX_WAIT_MS                    18000000u /* [EN] ids 110/111، تا ۵ ساعت */
#define IMBAL_MIN_STABILITY_MS               1000u     /* [EN] id 112, at least 1 s */
#define IMBAL_MAX_STABILITY_MS               600000u
#define IMBAL_MAX_HYSTERESIS_MV              1000u     /* [EN] id 113 */
#define IMBAL_MAX_COUNT                      255u      /* [EN] ids 114/118 */
#define IMBAL_MAX_BEEP_PERIOD_MS             86400000u /* [EN] id 115، تا ۲۴ ساعت */
#define IMBAL_MIN_BEEP_LEN_MS                20u       /* [EN] id 116 */
#define IMBAL_MAX_BEEP_LEN_MS                2000u
#define IMBAL_MAX_BEEP_COUNT                 10u       /* [EN] id 132 / شناسه ۱۳۲ */
#define IMBAL_MAX_BEEP_GAP_MS                5000u     /* [EN] id 133 / شناسه ۱۳۳ */
/* [EN] id 123: 0 = solid red (opt-out), otherwise 100 ms..10 s so the lamp
 *      is always visibly a BLINK and never a flicker nobody can see.
 * [FA] شناسهٔ ۱۲۳: صفر یعنی قرمز ثابت، وگرنه ۱۰۰ms تا ۱۰s تا چشمک واقعاً
 *      دیده شود نه لرزش. */
#define IMBAL_MIN_BLINK_PERIOD_MS            100u
#define IMBAL_MAX_BLINK_PERIOD_MS            10000u
/* [EN] id 124: 5..95% - a 0% or 100% duty would silently be "lamp off" or
 *      "solid", which the period knob already expresses honestly.
 * [FA] شناسهٔ ۱۲۴: ۵ تا ۹۵ درصد. */
#define IMBAL_MIN_BLINK_DUTY_PCT             5u
#define IMBAL_MAX_BLINK_DUTY_PCT             95u

/* [EN] Auto-reset qualifier: battery must stay absent continuously this
 *      long before the module believes a real battery swap happened.
 *      3000 ms > any measurement hiccup, < any human swap time... the point
 *      is the user physically removes the pack; by the time the new battery
 *      is connected, every counter and the latch are already zeroed.
 * [FA] گیت صفرکردن خودکار: نبود باتری باید این مدت پیوسته برقرار بماند. */
#define IMBAL_ABSENT_RESET_MS                3000u

/* ==================== Types / انواع ==================== */

/** [EN] Inputs for one evaluation pass. The caller maps board signals:
 *  - v_high_mv / v_low_mv from snapshot.v_bat_high_mv / v_bat_low_mv
 *  - input_present from snapshot.input_present
 *  - valid = snapshot.valid AND no ADC fault bit
 *  - bat_absent = Fault module battery-absent condition (param-29 based)
 *  - charging = func__Charger_IsAnyChannelActive()
 *  - on_battery = changeover state == APP_STATE_BATTERY (output on battery)
 * [FA] ورودی‌های یک پاس ارزیابی؛ نگاشت توسط فراخوان. */
typedef struct
{
    uint32_t uint32_t__vHighMv;      /* [EN] Upper half mV / ولتاژ نیم بالا */
    uint32_t uint32_t__vLowMv;       /* [EN] Lower half mV / ولتاژ نیم پایین */
    bool     bool__inputPresent;     /* [EN] Grid/DC input present / حضور ورودی */
    bool     bool__valid;            /* [EN] Measurements trustworthy / نمونه معتبر */
    bool     bool__batAbsent;        /* [EN] Battery absent / باتری نیست */
    bool     bool__charging;         /* [EN] Any charger channel active / در شارژ */
    bool     bool__chargeComplete;   /* [EN] All relevant channels reached FLOAT / شارژ کامل */
    /* [EN] A charge cycle may qualify at most one imbalance event. Before
       chargeComplete is true, a stable over-limit condition is only a
       candidate; it is never added to the persisted event counter.
       [FA] هر سیکل شارژ حداکثر یک رویداد عدم‌توازن دارد. پیش از درست‌شدن
       chargeComplete، شرط پایدار فقط نامزد است و به شمارندهٔ ماندگار اضافه نمی‌شود. */
    /* [EN] v1.72 (user order): the two halves may only be compared when they
       are in the SAME state. One half charging while the other rests lifts
       the charging half by its own charge voltage, and that difference is
       the charger talking, not an imbalance. The caller passes the state of
       each channel; when they disagree the evaluation window stays shut.
       [FA] دو نیم فقط وقتی قابل مقایسه‌اند که در یک حالت باشند. اگر یکی
       شارژ شود و دیگری استراحت کند، اختلافِ دیده‌شده کارِ شارژر است نه
       عدم‌توازن؛ پس پنجرهٔ ارزیابی بسته می‌ماند. */
    bool     bool__chargingCh1;      /* [EN] Charger 1 active / شارژ کانال ۱ */
    bool     bool__chargingCh2;      /* [EN] Charger 2 active / شارژ کانال ۲ */
    bool     bool__onBattery;        /* [EN] Output runs on battery / خروجی روی باتری */
} imbalance_inputs_t;

/** [EN] Outputs after one evaluation pass - everything consumers need.
 * [FA] خروجی‌های یک پاس ارزیابی. */
typedef struct
{
    uint32_t uint32_t__imbalanceMv;  /* [EN] |v_high - v_low|, live / اختلاف زنده */
    bool     bool__episode;          /* [EN] Over-limit episode in progress / اپیزود جاری */
    uint32_t uint32_t__events;       /* [EN] Committed full-cycle events / رویدادهای سیکل کامل */
    bool     bool__latched;          /* [EN] Persisted verdict until clean cycles / قفل ماندگار تا سیکل‌های پاک */
    uint32_t uint32_t__latchedCycles;/* [EN] Charge cycles since latch / سیکل‌های قفل */
    bool     bool__blockOutput;      /* [EN] Changeover veto: keep battery off / وتوی خروجی */
    bool     bool__chargingAllowed;  /* [EN] Charger gate: false = no charge / گیت شارژ */
    bool     bool__beepDue;          /* [EN] One-shot pulse: beep now / پالس بوق */
    bool     bool__halvesMismatch;   /* [EN] v1.72: halves in different states, not comparable / دو نیم هم‌حالت نیستند */
} imbalance_outputs_t;

/* ==================== Functions / توابع ==================== */

/** [EN] Load macro defaults, clear timers/episode, counters/latch stay as
 *      the NVM replay left them (replay runs before or after Init; both
 *      orders are safe because Set writes both RAM copies).
 * [FA] پیش‌فرض‌ها را بار می‌کند؛ شمارنده‌ها دست‌نخورده می‌مانند. */
void func__Imbalance_Init(void);

/** [EN] One evaluation pass.
 * @param imbalance_inputs_t__inputs [EN] Mapped inputs / ورودی‌های نگاشته (‎non-NULL)‎
 * @param uint32_t__nowMs [EN] Current time in milliseconds / زمان فعلی ms
 * @param imbalance_outputs_t__outputs [EN] Results out / خروجی‌ها (‎non-NULL)‎
 * @return bool [EN] true if any persisted value changed (caller should mark
 *         the NVM slots dirty) / اگر مقدار ماندگاری تغییر کرده BL درست */
bool func__Imbalance_Evaluate(const imbalance_inputs_t *imbalance_inputs_t__inputs,
                              uint32_t uint32_t__nowMs,
                              imbalance_outputs_t *imbalance_outputs_t__outputs);

/** [EN] Read the last outputs without evaluating (for EspLink TLM/UI).
 * [FA] خواندن آخرین خروجی‌ها بدون ارزیابی. */
void func__Imbalance_GetOutputs(imbalance_outputs_t *imbalance_outputs_t__outputs);

/** [EN] Parameter set with clamp (ids 108..118, 123..124, 132..133, 136) and
 *      runtime-slot set (ids 200..202, NVM boot replay only). Returns applied value.
 *      The new beep-shape and clean-cycle values are normal persisted
 *      parameters, not packed into legacy words.
 * [FA] تنظیم پارامتر با گیره (شناسه‌های ۱۰۸..۱۱۸، ۱۲۳..۱۲۴، ۱۳۲..۱۳۳ و ۱۳۶) و
 *      تنظیم اسلات زمان‌اجرا (۲۰۰..۲۰۲، فقط بازپخش NVM). مقدار اعمال‌شده را
 *      برمی‌گرداند؛ شکل بوق جدید پارامتر مستقل و ماندگار است.
 * @param uint32_t__appliedValue [EN] out, may be NULL / مقدار اعمال‌شده
 * @return bool [EN] true when the id belongs to this module / شناسه متعلق است */
bool func__Imbalance_SetParam(uint8_t uint8_t__paramId,
                              uint32_t uint32_t__value,
                              uint32_t *uint32_t__appliedValue);

/** [EN] Parameter / runtime-slot readback (NVM save snapshot, GET messages).
 * [FA] خواندن مقدار زندهٔ پارامتر/اسلات. */
bool func__Imbalance_GetParam(uint8_t uint8_t__paramId,
                              uint32_t *uint32_t__value);

#ifdef __cplusplus
}
#endif

#endif /* IMBALANCE_H */
