/**
 * @file    changeover.c
 * @brief   [EN] Input vs battery path state machine - timed cut/reconnect with 3000ms filtering.
 * @note    [EN] This module uses ONLY: snapshot.valid, snapshot.v_bat24_mv,
 *              snapshot.input_present, fault_mask (the low-battery latch and the
 *              healthy-voltage hold window are internal).
 *              No board_pins.h, no HAL, no PB5/PB7, no float/queue/task.
 *              Time conversion ONLY via rtos_time.h (MillisecondsToTicks); tick=1ms assumption is forbidden.
 *              PB11 (BSP_GPIO_PROTECT_BATTERY) is the ONLY logical pin used.
 */
#include "changeover.h"
#include "bsp_gpio.h"
#include "rtos_time.h"
#include "cmsis_os2.h"
#include "modules_enable.h"
#if MODULE_IMBALANCE
#include "imbalance.h"
#endif
#if MODULE_CHARGER
#include "charger.h"
#endif
#include <stdbool.h>
#include <stdint.h>
/* ==================== Static State ==================== */
/**
 * @brief  [EN] Current system state.
 */
static app_state_t APP_STATE_T__G__State = APP_STATE_BOOT;
/**
 * @brief  [EN] Logical PB11 protect state: true = battery path cut (protect asserted).
 */
static bool BOOL__G__ChangeoverProtectAsserted = false;
/**
 * @brief [EN] Boot stays protected until a valid sample passes blocking checks.
 */
static bool BOOL__G__BootPending = true;
/**
 * @brief  [EN] Tick at which the current cut condition became continuously true.
 */
static uint32_t UINT32_T__G__CutStartTick = 0u;
/**
 * @brief  [EN] Whether a cut condition is currently being timed.
 */
static bool BOOL__G__CutTimerActive = false;
/**
 * @brief  [EN] Internal low-battery latch (v1.74, was the UI flag + link params 72/73).
 *         Set while v_bat24_mv < CHANGEOVER_BAT_LOW_ALARM_CUT_MV, cleared at or above
 *         CHANGEOVER_BAT_LOW_ALARM_CLEAR_MV, held in between so the gated cut sees a
 *         continuous level and not a chattering edge.
 */
static bool BOOL__G__LowBatteryLatched = false;
/**
 * @brief  [EN] Tick at which the reconnect condition became continuously true.
 */
static uint32_t UINT32_T__G__ReconnectStartTick = 0u;
/**
 * @brief  [EN] Whether a reconnect condition is currently being timed.
 */
static bool BOOL__G__ReconnectTimerActive = false;
/**
 * @brief  [EN] Input stable tracking for 30s extra delay before reconnect.
 */
static uint32_t UINT32_T__G__InputStableSinceTick = 0u;
static bool BOOL__G__InputStableActive = false;
/**
 * @brief  [EN] Continuous healthy-voltage window for the mains-free recovery
 *         rule: true while v_bat24_mv has stayed at or above
 *         CHANGEOVER_BAT_RECONNECT_MV with no break, plus the tick that window
 *         started. One dip restarts it.
 */
static uint32_t UINT32_T__G__HealthySinceTick = 0u;
static bool BOOL__G__HealthyActive = false;
/**
 * @brief  [EN] Mains-free reconnect bookkeeping and cycle guard: tick and flag
 *         of the last successful mains-free reconnect, plus the latch that
 *         stops repeating it when the pack cuts again inside
 *         CHANGEOVER_PARAM_CYCLE_GUARD_MS. The scenario-8 verdict, not this
 *         bookkeeping, is what stops the retries.
 */
static uint32_t UINT32_T__G__MainsFreeReconnectTick = 0u;
static bool BOOL__G__MainsFreeReconnectDone = false;
/**
 * @brief  [EN] Scenario-8 verdict: latched when a mains-free reconnect is
 *         followed by another cut inside the guard window, i.e. the pack
 *         cannot carry the load. While it holds, the battery path stays cut
 *         (when id 157 is on) and the UI shows the weak-battery face. It
 *         clears by itself after a quiet rest with a healthy pack, which is
 *         the single retry the user ordered on 2026-10-23.
 */
static bool BOOL__G__WeakVerdict = false;
static uint32_t UINT32_T__G__WeakVerdictTick = 0u;
/*
 * [EN] v1.88 (second user order 2026-10-23): an empty pack is not a weak pack.
 *      The rest peak is the highest voltage the pack held while the load was
 *      OFF before the last mains-free reconnect; the verdict only latches when
 *      that rest peak reached CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV. The failed
 *      attempt counter bounds the reconnect/cut cycle for an empty pack, and
 *      once the budget is spent the path stays parked until the mains returns.
 */
static uint32_t UINT32_T__G__RestPeakMv = 0u;
static uint32_t UINT32_T__G__LastRestPeakMv = 0u;
static uint8_t UINT8_T__G__MainsFreeFailCount = 0u;
static bool BOOL__G__MainsFreeParked = false;
/* ==================== Scenario-8 parameter table ==================== */
/**
 * @brief  [EN] Compiled defaults, indexed by (id - CHANGEOVER_PARAM_FIRST_ID).
 */
static const uint32_t UINT32_T__G__WeakParamDefault[CHANGEOVER_PARAM_COUNT] =
{
    CHANGEOVER_DEF_WEAK_ENABLE,
    CHANGEOVER_DEF_WEAK_REST_MS,
    CHANGEOVER_DEF_CYCLE_GUARD_MS,
    CHANGEOVER_DEF_HEALTHY_HOLD_MS,
    CHANGEOVER_DEF_WEAK_LED_PERIOD_MS,
    CHANGEOVER_DEF_WEAK_LED_DUTY_PCT,
    CHANGEOVER_DEF_WEAK_BEEP_PERIOD_MS,
    CHANGEOVER_DEF_WEAK_BEEP_LEN_MS,
    CHANGEOVER_DEF_WEAK_BEEP_COUNT,
    CHANGEOVER_DEF_WEAK_BEEP_GAP_MS,
    CHANGEOVER_DEF_WEAK_BLOCK_OUTPUT,
    CHANGEOVER_DEF_WEAK_REST_FLOOR_MV
};
/**
 * @brief  [EN] Live values written by the panel/flash replay; seeded from the
 *         defaults on the first touch so a module Init never undoes a stored
 *         value (same rule as the other modules).
 */
static uint32_t UINT32_T__G__WeakParamLive[CHANGEOVER_PARAM_COUNT];
static bool BOOL__G__WeakParamSeeded = false;
/* ==================== Scenario-8 parameter plumbing ==================== */
/**
 * @brief  [EN] Map a scenario-8 id to its slot in the live table.
 *              v1.88: the ids are NOT contiguous any more - 147..157 are the
 *              original eleven and 165 is the rest floor appended after the
 *              v1.87 alarm ticks 158..164 - so the slot cannot be computed by
 *              subtracting the first id any more. A wrong mapping here would
 *              write the floor into another parameter's slot.
* @param uint8_t__paramId 147..165 /
 * @return uint8_t [EN] Slot index, CHANGEOVER_PARAM_INDEX_INVALID when foreign
 */
static uint8_t func__Changeover_ParamIndex(uint8_t uint8_t__paramId)
{
    if ((uint8_t__paramId >= CHANGEOVER_PARAM_WEAK_ENABLE) &&
        (uint8_t__paramId <= CHANGEOVER_PARAM_WEAK_BLOCK_OUTPUT))
    {
        return (uint8_t)(uint8_t__paramId - CHANGEOVER_PARAM_WEAK_ENABLE);
    }
    if (uint8_t__paramId == CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV)
    {
        return 11u;
    }
    return CHANGEOVER_PARAM_INDEX_INVALID;
}
/**
 * @brief  [EN] Seed the live table from the compiled defaults on first use.
 */
static void func__Changeover_SeedParamsOnce(void)
{
    if (BOOL__G__WeakParamSeeded == false)
    {
        uint8_t uint8_t__index;
        for (uint8_t__index = 0u; uint8_t__index < CHANGEOVER_PARAM_COUNT; uint8_t__index++)
        {
            UINT32_T__G__WeakParamLive[uint8_t__index] =
                UINT32_T__G__WeakParamDefault[uint8_t__index];
        }
        BOOL__G__WeakParamSeeded = true;
    }
}
/**
 * @brief  [EN] Clamp one scenario-8 value into its compiled window.
* @param uint8_t__paramId 147..157 /
* @param uint32_t__value Raw value /
* @return uint32_t Clamped value /
 */
static uint32_t func__Changeover_ClampParam(uint8_t uint8_t__paramId, uint32_t uint32_t__value)
{
    uint32_t uint32_t__ret = uint32_t__value;
    switch (uint8_t__paramId)
    {
        case CHANGEOVER_PARAM_WEAK_ENABLE:
        case CHANGEOVER_PARAM_WEAK_BLOCK_OUTPUT:
            if (uint32_t__ret > 1u) { uint32_t__ret = 1u; }
            break;
        case CHANGEOVER_PARAM_WEAK_REST_MS:
            if (uint32_t__ret < 60000u)   { uint32_t__ret = 60000u; }
            if (uint32_t__ret > 3600000u) { uint32_t__ret = 3600000u; }
            break;
        case CHANGEOVER_PARAM_CYCLE_GUARD_MS:
            if (uint32_t__ret < 1000u)   { uint32_t__ret = 1000u; }
            if (uint32_t__ret > 600000u) { uint32_t__ret = 600000u; }
            break;
        case CHANGEOVER_PARAM_HEALTHY_HOLD_MS:
            if (uint32_t__ret < 1000u)   { uint32_t__ret = 1000u; }
            if (uint32_t__ret > 300000u) { uint32_t__ret = 300000u; }
            break;
        case CHANGEOVER_PARAM_WEAK_LED_PERIOD_MS:
            /*
 * [EN] 0 = solid red is a legal opt-out; other values stay inside
 * the visible-blink window.
 */
            if (uint32_t__ret != 0u)
            {
                if (uint32_t__ret < 100u)   { uint32_t__ret = 100u; }
                if (uint32_t__ret > 10000u) { uint32_t__ret = 10000u; }
            }
            break;
        case CHANGEOVER_PARAM_WEAK_LED_DUTY_PCT:
            if (uint32_t__ret < 5u)  { uint32_t__ret = 5u; }
            if (uint32_t__ret > 95u) { uint32_t__ret = 95u; }
            break;
        case CHANGEOVER_PARAM_WEAK_BEEP_PERIOD_MS:
            /*
 * [EN] 0 = silent; any other value is pulled into 1 s..10 min.
 */
            if (uint32_t__ret != 0u)
            {
                if (uint32_t__ret < 1000u)   { uint32_t__ret = 1000u; }
                if (uint32_t__ret > 600000u) { uint32_t__ret = 600000u; }
            }
            break;
        case CHANGEOVER_PARAM_WEAK_BEEP_LEN_MS:
            if (uint32_t__ret < 20u)   { uint32_t__ret = 20u; }
            if (uint32_t__ret > 2000u) { uint32_t__ret = 2000u; }
            break;
        case CHANGEOVER_PARAM_WEAK_BEEP_COUNT:
            if (uint32_t__ret > 10u) { uint32_t__ret = 10u; }
            break;
        case CHANGEOVER_PARAM_WEAK_BEEP_GAP_MS:
            if (uint32_t__ret > 5000u) { uint32_t__ret = 5000u; }
            break;
        case CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV:
            /*
 * [EN] Below the reconnect threshold the floor would be dead text
 * (every reconnect needs 21.2 V anyway), above the charge ceiling
 * no pack could ever qualify.
 */
            if (uint32_t__ret < CHANGEOVER_BAT_RECONNECT_MV) { uint32_t__ret = CHANGEOVER_BAT_RECONNECT_MV; }
            if (uint32_t__ret > 29000u) { uint32_t__ret = 29000u; }
            break;
        default:
            break;
    }
    return uint32_t__ret;
}
/**
 * @brief  [EN] Live read of one scenario-8 value (seeds the table first).
* @param uint8_t__paramId 147..157 /
* @return uint32_t Live value, 0 for a foreign id /
 */
static uint32_t func__Changeover_ParamValue(uint8_t uint8_t__paramId)
{
    uint8_t uint8_t__index;
    func__Changeover_SeedParamsOnce();
    uint8_t__index = func__Changeover_ParamIndex(uint8_t__paramId);
    if (uint8_t__index == CHANGEOVER_PARAM_INDEX_INVALID)
    {
        return 0u;
    }
    return UINT32_T__G__WeakParamLive[uint8_t__index];
}
/* ==================== Changeover_SetParam ==================== */
/**
 * @brief  [EN] Write one scenario-8 parameter through the clamp table.
* @param uint8_t__paramId 147..157 /
* @param uint32_t__value Requested value /
* @param uint32_t__appliedValue Applied value out, NULL rejects /
* @return bool true when the id belongs to this module /
 */
bool func__Changeover_SetParam(uint8_t uint8_t__paramId,
                               uint32_t uint32_t__value,
                               uint32_t *uint32_t__appliedValue)
{
    uint32_t uint32_t__clamped;
    uint8_t uint8_t__index;
    if (uint32_t__appliedValue == NULL)
    {
        return false;
    }
    uint8_t__index = func__Changeover_ParamIndex(uint8_t__paramId);
    if (uint8_t__index == CHANGEOVER_PARAM_INDEX_INVALID)
    {
        return false;
    }
    func__Changeover_SeedParamsOnce();
    uint32_t__clamped = func__Changeover_ClampParam(uint8_t__paramId, uint32_t__value);
    UINT32_T__G__WeakParamLive[uint8_t__index] = uint32_t__clamped;
    *uint32_t__appliedValue = uint32_t__clamped;
    /*
 * [EN] Turning the scenario off must not leave a stale verdict behind.
 */
    if ((uint8_t__paramId == CHANGEOVER_PARAM_WEAK_ENABLE) && (uint32_t__clamped == 0u))
    {
        BOOL__G__WeakVerdict = false;
    }
    return true;
}
/* ==================== Changeover_GetParam ==================== */
/**
 * @brief  [EN] Read one live scenario-8 parameter.
* @param uint8_t__paramId 147..157 /
* @param uint32_t__value Value out /
* @return bool true when the id belongs to this module /
 */
bool func__Changeover_GetParam(uint8_t uint8_t__paramId,
                               uint32_t *uint32_t__value)
{
    if (uint32_t__value == NULL)
    {
        return false;
    }
    if (func__Changeover_ParamIndex(uint8_t__paramId) == CHANGEOVER_PARAM_INDEX_INVALID)
    {
        return false;
    }
    *uint32_t__value = func__Changeover_ParamValue(uint8_t__paramId);
    return true;
}
/* ==================== Changeover_WeakVerdictActive ==================== */
/**
 * @brief  [EN] Is the scenario-8 weak-battery verdict latched?
* @return bool true while latched / true
 */
bool func__Changeover_WeakVerdictActive(void)
{
    return BOOL__G__WeakVerdict;
}
/* ==================== Changeover_PathBlocked ==================== */
/**
 * @brief  [EN] Is the battery path physically cut right now? True covers every
 *              reason the PB11 line is asserted: the 3 s low-battery filter
 *              has completed, the scenario-8 block is holding the path, or the
 *              retry budget parked it. The UI uses this to keep the lamps dark
 *              instead of blinking "running on battery" while the load is off.
* @return bool true while the path is cut /
 */
bool func__Changeover_PathBlocked(void)
{
    return BOOL__G__ChangeoverProtectAsserted;
}
/* ==================== Changeover_MainsFreeParked ==================== */
/**
 * @brief  [EN] Is the mains-free reconnect parked (retry budget spent)?
 *              True means the battery path stays cut until the mains returns;
 *              this is the empty-pack guard, NOT the weak-battery verdict.
* @return bool true while parked / true
 */
bool func__Changeover_MainsFreeParked(void)
{
    return BOOL__G__MainsFreeParked;
}
/* ==================== Changeover_WeakVerdictReset ==================== */
/**
 * @brief  [EN] Drop the scenario-8 verdict on demand (panel reset key).
 *              The reconnect bookkeeping is cleared too, otherwise the next
 *              evaluation would re-latch on the same cut; the single retry
 *              becomes available again, and the normal gates still decide
 *              when the battery may actually come back.
* @return bool true when a verdict was latched before the call /
 */
bool func__Changeover_WeakVerdictReset(void)
{
    bool bool__wasLatched;
    bool__wasLatched = BOOL__G__WeakVerdict;
    BOOL__G__WeakVerdict = false;
    UINT32_T__G__WeakVerdictTick = 0u;
    BOOL__G__MainsFreeReconnectDone = false;
    UINT32_T__G__MainsFreeReconnectTick = 0u;
    BOOL__G__HealthyActive = false;
    UINT32_T__G__HealthySinceTick = 0u;
    /*
 * [EN] The manual reset is also the user's "try again" key: it gives the
 * full retry budget back and un-parks the path.
 */
    UINT8_T__G__MainsFreeFailCount = 0u;
    BOOL__G__MainsFreeParked = false;
    UINT32_T__G__RestPeakMv = 0u;
    UINT32_T__G__LastRestPeakMv = 0u;
    return bool__wasLatched;
}
/* ==================== Changeover_Init ==================== */
/**
 * @brief  [EN] Start protected in BOOT; connect only after valid measurements
 *              pass blocking checks and the existing battery threshold.
 */
void func__Changeover_Init(void)
{
    APP_STATE_T__G__State = APP_STATE_BOOT;
    BOOL__G__ChangeoverProtectAsserted = true;
    BOOL__G__BootPending = true;
    UINT32_T__G__CutStartTick = 0u;
    BOOL__G__CutTimerActive = false;
    UINT32_T__G__ReconnectStartTick = 0u;
    BOOL__G__ReconnectTimerActive = false;
    BOOL__G__LowBatteryLatched = false;
    UINT32_T__G__InputStableSinceTick = 0u;
    BOOL__G__InputStableActive = false;
    UINT32_T__G__HealthySinceTick = 0u;
    BOOL__G__HealthyActive = false;
    UINT32_T__G__MainsFreeReconnectTick = 0u;
    BOOL__G__MainsFreeReconnectDone = false;
    BOOL__G__WeakVerdict = false;
    UINT32_T__G__WeakVerdictTick = 0u;
    UINT32_T__G__RestPeakMv = 0u;
    UINT32_T__G__LastRestPeakMv = 0u;
    UINT8_T__G__MainsFreeFailCount = 0u;
    BOOL__G__MainsFreeParked = false;
    func__Changeover_SeedParamsOnce();
    /*
 * [EN] Keep the path physically cut until the first valid, non-blocked
 * sample confirms the pack has reached the 21.2 V hysteresis-clear
 */
    func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
}
/* ==================== Changeover_Evaluate ==================== */
/**
 * @brief  [EN] Evaluate next system state from snapshot and faults.
* @param measurement_snapshot_t__snap Snapshot from measurement, may be NULL /
* @param fault_mask_t__faults Fault bits from Fault module /
* @return app_state_t Next system state /
 */
app_state_t func__Changeover_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap, fault_mask_t fault_mask_t__faults)
{
    uint32_t uint32_t__nowTick;
    uint32_t uint32_t__durationTicks;
    uint32_t uint32_t__healthyHoldTicks;
    uint32_t uint32_t__restTicks;
    uint32_t uint32_t__guardTicks;
    bool bool__cutCondition;
    bool bool__reconnectCondition;
    uint32_t__nowTick = osKernelGetTickCount();
    uint32_t__durationTicks = func__Rtos_MillisecondsToTicks(CHANGEOVER_DURATION_MS);
    uint32_t__healthyHoldTicks = func__Rtos_MillisecondsToTicks(
        func__Changeover_ParamValue(CHANGEOVER_PARAM_HEALTHY_HOLD_MS));
    uint32_t__restTicks = func__Rtos_MillisecondsToTicks(
        func__Changeover_ParamValue(CHANGEOVER_PARAM_WEAK_REST_MS));
    uint32_t__guardTicks = func__Rtos_MillisecondsToTicks(
        func__Changeover_ParamValue(CHANGEOVER_PARAM_CYCLE_GUARD_MS));
    /*
 * [EN] Snapshot check FIRST before any decision: if NULL or invalid,
 * preserve state and PB11, reset pending timers, fault does NOT change state,
 * and invalid time is not counted.
 */
    if (measurement_snapshot_t__snap == NULL)
    {
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }
    if (measurement_snapshot_t__snap->valid == false)
    {
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }
    /*
 * [EN] Track input-stability on every valid pass before any early return
 * (fault, scenario-5/6 vetoes, or cut branch) so a mains loss/restore
 * during a fault or veto is never masked by a frozen pre-fault stamp.
 */
    if (measurement_snapshot_t__snap->input_present == true)
    {
        if (BOOL__G__InputStableActive == false)
        {
            BOOL__G__InputStableActive = true;
            UINT32_T__G__InputStableSinceTick = uint32_t__nowTick;
        }
    }
    else
    {
        BOOL__G__InputStableActive = false;
        UINT32_T__G__InputStableSinceTick = 0u;
    }
    /*
 * [EN] Healthy-pack window for the mains-free recovery rule (user decision
 * 2026-10-23): the voltage must stay at or above the reconnect
 * threshold WITHOUT A BREAK for CHANGEOVER_PARAM_HEALTHY_HOLD_MS before the
 * module will put the battery back on the output with no mains. Any
 * dip restarts the window, so a pack that only bounced for a moment
 * is not accepted. The absent signal of the Fault module is NOT used
 * here: it is a threshold guess, and one cut wire does not reliably
 * drive any sense input below its absent level (2026-10-23 order).
 * Returning mains clears the cycle guard of the next outage.
 */
    if (measurement_snapshot_t__snap->v_bat24_mv >= CHANGEOVER_BAT_RECONNECT_MV)
    {
        if (BOOL__G__HealthyActive == false)
        {
            BOOL__G__HealthyActive = true;
            UINT32_T__G__HealthySinceTick = uint32_t__nowTick;
        }
    }
    else
    {
        BOOL__G__HealthyActive = false;
        UINT32_T__G__HealthySinceTick = 0u;
    }
    /*
 * [EN] Rest peak: the highest pack voltage seen while the load was OFF
 * AND the mains was absent. That is the number a worn pack is judged
 * by - an empty pack rests low, a worn one rests high and still
 * collapses when loaded.
 * v1.90a (review 2026-10-23): the mains-present samples are excluded
 * on purpose. With the load cut and the mains back, the charger keeps
 * charging this pack, and its terminal voltage is the CHARGE voltage
 * (up to ~27 V), not a rest voltage. Counting it let a short mains
 * blip fake "rested at 25 V or more": the next mains loss then put a
 * discharged pack back on the load, and the collapse inside the guard
 * latched a false worn-battery verdict for a pack that was only
 * empty. Rest means no load and no charger - so only the no-mains
 * samples count.
 */
    if ((BOOL__G__ChangeoverProtectAsserted == true) &&
        (measurement_snapshot_t__snap->input_present == false))
    {
        if (measurement_snapshot_t__snap->v_bat24_mv > UINT32_T__G__RestPeakMv)
        {
            UINT32_T__G__RestPeakMv = measurement_snapshot_t__snap->v_bat24_mv;
        }
    }
    /*
 * [EN] Returning mains ends the mains-free bookkeeping, so the next
 * outage starts from a clean cycle guard. The scenario-8 verdict is
 * deliberately NOT cleared here: the user ordered the verdict to stay
 * until the pack has rested healthy for its own window.
 */
    if (measurement_snapshot_t__snap->input_present == true)
    {
        BOOL__G__MainsFreeReconnectDone = false;
        /*
 * [EN] v1.88: mains also pays the attempt budget back and un-parks the
 * battery path, so the next outage starts with its one fresh
 * mains-free attempt (v1.90: the budget is 1, not 3).
 */
        UINT8_T__G__MainsFreeFailCount = 0u;
        BOOL__G__MainsFreeParked = false;
    }
    /*
 * [EN] Snapshot is valid from here - use ONLY allowed fields.
 * Now handle fault: charger-only faults (FAULT_CHARGER_BAT_LOST /
 * FAULT_CHARGER_TECHNICAL) belong to the charger and must NOT disable
 * 24 V battery-to-load changeover or deep-discharge protection (PB11).
 * Only non-charger faults (such as FAULT_ADC) enter APP_STATE_FAULT
 * and freeze PB11.
 */
    if ((fault_mask_t__faults &
         (fault_mask_t)~(FAULT_CHARGER_BAT_LOST | FAULT_CHARGER_TECHNICAL)) != FAULT_NONE)
    {
        APP_STATE_T__G__State = APP_STATE_FAULT;
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }
#if MODULE_IMBALANCE
    /*
 * [EN] Scenario 5 output veto (user order 2026-10-04): while the
 * imbalance verdict is latched AND the block checkbox (param 117) is
 * on, the battery must NEVER be switched onto the output - behave
 * like the critical cut: assert the protect line, sit in SAFE, reset
 * the pending timers. Priority: right after hard faults (a latched
 * battery is a degraded asset, above every normal threshold logic).
 * Charging is NOT affected here (the charger has its own gate and
 * imbalance charging stays allowed until the cycle budget is spent).
 */
    {
        imbalance_outputs_t imbalance_outputs_t__imbalance;
        func__Imbalance_GetOutputs(&imbalance_outputs_t__imbalance);
        if (imbalance_outputs_t__imbalance.bool__blockOutput == true)
        {
            if (BOOL__G__ChangeoverProtectAsserted == false)
            {
                func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
                BOOL__G__ChangeoverProtectAsserted = true;
            }
            APP_STATE_T__G__State = APP_STATE_SAFE;
            BOOL__G__CutTimerActive = false;
            BOOL__G__ReconnectTimerActive = false;
            return APP_STATE_T__G__State;
        }
    }
#endif
#if MODULE_CHARGER
    /*
 * [EN] v1.72 scenario 6 output veto (user order: "declare the battery
 * faulty ... and charge it no more until the battery is replaced,
 * like the imbalance case"): with the dead-battery verdict latched
 * AND param 127 on, a condemned pack never reaches the output
 * either - same handling as the imbalance veto right above.
 */
    if (func__Charger_DeadBlocksOutput() != false)
    {
        if (BOOL__G__ChangeoverProtectAsserted == false)
        {
            func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
            BOOL__G__ChangeoverProtectAsserted = true;
        }
        APP_STATE_T__G__State = APP_STATE_SAFE;
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }
#endif
    /*
 * [EN] Guard: if Rtos tick conversion yields zero ticks (e.g., tick freq 0),
 * do not perform immediate cut/reconnect; preserve state and PB11, reset timers.
 */
    if ((uint32_t__durationTicks == 0u) || (uint32_t__healthyHoldTicks == 0u) ||
        (uint32_t__restTicks == 0u) || (uint32_t__guardTicks == 0u))
    {
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }
    /*
 * [EN] Scenario 8 - weak-battery verdict (user order 2026-10-23).
 * Clearing first: the verdict falls by itself when the scenario is
 * switched off, or after CHANGEOVER_PARAM_WEAK_REST_MS of a healthy,
 * unbroken pack voltage - that quiet rest is the single retry the
 * user asked for. Then, while the verdict still holds, the battery
 * path is held cut (id 157) so the weak pack is not loaded again.
 */
    if (func__Changeover_ParamValue(CHANGEOVER_PARAM_WEAK_ENABLE) == 0u)
    {
        BOOL__G__WeakVerdict = false;
    }
    else if ((BOOL__G__WeakVerdict == true) &&
             (BOOL__G__HealthyActive == true) &&
             ((uint32_t__nowTick - UINT32_T__G__HealthySinceTick) >= uint32_t__restTicks))
    {
        /*
 * [EN] The quiet rest IS the retry permission (user order): the verdict
 * falls AND the single mains-free attempt comes back, so a worn pack
 * is retried once per rest window - never every few seconds.
 * Turning the scenario off above deliberately does NOT do this,
 * otherwise the feature being off would mean endless attempts.
 */
        BOOL__G__WeakVerdict = false;
        UINT8_T__G__MainsFreeFailCount = 0u;
        BOOL__G__MainsFreeParked = false;
    }
    else
    {
    }
    if ((BOOL__G__WeakVerdict == true) &&
        (func__Changeover_ParamValue(CHANGEOVER_PARAM_WEAK_BLOCK_OUTPUT) != 0u) &&
        (measurement_snapshot_t__snap->input_present == false))
    {
        if (BOOL__G__ChangeoverProtectAsserted == false)
        {
            func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
            BOOL__G__ChangeoverProtectAsserted = true;
        }
        APP_STATE_T__G__State = APP_STATE_SAFE;
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }
    /*
 * [EN] v1.74: update the internal low-battery latch with hysteresis before
 * using it. Below 21000 mV it sets, at or above 21200 mV it clears, and in
 * between it holds its previous value.
 */
    if (measurement_snapshot_t__snap->v_bat24_mv < CHANGEOVER_BAT_LOW_ALARM_CUT_MV)
    {
        BOOL__G__LowBatteryLatched = true;
    }
    else if (measurement_snapshot_t__snap->v_bat24_mv >= CHANGEOVER_BAT_LOW_ALARM_CLEAR_MV)
    {
        BOOL__G__LowBatteryLatched = false;
    }
    else
    {
        /*
 * [EN] Hysteresis band 21000..21200 mV: keep the previous latch value.
 */
    }
    /*
 * [EN] Boot connection is distinct from recovery after a runtime
 * low-battery cut. Require a valid sample at the existing hysteresis
 * clear threshold; the reconnect input-stability delay is not needed
 */
    if (BOOL__G__BootPending == true)
    {
        if (measurement_snapshot_t__snap->v_bat24_mv >= CHANGEOVER_BAT_RECONNECT_MV)
        {
            func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, false);
            BOOL__G__ChangeoverProtectAsserted = false;
            BOOL__G__BootPending = false;
            APP_STATE_T__G__State =
                (measurement_snapshot_t__snap->input_present == true)
                    ? APP_STATE_INPUT
                    : APP_STATE_BATTERY;
        }
        else
        {
            APP_STATE_T__G__State = APP_STATE_SAFE;
        }
        return APP_STATE_T__G__State;
    }
    /*
 * [EN] Evaluate cut conditions (require continuous 3000ms):
 * - gated cut: the internal low-battery latch is set (closes below
 * 21000 mV, opens only at or above 21200 mV)
 * - independent cut: v <20800 independent of the latch
 * v1.81 (user order 2026-10-06, after the 2026-10-05 review): the
 * gated cut now tests the LATCH ALONE. Before, the second term
 * repeated the very comparison that had just set the latch, so the
 * 21000/21200 hysteresis shaped the latch and nothing else: a pack
 * that climbed to 21100 mV on second 2 of the 3 s window simply
 * froze the cut timer and kept feeding the load while still weak.
 * With the latch as the only term, the countdown keeps running
 * until the pack really passes CLEAR_MV (21200) and opens the
 * latch. The critical cut below CRITICAL_CUT_MV is untouched, the
 * 3 s filter is untouched, and reconnect is untouched.
 */
    bool__cutCondition = false;
    if (measurement_snapshot_t__snap->v_bat24_mv < CHANGEOVER_BAT_CRITICAL_CUT_MV)
    {
        bool__cutCondition = true;
    }
    else if (BOOL__G__LowBatteryLatched == true)
    {
        bool__cutCondition = true;
    }
    else
    {
        bool__cutCondition = false;
    }
    if (bool__cutCondition == true)
    {
        if (BOOL__G__CutTimerActive == false)
        {
            BOOL__G__CutTimerActive = true;
            UINT32_T__G__CutStartTick = uint32_t__nowTick;
        }
        else
        {
            uint32_t uint32_t__elapsedTicks;
            uint32_t__elapsedTicks = uint32_t__nowTick - UINT32_T__G__CutStartTick;
            if (uint32_t__elapsedTicks >= uint32_t__durationTicks)
            {
                if (BOOL__G__ChangeoverProtectAsserted == false)
                {
                    func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
                    BOOL__G__ChangeoverProtectAsserted = true;
                    /*
 * [EN] v1.88 (second user order 2026-10-23): ONE failed
 * attempt = a cut inside the guard window after a
 * mains-free reconnect. The attempt counter is what
 * stops an EMPTY pack from cycling the relay: after
 * CHANGEOVER_MAINS_FREE_RETRY_BUDGET failures with no
 * mains in between, the path stays parked until the
 * mains returns.
 * The verdict is a different question and needs one
 * more proof: the pack must have RESTED at or above
 * CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV before this
 * attempt. A pack that rests near empty only shows it
 * has no charge left - that is not a worn battery and
 * must not light the weak-battery face.
 */
                    if ((measurement_snapshot_t__snap->input_present == false) &&
                        (BOOL__G__MainsFreeReconnectDone == true) &&
                        ((uint32_t__nowTick - UINT32_T__G__MainsFreeReconnectTick) <
                         uint32_t__guardTicks))
                    {
                        if (UINT8_T__G__MainsFreeFailCount < 255u)
                        {
                            UINT8_T__G__MainsFreeFailCount++;
                        }
                        if (UINT8_T__G__MainsFreeFailCount >=
                            CHANGEOVER_MAINS_FREE_RETRY_BUDGET)
                        {
                            BOOL__G__MainsFreeParked = true;
                        }
                        if ((UINT32_T__G__LastRestPeakMv >=
                             func__Changeover_ParamValue(CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV)) &&
                            (func__Changeover_ParamValue(CHANGEOVER_PARAM_WEAK_ENABLE) != 0u) &&
                            (BOOL__G__WeakVerdict == false))
                        {
                            BOOL__G__WeakVerdict = true;
                            UINT32_T__G__WeakVerdictTick = uint32_t__nowTick;
                        }
                    }
                    /*
 * [EN] This rest peak belonged to the attempt that just
 * ended; the next one measures its own.
 */
                    UINT32_T__G__RestPeakMv = 0u;
                }
                APP_STATE_T__G__State = APP_STATE_SAFE;
            }
            else
            {
                /*
 * [EN] Cut pending but not yet elapsed: keep current state
 * as before cut (BATTERY/INPUT will be set below if not SAFE).
 */
                if (BOOL__G__ChangeoverProtectAsserted == false)
                {
                    if (measurement_snapshot_t__snap->input_present == true)
                    {
                        APP_STATE_T__G__State = APP_STATE_INPUT;
                    }
                    else
                    {
                        APP_STATE_T__G__State = APP_STATE_BATTERY;
                    }
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_SAFE;
                }
            }
        }
        /*
 * [EN] While cut condition holds, reconnect timer must not run.
 */
        BOOL__G__ReconnectTimerActive = false;
        /*
 * [EN] If cut timer just started (first tick), report current valid state
 * (BATTERY/INPUT) until duration completes.
 */
        if (BOOL__G__ChangeoverProtectAsserted == false)
        {
            if (APP_STATE_T__G__State == APP_STATE_BOOT)
            {
                if (measurement_snapshot_t__snap->input_present == true)
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_BATTERY;
                }
            }
            else if (APP_STATE_T__G__State == APP_STATE_SAFE)
            {
                /*
 * [EN] Keep SAFE if already cut.
 */
            }
            else
            {
                if (measurement_snapshot_t__snap->input_present == true)
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_BATTERY;
                }
            }
            /*
 * [EN] If protect already asserted, stay SAFE (already handled above).
 */
            if (BOOL__G__ChangeoverProtectAsserted == true)
            {
                APP_STATE_T__G__State = APP_STATE_SAFE;
            }
        }
        return APP_STATE_T__G__State;
    }
    else
    {
        BOOL__G__CutTimerActive = false;
    }
    /*
 * [EN] Evaluate reconnect: input_present true AND v >=21200 for 3000ms continuous
 * PLUS input stable for CHANGEOVER_INPUT_STABLE_MS (30s) after a cut.
 * CONFIRMED 2026-10-06 (user decision): requiring the input is
 * DELIBERATE. After a low-battery cut the pack voltage recovers on
 * its own once the load is gone; reconnecting without mains would
 * just put the load back on a weak pack and start a cut/reconnect
 * cycle. The battery stays parked until a real charging source is
 * present. Do not "fix" this. User order 2026-10-12: add 30s extra
 * stable-input delay before reconnect (time only, not voltage 24V).
 * REVISION (user order 2026-10-23): the pack that recovers after the
 * cut (or is replaced) may come back with NO mains, but only after the
 * healthy-voltage window above has held without a single break; a dip
 * restarts it, and one quick repeat cut latches the cycle guard. That
 * is the "several seconds in a stable state" test the 2026-10-06
 * decision was missing. Mains-present reconnect is unchanged, and the
 * 3 s voltage filter still runs after the gate opens.
 */
    bool__reconnectCondition = false;
    {
        bool bool__reconnectGateOpen = false;
        if (measurement_snapshot_t__snap->input_present == true)
        {
            if (BOOL__G__InputStableActive == true)
            {
                uint32_t uint32_t__inputStableElapsed;
                uint32_t__inputStableElapsed = uint32_t__nowTick - UINT32_T__G__InputStableSinceTick;
                if (uint32_t__inputStableElapsed >=
                    func__Rtos_MillisecondsToTicks(CHANGEOVER_INPUT_STABLE_MS))
                {
                    bool__reconnectGateOpen = true;
                }
                else
                {
                    bool__reconnectGateOpen = false;
                }
            }
            else
            {
                bool__reconnectGateOpen = false;
            }
        }
        else if ((BOOL__G__WeakVerdict == false) &&
                 (BOOL__G__MainsFreeParked == false) &&
                 (BOOL__G__HealthyActive == true) &&
                 (UINT32_T__G__RestPeakMv >=
                  func__Changeover_ParamValue(CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV)) &&
                 ((uint32_t__nowTick - UINT32_T__G__HealthySinceTick) >=
                  uint32_t__healthyHoldTicks))
        {
            /*
 * [EN] v1.90 (final user order 2026-10-23): no mains reconnect is
 * allowed unless the pack RESTED at or above the rest floor
 * (25.0 V by default). A pack that recovers only to 23 V after a
 * low-battery cut is DISCHARGED, not replaced - it must stay off
 * the load and wait for the charger, because putting it back
 * would only repeat the same cut.
 * Only a pack that rests at 25 V or more is put back on the
 * output, and if it then collapses again inside the cycle-guard
 * window the verdict says "worn battery".
 * The hold window below and the 3 s filter still apply.
 */
            bool__reconnectGateOpen = true;
        }
        if ((bool__reconnectGateOpen == true) &&
            (measurement_snapshot_t__snap->v_bat24_mv >= CHANGEOVER_BAT_RECONNECT_MV))
        {
            bool__reconnectCondition = true;
        }
        else
        {
            bool__reconnectCondition = false;
        }
    }
    if (bool__reconnectCondition == true)
    {
        if (BOOL__G__ReconnectTimerActive == false)
        {
            BOOL__G__ReconnectTimerActive = true;
            UINT32_T__G__ReconnectStartTick = uint32_t__nowTick;
            /*
 * [EN] While waiting for the reconnect duration, keep SAFE if
 * already cut, otherwise report the mapped state.
 */
            if (BOOL__G__ChangeoverProtectAsserted == true)
            {
                APP_STATE_T__G__State = APP_STATE_SAFE;
            }
            else
            {
                if (measurement_snapshot_t__snap->input_present == true)
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_BATTERY;
                }
            }
        }
        else
        {
            uint32_t uint32_t__elapsedTicks;
            uint32_t__elapsedTicks = uint32_t__nowTick - UINT32_T__G__ReconnectStartTick;
            if (uint32_t__elapsedTicks >= uint32_t__durationTicks)
            {
                if (BOOL__G__ChangeoverProtectAsserted == true)
                {
                    /*
 * [EN] Remember how high the pack rested before the load
 * came back; the verdict is judged against this.
 */
                    UINT32_T__G__LastRestPeakMv = UINT32_T__G__RestPeakMv;
                    UINT32_T__G__RestPeakMv = 0u;
                    func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, false);
                    BOOL__G__ChangeoverProtectAsserted = false;
                    /*
 * [EN] A reconnect with no mains is the one the cycle
 * guard watches: remember when it happened.
 */
                    if (measurement_snapshot_t__snap->input_present == false)
                    {
                        BOOL__G__MainsFreeReconnectDone = true;
                        UINT32_T__G__MainsFreeReconnectTick = uint32_t__nowTick;
                    }
                }
                if (measurement_snapshot_t__snap->input_present == true)
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_BATTERY;
                }
            }
            else
            {
                if (BOOL__G__ChangeoverProtectAsserted == true)
                {
                    APP_STATE_T__G__State = APP_STATE_SAFE;
                }
                else
                {
                    if (measurement_snapshot_t__snap->input_present == true)
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_BATTERY;
                }
                }
            }
        }
        return APP_STATE_T__G__State;
    }
    else
    {
        BOOL__G__ReconnectTimerActive = false;
    }
    /*
 * [EN] No fault, snapshot valid, no pending cut/reconnect:
 * Map state directly from input_present, but preserve SAFE until reconnect.
 */
    if (BOOL__G__ChangeoverProtectAsserted == true)
    {
        APP_STATE_T__G__State = APP_STATE_SAFE;
    }
    else
    {
        if (measurement_snapshot_t__snap->input_present == true)
        {
            APP_STATE_T__G__State = APP_STATE_INPUT;
        }
        else
        {
            APP_STATE_T__G__State = APP_STATE_BATTERY;
        }
    }
    return APP_STATE_T__G__State;
}
