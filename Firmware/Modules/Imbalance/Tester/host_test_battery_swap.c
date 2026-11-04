/**
 * @file    host_test_battery_swap.c
 * @brief   [EN] Integration regression for the production Fault -> TaskControl
 *              input mapping -> Imbalance battery-swap reset path. It uses a
 *              fake CMSIS clock and charger status; no board or HAL.
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "fault.h"
#include "charger.h"
#include "../imbalance.h"
/* ==================== Check helper ==================== */
static int32_t INT32_T__G__Checks = 0;
static int32_t INT32_T__G__Fails  = 0;
#define CHECK(cond) do { \
        INT32_T__G__Checks++; \
        if (!(cond)) { INT32_T__G__Fails++; \
            printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)
/* ==================== Fake world ==================== */
static uint32_t UINT32_T__G__FakeTick = 0u;
static bool BOOL__G__Ch0Active = false;
static bool BOOL__G__Ch1Active = false;
static bool BOOL__G__ManualTestActive = false;
static bool BOOL__G__ChargeComplete = false;
static bool BOOL__G__OnBattery = false;
static measurement_snapshot_t MEASUREMENT_SNAPSHOT_T__G__Snap;
static imbalance_inputs_t IMBALANCE_INPUTS_T__G__Inputs;
static imbalance_outputs_t IMBALANCE_OUTPUTS_T__G__Outputs;
/* ==================== Fake CMSIS tick / CMSIS ==================== */
/**
* @param void No parameters / .
* @return uint32_t Fake tick count, 1 ms per tick / ms.
 */
uint32_t osKernelGetTickCount(void)
{
    return UINT32_T__G__FakeTick;
}
/* ==================== Fake CMSIS lock / CMSIS ==================== */
/**
* @param void No parameters / .
* @return int32_t Placeholder previous state, always 0 / .
 */
int32_t osKernelLock(void)
{
    return 0;
}
/* ==================== Fake CMSIS restore / CMSIS ==================== */
/**
* @param int32_t__state Previous placeholder state / .
* @return int32_t The same state / .
 */
int32_t osKernelRestoreLock(int32_t int32_t__state)
{
    return int32_t__state;
}
/* ==================== Fake RTOS time / RTOS ==================== */
/**
* @param uint32_t__milliseconds Duration in ms / .
* @return uint32_t Same duration as ticks / .
 */
uint32_t func__Rtos_MillisecondsToTicks(uint32_t uint32_t__milliseconds)
{
    return uint32_t__milliseconds;
}
/* ==================== Fake charger channel ==================== */
/**
* @param uint8_t__channelIndex Channel index 0 or 1 / .
* @return bool Channel state; false for other indices / false.
 */
bool func__Charger_IsChannelActive(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return BOOL__G__Ch0Active;
    }
    if (uint8_t__channelIndex == 1u)
    {
        return BOOL__G__Ch1Active;
    }
    return false;
}
/* ==================== Fake charger state ==================== */
/**
* @param void No parameters / .
* @return bool true if channel 0 or 1 is active / true.
 */
bool func__Charger_IsAnyChannelActive(void)
{
    return ((BOOL__G__Ch0Active == true) || (BOOL__G__Ch1Active == true));
}
/* ==================== Fake manual-test state ==================== */
/**
* @param void No parameters / .
* @return bool true while manual test is active / true.
 */
bool func__Charger_IsManualTestModeActive(void)
{
    return BOOL__G__ManualTestActive;
}
/* ==================== Reset test world ==================== */
/**
 * @brief  [EN] Restore a healthy, idle test world and reset both production
 *              modules. The supplied time is in ms and must be nonzero so
 *              the Fault debounce sentinel is unambiguous.
* @param uint32_t__nowMs Fake monotonic time in ms /
* @return void No return value /
 */
static void func__ResetWorld(uint32_t uint32_t__nowMs)
{
    UINT32_T__G__FakeTick = uint32_t__nowMs;
    BOOL__G__Ch0Active = false;
    BOOL__G__Ch1Active = false;
    BOOL__G__ManualTestActive = false;
    BOOL__G__ChargeComplete = false;
    BOOL__G__OnBattery = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv = 24000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 24000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv = 12000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = 12000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv = 12000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch1_ma = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch2_ma = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = true;
    func__Fault_Init();
    func__Imbalance_Init();
}
/* ==================== Set measurements ==================== */
/**
 * @brief  [EN] Set the synthetic battery/input measurements for the next
 *              control pass. All voltages are in mV.
* @param uint32_t__highMv Upper-half voltage, 0..UINT32_MAX mV /
* @param uint32_t__lowMv Lower-half voltage, 0..UINT32_MAX mV /
* @param uint32_t__inputMv Input voltage, 0..UINT32_MAX mV /
* @param bool__inputPresent Input-presence GPIO state /
* @return void No return value /
 */
static void func__SetMeasurements(uint32_t uint32_t__highMv,
                                  uint32_t uint32_t__lowMv,
                                  uint32_t uint32_t__inputMv,
                                  bool bool__inputPresent)
{
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv = uint32_t__highMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv = uint32_t__lowMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = uint32_t__highMv + uint32_t__lowMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv = uint32_t__lowMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv = uint32_t__inputMv;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = bool__inputPresent;
}
/* ==================== Run production path ==================== */
/**
 * @brief  [EN] Mirror the production TaskControl mapping for the Fault and
 *              Imbalance modules, then run one production evaluation pass.
* @return bool true iff an Imbalance NVM-backed value changed / true
 */
static bool func__ControlPass(void)
{
    fault_mask_t fault_mask_t__faults;
    func__Fault_Evaluate(&MEASUREMENT_SNAPSHOT_T__G__Snap);
    fault_mask_t__faults = func__Fault_Get();
    IMBALANCE_INPUTS_T__G__Inputs.uint32_t__vHighMv =
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv;
    IMBALANCE_INPUTS_T__G__Inputs.uint32_t__vLowMv =
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv;
    IMBALANCE_INPUTS_T__G__Inputs.bool__inputPresent =
        MEASUREMENT_SNAPSHOT_T__G__Snap.input_present;
    IMBALANCE_INPUTS_T__G__Inputs.bool__valid =
        ((MEASUREMENT_SNAPSHOT_T__G__Snap.valid != false) &&
         ((fault_mask_t__faults & FAULT_ADC) == 0u));
    IMBALANCE_INPUTS_T__G__Inputs.bool__batAbsent =
        func__Fault_IsBatteryAbsent();
    IMBALANCE_INPUTS_T__G__Inputs.bool__charging =
        func__Charger_IsAnyChannelActive();
    IMBALANCE_INPUTS_T__G__Inputs.bool__chargeComplete = BOOL__G__ChargeComplete;
    IMBALANCE_INPUTS_T__G__Inputs.bool__chargingCh1 =
        func__Charger_IsChannelActive(0u);
    IMBALANCE_INPUTS_T__G__Inputs.bool__chargingCh2 =
        func__Charger_IsChannelActive(1u);
    IMBALANCE_INPUTS_T__G__Inputs.bool__onBattery = BOOL__G__OnBattery;
    return func__Imbalance_Evaluate(&IMBALANCE_INPUTS_T__G__Inputs,
                                    UINT32_T__G__FakeTick,
                                    &IMBALANCE_OUTPUTS_T__G__Outputs);
}
/* ==================== Advance fake time ==================== */
/**
 * @brief  [EN] Advance the fake clock and execute one control pass every
 *              10 ms. Duration is a nonnegative multiple of 10 ms.
* @param uint32_t__durationMs Elapsed fake time in ms /
* @return bool true iff the final pass changed persisted state / true
 */
static bool func__AdvanceControl(uint32_t uint32_t__durationMs)
{
    uint32_t uint32_t__elapsedMs;
    bool bool__persistChanged = false;
    for (uint32_t__elapsedMs = 0u;
         uint32_t__elapsedMs < uint32_t__durationMs;
         uint32_t__elapsedMs += 10u)
    {
        UINT32_T__G__FakeTick += 10u;
        bool__persistChanged = func__ControlPass();
    }
    return bool__persistChanged;
}
/* ==================== Seed persistent state ==================== */
/**
 * @brief  [EN] Load the three NVM-backed Imbalance values used by the test.
* @param uint32_t__events Event count, 0..255 /
* @param uint32_t__cycles Post-latch cycles, 0..255 /
* @param uint32_t__latched Latch state, 0 or 1 /
* @return void No return value /
 */
static void func__SetPersistedState(uint32_t uint32_t__events,
                                    uint32_t uint32_t__cycles,
                                    uint32_t uint32_t__latched)
{
    CHECK(func__Imbalance_SetParam(IMBAL_SLOT_EVENTS_ID, uint32_t__events, NULL));
    CHECK(func__Imbalance_SetParam(IMBAL_SLOT_CYCLES_ID, uint32_t__cycles, NULL));
    CHECK(func__Imbalance_SetParam(IMBAL_SLOT_LATCH_ID, uint32_t__latched, NULL));
}
/* ==================== Check persistent state ==================== */
/**
 * @brief  [EN] Assert all three NVM-backed state values.
* @param uint32_t__events Expected event count, 0..255 /
* @param uint32_t__cycles Expected cycle count, 0..255 /
* @param uint32_t__latched Expected latch state, 0 or 1 /
* @return void No return value /
 */
static void func__CheckPersistedState(uint32_t uint32_t__events,
                                      uint32_t uint32_t__cycles,
                                      uint32_t uint32_t__latched)
{
    uint32_t uint32_t__value = 0u;
    CHECK(func__Imbalance_GetParam(IMBAL_SLOT_EVENTS_ID, &uint32_t__value));
    CHECK(uint32_t__value == uint32_t__events);
    CHECK(func__Imbalance_GetParam(IMBAL_SLOT_CYCLES_ID, &uint32_t__value));
    CHECK(uint32_t__value == uint32_t__cycles);
    CHECK(func__Imbalance_GetParam(IMBAL_SLOT_LATCH_ID, &uint32_t__value));
    CHECK(uint32_t__value == uint32_t__latched);
}
/* ==================== Battery-swap integration test ==================== */
/**
 * @brief  [EN] Run the Fault-to-Imbalance battery-swap regression suite.
* @param void No parameters / .
* @return int Zero on success, nonzero on any failed check / .
 */
int main(void)
{
    uint32_t uint32_t__value = 0u;
    bool bool__changed;
    printf("== Battery swap integration host test ==\n");
    /* ---- 1. Real disconnect path with no mains; stale cycle must not relatch ---- */
    func__ResetWorld(1000u);
    func__SetPersistedState(1u, 0u, 1u);
    CHECK(func__Imbalance_SetParam(IMBAL_PARAM_MAX_EVENTS, 1u, &uint32_t__value));
    CHECK(func__Imbalance_SetParam(IMBAL_PARAM_IN_CHARGE_WAIT_MS, 1000u, &uint32_t__value));
    CHECK(func__Imbalance_SetParam(IMBAL_PARAM_STABILITY_MS, 1000u, &uint32_t__value));
    func__SetMeasurements(13000u, 12000u, 24000u, true);
    BOOL__G__Ch0Active = true;
    BOOL__G__Ch1Active = true;
    BOOL__G__ChargeComplete = false;
    (void)func__ControlPass();
    (void)func__AdvanceControl(1000u);
    (void)func__AdvanceControl(1000u);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.bool__episode == true);
    func__CheckPersistedState(1u, 1u, 1u);
    /*
 * [EN] With mains absent, a fitted half below 3 V signals physical removal
 * for reset only; it must not set the shared charger-fault bit.
 */
    func__SetMeasurements(12000u, FAULT_BAT_HARD_ABSENT_MV - 1u, 0u, false);
    BOOL__G__ChargeComplete = true; /* stale FLOAT must be ignored */
    bool__changed = func__ControlPass();
    CHECK(bool__changed == false);
    CHECK(func__Fault_IsBatteryAbsent() == true);
    CHECK(func__Fault_Get() == FAULT_NONE);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.bool__episode == false);
    func__CheckPersistedState(1u, 1u, 1u);
    CHECK(func__AdvanceControl(2990u) == false);
    func__CheckPersistedState(1u, 1u, 1u);
    CHECK(func__AdvanceControl(10u) == true);
    func__CheckPersistedState(0u, 0u, 0u);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.bool__latched == false);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.bool__blockOutput == false);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.bool__chargingAllowed == true);
    CHECK(func__AdvanceControl(10u) == false);
    func__CheckPersistedState(0u, 0u, 0u);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.uint32_t__events == 0u);
    CHECK(IMBALANCE_OUTPUTS_T__G__Outputs.bool__latched == false);
    /*
 * [EN] Reconnection drops the live signal immediately. A stale FLOAT with
 * no new charge must not resurrect the cleared full-cycle state.
 */
    func__SetMeasurements(12000u, 12000u, 0u, false);
    BOOL__G__Ch0Active = false;
    BOOL__G__Ch1Active = false;
    CHECK(func__ControlPass() == false);
    CHECK(func__Fault_IsBatteryAbsent() == false);
    func__CheckPersistedState(0u, 0u, 0u);
    /* ---- 2. A short absence that recovers before 3 s preserves the latch ---- */
    func__ResetWorld(20000u);
    func__SetPersistedState(2u, 3u, 1u);
    func__SetMeasurements(12000u, 5000u, 24000u, true);
    (void)func__ControlPass();
    CHECK(func__Fault_IsBatteryAbsent() == true);
    CHECK(func__AdvanceControl(2000u) == false);
    CHECK((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != 0u);
    func__CheckPersistedState(2u, 3u, 1u);
    func__SetMeasurements(12500u, 12500u, 24000u, true);
    CHECK(func__ControlPass() == false);
    CHECK(func__Fault_IsBatteryAbsent() == false);
    CHECK((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != 0u);
    func__CheckPersistedState(2u, 3u, 1u);
    CHECK(func__AdvanceControl(990u) == false);
    CHECK((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != 0u);
    CHECK(func__AdvanceControl(10u) == false);
    CHECK(func__Fault_Get() == FAULT_NONE);
    func__CheckPersistedState(2u, 3u, 1u);
    /* ---- 3. No mains + 5 V is not the hard-open signature ---- */
    func__ResetWorld(30000u);
    func__SetPersistedState(3u, 2u, 1u);
    func__SetMeasurements(12000u, 5000u, 0u, false);
    CHECK(func__ControlPass() == false);
    CHECK(func__Fault_IsBatteryAbsent() == false);
    CHECK(func__Fault_Get() == FAULT_NONE);
    CHECK(func__AdvanceControl(5000u) == false);
    func__CheckPersistedState(3u, 2u, 1u);
    /* ---- 4. Invalid measurements break the 3 s continuous proof ---- */
    func__ResetWorld(40000u);
    func__SetPersistedState(4u, 2u, 1u);
    func__SetMeasurements(12000u, FAULT_BAT_HARD_ABSENT_MV - 1u, 0u, false);
    CHECK(func__ControlPass() == false);
    CHECK(func__AdvanceControl(1990u) == false);
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
    CHECK(func__AdvanceControl(1000u) == false);
    CHECK(func__Fault_IsBatteryAbsent() == false);
    func__CheckPersistedState(4u, 2u, 1u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = true;
    CHECK(func__ControlPass() == false);
    CHECK(func__Fault_IsBatteryAbsent() == true);
    CHECK(func__AdvanceControl(2990u) == false);
    func__CheckPersistedState(4u, 2u, 1u);
    CHECK(func__AdvanceControl(10u) == true);
    func__CheckPersistedState(0u, 0u, 0u);
    printf("checks: %" PRId32 ", fails: %" PRId32 "\n",
           INT32_T__G__Checks,
           INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
