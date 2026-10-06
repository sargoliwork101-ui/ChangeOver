/**
 * @file    imbalance.h
 * @brief   [EN] Scenario 5 - two-half battery imbalance (unnam) monitor.
 *              Watches |v_bat_high - v_bat_low| (the absolute difference of
 *              the two 12 V halves of the 24 V pack) against rest and
 *              discharge limits, counts over-limit episodes with stability
 *              time + hysteresis (each episode counts ONCE), and latches a
 *              permanent verdict when the episode budget is spent:
 *              solid red LED + hourly short beep + (checkbox) battery never
 *              switched onto the output. Charging stays allowed while
 *              latched so the hourly beep keeps working, but after a
 *              configurable number of further charge cycles charging is
 *              blocked too. All counters and the latch survive power loss
 *              through EspLink NVM and reset automatically only when the
 *              battery goes absent (battery really replaced).
 *
 *              [EN] Evaluation time-gating (user spec): thresholds apply
 *              only (a) at rest, 10 minutes after the end of a charge
 *              session (param 110), and (b) during an ongoing charge,
 *              10 minutes after charge start (param 111, 0 disables).
 *              During charge the REST threshold is used - no separate
 *              charge threshold exists by design.
 *              In discharge (output running on battery) the discharge
 *              threshold (param 109) applies immediately.
 *
 *          [FA] سناریوی ۵ - پایش عدم‌توازن (آنبالانس) دو نیم‌باتری.
 *              قدرمطلق اختلاف دو نیم به شارژ ۱۲ ولتی پک ۲۴ ولتی
 *              (|‎v_high - v_low|)‎ با حد استراحت و حد دشارژ سنجیده می‌شود،
 *              رویدادهای فراتر از حد با زمان پایداری و هیسترزیس شمرده
 *              می‌شوند (هر اپیزود فقط یک‌بار) و پس از پر شدن سقف رویدادها
 *              قضاوت دائمی (قفل) صادر می‌شود: LED قرمز ثابت + بوق کوتاه
 *              ساعتی + (با تیک کاربر) ممنوعیت سوئیچ شدن باتری روی خروجی.
 *              شارژ در حالت قفل آزاد می‌ماند تا بوق ساعتی کار کند، ولی پس
 *              از تعداد قابل‌تنظیم سیکل شارژ در حالت قفل، شارژ هم بسته
 *              می‌شود. شمارنده‌ها و قفل در NVM ماندگارند و فقط با نبود
 *              باتری (تعویض واقعی) به‌صورت خودکار صفر می‌شوند.
 *
 *              گیت زمانی ارزیابی (مشخصات کاربر): آستانه‌ها فقط (الف) در
 *              استراحت و پس از ۱۰ دقیقه از پایان یک سیکل شارژ (پارامتر ۱۱۰)
 *              و (ب) در حین شارژ و پس از ۱۰ دقیقه از شروع آن (پارامتر ۱۱۱،
 *              صفر یعنی غیرفعال) اعمال می‌شوند. حین شارژ همان آستانهٔ
 *              استراحت استفاده می‌شود - آستانهٔ مجزای شارژ نداریم.
 *              در دشارژ (خروجی روی باتری) آستانهٔ دشارژ (پارامتر ۱۰۹)
 *              بلافاصله اعمال می‌شود.
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

/** [EN] Wait after charge session end before rest evaluation begins (ms),
 *  default 600000 (10 min); 0 disables rest evaluation entirely.
 *  [FA] مکث پس از پایان شارژ تا شروع ارزیابی در استراحت (ms)، پیش‌فرض
 *  ۶۰۰۰۰۰ (۱۰ دقیقه)؛ صفر یعنی ارزیابی استراحت کلاً خاموش. */
#define IMBAL_PARAM_POST_CHARGE_WAIT_MS      110u

/** [EN] Wait after charge start before during-charge evaluation begins (ms),
 *  default 600000 (10 min); 0 disables during-charge evaluation.
 *  [FA] مکث پس از شروع شارژ تا شروع ارزیابی حین شارژ (ms)؛ صفر یعنی خاموش. */
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

/* [EN] The module owns TWO id ranges: the original 108..118 block and the
 *      v1.68 blink pair at the end of the parameter space (119..122 belong
 *      to other modules, so the range could not simply be widened).
 * [FA] ماژول دو بازهٔ شناسه دارد: بلوک اصلی ۱۰۸..۱۱۸ و جفت چشمک ۱۲۳..۱۲۴
 *      (۱۱۹..۱۲۲ مال ماژول‌های دیگر است، پس بازه را نمی‌شد کش داد). */
#define IMBAL_PARAM_FIRST_ID                 IMBAL_PARAM_REST_LIMIT_MV   /* 108 */
#define IMBAL_PARAM_LAST_ID                  IMBAL_PARAM_MAX_LATCHED_CYCLES /* 118 */
#define IMBAL_PARAM_FIRST_ID2                IMBAL_PARAM_LATCH_BLINK_PERIOD_MS /* 123 */
#define IMBAL_PARAM_LAST_ID2                 IMBAL_PARAM_LATCH_BLINK_DUTY_PCT  /* 124 */

#define IMBAL_PARAM_BLOCK1_COUNT \
    ((uint32_t)IMBAL_PARAM_LAST_ID - (uint32_t)IMBAL_PARAM_FIRST_ID + 1u)
#define IMBAL_PARAM_BLOCK2_COUNT \
    ((uint32_t)IMBAL_PARAM_LAST_ID2 - (uint32_t)IMBAL_PARAM_FIRST_ID2 + 1u)
#define IMBAL_PARAM_TABLE_SIZE \
    (IMBAL_PARAM_BLOCK1_COUNT + IMBAL_PARAM_BLOCK2_COUNT)

/** [EN] Is this id a parameter of the imbalance module? / [FA] آیا این شناسه مال این ماژول است؟ */
#define IMBAL_PARAM_OWNS(id) \
    (((((uint32_t)(id)) >= (uint32_t)IMBAL_PARAM_FIRST_ID) && \
      (((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID)) || \
     ((((uint32_t)(id)) >= (uint32_t)IMBAL_PARAM_FIRST_ID2) && \
      (((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID2)))

/** [EN] Table index of an owned id (undefined for ids the module does not own).
 *  [FA] اندیس جدول برای شناسهٔ متعلق به ماژول. */
#define IMBAL_PARAM_INDEX(id) \
    ((((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID) \
        ? (((uint32_t)(id)) - (uint32_t)IMBAL_PARAM_FIRST_ID) \
        : (IMBAL_PARAM_BLOCK1_COUNT + (((uint32_t)(id)) - (uint32_t)IMBAL_PARAM_FIRST_ID2)))

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
    uint32_t uint32_t__events;       /* [EN] Counted episodes, persisted / رویدادها */
    bool     bool__latched;          /* [EN] Permanent verdict / قفل */
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

/** [EN] Parameter set with clamp (ids 108..118) and runtime-slot set
 *      (ids 200..202, NVM boot replay only). Returns applied value.
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
