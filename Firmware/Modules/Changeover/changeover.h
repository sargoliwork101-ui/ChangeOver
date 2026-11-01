/**
 * @file    changeover.h
 * @brief   [EN] Input vs battery path state machine - battery protect via BSP logical API only.
 * @note    [EN] This module uses ONLY: snapshot.valid, snapshot.v_bat24_mv,
 *              snapshot.input_present, fault_mask, an internal low-battery latch
 *              and (MODULE_IMBALANCE) the func__Imbalance_GetOutputs() veto
 *              flag, plus (MODULE_CHARGER) func__Charger_DeadBlocksOutput()
 *              for the condemned-pack veto - both vetos assert the battery
 *              protect line and sit in SAFE. A recovered pack may reconnect
 *              with no mains after CHANGEOVER_PARAM_HEALTHY_HOLD_MS of
 *              unbroken healthy voltage, and owns the scenario-8 weak-battery
 *              verdict (ids 147..157) that guards it.
 *              Time conversion uses rtos_time.h only, tick=1ms
 *              assumption is forbidden.
 *              Only BSP_GPIO_PROTECT_BATTERY is allowed; PB5/PB7 are forbidden.
 */
#ifndef CHANGEOVER_H
#define CHANGEOVER_H
/* ==================== Includes ==================== */
#include "app_types.h"
/* ==================== Functions ==================== */
/**
 * @brief  [EN] Start protected in BOOT; wait for a valid, non-blocked sample
 *              at or above the existing 21200 mV reconnect threshold.
 */
void func__Changeover_Init(void);
/* ==================== Changeover_Evaluate Thresholds / Changeover ==================== */
/**
 * @brief  [EN] Battery voltage below which the internal low-battery latch sets, in millivolts.
 *         Range 0..40000 mV; effect: v_bat24_mv < 21000 sets the latch; with the latch set for 3000 ms -> battery cut.
 */
#define CHANGEOVER_BAT_LOW_ALARM_CUT_MV   21000u
/**
 * @brief  [EN] Battery voltage at or above which the internal low-battery latch clears, in millivolts.
 *         Range 0..40000 mV; effect: the latch set below 21000 mV stays set until v_bat24_mv >= 21200.
 *         v1.74: this window used to be UI link parameters 72/73; the user ordered it into
 *         Changeover as a fixed constant because this module is its only consumer and it must
 *         not be settable from the panel.
 */
#define CHANGEOVER_BAT_LOW_ALARM_CLEAR_MV 21200u
/**
 * @brief  [EN] Battery voltage below which the independent (critical) cut triggers without waiting for the latch, in millivolts.
 *         Range 0..40000 mV; effect: v_bat24_mv < 20800 for 3000 ms -> battery cut.
 */
#define CHANGEOVER_BAT_CRITICAL_CUT_MV    20800u
/**
 * @brief  [EN] Battery voltage at or above which a reconnect is allowed, in millivolts.
 *         Range 0..40000 mV; effect: (input_present true with 30 s stable input, or a
 *         healthy hold of CHANGEOVER_PARAM_HEALTHY_HOLD_MS with mains absent) and
 *         v_bat24_mv >= 21200 for 3000 ms -> reconnect.
 */
#define CHANGEOVER_BAT_RECONNECT_MV       21200u
/**
 * @brief  [EN] Continuous duration required for cut or reconnect, in milliseconds.
 *         Range 1..60000 ms; effect: condition must be continuously true for this duration.
 */
#define CHANGEOVER_DURATION_MS            3000u
/**
 * @brief  [EN] Extra stable input time before reconnect allowed, in milliseconds.
 *         After a cut, input_present must stay true for this long in addition
 *         to the 3s voltage condition. Prevents immediate re-connect on a
 *         flickering input.
 */
#define CHANGEOVER_INPUT_STABLE_MS        30000u
/*
 * [EN] The healthy-hold window and the cycle-guard window are no longer
 * compile-time constants: scenario 8 owns them as panel parameters
 * (ids 150 and 149, defaults 10000 ms and 60000 ms) so a bench can tune the
 * "several seconds in a stable state" test without a rebuild.
 */
/* ==================== Scenario 8: weak battery / : ==================== */
/*
 * [EN] Scenario 8 (user order 2026-10-23): "the weak pack that empties fast and
 *      comes back when the load is removed must be detectable". The Changeover
 *      module is the only place that sees the mains-free reconnect and the cut
 *      that follows it, so the verdict lives here and the UI only draws it.
 *      A cut that happens inside CHANGEOVER_PARAM_CYCLE_GUARD_MS after a
 *      mains-free reconnect proves the pack cannot carry the load: the verdict
 *      latches, the battery path is held cut (id 157), red blink + beep are
 *      shown, and one retry is allowed after a quiet rest (id 148) with the
 *      pack resting above the reconnect threshold.
 *      Every id is independent, exactly like scenarios 5/6/7 - the LED shape
 *      and the beep shape never borrow from another scenario.
 */
/**
* @brief [EN] 0/1, def 1: the scenario-8 verdict is enabled.
 */
#define CHANGEOVER_PARAM_WEAK_ENABLE          147u
/**
* @brief [EN] ms, def 600000 (10 min), 60000..3600000: quiet rest with a healthy pack before one retry.
 */
#define CHANGEOVER_PARAM_WEAK_REST_MS         148u
/**
* @brief [EN] ms, def 60000, 1000..600000: a re-cut inside this window after a mains-free reconnect proves weakness.
 */
#define CHANGEOVER_PARAM_CYCLE_GUARD_MS       149u
/**
* @brief [EN] ms, def 10000, 1000..300000: unbroken healthy voltage needed before a mains-free reconnect.
 */
#define CHANGEOVER_PARAM_HEALTHY_HOLD_MS      150u
/**
* @brief [EN] ms, def 500, 0 = solid red else 100..10000: red blink period of scenario 8.
 */
#define CHANGEOVER_PARAM_WEAK_LED_PERIOD_MS   151u
/**
* @brief [EN] %, def 50, 5..95: red LED on-fraction of scenario 8.
 */
#define CHANGEOVER_PARAM_WEAK_LED_DUTY_PCT    152u
/**
* @brief [EN] ms, def 3000, 0 = silent else 1000..600000: beep repeat period of scenario 8.
 */
#define CHANGEOVER_PARAM_WEAK_BEEP_PERIOD_MS  153u
/**
* @brief [EN] ms, def 200, 20..2000: length of one beep of scenario 8.
 */
#define CHANGEOVER_PARAM_WEAK_BEEP_LEN_MS     154u
/**
* @brief [EN] count, def 3, 0..10: beeps per pattern of scenario 8.
 */
#define CHANGEOVER_PARAM_WEAK_BEEP_COUNT      155u
/**
* @brief [EN] ms, def 100, 0..5000: gap between beeps of scenario 8.
 */
#define CHANGEOVER_PARAM_WEAK_BEEP_GAP_MS     156u
/**
* @brief [EN] 0/1, def 1: while the verdict holds, keep the battery path cut (no load on a weak pack).
 */
#define CHANGEOVER_PARAM_WEAK_BLOCK_OUTPUT    157u
/**
* @brief [EN] mV, def 25000, 21200..29000: the resting voltage a pack must hold before the
 *             board will put it back on the load with NO mains. Below it the pack is
 *             treated as DISCHARGED and stays off the load until the charger arrives -
 *             it is never reconnected on a guess. A pack that rests at or above it IS
 *             reconnected once, and if it then collapses inside the cycle-guard window
 *             the verdict says WORN BATTERY.
 */
#define CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV   165u
/*
 * [EN] Retry budget (user decision 2026-10-23, final wording): ONE mains-free
 *      reconnect. A cut inside the cycle-guard window after that single attempt
 *      ends the mains-free reconnecting until the mains returns. The user's
 *      reasoning: a worn pack is already identified by the rest-floor rule on
 *      the first attempt, and an empty pack gets one chance and no more - the
 *      load must not be pushed back on a pack that has just proved it cannot
 *      carry it. Two separate questions therefore: ONE attempt here, and the
 *      25 V rest floor decides whether that attempt was a worn battery or an
 *      empty one.
 */
#define CHANGEOVER_MAINS_FREE_RETRY_BUDGET    1u
/*
 * [EN] Runtime command slot (not a user parameter): write 0 here through the
 *      link to drop the scenario-8 verdict right away, exactly like the
 *      scenario-5/6 reset keys. It is never persisted and never drawn as a
 *      parameter.
 */
#define CHANGEOVER_SLOT_WEAK_RESET_ID         204u
/*
 * [EN] 0xFF marks "not a scenario-8 parameter" inside the slot mapping.
 */
#define CHANGEOVER_PARAM_INDEX_INVALID        0xFFu
#define CHANGEOVER_PARAM_FIRST_ID             CHANGEOVER_PARAM_WEAK_ENABLE
#define CHANGEOVER_PARAM_LAST_ID              CHANGEOVER_PARAM_WEAK_REST_FLOOR_MV
#define CHANGEOVER_PARAM_COUNT                12u
/* ==================== Scenario 8 defaults ==================== */
#define CHANGEOVER_DEF_WEAK_ENABLE            1u
#define CHANGEOVER_DEF_WEAK_REST_MS           600000u
#define CHANGEOVER_DEF_CYCLE_GUARD_MS         60000u
#define CHANGEOVER_DEF_HEALTHY_HOLD_MS        10000u
#define CHANGEOVER_DEF_WEAK_LED_PERIOD_MS     500u
#define CHANGEOVER_DEF_WEAK_LED_DUTY_PCT      50u
#define CHANGEOVER_DEF_WEAK_BEEP_PERIOD_MS    3000u
#define CHANGEOVER_DEF_WEAK_BEEP_LEN_MS       200u
#define CHANGEOVER_DEF_WEAK_BEEP_COUNT        3u
#define CHANGEOVER_DEF_WEAK_BEEP_GAP_MS       100u
#define CHANGEOVER_DEF_WEAK_BLOCK_OUTPUT      1u
#define CHANGEOVER_DEF_WEAK_REST_FLOOR_MV     25000u
/* ==================== Scenario 8 live parameters ==================== */
/**
 * @brief  [EN] Write one scenario-8 parameter (147..157) through the clamp table
 *              and report the applied value.
* @param uint8_t__paramId 147..157 /
* @param uint32_t__value Requested value /
* @param uint32_t__appliedValue Applied value out, NULL rejects / NULL
* @return bool true when the id belongs to this module /
 */
bool func__Changeover_SetParam(uint8_t uint8_t__paramId,
                               uint32_t uint32_t__value,
                               uint32_t *uint32_t__appliedValue);
/**
 * @brief  [EN] Read one live scenario-8 parameter (147..157).
* @param uint8_t__paramId 147..157 /
* @param uint32_t__value Value out /
* @return bool true when the id belongs to this module /
 */
bool func__Changeover_GetParam(uint8_t uint8_t__paramId,
                               uint32_t *uint32_t__value);
/**
 * @brief  [EN] Is the scenario-8 weak-battery verdict latched right now?
* @return bool true while latched / true
 */
bool func__Changeover_WeakVerdictActive(void);
/**
 * @brief  [EN] Is the battery path cut right now (low-battery protection,
 *              scenario-8 block, or the parked retry budget)?
* @return bool true while the path is cut /
 */
bool func__Changeover_PathBlocked(void);
/**
 * @brief  [EN] Is the mains-free reconnect parked because the retry budget is spent?
* @return bool true while parked / true
 */
bool func__Changeover_MainsFreeParked(void);
/**
 * @brief  [EN] Drop the scenario-8 verdict now (panel reset key, slot 204).
 *              The mains-free reconnect bookkeeping restarts too, so the
 *              single retry the user ordered becomes available again.
* @return bool true when a verdict was actually latched /
 */
bool func__Changeover_WeakVerdictReset(void);
/* ==================== Changeover_Evaluate / Changeover ==================== */
/**
 * @brief  [EN] Evaluate next system state from snapshot and faults and drive
 *              BSP_GPIO_PROTECT_BATTERY only. Uses rtos_time for 3000ms.
* @param measurement_snapshot_t__snap Snapshot from Measurement, may be NULL /
* @param fault_mask_t__faults Fault bits from Fault module /
* @return app_state_t Next system state /
 */
app_state_t func__Changeover_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap, fault_mask_t fault_mask_t__faults);
#endif /* CHANGEOVER_H */
