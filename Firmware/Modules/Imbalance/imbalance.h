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
 * @note    [EN] Pure logic: Evaluate() receives every input and the current
 *              ms time; no BSP, no RTOS, no globals from other modules.
 *              Consume func__Imbalance_GetOutputs() after each call.
 */
#ifndef IMBALANCE_H
#define IMBALANCE_H
/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ==================== Parameter ids (108..118) ==================== */
/**
* [EN] Rest imbalance limit (mV, absolute difference), default 300 mV.
 */
#define IMBAL_PARAM_REST_LIMIT_MV            108u
/**
* [EN] Discharge imbalance limit (mV), default 500 mV.
 */
#define IMBAL_PARAM_DISCHARGE_LIMIT_MV       109u
/**
* [EN] Wait after a full charge session end before rest evaluation (ms),
 *  default 600000 (10 min), max 36000000 (10 h); 0 disables rest evaluation.
 */
#define IMBAL_PARAM_POST_CHARGE_WAIT_MS      110u
/**
* [EN] Wait after charge start before during-charge evaluation (ms),
 *  default 600000 (10 min), max 36000000 (10 h); 0 disables it.
 */
#define IMBAL_PARAM_IN_CHARGE_WAIT_MS        111u
/**
* [EN] Episode stability time (ms): imbalance must stay over the limit
 *  continuously for this long before the episode counts ONCE,
 *  default 30000 (30 s), max 36000000 (10 h).
 */
#define IMBAL_PARAM_STABILITY_MS             112u
/**
* [EN] Episode hysteresis (mV): an active episode ends only when the
 *  imbalance falls at or below (limit - hysteresis), default 100 mV.
 */
#define IMBAL_PARAM_HYSTERESIS_MV            113u
/**
* [EN] Episode budget: latched verdict after this many episodes,
 *  default 10.
 */
#define IMBAL_PARAM_MAX_EVENTS               114u
/**
* [EN] Beep period while latched (ms), default 3600000 (1 hour);
 *  0 disables the periodic beep.
 */
#define IMBAL_PARAM_LATCH_BEEP_PERIOD_MS     115u
/**
* [EN] Latched beep length (ms), default 200.
 */
#define IMBAL_PARAM_LATCH_BEEP_LEN_MS        116u
/**
* [EN] Block-output checkbox (0/1), default 1: while latched, the battery
 *  must never be switched onto the output (user bears the risk when 0).
 */
#define IMBAL_PARAM_BLOCK_OUTPUT_EN          117u
/**
* [EN] Charge cycles allowed after latch before charging is blocked too,
 *  default 20.
 */
#define IMBAL_PARAM_MAX_LATCHED_CYCLES       118u
/**
* [EN] v1.68 (user order 2026-10-05: "the red lamp must BLINK in the
 *  imbalance state"): cadence of the latched red lamp. Period in ms
 *  (default 1000) and ON share in percent (default 50) - the same
 *  period/duty shape every other scenario uses, so one pair of knobs
 *  describes the whole face. Period 0 is still accepted and means the old
 *  solid red, for anyone who wants it back without a rebuild.
 */
#define IMBAL_PARAM_LATCH_BLINK_PERIOD_MS    123u
#define IMBAL_PARAM_LATCH_BLINK_DUTY_PCT     124u
/**
* [EN] v1.81: independent per-pattern beep count and inter-beep gap for the
 *  latched imbalance alarm. Defaults preserve the existing one-beep shape.
 */
#define IMBAL_PARAM_LATCH_BEEP_COUNT         132u
#define IMBAL_PARAM_LATCH_BEEP_GAP_MS        133u
/**
* [EN] Number of complete FLOAT-qualified charge cycles without a new
 *  imbalance event before the event counter is cleared; default 3.
 */
#define IMBAL_PARAM_CLEAN_FULL_CYCLES        136u
/*
 * [EN] The module owns FOUR id ranges: 108..118, 123..124, 132..133 and
 *      136. Gaps belong to other modules.
 */
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
#define IMBAL_PARAM_OWNS(id) \
    (((((uint32_t)(id)) >= (uint32_t)IMBAL_PARAM_FIRST_ID) && \
      (((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID)) || \
     ((((uint32_t)(id)) >= (uint32_t)IMBAL_PARAM_FIRST_ID2) && \
      (((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID2)) || \
     ((((uint32_t)(id)) >= (uint32_t)IMBAL_PARAM_FIRST_ID3) && \
      (((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID3)) || \
     ((((uint32_t)(id)) >= (uint32_t)IMBAL_PARAM_FIRST_ID4) && \
      (((uint32_t)(id)) <= (uint32_t)IMBAL_PARAM_LAST_ID4)))
/**
* [EN] Table index of an owned id (undefined for ids the module does not own).
 */
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
/*
 * [EN] NVM-only runtime slots (ids 200..202): persisted through the same
 *      EspLink NVM machinery as parameters but never drawn on the panel and
 *      never included in a backup. Written by the module itself.
 */
#define IMBAL_SLOT_EVENTS_ID                 200u
#define IMBAL_SLOT_CYCLES_ID                 201u
#define IMBAL_SLOT_LATCH_ID                  202u
#define IMBAL_SLOT_FIRST_ID                  IMBAL_SLOT_EVENTS_ID
#define IMBAL_SLOT_LAST_ID                   IMBAL_SLOT_LATCH_ID
/* ==================== Defaults, bounds ==================== */
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
/*
 * [EN] Clamp windows (min..max per id, applied by Set): wide enough for a
 *      workshop, tight enough that a typo cannot create a dead monitor.
 */
#define IMBAL_MAX_LIMIT_MV                   2000u
#define IMBAL_MAX_WAIT_MS                    36000000u
#define IMBAL_MIN_STABILITY_MS               1000u     /* [EN] id 112, at least 1 s */
#define IMBAL_MAX_STABILITY_MS               36000000u
#define IMBAL_MAX_HYSTERESIS_MV              1000u     /* [EN] id 113 */
#define IMBAL_MAX_COUNT                      255u      /* [EN] ids 114/118 */
#define IMBAL_MAX_BEEP_PERIOD_MS             86400000u
#define IMBAL_MIN_BEEP_LEN_MS                20u       /* [EN] id 116 */
#define IMBAL_MAX_BEEP_LEN_MS                2000u
#define IMBAL_MAX_BEEP_COUNT                 10u
#define IMBAL_MAX_BEEP_GAP_MS                5000u
/*
 * [EN] id 123: 0 = solid red (opt-out), otherwise 100 ms..10 s so the lamp
 *      is always visibly a BLINK and never a flicker nobody can see.
 */
#define IMBAL_MIN_BLINK_PERIOD_MS            100u
#define IMBAL_MAX_BLINK_PERIOD_MS            10000u
/*
 * [EN] id 124: 5..95% - a 0% or 100% duty would silently be "lamp off" or
 *      "solid", which the period knob already expresses honestly.
 */
#define IMBAL_MIN_BLINK_DUTY_PCT             5u
#define IMBAL_MAX_BLINK_DUTY_PCT             95u
/*
 * [EN] Auto-reset qualifier: battery must stay absent continuously this
 *      long before the module believes a real battery swap happened.
 *      3000 ms > any measurement hiccup, < any human swap time... the point
 *      is the user physically removes the pack; by the time the new battery
 *      is connected, every counter and the latch are already zeroed.
 */
#define IMBAL_ABSENT_RESET_MS                3000u
/* ==================== Types ==================== */
/**
* [EN] Inputs for one evaluation pass. The caller maps board signals:
 *  - v_high_mv / v_low_mv from snapshot.v_bat_high_mv / v_bat_low_mv
 *  - input_present from snapshot.input_present
 *  - valid = snapshot.valid AND no ADC fault bit
 *  - bat_absent = Fault live removal signature (pump, healthy-input low
 *    half, or a fitted half below 3 V); not the shared latched fault bit
 *  - charging = func__Charger_IsAnyChannelActive()
 *  - on_battery = changeover state == APP_STATE_BATTERY (output on battery)
 */
typedef struct
{
    uint32_t uint32_t__vHighMv;
    uint32_t uint32_t__vLowMv;
    bool     bool__inputPresent;
    bool     bool__valid;
    bool     bool__batAbsent;
    bool     bool__charging;
    bool     bool__chargeComplete;
    /*
 * [EN] A charge cycle may qualify at most one imbalance event. Before
 * chargeComplete is true, a stable over-limit condition is only a
 * candidate; it is never added to the persisted event counter.
 */
    /*
 * [EN] v1.72 (user order): the two halves may only be compared when they
 * are in the SAME state. One half charging while the other rests lifts
 * the charging half by its own charge voltage, and that difference is
 * the charger talking, not an imbalance. The caller passes the state of
 * each channel; when they disagree the evaluation window stays shut.
 */
    bool     bool__chargingCh1;
    bool     bool__chargingCh2;
    bool     bool__onBattery;
    bool     bool__thermalRest;
} imbalance_inputs_t;
/**
* [EN] Outputs after one evaluation pass - everything consumers need.
 */
typedef struct
{
    uint32_t uint32_t__imbalanceMv;
    bool     bool__episode;
    uint32_t uint32_t__events;
    bool     bool__latched;
    uint32_t uint32_t__latchedCycles;
    bool     bool__blockOutput;
    bool     bool__chargingAllowed;
    bool     bool__beepDue;
    bool     bool__halvesMismatch;
} imbalance_outputs_t;
/* ==================== Functions ==================== */
/**
* [EN] Load macro defaults, clear timers/episode, counters/latch stay as
 *      the NVM replay left them (replay runs before or after Init; both
 *      orders are safe because Set writes both RAM copies).
 */
void func__Imbalance_Init(void);
/**
* [EN] One evaluation pass.
* @param imbalance_inputs_t__inputs Mapped inputs / (non-NULL)
* @param uint32_t__nowMs Current time in milliseconds / ms
* @param imbalance_outputs_t__outputs Results out / (non-NULL)
 * @return bool [EN] true if any persisted value changed (caller should mark
 */
bool func__Imbalance_Evaluate(const imbalance_inputs_t *imbalance_inputs_t__inputs,
                              uint32_t uint32_t__nowMs,
                              imbalance_outputs_t *imbalance_outputs_t__outputs);
/**
* [EN] Read the last outputs without evaluating (for EspLink TLM/UI).
 */
void func__Imbalance_GetOutputs(imbalance_outputs_t *imbalance_outputs_t__outputs);
/**
* [EN] Parameter set with clamp (ids 108..118, 123..124, 132..133, 136) and
 *      runtime-slot set (ids 200..202, NVM boot replay only). Returns applied value.
 *      The new beep-shape and clean-cycle values are normal persisted
 *      parameters, not packed into legacy words.
* @param uint32_t__appliedValue out, may be NULL /
* @return bool true when the id belongs to this module /
 */
bool func__Imbalance_SetParam(uint8_t uint8_t__paramId,
                              uint32_t uint32_t__value,
                              uint32_t *uint32_t__appliedValue);
/**
* [EN] Parameter / runtime-slot readback (NVM save snapshot, GET messages).
 */
bool func__Imbalance_GetParam(uint8_t uint8_t__paramId,
                              uint32_t *uint32_t__value);
#ifdef __cplusplus
}
#endif
#endif /* IMBALANCE_H */
