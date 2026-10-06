/**
 * @file    host_test_imbalance.c
 * @brief   [EN] Host unit test for the Imbalance module (scenario 5).
 *              Compiles the production imbalance.c directly and drives it
 *              with synthetic inputs/timestamps. No board, no RTOS.
 *          [FA] تست هاست ماژول عدم‌توازن با زمان/ورودی مصنوعی.
 */

#include <stdio.h>
#include <stdlib.h>
#include "../imbalance.h"

static int INT32_T__G__Checks = 0;
static int INT32_T__G__Fails  = 0;

#define CHECK(cond) do { \
        INT32_T__G__Checks++; \
        if (!(cond)) { INT32_T__G__Fails++; \
            printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

static imbalance_inputs_t  IMBAL_INPUTS_T__G__In;
static imbalance_outputs_t IMBAL_OUTPUTS_T__G__Out;

static void func__Run(uint32_t uint32_t__nowMs, bool bool__expectPersist)
{
    bool bool__persist;

    bool__persist = func__Imbalance_Evaluate(&IMBAL_INPUTS_T__G__In, uint32_t__nowMs, &IMBAL_OUTPUTS_T__G__Out);
    CHECK(bool__persist == bool__expectPersist);
}

int main(void)
{
    uint32_t uint32_t__t;
    uint32_t uint32_t__value;

    printf("== Imbalance host test (scenario 5) ==\n");

    /* ---- v1.81 regression: boot order "NVM replay -> Init" ----
       [EN] func__EspLink_NvmInit() replays the stored panel values BEFORE the
            scheduler starts, while func__Imbalance_Init() runs later inside
            the control thread. A stored setting must therefore survive Init;
            the old unconditional default loop wiped it on every power-up
            (audit 2026-10-06). Params not present in the record must still
            come up with their compiled default.
       [FA] بازپخش ‎NVM‎ پیش از ‎scheduler‎ و ‎Init‎ بعد از آن اجرا می‌شود، پس
            مقدار ذخیره‌شده باید از ‎Init‎ جان سالم به‌در ببرد؛ قبلاً هر بار
            روشن‌شدن پاک می‌شد. پارامتر غایب در رکورد باید پیش‌فرض بگیرد. */
    CHECK(func__Imbalance_SetParam(108u, 777u, &uint32_t__value) && (uint32_t__value == 777u));
    CHECK(func__Imbalance_SetParam(110u, 18000000u, &uint32_t__value) && (uint32_t__value == 18000000u));
    CHECK(func__Imbalance_SetParam(111u, 18000000u, &uint32_t__value) && (uint32_t__value == 18000000u));
    CHECK(func__Imbalance_SetParam(113u, 250u, &uint32_t__value) && (uint32_t__value == 250u));

    func__Imbalance_Init();

    CHECK(func__Imbalance_GetParam(108u, &uint32_t__value) && (uint32_t__value == 777u));
    CHECK(func__Imbalance_GetParam(110u, &uint32_t__value) && (uint32_t__value == 18000000u));
    CHECK(func__Imbalance_GetParam(111u, &uint32_t__value) && (uint32_t__value == 18000000u));
    CHECK(func__Imbalance_GetParam(113u, &uint32_t__value) && (uint32_t__value == 250u));
    CHECK(func__Imbalance_GetParam(109u, &uint32_t__value) && (uint32_t__value == 500u));

    /* [EN] Back to the compiled defaults for the checks that follow.
       [FA] بازگشت به پیش‌فرض‌ها برای بررسی‌های بعدی. */
    CHECK(func__Imbalance_SetParam(108u, 300u, &uint32_t__value) && (uint32_t__value == 300u));
    CHECK(func__Imbalance_SetParam(110u, 600000u, &uint32_t__value) && (uint32_t__value == 600000u));
    CHECK(func__Imbalance_SetParam(111u, 600000u, &uint32_t__value) && (uint32_t__value == 600000u));
    CHECK(func__Imbalance_SetParam(113u, 100u, &uint32_t__value) && (uint32_t__value == 100u));

    /* ---- defaults ---- */
    CHECK(func__Imbalance_GetParam(108u, &uint32_t__value) && (uint32_t__value == 300u));
    CHECK(func__Imbalance_GetParam(109u, &uint32_t__value) && (uint32_t__value == 500u));
    CHECK(func__Imbalance_GetParam(110u, &uint32_t__value) && (uint32_t__value == 600000u));
    CHECK(func__Imbalance_GetParam(111u, &uint32_t__value) && (uint32_t__value == 600000u));
    CHECK(func__Imbalance_GetParam(112u, &uint32_t__value) && (uint32_t__value == 30000u));
    CHECK(func__Imbalance_GetParam(113u, &uint32_t__value) && (uint32_t__value == 100u));
    CHECK(func__Imbalance_GetParam(114u, &uint32_t__value) && (uint32_t__value == 10u));
    CHECK(func__Imbalance_GetParam(115u, &uint32_t__value) && (uint32_t__value == 3600000u));
    CHECK(func__Imbalance_GetParam(116u, &uint32_t__value) && (uint32_t__value == 200u));
    CHECK(func__Imbalance_GetParam(117u, &uint32_t__value) && (uint32_t__value == 1u));
    CHECK(func__Imbalance_GetParam(118u, &uint32_t__value) && (uint32_t__value == 20u));
    CHECK(func__Imbalance_GetParam(132u, &uint32_t__value) && (uint32_t__value == 1u));
    CHECK(func__Imbalance_GetParam(133u, &uint32_t__value) && (uint32_t__value == 0u));
    CHECK(!func__Imbalance_GetParam(107u, &uint32_t__value));
    CHECK(!func__Imbalance_GetParam(119u, &uint32_t__value));

    /* ---- v1.81 validation gap: the latch-blink block (123/124) ----
       [EN] These two ids live in the SECOND owned block, so they also check
            IMBAL_PARAM_INDEX()'s two-block mapping. Period 0 is a legal
            "blink off" value; any other value is clamped into
            100..10000 ms, and the duty into 5..95 %.
       [FA] این دو شناسه در بلوک دوم جدول‌اند و نگاشت دو-بلوکی را هم می‌سنجند.
            دورهٔ صفر یعنی «چشمک خاموش» و مجاز است؛ باقی مقادیر به
            ۱۰۰..۱۰۰۰۰ms و وظیفه به ۵..۹۵٪ گیره می‌شوند. */
    CHECK(func__Imbalance_GetParam(123u, &uint32_t__value) && (uint32_t__value == 1000u));
    CHECK(func__Imbalance_GetParam(124u, &uint32_t__value) && (uint32_t__value == 50u));
    CHECK(!func__Imbalance_GetParam(122u, &uint32_t__value));
    CHECK(!func__Imbalance_GetParam(125u, &uint32_t__value));
    CHECK(func__Imbalance_SetParam(123u, 0u, &uint32_t__value) && (uint32_t__value == 0u));
    CHECK(func__Imbalance_SetParam(123u, 1u, &uint32_t__value) && (uint32_t__value == 100u));
    CHECK(func__Imbalance_SetParam(123u, 99999u, &uint32_t__value) && (uint32_t__value == 10000u));
    CHECK(func__Imbalance_SetParam(124u, 0u, &uint32_t__value) && (uint32_t__value == 5u));
    CHECK(func__Imbalance_SetParam(124u, 100u, &uint32_t__value) && (uint32_t__value == 95u));
    /* [EN] Back to the compiled defaults. / [FA] بازگشت به پیش‌فرض. */
    CHECK(func__Imbalance_SetParam(123u, 1000u, &uint32_t__value) && (uint32_t__value == 1000u));
    CHECK(func__Imbalance_SetParam(124u, 50u, &uint32_t__value) && (uint32_t__value == 50u));

    /* ---- v1.81 independent beep shape: clamp and boot-order persistence ----
       [EN] Count and gap must have their own table slots; changing one must
            not alter the legacy period/length or the other new slot.
       [FA] تعداد و گپ باید خانهٔ مستقل داشته باشند؛ تغییر یکی نباید دوره/طول
            قدیمی یا خانهٔ جدید دیگر را تغییر دهد. */
    CHECK(func__Imbalance_SetParam(132u, 4u, &uint32_t__value) && (uint32_t__value == 4u));
    CHECK(func__Imbalance_SetParam(133u, 250u, &uint32_t__value) && (uint32_t__value == 250u));
    CHECK(func__Imbalance_GetParam(132u, &uint32_t__value) && (uint32_t__value == 4u));
    CHECK(func__Imbalance_GetParam(133u, &uint32_t__value) && (uint32_t__value == 250u));
    CHECK(func__Imbalance_SetParam(132u, 0u, &uint32_t__value) && (uint32_t__value == 1u));
    CHECK(func__Imbalance_SetParam(132u, 99u, &uint32_t__value) && (uint32_t__value == 10u));
    CHECK(func__Imbalance_SetParam(133u, 6000u, &uint32_t__value) && (uint32_t__value == 5000u));
    CHECK(func__Imbalance_SetParam(133u, 0u, &uint32_t__value) && (uint32_t__value == 0u));
    CHECK(func__Imbalance_SetParam(132u, 4u, &uint32_t__value) && (uint32_t__value == 4u));
    CHECK(func__Imbalance_SetParam(133u, 250u, &uint32_t__value) && (uint32_t__value == 250u));
    func__Imbalance_Init();
    CHECK(func__Imbalance_GetParam(132u, &uint32_t__value) && (uint32_t__value == 4u));
    CHECK(func__Imbalance_GetParam(133u, &uint32_t__value) && (uint32_t__value == 250u));

    /* ---- clamp ---- */
    uint32_t__value = 0u;
    CHECK(func__Imbalance_SetParam(108u, 99999u, &uint32_t__value) && (uint32_t__value == 2000u));
    CHECK(func__Imbalance_SetParam(111u, 18000001u, &uint32_t__value) && (uint32_t__value == 18000000u));
    CHECK(func__Imbalance_SetParam(112u, 0u, &uint32_t__value) && (uint32_t__value == 1000u));
    CHECK(func__Imbalance_SetParam(114u, 0u, &uint32_t__value) && (uint32_t__value == 1u));
    CHECK(func__Imbalance_SetParam(117u, 7u, &uint32_t__value) && (uint32_t__value == 1u));
    CHECK(!func__Imbalance_SetParam(119u, 1u, &uint32_t__value));
    /* [EN] Restore defaults after clamp checks. */
    CHECK(func__Imbalance_SetParam(108u, 300u, &uint32_t__value) && (uint32_t__value == 300u));
    CHECK(func__Imbalance_SetParam(111u, 600000u, &uint32_t__value) && (uint32_t__value == 600000u));
    CHECK(func__Imbalance_SetParam(112u, 30000u, &uint32_t__value) && (uint32_t__value == 30000u));
    CHECK(func__Imbalance_SetParam(114u, 10u, &uint32_t__value) && (uint32_t__value == 10u));

    /* ---- baseline inputs: valid, but not full yet ---- */
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv   = 12000u;
    IMBAL_INPUTS_T__G__In.uint32_t__vLowMv    = 12000u;
    IMBAL_INPUTS_T__G__In.bool__inputPresent  = true;
    IMBAL_INPUTS_T__G__In.bool__valid         = true;
    IMBAL_INPUTS_T__G__In.bool__batAbsent     = false;
    IMBAL_INPUTS_T__G__In.bool__charging      = false;
    IMBAL_INPUTS_T__G__In.bool__chargeComplete = false;
    IMBAL_INPUTS_T__G__In.bool__chargingCh1   = false;
    IMBAL_INPUTS_T__G__In.bool__chargingCh2   = false;
    IMBAL_INPUTS_T__G__In.bool__onBattery     = false;

    /* [EN] A partial charge may reach the imbalance numbers, but it must not
       register an event. The stable condition is only a deferred candidate.
       [FA] شارژ ناقص می‌تواند به عدد عدم‌توازن برسد، اما رویداد ثبت نمی‌شود؛
       شرط پایدار فقط نامزد معوق است. */
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 13000u;
    IMBAL_INPUTS_T__G__In.bool__charging = true;
    func__Run(1000u, false);              /* charge starts */
    func__Run(601000u, false);            /* in-charge wait opens */
    func__Run(631000u, false);            /* stability elapsed: candidate only */
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__episode == true);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__imbalanceMv == 1000u);

    /* [EN] Stopping before FLOAT discards the candidate, still with no event.
       [FA] توقف پیش از FLOAT نامزد را دور می‌ریزد و هنوز رویدادی نیست. */
    IMBAL_INPUTS_T__G__In.bool__charging = false;
    func__Run(632000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__episode == false);

    /* [EN] One complete cycle can add exactly one event. Repeated passes in
       the same completed cycle cannot add another one.
       [FA] هر سیکل کامل دقیقاً حداکثر یک رویداد اضافه می‌کند و پاس‌های تکراری
       همان سیکل رویداد دوم نمی‌سازند. */
    IMBAL_INPUTS_T__G__In.bool__charging = true;
    IMBAL_INPUTS_T__G__In.bool__chargeComplete = false;
    func__Run(700000u, false);
    func__Run(1300001u, false);
    func__Run(1330001u, false);            /* candidate during this cycle */
    IMBAL_INPUTS_T__G__In.bool__charging = false;
    IMBAL_INPUTS_T__G__In.bool__chargeComplete = true; /* FLOAT */
    func__Run(1331001u, true);              /* commit exactly one */
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 1u);
    func__Run(1332001u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 1u);

    /* [EN] Hysteresis still closes the live episode, but does not affect the
       one-event-per-cycle rule. [FA] هیسترزیس اپیزود زنده را می‌بندد. */
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 12200u;
    func__Run(1340000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__episode == false);

    /* ---- persistence via runtime slots (NVM round trip) ---- */
    {
        uint32_t uint32_t__ev, uint32_t__cy, uint32_t__la;

        CHECK(func__Imbalance_GetParam(200u, &uint32_t__ev) && (uint32_t__ev == 1u));
        CHECK(func__Imbalance_GetParam(201u, &uint32_t__cy) && (uint32_t__cy == 0u));
        CHECK(func__Imbalance_GetParam(202u, &uint32_t__la) && (uint32_t__la == 0u));

        /* [EN] Simulate power loss and NVM replay. */
        func__Imbalance_Init();
        CHECK(func__Imbalance_SetParam(200u, 0u, NULL));
        CHECK(func__Imbalance_GetParam(200u, &uint32_t__ev) && (uint32_t__ev == 0u));
        CHECK(func__Imbalance_SetParam(200u, 1u, NULL));
        CHECK(func__Imbalance_SetParam(201u, 0u, NULL));
        CHECK(func__Imbalance_SetParam(202u, 0u, NULL));
        IMBAL_INPUTS_T__G__In.bool__chargeComplete = true;
        func__Run(1400000u, false);
        CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 1u);
    }

    /* ---- fast-forward to latch: each new event needs a new cycle ---- */
    uint32_t__t = 1400000u;
    for (int i = 0; i < 9; i++)
    {
        IMBAL_INPUTS_T__G__In.bool__charging = true;
        IMBAL_INPUTS_T__G__In.bool__chargeComplete = false;
        IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 13000u;
        uint32_t__t += 1000u;
        func__Run(uint32_t__t, false);       /* new cycle */
        func__Run(uint32_t__t + 600000u, false); /* q111 opens */
        func__Run(uint32_t__t + 630000u, false); /* candidate, not registered yet */
        IMBAL_INPUTS_T__G__In.bool__charging = false;
        IMBAL_INPUTS_T__G__In.bool__chargeComplete = true;
        uint32_t__t += 631000u;
        func__Run(uint32_t__t, true);        /* full: commit one */
        CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == (uint32_t)(2u + i));
        IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 12000u;
        func__Run(uint32_t__t + 1000u, false);
    }
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 10u);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__latched == true);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__blockOutput == true);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__chargingAllowed == true);

    /* [EN] Latch confirmation and hourly beep remain intact. */
    func__Run(uint32_t__t + 1001u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__beepDue == false);
    func__Run(uint32_t__t + 3000000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__beepDue == false);
    func__Run(uint32_t__t + 3600000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__beepDue == true);

    /* [EN] Three complete cycles with no new imbalance clear only the
       persisted event counter. The latch and its budget remain; an incomplete
       cycle does not count as clean.
       [FA] سه سیکل کامل بدون عدم‌توازن جدید فقط شمارندهٔ ماندگار را پاک
       می‌کنند؛ قفل و بودجهٔ آن می‌مانند و سیکل ناقص پاک محسوب نمی‌شود. */
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 12000u;
    for (int clean = 0; clean < 4; clean++)
    {
        IMBAL_INPUTS_T__G__In.bool__charging = true;
        IMBAL_INPUTS_T__G__In.bool__chargeComplete = false;
        uint32_t__t += 10000u;
        func__Run(uint32_t__t, true);
        IMBAL_INPUTS_T__G__In.bool__charging = false;
        IMBAL_INPUTS_T__G__In.bool__chargeComplete = true;
        uint32_t__t += 10000u;
        func__Run(uint32_t__t, false);
        if (clean < 2)
        {
            CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 10u);
            CHECK(IMBAL_OUTPUTS_T__G__Out.bool__latched == true);
        }
    }
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
    /* [EN] Clean cycles clear only the imbalance counter; the old latch and
       its cycle budget remain until the battery-absence reset path.
       [FA] سیکل‌های پاک فقط شمارنده را صفر می‌کنند؛ قفل و بودجهٔ آن تا مسیر
       نبود باتری حفظ می‌شوند. */
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__latched == true);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__latchedCycles == 4u);

    /* ---- battery absent 3 s -> automatic full reset remains a second path ---- */
    CHECK(func__Imbalance_SetParam(200u, 4u, NULL));
    CHECK(func__Imbalance_SetParam(202u, 1u, NULL));
    IMBAL_INPUTS_T__G__In.bool__batAbsent = true;
    func__Run(uint32_t__t + 1u, false);
    IMBAL_INPUTS_T__G__In.bool__batAbsent = false;
    func__Run(uint32_t__t + 1000u, false);
    IMBAL_INPUTS_T__G__In.bool__batAbsent = true;
    func__Run(uint32_t__t + 2000u, false);
    func__Run(uint32_t__t + 5000u, true);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__latched == false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__latchedCycles == 0u);

    /* ---- invalid snapshot freezes progress ---- */
    IMBAL_INPUTS_T__G__In.bool__batAbsent = false;
    IMBAL_INPUTS_T__G__In.bool__valid = false;
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 15000u;
    func__Imbalance_Init();
    CHECK(func__Imbalance_SetParam(200u, 0u, NULL));
    func__Run(700000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);

    /* ---- discharge window uses its own limit, but only after a full charge ---- */
    func__Imbalance_Init();
    IMBAL_INPUTS_T__G__In.bool__valid = true;
    IMBAL_INPUTS_T__G__In.bool__chargeComplete = true;
    IMBAL_INPUTS_T__G__In.bool__onBattery = true;
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 12400u; /* 400 < discharge 500 */
    func__Run(1000u, false);
    func__Run(40000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 12600u;
    func__Run(41000u, false);
    func__Run(71000u, true);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 1u);

    /* ---- during-charge condition is deferred until FLOAT ---- */
    func__Imbalance_Init();
    CHECK(func__Imbalance_SetParam(200u, 0u, NULL));
    IMBAL_INPUTS_T__G__In.bool__onBattery = false;
    IMBAL_INPUTS_T__G__In.bool__charging = true;
    IMBAL_INPUTS_T__G__In.bool__chargeComplete = false;
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 13000u;
    func__Run(1000u, false);
    func__Run(601000u, false);
    func__Run(631000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
    IMBAL_INPUTS_T__G__In.bool__charging = false;
    IMBAL_INPUTS_T__G__In.bool__chargeComplete = true;
    func__Run(632000u, true);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 1u);

    /* ---- halves in different states are NOT comparable ---- */
    func__Imbalance_Init();
    CHECK(func__Imbalance_SetParam(200u, 0u, NULL));
    IMBAL_INPUTS_T__G__In.bool__onBattery = false;
    IMBAL_INPUTS_T__G__In.bool__charging = true;
    IMBAL_INPUTS_T__G__In.bool__chargeComplete = false;
    IMBAL_INPUTS_T__G__In.bool__chargingCh1 = true;
    IMBAL_INPUTS_T__G__In.bool__chargingCh2 = false;
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 13000u;
    for (uint32_t__t = 1000u; uint32_t__t <= 1200000u; uint32_t__t += 1000u)
    {
        func__Run(uint32_t__t, false);
        CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
        CHECK(IMBAL_OUTPUTS_T__G__Out.bool__halvesMismatch == true);
    }
    IMBAL_INPUTS_T__G__In.bool__chargingCh2 = true;
    func__Run(1201000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__halvesMismatch == false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
    IMBAL_INPUTS_T__G__In.bool__charging = true;
    IMBAL_INPUTS_T__G__In.bool__chargeComplete = false;
    func__Run(1231000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
    IMBAL_INPUTS_T__G__In.bool__charging = false;
    IMBAL_INPUTS_T__G__In.bool__chargeComplete = true;
    func__Run(1232000u, true);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 1u);

    printf("checks: %d, fails: %d\n", INT32_T__G__Checks, INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
