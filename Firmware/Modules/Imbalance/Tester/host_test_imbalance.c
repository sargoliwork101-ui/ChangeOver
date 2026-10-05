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

    func__Imbalance_Init();

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
    CHECK(!func__Imbalance_GetParam(107u, &uint32_t__value));
    CHECK(!func__Imbalance_GetParam(119u, &uint32_t__value));

    /* ---- clamp ---- */
    uint32_t__value = 0u;
    CHECK(func__Imbalance_SetParam(108u, 99999u, &uint32_t__value) && (uint32_t__value == 2000u));
    CHECK(func__Imbalance_SetParam(112u, 0u, &uint32_t__value) && (uint32_t__value == 1000u));
    CHECK(func__Imbalance_SetParam(114u, 0u, &uint32_t__value) && (uint32_t__value == 1u));
    CHECK(func__Imbalance_SetParam(117u, 7u, &uint32_t__value) && (uint32_t__value == 1u));
    CHECK(!func__Imbalance_SetParam(119u, 1u, &uint32_t__value));
    /* [EN] Restore defaults after clamp checks. */
    CHECK(func__Imbalance_SetParam(108u, 300u, &uint32_t__value) && (uint32_t__value == 300u));
    CHECK(func__Imbalance_SetParam(112u, 30000u, &uint32_t__value) && (uint32_t__value == 30000u));
    CHECK(func__Imbalance_SetParam(114u, 10u, &uint32_t__value) && (uint32_t__value == 10u));

    /* ---- baseline inputs: at rest, balanced, valid ---- */
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv  = 12000u;
    IMBAL_INPUTS_T__G__In.uint32_t__vLowMv   = 12000u;
    IMBAL_INPUTS_T__G__In.bool__inputPresent = true;
    IMBAL_INPUTS_T__G__In.bool__valid        = true;
    IMBAL_INPUTS_T__G__In.bool__batAbsent    = false;
    IMBAL_INPUTS_T__G__In.bool__charging     = false;
    IMBAL_INPUTS_T__G__In.bool__onBattery    = false;

    /* [EN] Before the 10-minute boot-anchored rest window opens, a big
     *      imbalance must NOT count (time gating, user spec).
     * [FA] پیش از باز شدن پنجرهٔ ۱۰ دقیقه‌ای، عدم‌توازن بزرگ نباید بشمارد. */
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 13000u;   /* imbalance 1000 mV */
    for (uint32_t__t = 1000u; uint32_t__t <= 599000u; uint32_t__t += 1000u)
    {
        func__Run(uint32_t__t, false);
        CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
    }

    /* [EN] t=600000 (10 min): window opens; 30 s stability still required. */
    func__Run(600000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
    /* stability elapsed: 600000 + 30000 = 630000 */
    func__Run(630000u, true);           /* events: 0 -> 1, persist changed */
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 1u);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__episode == true);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__imbalanceMv == 1000u);

    /* [EN] Same episode does NOT count again even after long time. */
    for (uint32_t__t = 631000u; uint32_t__t <= 690000u; uint32_t__t += 5000u)
    {
        func__Run(uint32_t__t, false);
        CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 1u);
    }

    /* [EN] Hysteresis: drop to 250 mV (> 300-100=200) keeps the episode. */
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 12250u;   /* imbalance 250 */
    func__Run(695000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__episode == true);
    func__Run(700000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__episode == true);

    /* [EN] Drop to 200 mV (<= 300-100): episode ends. */
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 12200u;   /* imbalance 200 */
    func__Run(705000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__episode == false);

    /* [EN] 29 s of excess is NOT enough (stability), no second count. */
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 13000u;
    func__Run(706000u, false);
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 12000u;   /* clears before 30 s */
    func__Run(734999u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 1u);

    /* [EN] 30+ s of excess after the clear counts the second episode. */
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 13000u;
    func__Run(740000u, false);
    func__Run(770000u, true);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 2u);
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 12000u;
    func__Run(771000u, false);          /* episode ends */

    /* ---- persistence via runtime slots (NVM round trip) ---- */
    {
        uint32_t uint32_t__ev, uint32_t__cy, uint32_t__la;

        CHECK(func__Imbalance_GetParam(200u, &uint32_t__ev) && (uint32_t__ev == 2u));
        CHECK(func__Imbalance_GetParam(201u, &uint32_t__cy) && (uint32_t__cy == 0u));
        CHECK(func__Imbalance_GetParam(202u, &uint32_t__la) && (uint32_t__la == 0u));

        /* [EN] Simulate power loss: RAM zeroes on a real power cycle (Init
         *      keeps persisted fields by design), then the NVM replay of
         *      slots 200..202 restores the counters. */
        func__Imbalance_Init();
        CHECK(func__Imbalance_SetParam(200u, 0u, NULL));   /* [EN] = fresh RAM after power loss */
        CHECK(func__Imbalance_GetParam(200u, &uint32_t__ev) && (uint32_t__ev == 0u));
        CHECK(func__Imbalance_SetParam(200u, 2u, NULL));   /* [EN] NVM replay */
        CHECK(func__Imbalance_SetParam(201u, 0u, NULL));
        CHECK(func__Imbalance_SetParam(202u, 0u, NULL));
        func__Run(800000u, false);
        CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 2u);
    }

    /* ---- fast-forward to latch: 8 more episodes -> 10 total ---- */
    uint32_t__t = 800000u;
    for (int i = 0; i < 8; i++)
    {
        uint32_t__t += 1000u;
        IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 13000u;
        func__Run(uint32_t__t, false);
        uint32_t__t += 30000u;
        func__Run(uint32_t__t, true);   /* every episode count changes persisted state */
        CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == (uint32_t)(3u + i));
        if (i == 7)
        {
            /* [EN] The 10th episode spends the budget: latch fires in the
             *      SAME pass, with its confirmation beep pulse.
             * [FA] رویداد دهم: قفل و بوق تأیید در همان پاس. */
            CHECK(IMBAL_OUTPUTS_T__G__Out.bool__beepDue == true);
        }
        IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 12000u;
        uint32_t__t += 1000u;
        func__Run(uint32_t__t, false);
    }
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 10u);

    /* [EN] 10th count latched the verdict: solid outputs. */
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__latched == true);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__blockOutput == true);    /* id 117 = 1 */
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__chargingAllowed == true);/* 0 of 20 cycles spent */

    /* [EN] Beep repeats after one period, not before. */
    uint32_t__t++;                                    /* beep fired at nowMs = uint32_t__t-1 */
    func__Run(uint32_t__t, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__beepDue == false);
    func__Run(uint32_t__t + 3000000u, false);         /* 50 min in: inside the hourly period */
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__beepDue == false);
    /* next beep at/after +3600000 ms from the latch beep */
    func__Run(uint32_t__t + 3600000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__beepDue == true);

    /* ---- charge cycles while latched spend the charge budget ---- */
    uint32_t__t += 3600001u;
    for (int cyc = 1; cyc <= 20; cyc++)
    {
        IMBAL_INPUTS_T__G__In.bool__charging = true;
        func__Run(uint32_t__t, true);                 /* cycle counted */
        CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__latchedCycles == (uint32_t)cyc);
        CHECK(IMBAL_OUTPUTS_T__G__Out.bool__chargingAllowed == (cyc < 20));
        uint32_t__t += 5000u;
        IMBAL_INPUTS_T__G__In.bool__charging = false;
        func__Run(uint32_t__t + 5000u, false);
        uint32_t__t += 10000u;
    }
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__chargingAllowed == false);

    /* [EN] Uncheck the block checkbox (user risk): veto lifts, latch stays. */
    CHECK(func__Imbalance_SetParam(117u, 0u, NULL));
    func__Run(uint32_t__t, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__latched == true);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__blockOutput == false);
    CHECK(func__Imbalance_SetParam(117u, 1u, NULL));

    /* ---- battery absent 3 s -> automatic full reset ---- */
    IMBAL_INPUTS_T__G__In.bool__batAbsent = true;
    func__Run(uint32_t__t + 1u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__latched == true);   /* not yet */
    /* [EN] Absent hiccup < 3 s, then battery "returns": no reset. */
    IMBAL_INPUTS_T__G__In.bool__batAbsent = false;
    func__Run(uint32_t__t + 2999u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__latched == true);
    /* [EN] Continuous 3000 ms absence -> reset everything. */
    IMBAL_INPUTS_T__G__In.bool__batAbsent = true;
    func__Run(uint32_t__t + 3001u, false);            /* timer starts */
    func__Run(uint32_t__t + 6001u, true);             /* 3000 reached: reset, persist */
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__latched == false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__latchedCycles == 0u);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__blockOutput == false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__chargingAllowed == true);

    /* ---- invalid snapshot freezes progress ---- */
    IMBAL_INPUTS_T__G__In.bool__batAbsent = false;
    IMBAL_INPUTS_T__G__In.bool__valid     = false;
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 15000u;
    func__Imbalance_Init();      /* [EN] fresh module, fresh window from boot=0 anchored */
    CHECK(func__Imbalance_SetParam(200u, 0u, NULL));
    func__Run(700000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);

    /* ---- discharge window uses its own limit immediately ---- */
    func__Imbalance_Init();
    IMBAL_INPUTS_T__G__In.bool__valid     = true;
    IMBAL_INPUTS_T__G__In.bool__onBattery = true;    /* output on battery = discharge */
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 12400u; /* imbalance 400: > rest 300 but < discharge 500 */
    func__Run(1000u, false);
    func__Run(40000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);   /* 400 < 500: no count */
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 12600u;        /* 600 > 500 */
    func__Run(41000u, false);
    func__Run(71000u, true);                                 /* 30 s later: count */
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 1u);

    /* ---- during-charge window only after id 111 ---- */
    func__Imbalance_Init();
    CHECK(func__Imbalance_SetParam(200u, 0u, NULL));   /* [EN] fresh counters for this scenario */
    IMBAL_INPUTS_T__G__In.bool__onBattery = false;
    IMBAL_INPUTS_T__G__In.bool__charging  = true;
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 13000u;   /* 1000 mV, over rest limit */
    func__Run(1000u, false);                            /* session starts; unlatched: no persist */
    func__Run(599000u, false);                          /* 598 s of charge: window closed */
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
    func__Run(601000u, false);                          /* 600 s: window opens */
    func__Run(631000u, true);                           /* +30 s stability: count */
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 1u);

    /* ---- v1.72: halves in different states are NOT comparable ----
       [EN] User order: "in the imbalance error both batteries must be in the
            same state - if one is charging and the other is resting they must
            not be compared". Same scenario as the block right above, but with
            channel 1 charging and channel 2 resting: the window must stay shut
            for ever, so no event is ever counted.
       [FA] دستور کاربر: دو نیم باید هم‌حالت باشند؛ یکی در شارژ و یکی در
            استراحت هرگز مقایسه نمی‌شوند - پس هیچ رویدادی شمرده نمی‌شود. */
    func__Imbalance_Init();
    CHECK(func__Imbalance_SetParam(200u, 0u, NULL));
    IMBAL_INPUTS_T__G__In.bool__onBattery   = false;
    IMBAL_INPUTS_T__G__In.bool__charging    = true;
    IMBAL_INPUTS_T__G__In.bool__chargingCh1 = true;
    IMBAL_INPUTS_T__G__In.bool__chargingCh2 = false;
    IMBAL_INPUTS_T__G__In.uint32_t__vHighMv = 13000u;   /* 1000 mV, far over the limit */
    for (uint32_t__t = 1000u; uint32_t__t <= 1200000u; uint32_t__t += 1000u)
    {
        func__Run(uint32_t__t, false);
        CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
        CHECK(IMBAL_OUTPUTS_T__G__Out.bool__halvesMismatch == true);
    }
    /* [EN] The moment both halves charge again the window behaves normally:
            the in-charge wait (600 s) then the 30 s stability still apply.
       [FA] به‌محض هم‌حالت شدن، همان گیت‌های همیشگی برقرارند. */
    IMBAL_INPUTS_T__G__In.bool__chargingCh2 = true;
    func__Run(1201000u, false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.bool__halvesMismatch == false);
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 0u);
    func__Run(1231000u, true);                          /* +30 s stability: count */
    CHECK(IMBAL_OUTPUTS_T__G__Out.uint32_t__events == 1u);

    printf("checks: %d, fails: %d\n", INT32_T__G__Checks, INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
