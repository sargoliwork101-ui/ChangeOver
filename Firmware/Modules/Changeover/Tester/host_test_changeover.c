/**
 * @file    host_test_changeover.c
 * @brief   [EN] Host unit test for the Changeover module. The PRODUCTION
 *              changeover.c is compiled as-is and driven through a fake
 *              clock and fake collaborators (BSP GPIO, imbalance veto,
 *              dead-battery veto). No board, no RTOS, no HAL.
 * @note    [EN] The harness only provides the symbols changeover.c needs to
 *              link; it never re-implements any decision the module makes.
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "changeover.h"
#include "bsp_gpio.h"
#include "modules_enable.h"
#if MODULE_IMBALANCE
#include "imbalance.h"
#endif
/* ==================== Check helper ==================== */
static int INT32_T__G__Checks = 0;
static int INT32_T__G__Fails  = 0;
#define CHECK(cond) do { \
        INT32_T__G__Checks++; \
        if (!(cond)) { INT32_T__G__Fails++; \
            printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)
/* ==================== Fake world ==================== */
/*
 * [EN] Fake kernel clock in ticks; the test moves it by hand.
 */
static uint32_t UINT32_T__G__FakeTick = 0u;
/*
 * [EN] Last value written to the battery-protect line, and a write counter so
 * a test can prove the module does NOT re-drive an already-driven pin.
 */
static bool     BOOL__G__ProtectLine  = false;
static uint32_t UINT32_T__G__ProtectWrites = 0u;
static bool BOOL__G__ImbalanceBlocks = false;
static bool BOOL__G__DeadBlocks      = false;
uint32_t osKernelGetTickCount(void);
uint32_t osKernelGetTickCount(void)
{
    return UINT32_T__G__FakeTick;
}
void func__BspGpio_Write(bsp_gpio_id_t bsp_gpio_id_t__id, bool bool__asserted)
{
    /*
 * [EN] Only the battery-protect line may be touched by this module.
 */
    CHECK(bsp_gpio_id_t__id == BSP_GPIO_PROTECT_BATTERY);
    BOOL__G__ProtectLine = bool__asserted;
    UINT32_T__G__ProtectWrites++;
}
/*
 * [EN] 1 kHz kernel: one tick is one millisecond, like the real config.
 */
uint32_t func__Rtos_MillisecondsToTicks(uint32_t uint32_t__milliseconds)
{
    return uint32_t__milliseconds;
}
#if MODULE_IMBALANCE
void func__Imbalance_GetOutputs(imbalance_outputs_t *imbalance_outputs_t__outputs)
{
    if (imbalance_outputs_t__outputs != NULL)
    {
        imbalance_outputs_t__outputs->uint32_t__imbalanceMv   = 0u;
        imbalance_outputs_t__outputs->bool__episode           = false;
        imbalance_outputs_t__outputs->uint32_t__events        = 0u;
        imbalance_outputs_t__outputs->bool__latched           = BOOL__G__ImbalanceBlocks;
        imbalance_outputs_t__outputs->uint32_t__latchedCycles = 0u;
        imbalance_outputs_t__outputs->bool__blockOutput       = BOOL__G__ImbalanceBlocks;
        imbalance_outputs_t__outputs->bool__chargingAllowed   = true;
        imbalance_outputs_t__outputs->bool__beepDue           = false;
        imbalance_outputs_t__outputs->bool__halvesMismatch    = false;
    }
}
#endif
#if MODULE_CHARGER
bool func__Charger_DeadBlocksOutput(void);
bool func__Charger_DeadBlocksOutput(void)
{
    return BOOL__G__DeadBlocks;
}
#endif
/* ==================== Test driver ==================== */
static measurement_snapshot_t MEASUREMENT_SNAPSHOT_T__G__Snap;
/**
 * @brief  [EN] Advance the fake clock and run one evaluation pass.
 */
static app_state_t func__Step(uint32_t uint32_t__advanceMs, fault_mask_t fault_mask_t__faults)
{
    UINT32_T__G__FakeTick += uint32_t__advanceMs;
    return func__Changeover_Evaluate(&MEASUREMENT_SNAPSHOT_T__G__Snap, fault_mask_t__faults);
}
/**
 * @brief  [EN] Fresh module + fresh fake world, battery healthy on mains.
 */
static void func__Reset(uint32_t uint32_t__startTick)
{
    UINT32_T__G__FakeTick       = uint32_t__startTick;
    BOOL__G__ImbalanceBlocks    = false;
    BOOL__G__DeadBlocks         = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv       = 230000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv    = 25000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv    = 12500u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv  = 12500u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv = 12500u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch1_ma      = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch2_ma      = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid         = true;
    /*
 * [EN] Simulate the safe boot pin, then the first healthy sample that
 * permits connection. Clear the write counter so each test measures only
 */
    func__Changeover_Init();
    BOOL__G__ProtectLine = true;
    (void)func__Changeover_Evaluate(&MEASUREMENT_SNAPSHOT_T__G__Snap, FAULT_NONE);
    UINT32_T__G__ProtectWrites = 0u;
}
/**
 * @brief  [EN] Drive the pack low enough to cut and hold it for the full
 *              3 s filter, then assert the cut happened exactly on time.
 */
static void func__CutAndVerifyTiming(void)
{
    (void)func__Step(10u, FAULT_NONE);
    CHECK(BOOL__G__ProtectLine == false);
    (void)func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE);
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
}
int main(void)
{
    app_state_t app_state_t__state;
    uint32_t    uint32_t__i;
    printf("== Changeover host test ==\n");
    /*
 * ---- 1. boot remains protected until a valid, healthy sample ----
 * [EN] Invalid data, a blocking fault, or voltage below the existing
 * 21.2 V hysteresis-clear threshold must not connect the output.
 */
    func__Reset(0u);
    func__Changeover_Init();
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 1u);
    UINT32_T__G__ProtectWrites = 0u;
    CHECK(func__Changeover_Evaluate(NULL, FAULT_NONE) == APP_STATE_BOOT);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 0u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_BOOT);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 0u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = true;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_RECONNECT_MV - 1u;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 0u);
    CHECK(func__Step(10u, FAULT_ADC) == APP_STATE_FAULT);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 0u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_RECONNECT_MV;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(UINT32_T__G__ProtectWrites == 1u);
    /*
 * [EN] A healthy no-mains sample connects immediately after validation.
 */
    func__Reset(100u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_BATTERY);
    CHECK(BOOL__G__ProtectLine == false);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    /* ---- 2. plain mapping from input presence ---- */
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_BATTERY);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);
    CHECK(UINT32_T__G__ProtectWrites == 0u);
    /* ---- 3. any fault wins, and it never moves the pin ---- */
    CHECK(func__Step(10u, FAULT_ADC) == APP_STATE_FAULT);
    CHECK(UINT32_T__G__ProtectWrites == 0u);
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);
    /* ---- 3b. charger-only faults must NOT disable battery cut or BATTERY state ---- */
    func__Reset(500u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    CHECK(func__Step(10u, FAULT_CHARGER_TECHNICAL) == APP_STATE_BATTERY);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    (void)func__Step(10u, FAULT_CHARGER_TECHNICAL | FAULT_CHARGER_BAT_LOST);
    CHECK(BOOL__G__ProtectLine == false);
    (void)func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_CHARGER_TECHNICAL | FAULT_CHARGER_BAT_LOST);
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(func__Step(1u, FAULT_CHARGER_TECHNICAL | FAULT_CHARGER_BAT_LOST) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    /* ---- 4. critical cut: below 20800 mV, 3 s filter ---- */
    func__Reset(1000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();
    CHECK(UINT32_T__G__ProtectWrites == 1u);
    /*
 * [EN] Once cut, further passes keep SAFE and do NOT re-drive the pin.
 */
    for (uint32_t__i = 0u; uint32_t__i < 10u; uint32_t__i++)
    {
        CHECK(func__Step(100u, FAULT_NONE) == APP_STATE_SAFE);
    }
    CHECK(UINT32_T__G__ProtectWrites == 1u);
    /*
 * ---- 5. v1.81 regression: the low-battery LATCH alone drives the cut ----
 * [EN] The pack dips under 21000 mV (latch closes), then recovers to
 * 21100 mV - inside the 21000..21200 hysteresis band - in the middle
 * of the 3 s window. Before v1.81 that froze the countdown and the
 * weak pack kept feeding the load. The latch only opens at 21200 mV,
 * so the cut must still land exactly 3 s after the window opened.
 */
    func__Reset(50000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_LOW_ALARM_CUT_MV - 1u;
    (void)func__Step(10u, FAULT_NONE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_LOW_ALARM_CUT_MV + 100u;
    (void)func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE);
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    /*
 * [EN] Mirror case: a real recovery to 21200 mV opens the latch and the
 * countdown is abandoned - no cut, ever.
 */
    func__Reset(90000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_LOW_ALARM_CUT_MV - 1u;
    (void)func__Step(10u, FAULT_NONE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_LOW_ALARM_CLEAR_MV;
    for (uint32_t__i = 0u; uint32_t__i < 20u; uint32_t__i++)
    {
        CHECK(func__Step(500u, FAULT_NONE) == APP_STATE_BATTERY);
    }
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(UINT32_T__G__ProtectWrites == 0u);
    /*
 * 2026-10-12) still govern: the input must be stable for 30 s and the
 * pack must hold 21200 mV for the whole 3 s filter.
 */
    func__Reset(200000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;    /* [EN] cut happens with no mains */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;     /* [EN] mains returns after the cut */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 24000u;      /* [EN] fully recovered */
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);     /* [EN] input stable starts */
    CHECK(func__Step(CHANGEOVER_INPUT_STABLE_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);     /* [EN] 30 s done, 3 s filter starts */
    CHECK(func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_INPUT);
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(UINT32_T__G__ProtectWrites == 2u);
    /*
 * ---- 6a. no mains, recovered pack: 10 s unbroken healthy + 3 s filter ----
 * [EN] User decision 2026-10-23: the pack that recovers after the cut (or is
 * replaced) must come back on its own, but only after the healthy
 * voltage has held WITHOUT A BREAK for CHANGEOVER_DEF_HEALTHY_HOLD_MS.
 * One dip inside the window restarts it; the state is BATTERY because
 * the mains really is absent.
 */
    func__Reset(250000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();
    /*
 * [EN] v1.90: the pack rests at 25.0 V - exactly the rest floor - because a
 * pack below it is no longer put back on the load (case 6e).
 */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);     /* [EN] healthy window opens */
    CHECK(func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS - 2000u, FAULT_NONE) == APP_STATE_SAFE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 21000u;      /* [EN] one dip -> restart */
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);      /* [EN] hold done, 3 s filter starts */
    CHECK(func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_BATTERY);   /* [EN] back on battery, no mains */
    CHECK(BOOL__G__ProtectLine == false);
    /*
 * ---- 6b. the 30 s stable-input clock must keep running DURING a cut ----
 * [EN] Regression for the 2026-10-20 audit: the cut branch returns early,
 * so a stamp that was only written below it froze at its pre-cut
 * value. If the mains dropped and came back while the cut condition
 * was still true, the reconnect check measured that stale stamp and
 * the 30 s order of 2026-10-12 was skipped. Here the input is present
 * for 40 s, then the pack is cut, the input is lost AND restored
 * while the cut still holds, and only then does the pack recover:
 * the reconnect must wait a FULL 30 s counted from the restore.
 */
    func__Reset(700000u);
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);
    CHECK(func__Step(40000u, FAULT_NONE) == APP_STATE_INPUT);   /* [EN] stamp is now 40 s old */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_LOW_ALARM_CUT_MV - 100u;
    (void)func__Step(10u, FAULT_NONE);
    CHECK(func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;      /* [EN] mains lost mid-cut */
    (void)func__Step(10u, FAULT_NONE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;       /* [EN] restored mid-cut */
    (void)func__Step(10u, FAULT_NONE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 22000u;        /* [EN] cut clears */
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    /*
 * [EN] 12 s after the restore: the old code would already have reconnected
 * (stale stamp 40 s old + the 3 s filter); the 30 s clock must hold.
 */
    for (uint32_t__i = 0u; uint32_t__i < 4u; uint32_t__i++)
    {
        CHECK(func__Step(3000u, FAULT_NONE) == APP_STATE_SAFE);
    }
    CHECK(BOOL__G__ProtectLine == true);
    (void)func__Step(CHANGEOVER_INPUT_STABLE_MS - 12010u - 1u, FAULT_NONE); /* [EN] just short */
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);        /* [EN] 30 s done, 3 s starts */
    CHECK(func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_INPUT);
    CHECK(BOOL__G__ProtectLine == false);
    /*
 * ---- 6c. cycle guard: a really weak pack must not loop forever ----
 * [EN] User decision 2026-10-23: the recovered pack may reconnect with no
 * mains, but if it cuts again inside CHANGEOVER_DEF_CYCLE_GUARD_MS the
 * pack is really weak and auto-reconnect without mains latches off
 * until the mains returns. Here: mains-free reconnect, a quick
 * re-cut, then 40 s of healthy voltage - it must stay SAFE.
 */
    func__Reset(800000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);     /* [EN] healthy window opens */
    CHECK(func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);      /* [EN] hold done, 3 s starts */
    CHECK(func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE) == APP_STATE_BATTERY);
    CHECK(BOOL__G__ProtectLine == false);
    /*
 * [EN] The weak pack sags again a few seconds later: cut, and the guard
 * latches because this is well inside the 60 s window.
 */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    (void)func__Step(10u, FAULT_NONE);
    CHECK(func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    /*
 * [EN] Even a long healthy stretch cannot undo the latch without mains.
 */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
    for (uint32_t__i = 0u; uint32_t__i < 10u; uint32_t__i++)
    {
        CHECK(func__Step(4000u, FAULT_NONE) == APP_STATE_SAFE);
    }
    CHECK(BOOL__G__ProtectLine == true);
    /*
 * [EN] The mains is the documented recovery from the latch: 30 s + 3 s.
 */
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(CHANGEOVER_INPUT_STABLE_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_INPUT);
    CHECK(BOOL__G__ProtectLine == false);
    /*
 * ---- 6d. scenario 8: weak battery verdict, block, rest and one retry ----
 * [EN] User order 2026-10-23: a pack that reconnects with no mains and
 * then cuts again inside the guard window is weak. The verdict must
 * latch, hold the battery path cut, keep the alarm face alive, and
 * fall only after its own quiet-rest window with a healthy pack -
 * then exactly one retry is allowed. The shape parameters must be
 * reachable and clamped like every other scenario.
 */
    func__Reset(950000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();
    CHECK(func__Changeover_WeakVerdictActive() == false);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;          /* [EN] recovers, no mains */
    (void)func__Step(10u, FAULT_NONE);
    (void)func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS, FAULT_NONE);
    (void)func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE);
    CHECK(BOOL__G__ProtectLine == false);                          /* [EN] first mains-free reconnect */
    /*
 * [EN] The weak pack sags again a few seconds later -> verdict.
 */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_BATTERY);
    CHECK(func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Changeover_WeakVerdictActive() == true);
    CHECK(BOOL__G__ProtectLine == true);
    /*
 * [EN] Healthy voltage alone must NOT reconnect while the verdict holds,
 * for the whole rest window except the last millisecond.
 */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(CHANGEOVER_DEF_WEAK_REST_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Changeover_WeakVerdictActive() == true);
    CHECK(BOOL__G__ProtectLine == true);
    /*
 * [EN] One millisecond later the verdict falls and the single retry runs:
 * the 3 s filter, then the pack is back on the output with no mains.
 */
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Changeover_WeakVerdictActive() == false);
    CHECK(func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_BATTERY);
    CHECK(BOOL__G__ProtectLine == false);
    /*
 * [EN] Shape parameters: clamped on write, readable back, foreign ids rejected.
 */
    {
        uint32_t uint32_t__applied = 0u;
        uint32_t uint32_t__readBack = 0u;
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_LED_PERIOD_MS, 0u, &uint32_t__applied) == true);
        CHECK(uint32_t__applied == 0u);                            /* [EN] 0 = solid red is legal */
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_LED_DUTY_PCT, 2u, &uint32_t__applied) == true);
        CHECK(uint32_t__applied == 5u);                            /* [EN] floor */
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_BEEP_COUNT, 20u, &uint32_t__applied) == true);
        CHECK(uint32_t__applied == 10u);                           /* [EN] ceiling */
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_BEEP_LEN_MS, 5u, &uint32_t__applied) == true);
        CHECK(uint32_t__applied == 20u);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_REST_MS, 1000u, &uint32_t__applied) == true);
        CHECK(uint32_t__applied == 60000u);
        CHECK(func__Changeover_GetParam(CHANGEOVER_PARAM_WEAK_BEEP_COUNT, &uint32_t__readBack) == true);
        CHECK(uint32_t__readBack == 10u);
        CHECK(func__Changeover_SetParam(1u, 0u, &uint32_t__applied) == false);
        CHECK(func__Changeover_GetParam(1u, &uint32_t__readBack) == false);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_ENABLE, 1u, &uint32_t__applied) == true);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_BEEP_COUNT,
                                        CHANGEOVER_DEF_WEAK_BEEP_COUNT, &uint32_t__applied) == true);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_BEEP_LEN_MS,
                                        CHANGEOVER_DEF_WEAK_BEEP_LEN_MS, &uint32_t__applied) == true);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_LED_DUTY_PCT,
                                        CHANGEOVER_DEF_WEAK_LED_DUTY_PCT, &uint32_t__applied) == true);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_LED_PERIOD_MS,
                                        CHANGEOVER_DEF_WEAK_LED_PERIOD_MS, &uint32_t__applied) == true);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_REST_MS,
                                        CHANGEOVER_DEF_WEAK_REST_MS, &uint32_t__applied) == true);
    }
    /*
 * [EN] Switching the scenario off releases a latched verdict immediately.
 */
    func__Reset(960000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
    (void)func__Step(10u, FAULT_NONE);
    (void)func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS, FAULT_NONE);
    (void)func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    (void)func__Step(10u, FAULT_NONE);
    (void)func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE);
    CHECK(func__Changeover_WeakVerdictActive() == true);
    {
        uint32_t uint32_t__applied = 0u;
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_ENABLE, 0u, &uint32_t__applied) == true);
        CHECK(uint32_t__applied == 0u);
    }
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
    (void)func__Step(10u, FAULT_NONE);
    CHECK(func__Changeover_WeakVerdictActive() == false);
    {
        uint32_t uint32_t__applied = 0u;
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_ENABLE,
                                        CHANGEOVER_DEF_WEAK_ENABLE, &uint32_t__applied) == true);
    }
    /*
 * [EN] Scenario-8 reset key (slot 204, the panel button): one call drops a
 * latched verdict and restarts the mains-free reconnect bookkeeping so
 * the single retry is available again. With no verdict latched it is a
 * harmless no-op that reports false.
 */
    {
        uint32_t uint32_t__applied = 0u;
        func__Reset(980000u);
        MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
        func__CutAndVerifyTiming();
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
        (void)func__Step(10u, FAULT_NONE);
        (void)func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS, FAULT_NONE);
        (void)func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE);
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
        (void)func__Step(10u, FAULT_NONE);
        (void)func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE);
        CHECK(func__Changeover_WeakVerdictActive() == true);
        CHECK(func__Changeover_WeakVerdictReset() == true);
        CHECK(func__Changeover_WeakVerdictActive() == false);
        CHECK(func__Changeover_WeakVerdictReset() == false);
        CHECK(uint32_t__applied == 0u);
    }
    /* ---- 6e. a discharged pack is NEVER put back on the load (v1.90) ---- */
    /*
 * [EN] Final user order 2026-10-23: after a low-battery cut, a pack that
 * only recovers to ~23 V is DISCHARGED, not replaced. It must stay off
 * the load and wait for the charger - no automatic reconnect, no
 * verdict, no attempt at all, with the scenario on or off. Only a pack
 * that rests at 25.0 V or more is put back, and that single attempt is
 * what decides "worn battery" (case 6d).
 */
    {
        func__Reset(1000000u);
        MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
        func__CutAndVerifyTiming();
        CHECK(func__Changeover_MainsFreeParked() == false);
        /*
 * [EN] Rests at 22.0 V for far longer than the hold window: still no
 * reconnect, because 22.0 V is below the 25.0 V rest floor.
 */
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 22000u;
        (void)func__Step(10u, FAULT_NONE);
        (void)func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS + 1000u, FAULT_NONE);
        (void)func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE);
        CHECK(BOOL__G__ProtectLine == true);
        CHECK(func__Changeover_WeakVerdictActive() == false);
        CHECK(func__Changeover_MainsFreeParked() == false);
        /*
 * [EN] Turning the scenario OFF must not change this: the floor rule is
 * battery protection, not a scenario feature.
 */
        {
            uint32_t uint32_t__applied = 0u;
            CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_ENABLE, 0u,
                                            &uint32_t__applied) == true);
        }
        (void)func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS + 1000u, FAULT_NONE);
        CHECK(BOOL__G__ProtectLine == true);
        {
            uint32_t uint32_t__applied = 0u;
            CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_ENABLE,
                                            CHANGEOVER_DEF_WEAK_ENABLE,
                                            &uint32_t__applied) == true);
        }
        /*
 * [EN] Mains returns: the normal input path brings the output back.
 */
        MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 26000u;
        (void)func__Step(10u, FAULT_NONE);
        (void)func__Step(CHANGEOVER_INPUT_STABLE_MS - 1u, FAULT_NONE);
        (void)func__Step(1u, FAULT_NONE);
        (void)func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE);
        CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_INPUT);
        CHECK(BOOL__G__ProtectLine == false);
    }
    /* ---- 6f. charger voltage is NOT rest voltage (2026-10-23 review) ---- */
    /*
 * [EN] The rest floor must be judged on a pack that is really resting: no
 * load AND no charger. A mains blip that lifts the pack to 27 V while
 * the charger pumps must never be mistaken for "this pack rested at
 * 25 V or more", otherwise the very next mains loss puts a discharged
 * pack back on the load and the collapse inside the guard latches a
 * FALSE worn-battery verdict - exactly the complaint of 2026-10-23.
 */
    {
        func__Reset(1020000u);
        MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
        func__CutAndVerifyTiming();
        /*
 * [EN] A 20 s mains blip: the charger lifts the pack to 27 V, but the
 * input never stays long enough to reconnect through the mains
 * path, so the battery switch is still open when it goes away.
 */
        MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 27000u;
        (void)func__Step(20000u, FAULT_NONE);
        CHECK(BOOL__G__ProtectLine == true);
        /*
 * [EN] Mains gone, the pack settles at 23.0 V: discharged, so it must
 * stay off the load with no verdict.
 */
        MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 23000u;
        (void)func__Step(10u, FAULT_NONE);
        (void)func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS + 1000u, FAULT_NONE);
        (void)func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE);
        CHECK(BOOL__G__ProtectLine == true);
        CHECK(func__Changeover_WeakVerdictActive() == false);
    }
    /*
 * [EN] The panel reset key hands the attempt back, but the rest floor still
 * decides whether the pack may come back with no mains.
 */
    {
        func__Reset(1010000u);
        MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
        func__CutAndVerifyTiming();
        (void)func__Changeover_WeakVerdictReset();
        CHECK(func__Changeover_MainsFreeParked() == false);
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 24000u;
        (void)func__Step(10u, FAULT_NONE);
        (void)func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS + 1000u, FAULT_NONE);
        CHECK(BOOL__G__ProtectLine == true);
    }
    /*
 * [EN] The rest floor itself: clamped on both sides, readable back.
 */
    {
        uint32_t uint32_t__applied = 0u;
        uint32_t uint32_t__readBack = 0u;
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV, 1000u,
                                        &uint32_t__applied) == true);
        CHECK(uint32_t__applied == CHANGEOVER_BAT_RECONNECT_MV);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV, 40000u,
                                        &uint32_t__applied) == true);
        CHECK(uint32_t__applied == 29000u);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV,
                                        CHANGEOVER_DEF_WEAK_REST_FLOOR_MV,
                                        &uint32_t__applied) == true);
        CHECK(uint32_t__applied == CHANGEOVER_DEF_WEAK_REST_FLOOR_MV);
        CHECK(func__Changeover_GetParam(CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV,
                                        &uint32_t__readBack) == true);
        CHECK(uint32_t__readBack == CHANGEOVER_DEF_WEAK_REST_FLOOR_MV);
    }
#if MODULE_IMBALANCE
    /* ---- 7. scenario-5 veto: a latched imbalance never reaches the output ---- */
    func__Reset(400000u);
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);
    BOOL__G__ImbalanceBlocks = true;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);     /* [EN] immediate, no 3 s */
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 1u);
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(10u, FAULT_ADC) == APP_STATE_FAULT);
    BOOL__G__ImbalanceBlocks = false;
#endif
#if MODULE_CHARGER
    /* ---- 8. scenario-6 veto: a condemned pack never reaches the output ---- */
    func__Reset(500000u);
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);
    BOOL__G__DeadBlocks = true;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    BOOL__G__DeadBlocks = false;
#endif
    /*
 * ---- 9. the 49.7-day tick wrap must not break the 3 s filter ----
 * [EN] Start the cut window a few ms before the tick counter wraps; the
 * unsigned subtraction has to carry the elapsed time across zero.
 */
    func__Reset(0xFFFFFF00u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();
    CHECK(UINT32_T__G__FakeTick < 0xFFFFFF00u);               /* [EN] really wrapped */
    /* ---- 10. the state getter mirrors the evaluation ---- */
    func__Reset(600000u);
    app_state_t__state = func__Step(10u, FAULT_NONE);
    CHECK(app_state_t__state == APP_STATE_INPUT);
    /* ---- 11. boot validation with no mains, healthy battery ---- */
    /*
 * [EN] Init must keep the battery cut until a valid sample arrives; with
 * no mains, a healthy 24 V pack then connects and remains connected.
 */
    func__Reset(700000u);
    func__Changeover_Init();
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 1u);
    UINT32_T__G__ProtectWrites = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_BOOT);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 0u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = true;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_BATTERY);
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(UINT32_T__G__ProtectWrites == 1u);
    for (uint32_t__i = 0u; uint32_t__i < 300u; uint32_t__i++)
    {
        (void)func__Step(10u, FAULT_NONE);
    }
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(UINT32_T__G__ProtectWrites == 1u);
    printf("checks: %d, fails: %d\n", INT32_T__G__Checks, INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
