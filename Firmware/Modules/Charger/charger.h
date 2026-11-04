/**
 * @file    charger.h
 * @brief   [EN] Two independent 12 V flyback charger channels. CHG_MASTER_ENABLE
 *          is the single overall activation gate; when it is 0 the whole
 *          Charger must remain safe-off regardless of runtime flags. All
 *          numerical protections (voltage, current, JIT and missing-battery
 *          thresholds) remain enforced whenever control is allowed.
 */
#ifndef CHARGER_H
#define CHARGER_H
/* ==================== Includes ==================== */
#include "app_types.h"
#include <stdint.h>
/* ==================== Master enable ==================== */
/*
 * [EN] Single master switch for Charger control.
 *   0 = safe-off skeleton only: all PWM stopped, the transformer input
 *       relay keeps its NC contact closed (coil off, safe-idle), JIT and
 *       relay disconnect policy inactive. Runtime flags such as
 *       power_stage_enabled/pwm_max_duty are NOT hidden hard gates for
 *       Charger; numeric protections are never bypassed.
 *   1 = the per-channel control loop runs only when every explicit
 *       hardware condition is also satisfied (valid snapshot, installed
 *       channel, Vin ADC inside the shared q33/q34 window, valid battery sense, current
 *       limits, JIT policy).
 */
#define CHG_MASTER_ENABLE             1u
/* ==================== Board/test selection constants ==================== */
/*
 * [EN] The only two assembly-selection constants to change when the
 * installed transformer changes. Both are installed now:
 *   Channel 1 = 1 (user bring-up order 2026-09-20) -> PWM1 PA0 / Current1
 *               / JIT1 active (bench verification stays on the board
 *               checklist).
 *   Channel 2 = 1 -> PWM2 PA6 / Current2 PA7 / JIT2 PB6. Trans2 feeds an
 *               independent 12 V battery on VLOW = MID - GND; the 24 V
 *               pack measurement is monitor-only, never a charge setpoint
 *               or missing-battery condition for CH2.
 */
/*
 * [EN] Number of charger channels the module carries. Hardware fits two
 *      independent chains; the literal 2 used to be repeated in every loop
 *      bound and every bounds check, which is exactly the kind of duplicated
 *      constant this project forbids (clean-up 2026-10-06, no behaviour
 *      change).
 */
#define CHG_CHANNEL_COUNT             2u
#define CHG_CHANNEL_1_INSTALLED       1u
#define CHG_CHANNEL_2_INSTALLED       1u
#define CHG_CHANNEL_1_MASK            (1u << 0)
#define CHG_CHANNEL_2_MASK            (1u << 1)
#define CHG_INSTALLED_CHANNEL_MASK    \
    (((CHG_CHANNEL_1_INSTALLED != 0u) ? CHG_CHANNEL_1_MASK : 0u) | \
     ((CHG_CHANNEL_2_INSTALLED != 0u) ? CHG_CHANNEL_2_MASK : 0u))
/* ==================== Conservative bring-up gate ==================== */
/*
 * [EN] CHG_TRANSFORMER_KNOWN = 1: transformer data and the current-sense
 * chain are board-verified (gate/shunt waveforms at the 10% stage, R41/R42
 * divider compensated in the BSP, bench-calibrated zero offset, R77 confirmed 100k,
 * shunt reading physically consistent with a 23.5 V / 12.55 V energy audit).
 * Normal Bulk/Absorb/Float control below now runs. It is still a separate
 * compile-time safety gate from CHG_MASTER_ENABLE.
 */
#define CHG_TRANSFORMER_KNOWN         1u
/* ==================== Explicit limited bring-up test mode / bring-up ==================== */
/*
 * [EN] Explicit bring-up-only mode and the ONLY way to run switching when
 * CHG_TRANSFORMER_KNOWN=0: only Trans2 (CH2), NO real battery, external
 * source current-limited, waveform validation only - normal Bulk/Absorb/
 * Float control does NOT run here. Limits: only installed CH2 is allowed
 * (CH1 forced 0); start duty 1%, duty step as usual but clamped to
 * CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE - current stage max 10% duty with a
 * 100 mA external source limit; do not raise until the transformer data
 * is measured and CHG_TRANSFORMER_KNOWN flips to 1. Never enters Absorb/
 * Float, never uses a 24 V pack setpoint, still gated by
 * CHG_MASTER_ENABLE=1, Vin inside the shared q33/q34 window and all numeric protections.
 * (With CHG_BRINGUP_TEST_ENABLE=1: gate/shunt waveforms scope-verified at
 * the 10% stage with a calibrated current path.)
 */
#define CHG_BRINGUP_TEST_ENABLE                0u
#define CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE    100u
#define CHG_BRINGUP_TEST_SOURCE_LIMIT_MA      150u
/* ==================== Electrical policy ==================== */
#define CHG_PWM_FREQUENCY_HZ          50000u
#define CHG_PWM_TIMER_CLOCK_HZ        72000000u
#define CHG_PWM_PRESCALER             0u
#define CHG_PWM_AUTO_RELOAD           1439u
#define CHG_ABSORB_MV                 14400u
/*
 * [EN] Absorb as a voltage-hold window (user directive 2026-09-19): enter the
 *      window at CHG_ABSORB_ENTER_MV, hold the 14.4 V setpoint; the
 *      10-minute soak counts while the WHOLE ABSORB state lasts (his final
 *      directive) and a dip below 14.3 V returns to BULK and RESETS the soak.
 *      DUTY SHAPING IS THE PID, NOT A STEP TABLE (v1.23, 2026-09-29): the
 *      fine/coarse step chain was deleted, so there is no 0.1% or 0.5% step
 *      anywhere in the absorber. func__Charger_PidStep() updates once per
 *      CHG_PID_PERIOD_MS (100 ms) and moves duty by at most
 *      CHG_PID_MAX_STEP_PERMILLE (8 permille) per update, so an overshoot is
 *      walked back by the PID's own down-gain, not by a special coarse step.
 *      CHG_ABSORB_OVER_MV is therefore ONLY the factory default of profile
 *      parameter 22; the live value is read in func__Charger_ClampAlarms()
 *      as the floor of the over-voltage cut-off (ov >= over + 150 mV) and by
 *      nothing in the regulator. Still safely below
 *      FAULT_BAT_DISCONNECT_MV (14.8 V).
 */
#define CHG_ABSORB_ENTER_MV           14300u
#define CHG_ABSORB_OVER_MV            14600u
#define CHG_FLOAT_MV                  13500u
#define CHG_REENTRY_MV               12800u
/*
 * [EN] How long a channel must CONTINUOUSLY SEE battery voltage before
 *      a bulk start is allowed (user 2026-09-19: "10..20 s to settle, then
 *      start"; the count keys on BATTERY voltage only - input validity
 *      gates the bulk start separately). 15 s = middle of the window.
 *      Mid-cycle paths (JIT resume, 12.8 V reentry) are exempt (their
 *      presence stamp is already live). This gate also kills the bat-lost
 *      FLAP (clear -> BULK -> re-pump -> set again every 30 s, with the
 *      yellow blink): with the cable out the voltage is invalid, the
 *      stamp stays 0, BULK never re-arms.
 */
#define CHG_CONNECT_SETTLE_MS        15000u
#define CHG_BULK_CURRENT_MAX_MA       650u
/*
 * [EN] TEMPORARY bench diagnostic (2026-09-18): fixed 15% duty, NO
 *      ramp, NO band regulation - one stable operating point to calibrate
 *      the measurement coefficients (scope MEAN at LM358 out, bench V/I
 *      vs. the firmware readings). All usual gates stay (snapshot,
 *      Vin inside the shared q33/q34 window, battery sense, JIT, >950 mA hard fault) and
 *      switching stops while Vbat >= 14.4 V (no overcharge with regulation
 *      off). Set 0 to return to normal charge control.
 */
#define CHG_FIXED_DUTY_TEST_ENABLE              0u
#define CHG_FIXED_DUTY_TEST_DUTY_PERMILLE     150u
/*
 * [EN] Open-power-stage rule (scenario 7) needs a battery present: below this
 * pack voltage the channel is treated as unplugged, not as a dead stage. It equals
 * the Fault module's absent threshold (FAULT_BAT_ABSENT_MV, 6000 mV).
 */
#define CHG_TECH_OPEN_MIN_BATTERY_MV           6000u
/*
 * [EN] CHG_CURRENT_LIMIT_MA (675) was deleted on 2026-10-03: a grep of the
 *      whole firmware found zero readers, so it had been describing a limit
 *      that did not exist since the step chain was removed in v1.23. It was
 *      a candidate for the new panel-settable limit block until that grep -
 *      publishing it would have given the panel a control wired to nothing,
 *      which is worse than leaving it out.
 */
/*
 * [EN] The 630..650 mA hysteresis band that the deleted step chain used is
 *      gone with it (v1.23): a PID has no band, it has a setpoint
 *      (CHG_BULK_CURRENT_MAX_MA minus CHG_PID_CURRENT_MARGIN_MA = 640 mA)
 *      and it sits on it. CHG_BULK_CURRENT_MAX_MA now has a second job as
 *      the hard over-current BACKSTOP inside the PID (user order
 *      2026-09-29), while the hard-fault trip stays what it always was:
 *      the trip that cuts the channel and raises the fault. Its factory
 *      value is still 950 mA; only its settable ceiling moved.
 */
/*
 * [EN] ===== Settable-current envelope (USER-ORDERED LOGIC CHANGE
 *      2026-10-03: "why do the numbers have limits? I cannot raise the hard
 *      fault current - my hand has to be free; my batteries may be in
 *      parallel and I may want more current") =====
 *      The old 950 mA was NOT a hardware limit. It was a compile-time
 *      preference, written when a single 7 Ah battery per channel was the
 *      only case, and it silently capped the panel. Three separate things
 *      were being conflated, and only one of them is physics:
 *        1. MAGNETICS. Protected by CHG_DUTY_MAX_PERMILLE (50 %), which is
 *           a property of the board's transformer, not a taste. UNCHANGED
 *           and still not settable - raising the trip level cannot make the
 *           converter push more through the core than the duty cap allows.
 *        2. MEASURABILITY. A trip the ADC can never reach is not a
 *           protection, it is decoration. The chain saturates at 4095
 *           counts = 3594 mA chain current before bench gain (bsp_measurement.c
 *           ConvertCurrent: counts x 24200 / 27573). The battery-current
 *           table is on the counts axis and is read directly in mA, so the
 *           observable ceiling is that full-scale value. It must be
 *           re-read from the firmware after the first bench test.
 *        3. POLICY. Where the user wants the charger to shout. That is the
 *           user's call, not the firmware's, and it is now settable across
 *           the whole measurable range.
 *      CALIBRATED vs MEASURABLE - the honest caveat. The LUTs are fitted
 *      from bench data only up to chain 640 mA (ch1) and 707 mA (ch2),
 *      which at 14.4 V is 631 mA and 749 mA of battery current. Above the
 *      last anchor the gain and offset line extends the last slope, so readings there
 *      are extrapolated, not measured. Extrapolation is fine for a trip
 *      level - it only has to be monotonic to fire - but the panel marks
 *      where calibration ends so a current set above it is an informed
 *      choice rather than a hidden one.
 *      Parallel batteries, which is what prompted this, need more CURRENT
 *      at the SAME voltage. The voltage ceilings (OV cutoff, absorb, the
 *      15 V valid-range limit) are therefore deliberately NOT raised:
 *      paralleling does not change battery chemistry, and a 12 V lead-acid
 *      string damaged above 15 V is damaged whether there is one of them
 *      or four.
 */
/*
 * [EN] 4095 counts x 24200 / 27573 - the chain's full-scale current.
 */
/*
 * [EN] Full-scale chain current (4095 counts x 24200 / 27573, rounded).
 *      The table is read directly in mA on the counts axis, so no voltage
 *      division is involved. Re-check after the first bench test.
 */
#define CHG_CURRENT_MEASURABLE_MAX_MA  3594u
/*
 * [EN] The settable ceiling. Below the measurable maximum so every value
 *      the panel can ask for is one the firmware can actually observe.
 */
#define CHG_CURRENT_HARD_FAULT_MAX_MA  3000u
/*
 * [EN] Where the LUT fit stops and extrapolation begins, at 14.4 V. The
 *      panel draws this so the user can see the edge of calibrated data.
 */
#define CHG_CURRENT_CALIBRATED_MA      631u
/*
 * [EN] The old single name CHG_CURRENT_HARD_FAULT_MA is deliberately NOT
 *      kept as an alias. It was used for two different jobs - the power-on
 *      value of the trip AND the ceiling the panel is clamped to - which
 *      was harmless only while they were the same number. Opening the
 *      ceiling makes them different, and an alias would have silently
 *      shipped a 3000 mA factory default. Every use site now says which
 *      one it means.
 */
/*
 * [EN] Factory default stays where it was: opening a range must not move
 *      anyone's working setup. A board that is flashed and never touched
 *      behaves exactly as before.
 */
#define CHG_CURRENT_HARD_FAULT_DEFAULT_MA  950u
_Static_assert(CHG_CURRENT_HARD_FAULT_MAX_MA <= CHG_CURRENT_MEASURABLE_MAX_MA,
               "hard-fault ceiling above what the ADC chain can represent");
_Static_assert(CHG_CURRENT_HARD_FAULT_DEFAULT_MA <= CHG_CURRENT_HARD_FAULT_MAX_MA,
               "factory default above its own ceiling");
_Static_assert(CHG_CURRENT_CALIBRATED_MA < CHG_CURRENT_HARD_FAULT_DEFAULT_MA,
               "calibrated edge must sit below the default trip");
/*
 * [EN] Shared healthy-input Schmitt boot fallback. With MODULE_FAULT=1 the
 *      live shared permission (panel q33/q34 window + q169/q170 re-arm margins)
 *      is read through Fault_GetInputPermission; these macros mirror the same
 *      factory defaults for the Fault-off build only. Leaving the raw window
 *      still stops the charger immediately.
 */
#define CHG_INPUT_VALID_MV           21000u
#define CHG_INPUT_VALID_MAX_MV       28000u
#define CHG_INPUT_RECOVERY_HYSTERESIS_MV 500u
/*
 * [EN] v1.100 (user 2026-10-24): 10 healthy minutes with no hardware trip
 * heal the JIT strike budget; the FINAL_FAULT latch stays boot-only.
 */
#define CHG_JIT_TRIP_CLEAR_MS        600000u
#define CHG_THERMAL_CONTINUOUS_MS    3600000u
#define CHG_THERMAL_REST_MS           600000u
#define CHG_THERMAL_CURRENT_MARGIN_MA    50u
/*
 * [EN] Thermal-rest tuning, wire ids 166..168 (user order 2026-10-24: the
 *      current we go to rest at and the 1 h / 10 min pair must be adjustable
 *      from the panel). The three compile-time numbers above stay as the
 *      factory defaults, so a board that is flashed and never touched
 *      behaves exactly as before.
 */
/**
* [EN] Current threshold that starts the continuous-charge clock (mA).
 *      0 = automatic: bulk band minus CHG_THERMAL_CURRENT_MARGIN_MA (the
 *      original rule); any other value pins the threshold.
 */
#define CHG_THERMAL_PARAM_TRIGGER_MA       166u
/**
* [EN] Continuous charge time before the rest (ms, 0 = rest disabled),
 *      default CHG_THERMAL_CONTINUOUS_MS = 1 h.
 */
#define CHG_THERMAL_PARAM_CONTINUOUS_MS    167u
/**
* [EN] Rest length (ms, 0 = rest disabled), default CHG_THERMAL_REST_MS
 *      = 10 min.
 */
#define CHG_THERMAL_PARAM_REST_MS          168u
#define CHG_THERMAL_PARAM_FIRST_ID         CHG_THERMAL_PARAM_TRIGGER_MA
#define CHG_THERMAL_PARAM_LAST_ID          CHG_THERMAL_PARAM_REST_MS
#define CHG_THERMAL_PARAM_COUNT            3u
#define CHG_THERMAL_PARAM_INDEX(id) ((uint8_t)((id) - CHG_THERMAL_PARAM_FIRST_ID))
#define CHG_THERMAL_PARAM_OWNS(id)  (((id) >= CHG_THERMAL_PARAM_FIRST_ID) && \
                                     ((id) <= CHG_THERMAL_PARAM_LAST_ID))
#define CHG_THERMAL_YELLOW_PERIOD_MS   1000u
#define CHG_THERMAL_YELLOW_DUTY_PCT      50u
#define CHG_DUTY_START_PERMILLE        10u
#define CHG_DUTY_STEP_PERMILLE          5u
/*
 * [EN] Resume scale after a temporary invalid/blind ADC snapshot (self-heal):
 *      restart active BULK/ABSORB channels from 70% of their pre-pause duty
 *      (compensated down if Vin rose during the blind pause) instead of 1%.
 *      When a channel was in ABSORB, CHG_BLIND_ABSORB_GRACE_MS bounds the
 *      post-resume ramp window while duty climbs from 70% back to its
 *      pre-pause operating point so temporary below-knee current / relaxed
 *      terminal voltage neither wipes absorbAccumTicks nor falsely arms
 *      absorbMaxArmTick / taperSinceTick.
 */
#define CHG_BLIND_RESUME_PERCENT       70u
#define CHG_BLIND_ABSORB_GRACE_MS  180000u
/*
 * [EN] The only fixed-step cadence left after v1.23. The BULK/ABSORB step
 *      chain and its four other intervals were deleted with the legacy
 *      regulator (user order 2026-09-29: "remove the previous logic, the
 *      charger must be PID only"); what survives is the FLOAT park-down,
 *      which is not regulation - it walks the duty to zero at one 0.5%
 *      step per CHG_DUTY_RAMP_DOWN_INTERVAL_MS and leaves it there. The PID
 *      is explicitly invalidated while that runs, so the next reentry seeds
 *      from the real hardware duty rather than a stale integral.
 */
#define CHG_DUTY_RAMP_DOWN_INTERVAL_MS  500u
/*
 * ==================== Two-loop CC/CV PID regulator (v1.22) ====================
 * [EN] USER ORDER 2026-09-28: "slow the absorb duty rise down, and instead of
 *      all that complexity write a two-loop CC/CV PID - one gain set at the
 *      start, one in the middle, one in the last region - and expose it on
 *      the ESP panel." This block replaces the fixed-step bang-bang chain
 *      (0.5%/0.1% steps on 500/1000/2000 ms timers) with ONE positional PID
 *      core running a textbook CC/CV min-select:
 *        branch C (current) : error = current setpoint - measured current,
 *                             ALWAYS uses the stage-1 gain row.
 *        branch V (voltage) : error = absorb setpoint - measured voltage,
 *                             uses the stage-2 row while Vbat is BELOW the
 *                             setpoint and the stage-3 row once it is at or
 *                             above it.
 *      Both branches are evaluated every update and the one asking for the
 *      SMALLER duty wins; the winner alone drives the shared integrator and
 *      its own slew limits. That is why each loop needs its own gain row:
 *      a volt of voltage error and an amp of current error are different
 *      physical quantities, so one shared Kp would make the comparison
 *      meaningless and the CC->CV knee would never happen (simulated: the
 *      pack sails to 14.6 V before the voltage branch ever wins).
 *      So the three user-visible "stages" are, in charging order:
 *        stage 1 "start"  = the constant-current / bulk loop (branch C)
 *        stage 2 "middle" = the voltage loop climbing to the setpoint - the
 *                           region whose rise the user asked to slow down
 *        stage 3 "last"   = the voltage loop holding at / backing off from
 *                           the setpoint
 *      The state machine (BULK/ABSORB/FLOAT, soak, taper, dip, reentry) and
 *      EVERY protection (OV cutoff, hard current limit, JIT, battery-valid,
 *      duty ceiling, FLOAT parks at zero) are untouched: the PID only
 *      decides the duty number inside the window they already allow.
 *      Setting id 83 to 0 restores the legacy step regulator unchanged.
 */
/*
 * [EN] Update cadence. The control task still runs every 10 ms, but the PID
 *      math advances once per CHG_PID_PERIOD_MS - one window of the
 *      current filter (median-3 + average-10 = ~100 ms), so the loop never
 *      reacts to a value the filter has not finished forming.
 */
#define CHG_PID_PERIOD_MS               100u
#define CHG_PID_DT_MIN_MS                10u
#define CHG_PID_DT_MAX_MS              1000u
/*
 * [EN] Internal duty resolution: milli-permille (1 unit = 0.001 permille =
 *      0.0001%). The PWM stage takes whole permille, so the fine units are
 *      what let a 0.03 permille/s creep exist at all - the integrator moves
 *      inside the unit and the applied duty steps when it crosses.
 */
#define CHG_PID_DUTY_SCALE             1000u
/*
 * [EN] Gain units (integers only, panel-friendly, no floats in firmware).
 *      Each term has its OWN divider so every gain lands on a round, human
 *      number instead of a five-digit one:
 *        P term  [milli-permille]   = Kp x error / CHG_PID_KP_DIV
 *        I rate  [milli-permille/s] = Ki x error / CHG_PID_KI_DIV
 *        D term  [milli-permille]   = Kd x (error - previous) / CHG_PID_KD_DIV
 *      error is mV in the voltage branch and mA in the current branch, so
 *      with KP_DIV = 1 the readable unit of Kp is "permille of duty per
 *      volt of error" (voltage branch) or "permille per amp" (current
 *      branch), and with KI_DIV = 1000 the unit of Ki is "milli-permille
 *      per second per millivolt" - Ki = 1000 means one permille per second
 *      for every volt of error.
 *      Worked example (defaults, stage 2): 100 mV under the setpoint gives
 *      an integral rate of 300 x 100 / 1000 = 30 milli-permille/s =
 *      0.03 permille/s - SLOWER than the legacy 0.1 permille per 2000 ms
 *      (0.05 permille/s) the user asked to slow down, and it fades to zero
 *      as the error closes instead of stepping over the setpoint.
 */
#define CHG_PID_KP_DIV                    1u
#define CHG_PID_KI_DIV                 1000u
#define CHG_PID_KD_DIV                    1u
#define CHG_PID_GAIN_MAX              20000u
/*
 * [EN] Error saturation before any gain is applied (a 24 V pack read on a
 *      12 V channel, or a first-frame garbage current, must not slam the
 *      integrator). Both branches share it; mV and mA both fit.
 */
#define CHG_PID_ERROR_CLAMP            4000
/*
 * [EN] Slew limits in milli-permille per second. IMPORTANT: they cap the
 *      rate of the INTEGRAL (the operating point the loop is walking
 *      toward), not the finished output. Rate-limiting the finished output
 *      instead looks equivalent but is not: the P term ripples by a few
 *      milli-permille every time the applied duty quantises to the next
 *      whole permille, and an output limiter with a fast down-rate and a
 *      slow up-rate RECTIFIES that ripple into a steady downward ratchet -
 *      simulated, the loop stalled at 268 mA and never reached the 640 mA
 *      bulk band. Limiting the integral leaves the ripple zero-mean and the
 *      ramp rate exactly what the user dialled in.
 *      One up-rate and one down-rate per stage: the stage-2 up-rate is the
 *      user's "grow slower in absorb" knob, and the default down-rates of
 *      1000 = 1 permille/s match the legacy coarse escape rate (0.5
 *      permille per 500 ms) so backing off is never slower than before.
 */
#define CHG_PID_RATE_MIN                 10u
#define CHG_PID_RATE_MAX              20000u
/*
 * [EN] The current branch aims BELOW the profile ceiling by this margin, so
 *      the loop settles in the middle of the old 630..650 mA band instead
 *      of resting exactly on its top edge (a setpoint sitting on the limit
 *      would let every noise sample ask for a duty cut).
 */
#define CHG_PID_CURRENT_MARGIN_MA        10u
/*
 * [EN] Boot defaults, one row per stage (Kp, Ki, Kd, up-rate, down-rate).
 *      Tuned against a flyback + lead-acid plant model (DCM transfer
 *      i = 230.4 x D^2 / V, 0.15 ohm series resistance, an exponential
 *      gassing sink that puts the absorb operating point near 50 mA /
 *      56 permille) and re-checked over 0.05..0.30 ohm, a new pack and a
 *      worn pack, and +/-20 mV of sensor noise:
 *        stage 1 current 20 / 800 / 0 /  500 / 1000
 *              -> Kp small on purpose. The current branch sees about
 *                 7 mA of change per applied permille at the bulk operating
 *                 point, so a big Kp would swing the P term by more than a
 *                 whole permille on every quantisation step and the loop
 *                 would chatter instead of climb. 800 gives the familiar
 *                 0.5 permille/s soft ramp at a 640 mA error and fades out
 *                 as the band is reached. Result: 633..640 mA held flat,
 *                 no reversals at all during bulk.
 *        stage 2 voltage 150 / 300 / 0 /   30 / 1000
 *              -> 0.03 permille/s at 100 mV below the setpoint: the slowed
 *                 absorb rise the user asked for, proportional so it eases
 *                 off further as the setpoint is approached.
 *        stage 3 voltage 300 / 12000 / 0 / 10 / 1000
 *              -> the setpoint region needs real authority: the duty has to
 *                 fall from ~190 to ~56 permille as the pack stops taking
 *                 current. Ki does that work (the down-rate still caps it
 *                 at 1 permille/s) while Kp stays modest so sensor noise is
 *                 not amplified into duty jitter. Simulated peak overshoot
 *                 +19 mV (14419 mV), settling to 14400 +/- 1 mV at 55..56
 *                 permille - far below the 14.6 V over-voltage step and the
 *                 14.8 V fault threshold.
 *      Kd = 0 on purpose: the current/voltage chain is filtered but still
 *      noisy, and a derivative on noise is duty jitter. The panel can raise
 *      it if the bench ever wants damping.
 */
/*
 * [EN] FACTORY CALIBRATION - TWO loops, ten numbers (user orders
 *      2026-09-29: "calibrate it yourself from the tables for the first
 *      time, so I can optimise it later", then "if we do it with one PID
 *      over the whole path does it not work? it does not matter if it is
 *      slow, because the battery itself is slow; maximum accuracy by the
 *      SIMPLEST method").
 *      HOW SIMPLE CAN IT GET - measured, not argued (Firmware/Modules/Charger/Tester/pid_tuning_sim.py):
 *        1 shared row .... FAILS. A volt of voltage error and an amp of
 *                          current error are not the same quantity, so the
 *                          min-select compares millivolts with milliamps.
 *                          1408..84660 duty reversals per 10 h versus 4.
 *        1 voltage row + the 650 mA backstop as the only current limiter
 *                   ...... FAILS, and on the safety requirement itself: a
 *                          backstop is reactive, so current limit-cycles
 *                          and peaks at 707 mA instead of sitting on 640.
 *        2 rows ........... CORRECT. One PI for current, one PI for
 *                          voltage - textbook CC/CV, and the simplest
 *                          thing that actually regulates both.
 *        3 rows ........... no better. The extra at/above-setpoint row was
 *                          measured to buy nothing once the noise model
 *                          was broadband instead of a tone, so it is gone.
 *      THE TEN NUMBERS, each from a sweep against the charge-profile table
 *      (14.4 V absorb, 650 mA bulk, 500 permille DCM ceiling):
 *        Kp_I 12     flat optimum 8..15; above ~45 the 7 mA-per-permille
 *                    plant quantisation makes the loop chatter
 *        Ki_I 1600   reaches the 640 mA band in 545 s from a flat pack with
 *                    ZERO duty reversals; 800 needed 1373 s and held a
 *                    looser 617..640 mA; 3000 got there in 350 s but cost
 *                    44 reversals
 *        Kp_V 50     the voltage loop's NOISE GAIN, and the reason this is
 *                    not 150: the pack voltage is unfiltered upstream, so
 *                    Kp multiplies raw ADC noise straight onto the duty.
 *                    With broadband +/-7 mV noise, Kp=150 gives 744 duty
 *                    reversals per 10 h, Kp=50 gives 62 - with identical
 *                    overshoot, identical hold error (0.5 mV) and the same
 *                    113 min to reach 14.4 V on the clean plant
 *        Ki_V 18000  the setpoint region needs the authority to walk the
 *                    duty from ~190 down to ~56 permille as the pack stops
 *                    accepting current. Least overshoot of the sweep
 *                    (13.5 mV versus 18.7 at 12000 and 33.6 at 6000);
 *                    stability bound is ~37000
 *        up_V 10     THE "SLOW THE ABSORB RISE" KNOB of the earlier order,
 *                    now the only voltage up-rate, so it governs the whole
 *                    climb: 0.01 permille/s against the legacy chain's 0.05
 *        up_I 1000   the current loop may climb at 1 permille/s
 *        down 1000   both loops fall at 1 permille/s so a sagging pack is
 *                    followed promptly
 *        Kd   0      the derivative of a noisy sensor is duty jitter; the
 *                    panel can raise it if the bench ever wants damping
 */
#define CHG_PID_CURRENT_KP               12u
#define CHG_PID_CURRENT_KI             1600u
#define CHG_PID_CURRENT_KD                0u
#define CHG_PID_CURRENT_UP_RATE        1000u
#define CHG_PID_CURRENT_DOWN_RATE      1000u
#define CHG_PID_VOLTAGE_KP               50u
#define CHG_PID_VOLTAGE_KI            18000u
#define CHG_PID_VOLTAGE_KD                0u
#define CHG_PID_VOLTAGE_UP_RATE          10u
#define CHG_PID_VOLTAGE_DOWN_RATE      1000u
/*
 * [EN] Re-seed guard: whenever the duty actually applied to the hardware
 *      differs from the PID's own integral by more than this many permille,
 *      the PID re-seeds from the hardware value (bumpless transfer). That
 *      single rule covers every foreign writer - JIT retry halving, manual
 *      test mode, fixed-duty mode, a lowered panel duty ceiling, the BULK
 *      soft start - without any of them having to know the PID exists.
 */
#define CHG_PID_RESEED_TOLERANCE_PERMILLE 1u
/*
 * [EN] Output quantisation hysteresis, in milli-permille. The PWM stage
 *      takes WHOLE permille while the loop thinks in thousandths of one, so
 *      without this the applied duty toggles between two neighbouring
 *      integers every time the integral sits near a boundary - a 1 permille
 *      dither at up to 10 Hz. It is harmless electrically but it is
 *      exactly the "hunting" the user complained about, and it is what the
 *      duty readout shows. So the applied duty only moves once the demand
 *      has drifted at least this far from the value already on the
 *      hardware. Measured on the plant model over 10 h: 15570 duty
 *      direction changes without it, 4 with it (the legacy step chain
 *      managed 6456), with no measurable loss of regulation quality.
 *      MUST stay below CHG_PID_RESEED_TOLERANCE_PERMILLE x
 *      CHG_PID_DUTY_SCALE (asserted in charger.c): the deliberate lag it
 *      introduces must never look like a foreign writer and trigger a
 *      bumpless re-seed, or the two mechanisms would fight each other.
 */
#define CHG_PID_OUTPUT_HYST_MILLI       700u
/*
 * ==================== Hard backstops (user order 2026-09-29) ====================
 * [EN] "the 650 mA limit and the 14.8 V must be ACTIVE so the batteries are
 *      not damaged." These two limits are the last line of defence inside
 *      the PID and they are deliberately NOT tunable from the panel. The
 *      reason is the user's own next sentence: they intend to re-tune the
 *      coefficients later. A badly tuned gain set can overshoot; the
 *      backstops are what makes that experiment safe, so they must not be
 *      reachable from the same screen as the gains.
 *      They work as a SHRINKING DUTY CEILING, not as a trip:
 *          over-current -> ceiling = applied - GAIN_I x (I - limit_i)
 *          over-voltage -> ceiling = applied - GAIN_V x (V - limit_v)
 *      Proportional to the excess, so 1 mA over costs 0.1 permille and
 *      nothing visibly moves, while 50 mA over costs 5 permille and the
 *      duty is pulled down hard. That shape matters: a fixed step would
 *      re-introduce exactly the hunting this whole rewrite removed, and a
 *      latching trip would stop a healthy charge over one noisy sample.
 *      The integral is clamped to the same shrunk ceiling, so there is no
 *      windup to unwind once the excess clears.
 *      Current limit = the live profile value (id 25, 650 mA default), so
 *      lowering the charge current on the panel lowers the backstop with
 *      it. Voltage limit is the compile-time CHG_PID_BACKSTOP_MV (14.8 V),
 *      which is 400 mV above the absorb setpoint and 200 mV below the OV
 *      cutoff - it can only ever act on a real fault (a disconnected pack,
 *      a drifted sensor, or a future mis-tune), never during normal
 *      charging, where the measured peak is 14419 mV.
 *      These do NOT replace the existing protections. The 950 mA hard
 *      fault, the 15.0 V OV cutoff, the battery-valid floor and the JIT
 *      gating all still run: those cut the charge and raise alarms, these
 *      merely keep the regulator from ever taking the pack there.
 */
#define CHG_PID_BACKSTOP_MV           14800u
#define CHG_PID_BACKSTOP_GAIN_I         100u
#define CHG_PID_BACKSTOP_GAIN_V         500u
/*
 * [EN] Voltage prefilter for the PID input, as a leaky-integrator divisor:
 *      S += V - S/N, and the loop reads S/N. WHY IT EXISTS: the charge
 *      CURRENT is filtered upstream (median-3 + moving average in
 *      measurement.c) but the battery VOLTAGE is not, and one ADC step on
 *      the pack divider is about 7 mV, so the raw reading carries roughly
 *      +/-15 mV of noise. That noise lands directly on the voltage loop's
 *      P term - at Kp = 150 that is 2.25 permille of pure noise on the
 *      duty, far more than the output hysteresis can absorb, and the duty
 *      visibly hunts again. Measured over 4 h with +/-15 mV of sensor
 *      noise: 50913 duty direction changes unfiltered, 217 at N = 8, and
 *      62 at N = 32 (broadband noise, mean of four seeds). N = 32 at one
 *      update per 100 ms is a 3.2 s time constant - invisible against a battery whose own dynamics are
 *      measured in minutes, and it costs 1 mV of truncation bias.
 *      The HARD BACKSTOPS deliberately do NOT use this filtered value:
 *      protection reads the raw sample so a real over-voltage is never
 *      delayed by a filter.
 */
#define CHG_PID_VOLT_FILTER_N            32u
/*
 * [EN] Symmetric per-update cap on how far the APPLIED duty may move, in
 *      permille. This exists purely to bound a mis-tune, and it was added
 *      because the audit of the backstops proved they were not enough on
 *      their own: the integral is rate-limited but the P term is not (by
 *      design - rate-limiting P is what created the downward ratchet), so
 *      with every gain pushed to the panel maximum of 20000 the duty could
 *      jump from 190 to 500 permille inside ONE update and the pack saw
 *      4475 mA for 100 ms before the backstop could react. With this cap
 *      the same abuse peaks at 704 mA, below the 950 mA hard fault.
 *      SYMMETRIC is the whole point. The asymmetric output limiter tried
 *      earlier (slow up, fast down) rectified the P-term ripple into a
 *      downward ratchet and the loop stalled at 268 mA. A symmetric cap
 *      cannot rectify anything, and at 8 permille it sits 2.7x above the
 *      largest move the calibrated loop ever makes (3 permille, measured
 *      across flat/worn/new packs, 0.05..0.30 ohm and 40 mV of noise), so
 *      in normal charging it never engages at all - verified: identical
 *      reversal count, peak and time-to-setpoint with the cap present.
 *      It is applied BEFORE the hard ceiling so it can never slow a
 *      backstop down: protection always reaches the hardware in one pass.
 */
#define CHG_PID_MAX_STEP_PERMILLE         8u
#define CHG_DUTY_RETRY_SECOND_MAX       100u
/*
 * [EN] DCM ceiling: 50% max - anything higher risks core/MOSFET overlap and
 *      burns the MOSFET (board requirement). The regulation band settles near
 *      ~19%, so this cap is only an upper bound.
 */
#define CHG_DUTY_MAX_PERMILLE          500u
#define CHG_ABSORB_HOLD_MS          600000u
/*
 * [EN] Tail-current ("taper") absorb completion - the classic lead-acid
 *      criterion, user bench decisions 2026-09-20 (bench pack = 4.5 Ah, so
 *      CHG_TAPER_CURRENT_MA 50 is ~C/90): absorb ends into FLOAT only when
 *      the minimum soak (CHG_ABSORB_HOLD_MS) has passed AND the tail current
 *      stays below CHG_TAPER_CURRENT_MA steadily for CHG_TAPER_SUSTAIN_MS
 *      (60 s; the sense chain wobbles +/-10..20 mA, so a single dipping frame
 *      must not complete the charge). A never-tapering battery still leaves
 *      absorb at the CHG_ABSORB_MAX_MS = 1 hour ceiling (forced FLOAT), so
 *      the pump cannot stay awake forever.
 */
#define CHG_TAPER_CURRENT_MA           50u
#define CHG_TAPER_SUSTAIN_MS        60000u
/*
 * [EN] USER-ORDERED LOGIC CHANGE 2026-09-29. The one-hour absorb ceiling used
 *      to start the moment absorb was ENTERED, at 14.3 V, and it ended the
 *      charge an hour later whether the tail current had come down or not. On
 *      a pack that is still pulling hundreds of milliamps that cuts the charge
 *      off before the battery is full - it then sags past the 12.8 V reentry
 *      and starts all over again, which is the restart loop the user was
 *      seeing on the bench.
 *      The ceiling now ARMS on current, not on voltage: it starts counting the
 *      first time the tail falls below CHG_ABSORB_MAX_ARM_MA. Its job was
 *      never "limit absorb to an hour" - it was "once we are plainly in the
 *      tail, do not sit here forever" - and that is what it now does.
 *      Once armed it stays armed for the episode: re-arming on every wobble
 *      would let a noisy sense chain defeat the ceiling completely, and the
 *      chain is specified at +/-10..20 mA.
 */
#define CHG_ABSORB_MAX_ARM_MA          100u
#define CHG_ABSORB_MAX_MS         3600000u
/*
 * [EN] Battery-lost (both cases: pumped >14.8 V while charging, and battery
 *      absent with valid input) is OWNED BY THE FAULT MODULE since 2026-09-19
 *      per user directive: fault.c evaluates the snapshot centrally and
 *      latches/clears FAULT_CHARGER_BAT_LOST. The charger only MIRRORS the
 *      bit into CHG_STATE_BAT_LOST (stop PWM now) and releases the channel
 *      to OFF when the bit clears, so a soft BULK restart at 1% duty follows.
 *      Thresholds/timers: FAULT_BAT_* in fault.h, not here.
 */
#define CHG_JIT_LOCKOUT_MS            3000u
/*
 * [EN] Manual test mode link dead-man (user order 2026-09-23): while the
 *      mode is on, the STM expects at least one valid ESP frame every
 *      3 s; on expiry both duties drop to 0 and the autonomous charger
 *      resumes. A crashed browser must never leave a battery on an
 *      unregulated fixed duty.
 */
#define CHG_MANUAL_WATCHDOG_MS        3000u
/*
 * [EN] 2000 mV is battery-sense validity for the same channel, NOT a charge
 * setpoint. For Trans2 it is VLOW = MID - GND. A free resistor alone is not
 * a valid battery simulator; only an electronic load with voltage clamp or a
 * battery simulator is allowed for no-battery tests.
 */
#define CHG_MIN_VALID_BATTERY_MV      2000u
/*
 * [EN] Hard upper battery-sense cutoff for every control path. A 12 V VRLA
 *      battery must not be connected to a charger that is already above this
 *      conservative 15.0 V limit. This is a firmware cutoff, not a substitute
 *      for a fuse, current-limited source, or a battery manufacturer's limits.
 */
/*
 * [EN] DECIDE EARLIER, DO NOT BEND THE SCALE (user order 2026-09-29).
 *      The pack divider used to be falsified (TOP 66200 instead of the real
 *      68K+1.2K) partly so that the over-voltage cut would trip ~100 mV
 *      early. That bought a little safety margin at the cost of every
 *      voltage the product reports. The divider is honest again, so the
 *      margin is taken where it belongs - at the DECISION.
 *      CHG_MAX_VALID_BATTERY_MV stays the absolute ceiling a half-pack may
 *      ever read; CHG_OV_DECIDE_EARLY_MV is how far BELOW it the default
 *      cut-off sits, so the charger stops before the ceiling rather than at
 *      it. Raise the early margin to act sooner; never re-scale a reading to
 *      fake it.
 */
#define CHG_MAX_VALID_BATTERY_MV     15000u
#define CHG_OV_DECIDE_EARLY_MV         150u
#define CHG_OV_CUTOFF_DEFAULT_MV \
    (CHG_MAX_VALID_BATTERY_MV - CHG_OV_DECIDE_EARLY_MV)
/* ==================== Charger diag array ==================== */
/*
 * [EN] Live-diagnostics array (user order 2026-09-22): every value the charge
 *      decisions run on, refreshed at the top of EVERY Charger_Evaluate pass
 *      (control period), so it can be watched in one Live Expressions entry.
 *      Layout (5 slots per channel, then shared):
 *        [0..4]  ch0 = VHIGH half: state, duty permille, vbat mV,
 *                primary mA, output-estimate mA (the regulated value)
 *        [5..9]  ch1 = VLOW half: same five
 *        [10] v_in_mv, [11] v_bat24_mv, [12] v_bat12_mv,
 *        [13] v_bat_high_mv (derived = V24 - V12), [14] fault mask,
 *        [15] snapshot valid flag (0/1; 0 => current/voltage slots are 0)
 *      State codes: 0=OFF 1=BULK 2=ABSORB 3=FLOAT 4=BRINGUP
 *                   5=JIT_RETRY_WAIT 6=INPUT_WAIT 7=FINAL_FAULT 8=BAT_LOST
 *                   5=JIT_RETRY_WAIT 6=INPUT_WAIT 7=FINAL_FAULT 8=BAT_LOST
 */
#define CHG_DIAG_COUNT                 16u
#define CHG_DIAG_CHANNEL_STRIDE         5u
#define CHG_DIAG_IDX_STATE              0u
#define CHG_DIAG_IDX_DUTY               1u
#define CHG_DIAG_IDX_VBAT               2u
#define CHG_DIAG_IDX_IPRI               3u
#define CHG_DIAG_IDX_IEST               4u
#define CHG_DIAG_IDX_VIN               10u
#define CHG_DIAG_IDX_V24               11u
#define CHG_DIAG_IDX_V12               12u
#define CHG_DIAG_IDX_VHIGH             13u
#define CHG_DIAG_IDX_FAULT             14u
#define CHG_DIAG_IDX_VALID             15u
extern volatile uint32_t UINT32_T__G__ChargerDiag[CHG_DIAG_COUNT];
/* ==================== Charger calib capture ==================== */
/*
 * [EN] Calibration worksheet array (user order 2026-09-22): ONE variable to
 *      read during bench calibration, ordered exactly as the calibration
 *      math consumes it; the user records the real-world counterpart values
 *      (multimeter) at the same moment, with duty held steady.
 *        [0]  snapshot valid (0/1)
 *        [1]  v_in_mv (fw)      [2]  v_bat24_mv (fw)
 *        [3]  v_bat12_mv (fw)   [4]  v_bat_high_mv (fw, derived)
 *        [5..9]   channel UP   (upper battery): duty, state, vbat mV,
 *                                     Ipri mA, Iout_est mA
 *        [10..14] channel DOWN (lower battery): same five
 *      State codes as CHG_DIAG above.
 */
#define CHG_CALIB_COUNT                    15u
#define CHG_CALIB_IDX_VALID                 0u
#define CHG_CALIB_IDX_VIN                   1u
#define CHG_CALIB_IDX_V24                   2u
#define CHG_CALIB_IDX_V12                   3u
#define CHG_CALIB_IDX_VHIGH                 4u
#define CHG_CALIB_CH_UP_BASE                5u
#define CHG_CALIB_CH_STRIDE                 5u
#define CHG_CALIB_OFF_DUTY                  0u
#define CHG_CALIB_OFF_STATE                 1u
#define CHG_CALIB_OFF_VBAT                  2u
#define CHG_CALIB_OFF_IPRI                  3u
#define CHG_CALIB_OFF_IEST                  4u
extern volatile uint32_t UINT32_T__G__ChargerCalib[CHG_CALIB_COUNT];
/* ==================== Charger live estimate currents ==================== */
/*
 * [EN] The two OUTPUT-current estimates the charger actually decides with
 *      (user order 2026-09-22: watch them directly in Live Expressions).
 *      ch1 = VHIGH half (upper battery), ch2 = VLOW half (lower battery);
 *      refreshed every control pass together with the diag/calib arrays.
 *      The PID drives this value onto its setpoint of
 *      CHG_BULK_CURRENT_MAX_MA - CHG_PID_CURRENT_MARGIN_MA (640 mA).
 */
extern volatile uint32_t UINT32_T__G__ChargerIest1Ma;
extern volatile uint32_t UINT32_T__G__ChargerIest2Ma;
/* ==================== Runtime config API (ESP panel) / API ==================== */
/**
 * @brief  [EN] Set the ESP enable gate of one charger channel: false = PWM
 *              off + state OFF (FINAL_FAULT never released by this gate),
 *              true = soft BULK restart; flash-persisted since v1.14
 *              (user order 2026-09-22).
* @param uint8_t__channelIndex 0 = charger 1, 1 = charger 2 /
* @param bool__enable true = channel allowed /
 */
void func__Charger_SetChannelEspEnable(uint8_t uint8_t__channelIndex, bool bool__enable);
/**
 * @brief  [EN] Read the ESP enable gate of one charger channel.
* @param uint8_t__channelIndex 0 = charger 1, 1 = charger 2 /
* @return bool true = channel allowed /
 */
bool func__Charger_GetChannelEspEnable(uint8_t uint8_t__channelIndex);
/**
 * @brief  [EN] Set the runtime PWM duty ceiling of one channel, clamped to
 *              0..CHG_DUTY_MAX_PERMILLE; every applied duty respects it.
 *              Flash-persisted since v1.14, ESP panel (user order 2026-09-22).
* @param uint8_t__channelIndex 0 = charger 1, 1 = charger 2 /
* @param uint32_t__ceilingPermille Requested ceiling /
* @return uint32_t Applied ceiling /
 */
uint32_t func__Charger_SetDutyCeilingPermille(uint8_t uint8_t__channelIndex,
                                              uint32_t uint32_t__ceilingPermille);
/**
 * @brief  [EN] Read the live PWM duty ceiling of one channel.
* @param uint8_t__channelIndex 0 = charger 1, 1 = charger 2 /
* @return uint32_t Ceiling permille /
 */
uint32_t func__Charger_GetDutyCeilingPermille(uint8_t uint8_t__channelIndex);
/**
 * @brief  [EN] Set the runtime fixed-duty mode of one channel: hold the PWM
 *              at one chosen number instead of the regulation loop, with
 *              the bench-test safety wrapper (switching stops above
 *              CHG_ABSORB_MV; JIT/input/battery/ESP cuts stay active). RAM
 *              only (user order 2026-09-22).
* @param uint8_t__channelIndex 0 = charger 1, 1 = charger 2 /
* @param bool__enable true = fixed mode on /
 */
void func__Charger_SetDutyFixedEnable(uint8_t uint8_t__channelIndex, bool bool__enable);
/**
 * @brief  [EN] Read the runtime fixed-duty switch of one channel.
* @param uint8_t__channelIndex 0 = charger 1, 1 = charger 2 /
* @return bool true = fixed mode on /
 */
bool func__Charger_GetDutyFixedEnable(uint8_t uint8_t__channelIndex);
/**
 * @brief  [EN] Set the fixed duty value of one channel, clamped to
 *              0..CHG_DUTY_MAX_PERMILLE; effective only while fixed mode is
 *              enabled, and additionally clamped to the runtime ceiling on
 *              apply. Flash-persisted since v1.14 (user order 2026-09-22).
* @param uint8_t__channelIndex 0 = charger 1, 1 = charger 2 /
* @param uint32_t__dutyPermille Requested duty / duty
* @return uint32_t Applied stored value /
 */
uint32_t func__Charger_SetDutyFixedPermille(uint8_t uint8_t__channelIndex,
                                            uint32_t uint32_t__dutyPermille);
/**
 * @brief  [EN] Read the stored fixed duty value of one channel.
* @param uint8_t__channelIndex 0 = charger 1, 1 = charger 2 /
* @return uint32_t Duty permille / duty
 */
uint32_t func__Charger_GetDutyFixedPermille(uint8_t uint8_t__channelIndex);
/* ==================== Manual test mode ==================== */
/*
 * [EN] Global Calibration Data/test switch (user order 2026-09-23, protocol v1.2
 *      param ID 19). While active the automatic charger is suspended and
 *      each channel is driven directly at its stored fixed-duty value
 *      (SetDutyFixedPermille) with every battery condition bypassed; the
 *      hardware floor stays: input presence, the JIT trip, the 15.0 V
 *      overvoltage cutoff, the duty ceilings and the FINAL_FAULT latch.
 *      See ESP_AGENT_SPEC.md section 19 for the full contract.
 */
/**
 * @brief  [EN] Request manual test mode on/off (called by the ESP link
 *              task; the charger task adopts the request on its next pass
 *              and performs the enter/exit actions in its own context).
* @param bool__enable true = manual on /
 */
void func__Charger_SetManualTestMode(bool bool__enable);
/**
 * @brief  [EN] Read the REQUESTED manual mode flag (the parameter value
 *              the panel wrote; equals IsManualTestModeActive within one
 *              control period).
* @return bool true = requested on /
 */
bool func__Charger_GetManualTestMode(void);
/**
 * @brief  [EN] Read the ACTIVE manual mode flag (owned by the charger
 *              task; used by telemetry flag b5 and by the Fault module to
 *              freeze battery-lost detection).
* @return bool true = manual active /
 */
bool func__Charger_IsManualTestModeActive(void);
/**
 * @brief  [EN] Feed the manual-mode link dead-man: every VALID frame from
 *              the ESP (SET_PARAM / GET_PARAMS) refreshes the stamp; 3 s
 *              of silence exits manual mode and drops both duties.
 */
void func__Charger_NotifyEspLinkActivity(void);
/* ==================== Charger NVM-save suspension / NVM ==================== */
/**
 * @brief  [EN] Suspend / resume switching for an NVM flash save (user order
 *              2026-09-27: idle the charger, save, restart it). While
 *              suspended, Charger_Evaluate holds both gates at 0 and skips
 *              the pass; state machines, soak accumulators and the 15 s
 *              settle gate are untouched, so the resume continues
 *              seamlessly. Fault and Changeover keep evaluating (separate
 *              calls in the control task). Single volatile flag, no lock
 *              needed. Pre-kernel safe (plain bool store/load).
* @param bool__suspended true = hold gates at 0 /
 */
void func__Charger_SetSuspended(bool bool__suspended);
/**
 * @brief  [EN] Read the NVM-save suspension flag.
* @return bool true = suspension active /
 */
bool func__Charger_IsSuspended(void);
/* ==================== Charger_Init ==================== */
/**
 * @brief  [EN] Initialize policy state, stop every PWM channel and force a
 *              deterministic safe-idle state. With CHG_MASTER_ENABLE=0 this
 *              remains the only active behavior.
 */
void func__Charger_Init(void);
/* ==================== Charger_Evaluate ==================== */
/**
 * @brief  [EN] Run the same control/protection algorithm independently for
 *              every installed 12 V channel. Input voltage is checked from
 *              real ADC mV, not only the PB4 digital signal. A low current is
 *              a regulation request to increase duty, not a fault.
* @param measurement_snapshot_t__snap Independent low/high battery snapshot /
* @param app_state_t__state System state /
 */
void func__Charger_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap,
                            app_state_t app_state_t__state);
/**
 * @brief  [EN] True while at least one installed channel is actually
 *         charging - its state machine sits in BULK or ABSORB (user
 *         directive 2026-09-19: a FLOAT channel is parked at zero duty, the
 *         charge is DONE, not active - v1.17b: the UI full face keys on
 *         IsChargeComplete, and the fault pump-window is not armed there
 *         either).
* @return bool true if any channel is charging / true
 */
bool func__Charger_IsAnyChannelActive(void);
/**
 * @brief  [EN] True while any channel is in thermal rest (1h max -> 10min).
 *         Used by UI to show yellow blink during rest.
* @return bool true if any channel in THERMAL_REST / true
 */
bool func__Charger_IsAnyChannelInThermalRest(void);
/**
 * @brief  [EN] True while ONE given channel is in thermal rest.
 * @param  uint8_t__channelIndex [EN] 0 or 1
 * @return bool [EN] true while that channel in THERMAL_REST
 */
bool func__Charger_IsThermalRestActive(uint8_t uint8_t__channelIndex);
/**
 * @brief  [EN] True while ONE given channel (0 = charger 1 / upper battery,
 *         1 = charger 2 / lower battery) is actually pumping: installed and
 *         in BULK or ABSORB - the per-channel split of the predicate above
 *         (v1.21). The Fault module arms its 14.8 V pump-rule PER HALF with
 *         it: a parked channel has no pump, so its half cannot fly up and
 *         must not be judged (kills the false 3-beep cycle during charge,
 *         user order 2026-09-28).
* @param uint8_t__channelIndex 0 = charger 1, 1 = charger 2 /
* @return bool true while that channel pumps /
 */
bool func__Charger_IsChannelActive(uint8_t uint8_t__channelIndex);
/**
 * @brief [EN] Read the PWM duty actually applied to one charger channel,
 *        in permille. This is deliberately not the requested PID duty: the
 *        technical-fault detector must judge the hardware-facing output.
 */
uint32_t func__Charger_GetAppliedDutyPermille(uint8_t uint8_t__channelIndex);
/**
 * @brief [EN] True when the shared charger relay contact is open.
 */
bool func__Charger_IsRelayOpen(void);
/**
 * @brief [EN] Evaluate scenario-7 charger power-stage faults from the fresh
 *        ADC snapshot and the already-run JIT latch. On a match this sets
 *        FAULT_CHARGER_TECHNICAL and permanently blocks charger output until
 *        the next charger init/reset.
 */
void func__Charger_EvaluateTechnicalFault(
    const measurement_snapshot_t *measurement_snapshot_t__snap);
/**
 * @brief  [EN] True while scenario-7 output lockout is held.
 */
bool func__Charger_IsTechnicalFaultLocked(void);
/**
 * @brief  [EN] True when every relevant channel finished its charge: at
 *         least one installed+enabled channel exists and ALL of them sit
 *         in FLOAT. FLOAT is entered from one place only (ABSORB done:
 *         soak + taper, or the 1 h safety ceiling), so FLOAT means DONE;
 *         any restart (reentry BULK, fresh OFF->BULK, disable) leaves it.
 *         v1.17b (user order 2026-09-27: "after a full charge the blinking
 *         must be gone"): the UI latches its full face on this - not on
 *         the unreachable voltage 100 (pack 29 V tops the 28.8 V absorb).
 * @return bool [EN] true if the charge is complete on all relevant channels /
 */
bool func__Charger_IsChargeComplete(void);
/*
 * [EN] Charge-profile wire ids (MUST equal ESPLINK_PARAM_CHG_PROFILE_* in
 *      esp_link.h; the host test enforces the match).
 */
#define CHG_PROFILE_PARAM_ABSORB_MV           20u  /* [EN] mV, 11000..14600 / mV */
#define CHG_PROFILE_PARAM_ABSORB_ENTER_MV     21u  /* [EN] mV, absorb-500..absorb-50 / mV */
/*
 * [EN] Profile id 22 is the FLOOR of the over-voltage cut-off (the cut-off
 *      is always at least 150 mV above it) and it drives NO duty step any
 *      more - the PID shapes duty since v1.23.
 */
#define CHG_PROFILE_PARAM_ABSORB_OVER_MV      22u  /* [EN] mV, absorb+100..min(absorb+400,14750) / mV */
#define CHG_PROFILE_PARAM_FLOAT_MV            23u  /* [EN] mV, 9000..absorb-300 / mV */
#define CHG_PROFILE_PARAM_REENTRY_MV          24u  /* [EN] mV, 8000..float-300 / mV */
#define CHG_PROFILE_PARAM_BULK_CURRENT_MAX_MA 25u
#define CHG_PROFILE_PARAM_TAPER_CURRENT_MA    26u  /* [EN] mA, 10..min(300,imax) / mA */
/*
 * ==================== Charge Profile (user order 2026-09-25) ====================
 * [EN] Runtime-settable charge profile, shared by BOTH channels, written from
 *      ESP_AGENT_SPEC.md). Boot defaults equal the old compile-time setpoints
 *      (CHG_*_MV / CHG_*_MA below); values live in RAM and reset at boot,
 *      like every other parameter. v1.15: the hard safety stack
 *      (the hard-fault trip, CHG_MAX_VALID_BATTERY_MV, the 15.0 V
 *      overvoltage cutoff) is runtime-LOWERABLE from the alarms tab (ids
 *      35..37) but can NEVER be raised above the compile maxima.
 */
/**
 * @brief  [EN] Write one charge-profile parameter (ESP link, ids 20..26:
 *              20=ABSORB_MV, 21=ABSORB_ENTER_MV, 22=ABSORB_OVER_MV,
 *              23=FLOAT_MV, 24=REENTRY_MV, 25=BULK_CURRENT_MAX_MA,
 *              26=TAPER_CURRENT_MA). The value is clamped and every
 *              dependent is re-clamped so the set stays consistent
 *              (ENTER in [ABSORB-500, ABSORB-50], OVER in
 *              [ABSORB+100, ABSORB+400], FLOAT <= ABSORB-300, REENTRY <=
 *              FLOAT-300, TAPER <= BULK_CURRENT_MAX). Returns the APPLIED
 *              value (SET_PARAM reports it back).
* @param uint8_t__paramId 20..26 /
* @param uint32_t__value Raw requested value /
 * @param  uint32_t *uint32_t__appliedValue [EN] Required applied-value out;
* @return bool true = id known and output supplied /
 */
bool func__Charger_SetProfileParam(uint8_t uint8_t__paramId,
                                   uint32_t uint32_t__value,
                                   uint32_t *uint32_t__appliedValue);
/**
 * @brief  [EN] Read one charge-profile parameter (ESP link GET/PARAMS_BULK).
* @param uint8_t__paramId 20..26 /
* @param uint32_t *uint32_t__value Live value out /
* @return bool true = id known /
 */
bool func__Charger_GetProfileParam(uint8_t uint8_t__paramId,
                                   uint32_t *uint32_t__value);
/*
 * [EN] Charger-owned alarm wire ids (MUST equal ESPLINK_PARAM_CHG_ALARM_* in
 *      esp_link.h; the host test enforces the match). v1.15: the hard
 *      current/voltage ceilings become runtime-lowerable (never raisable
 *      above the compile maxima) from the alarms tab.
 */
#define CHG_ALARM_PARAM_HARD_CURRENT_MA       35u  /* [EN] mA, imax+50 .. CHG_CURRENT_HARD_FAULT_MAX_MA / mA */
#define CHG_ALARM_PARAM_OV_CUTOFF_MV          36u  /* [EN] mV, over+150..15000, never above 15000 / mV */
#define CHG_ALARM_PARAM_VALID_FLOOR_MV        37u  /* [EN] mV, 0..8000 / mV */
/**
 * @brief  [EN] Write one charger alarm parameter (ESP link, ids 35..37:
 *              35=HARD_CURRENT_MA, 36=OV_CUTOFF_MV, 37=VALID_FLOOR_MV).
 *              Safety direction is DOWN ONLY: the hard current fault can
 *              never exceed 950 mA and the OV cutoff never 15000 mV; both
 *              keep clearance above the live profile band (hard >= imax+50,
 *              OV >= over+150) so legit regulation can never trip them.
 *              Returns the APPLIED value (SET_PARAM reports it back).
* @param uint8_t__paramId 35..37 /
* @param uint32_t__value Raw requested value /
* @param uint32_t *uint32_t__appliedValue Applied value out /
* @return bool true = id known /
 */
bool func__Charger_SetAlarmParam(uint8_t uint8_t__paramId,
                                 uint32_t uint32_t__value,
                                 uint32_t *uint32_t__appliedValue);
/**
 * @brief  [EN] Read one charger alarm parameter (ESP link GET/PARAMS_BULK).
* @param uint8_t__paramId 35..37 /
* @param uint32_t *uint32_t__value Live value out /
* @return bool true = id known /
 */
bool func__Charger_GetAlarmParam(uint8_t uint8_t__paramId,
                                 uint32_t *uint32_t__value);
/*
 * [EN] Two-loop CC/CV PID wire ids (MUST equal ESPLINK_PARAM_CHG_PID_* in
 *      esp_link.h; the host test enforces the match). Dense 83..92 in the
 *      same order as charger_pid_t packs them, so Set/Get index instead of
 *      switching (same "flash diet" contract as the profile ids 20..26).
 *      Each stage is a complete five-field row (Kp, Ki, Kd, up-rate,
 *      down-rate), so the index arithmetic is uniform: (id - 84) % 5 says
 *      which field, (id - 84) / 5 says which stage, and fields 3 and 4 are
 *      the slew rates. Remember what a "stage" is here (see the regulator
 *      block above): stage 1 is the CURRENT loop, stages 2 and 3 are the
 *      VOLTAGE loop below and at the setpoint.
 */
#define CHG_PID_PARAM_CURRENT_KP        83u
#define CHG_PID_PARAM_CURRENT_KI        84u  /* [EN] 0..20000 */
#define CHG_PID_PARAM_CURRENT_KD        85u  /* [EN] 0..20000 */
#define CHG_PID_PARAM_CURRENT_UP_RATE   86u  /* [EN] milli-permille/s, 10..20000 */
#define CHG_PID_PARAM_CURRENT_DOWN_RATE 87u  /* [EN] milli-permille/s, 10..20000 */
#define CHG_PID_PARAM_VOLTAGE_KP        88u
#define CHG_PID_PARAM_VOLTAGE_KI        89u  /* [EN] 0..20000 */
#define CHG_PID_PARAM_VOLTAGE_KD        90u  /* [EN] 0..20000 */
#define CHG_PID_PARAM_VOLTAGE_UP_RATE   91u
#define CHG_PID_PARAM_VOLTAGE_DOWN_RATE 92u  /* [EN] milli-permille/s, 10..20000 */
/**
 * @brief  [EN] Write one two-loop CC/CV PID parameter (ESP link, ids 83..92).
 *              Values are clamped to the compiled windows: enable 0/1,
 *              gains 0..CHG_PID_GAIN_MAX, slew rates CHG_PID_RATE_MIN..
 *              CHG_PID_RATE_MAX milli-permille/s. Changing a gain never
 *              bumps the duty: the integrator keeps the present operating
 *              point and only its rate of change is re-scheduled. Returns
 *              the APPLIED value.
* @param uint8_t__paramId 83..92 /
* @param uint32_t__value Raw requested value /
* @param uint32_t *uint32_t__appliedValue Applied value out /
* @return bool true = id known /
 */
bool func__Charger_SetPidParam(uint8_t uint8_t__paramId,
                               uint32_t uint32_t__value,
                               uint32_t *uint32_t__appliedValue);
/**
 * @brief  [EN] Read one two-loop CC/CV PID parameter (ESP link GET/PARAMS_BULK).
* @param uint8_t__paramId 83..92 /
* @param uint32_t *uint32_t__value Live value out /
* @return bool true = id known /
 */
bool func__Charger_GetPidParam(uint8_t uint8_t__paramId,
                               uint32_t *uint32_t__value);
/*
 * ============ Charger limits & backstop gains (user order 2026-10-03) ============
 * [EN] USER-ORDERED LOGIC CHANGE. Every remaining charger gain, limit and
 *      stage timer becomes settable from the panel. These were compile-time
 *      constants, and the panel even admitted it in writing - the PID help
 *      bubble said the two hard backstops "are always on and are not
 *      adjustable from the panel", and the absorb ceiling was described as
 *      "1 hour" with no way to change it. Anything a bench session needs to
 *      try is now a parameter.
 *      Deliberately NOT in this block, and why:
 *        - CHG_DUTY_MAX_PERMILLE (500) stays a compile constant: it is the
 *          DCM/MOSFET rating of the board, not a tuning choice, and the
 *          per-channel ceiling (ids 13/14) already limits duty downward.
 *        - CHG_CURRENT_HARD_FAULT_MA (950) and the OV cutoff keep their
 *          existing down-only ids 35/36. A hard fault limit that can be
 *          RAISED from a web page is not a hard fault limit.
 *        - CHG_CURRENT_LIMIT_MA was deleted outright: nothing read it, so
 *          publishing it would have handed the user a knob wired to nothing.
 *      Implementation is TABLE-DRIVEN on purpose. Fifteen more switch cases
 *      with hand-written clamps is the pattern that has repeatedly gone
 *      stale here, and this part is already tight on flash (a 92-byte
 *      overflow had to be cleared in v1.26). One const row per limit -
 *      {min, max, default} - costs a few hundred bytes of rodata and makes
 *      the clamp impossible to forget.
 */
#define CHG_LIMIT_PARAM_ABSORB_MAX_MS        93u  /* [EN] ms, 0=no ceiling..21600000 (6 h) */
#define CHG_LIMIT_PARAM_ABSORB_MAX_ARM_MA    94u  /* [EN] mA, 10..500 - tail level that ARMS the ceiling */
#define CHG_LIMIT_PARAM_ABSORB_HOLD_MS       95u  /* [EN] ms, 0..7200000 - minimum soak before FLOAT */
#define CHG_LIMIT_PARAM_TAPER_SUSTAIN_MS     96u  /* [EN] ms, 1000..600000 - tail must hold this long */
#define CHG_LIMIT_PARAM_PID_MAX_STEP_PM      97u  /* [EN] permille/tick, 1..100 */
#define CHG_LIMIT_PARAM_PID_OUT_HYST_MILLI   98u  /* [EN] milli-permille, 0..5000 */
#define CHG_LIMIT_PARAM_PID_VOLT_FILTER_N    99u  /* [EN] EWMA divisor, 1..64 (1 = filter off) */
#define CHG_LIMIT_PARAM_BACKSTOP_MV         100u  /* [EN] mV, 13000..14800 - down only, never above compile max */
#define CHG_LIMIT_PARAM_BACKSTOP_GAIN_I     101u  /* [EN] 0..2000 - permille of duty per amp of overshoot */
#define CHG_LIMIT_PARAM_BACKSTOP_GAIN_V     102u  /* [EN] 0..2000 - permille of duty per volt of overshoot */
#define CHG_LIMIT_PARAM_PID_CUR_MARGIN_MA   103u  /* [EN] mA, 0..100 */
#define CHG_LIMIT_PARAM_CONNECT_SETTLE_MS   104u  /* [EN] ms, 0..120000 */
#define CHG_LIMIT_PARAM_JIT_LOCKOUT_MS      105u  /* [EN] ms, 0..60000 */
#define CHG_LIMIT_PARAM_MANUAL_WATCHDOG_MS  106u  /* [EN] ms, 500..60000 - deadman for manual mode */
#define CHG_LIMIT_PARAM_RAMP_DOWN_INT_MS    107u  /* [EN] ms, 50..5000 */
#define CHG_LIMIT_PARAM_FIRST_ID            CHG_LIMIT_PARAM_ABSORB_MAX_MS
#define CHG_LIMIT_PARAM_LAST_ID             CHG_LIMIT_PARAM_RAMP_DOWN_INT_MS
#define CHG_LIMIT_COUNT  \
    ((CHG_LIMIT_PARAM_LAST_ID - CHG_LIMIT_PARAM_FIRST_ID) + 1u)
/**
 * @brief  [EN] Write one charger limit / backstop gain (ESP link, ids
 *              93..107). The value is clamped into the compiled window for
 *              that row and the APPLIED value is returned, exactly like the
 *              profile and PID blocks.
* @param uint8_t__paramId 93..107 /
* @param uint32_t__value Raw requested value /
* @param uint32_t *uint32_t__appliedValue Applied value out /
* @return bool true = id known /
 */
bool func__Charger_SetLimitParam(uint8_t uint8_t__paramId,
                                 uint32_t uint32_t__value,
                                 uint32_t *uint32_t__appliedValue);
/**
 * @brief  [EN] Read one charger limit / backstop gain (ESP link GET/BULK).
* @param uint8_t__paramId 93..107 /
* @param uint32_t *uint32_t__value Live value out /
* @return bool true = id known /
 */
bool func__Charger_GetLimitParam(uint8_t uint8_t__paramId,
                                 uint32_t *uint32_t__value);
/*
 * ==================== Scenario 6: dead battery / : ====================
 * [EN] User order (2026-10-05): "a battery may never charge; it must not sit
 *      under the charger forever. After about 24 hours of continuous
 *      charging without reaching FLOAT, declare the battery dead: red LED,
 *      stop charging, and keep it stopped until the battery is replaced -
 *      like the imbalance latch."
 *      The timer is PER CHANNEL, because each half has its own charger and
 *      only one of them may be the dead one. It counts the time a channel
 *      spends actively charging (BULK or ABSORB). Reaching FLOAT - the
 *      definition of "it charged" - clears it. A charge that merely pauses
 *      (input lost, JIT retry, a short OFF) does NOT clear it: the pause has
 *      to last longer than the reset gap (id 126), otherwise a battery that
 *      flickers in and out of charge would reset the clock forever and the
 *      24 h would never be reached.
 *      The verdict is latched and persisted (slot 203, one bit per channel)
 *      and only a real battery swap clears it, exactly like scenario 5:
 *      the channel must report battery-absent continuously for 3 s.
 */
/**
* [EN] Continuous charge time after which the battery is declared dead
 *  (ms, 0 = scenario off), default 86400000 = 24 h.
 */
#define CHG_DEAD_PARAM_TIMEOUT_MS          125u
/**
* [EN] A charge pause shorter than this does NOT restart the timer (ms),
 *  default 60000 = 1 min.
 */
#define CHG_DEAD_PARAM_RESET_GAP_MS        126u
/**
* [EN] 1 = also keep the pack off the output while the dead verdict is
 *  latched (mirror of the imbalance checkbox, id 117), default 0.
 */
#define CHG_DEAD_PARAM_BLOCK_OUTPUT        127u
/*
 * [EN] Scenario 6 owns its complete lamp and beep shape: period, length,
 *      count, gap, blink period and blink duty. Factory values preserve the
 *      established one-beep, solid-red behaviour.
 */
/**
* [EN] Beep period of the dead-battery latch (ms, 0 = silent), default
 *  600000 = one beep every 10 min.
 */
#define CHG_DEAD_PARAM_BEEP_PERIOD_MS      128u
/**
* [EN] Length of one beep (ms), default 120.
 */
#define CHG_DEAD_PARAM_BEEP_LEN_MS         129u
/**
* [EN] Red-lamp blink period in the latch (ms, 0 = SOLID red), default 0 -
 *  scenario 6 stays solid out of the box so it still reads differently from
 *  the blinking imbalance latch, but the installer may now change it.
 */
#define CHG_DEAD_PARAM_BLINK_PERIOD_MS     130u
/**
* [EN] On-share of that blink (%), default 50; ignored while the period is 0.
 */
#define CHG_DEAD_PARAM_BLINK_DUTY_PCT      131u
/**
* [EN] Independent per-pattern beep shape, matching scenario 5: count
 *  defaults to one and the inter-beep gap defaults to zero.
 */
#define CHG_DEAD_PARAM_BEEP_COUNT          134u
#define CHG_DEAD_PARAM_BEEP_GAP_MS         135u
#define CHG_DEAD_DEF_BEEP_COUNT            1u
#define CHG_DEAD_DEF_BEEP_GAP_MS           0u
#define CHG_DEAD_PARAM_FIRST_ID            CHG_DEAD_PARAM_TIMEOUT_MS
#define CHG_DEAD_PARAM_LAST_ID             CHG_DEAD_PARAM_BEEP_GAP_MS
/*
 * [EN] Scenario 6 has nine dense storage slots for its non-contiguous wire
 *      ids 125..131 and 134..135. The explicit count and index helper below
 *      keep the 132..133 imbalance hole from becoming an out-of-bounds slot.
 */
/*
 * [EN] The wire ids have a deliberate 132..133 hole: those two ids belong
 *      to imbalance scenario 5. Keep the storage dense without claiming the
 *      other module's ids in the ESP router.
 */
#define CHG_DEAD_PARAM_COUNT              9u
#define CHG_DEAD_PARAM_OWNS(id) \
    ((((id) >= CHG_DEAD_PARAM_FIRST_ID) && ((id) <= CHG_DEAD_PARAM_BLINK_DUTY_PCT)) || \
     ((id) == CHG_DEAD_PARAM_BEEP_COUNT) || ((id) == CHG_DEAD_PARAM_BEEP_GAP_MS))
#define CHG_DEAD_PARAM_INDEX(id) \
    (((id) <= CHG_DEAD_PARAM_BLINK_DUTY_PCT) ? \
        ((uint8_t)(id) - CHG_DEAD_PARAM_FIRST_ID) : \
        ((uint8_t)(id) - CHG_DEAD_PARAM_BEEP_COUNT + 7u))
/**
* [EN] Persisted runtime slot (never a user parameter, never in a backup):
 *  bit 0 = charger 1 dead, bit 1 = charger 2 dead.
 */
#define CHG_DEAD_SLOT_MASK_ID              203u
/**
* [EN] Battery-absent time that clears the verdict (ms) - same 3 s the
 *  imbalance scenario uses, for the same reason: a swap takes minutes.
 */
#define CHG_DEAD_ABSENT_RESET_MS           3000u
/**
 * @brief  [EN] Write one scenario-6 parameter (ids 125..131 or 134..135)
 *              or replay the persisted verdict slot 203 at boot. Clamped like
 *              every other block; the applied value is returned.
* @param uint8_t__paramId 125..131, 134..135 or 203 /
* @param uint32_t__value Requested value /
* @param uint32_t__appliedValue Applied value out, may be NULL /
* @return bool true when the id belongs here /
 */
bool func__Charger_SetDeadParam(uint8_t uint8_t__paramId,
                                uint32_t uint32_t__value,
                                uint32_t *uint32_t__appliedValue);
/**
 * @brief  [EN] Read one scenario-6 parameter or the verdict slot.
* @param uint8_t__paramId 125..127 or 203 /
* @param uint32_t__value Live value out /
* @return bool true when the id belongs here /
 */
bool func__Charger_GetDeadParam(uint8_t uint8_t__paramId,
                                uint32_t *uint32_t__value);
/**
 * @brief  [EN] Set one thermal-rest parameter (ids 166..168), clamped to its
 *         factory window; the APPLIED value is reported back.
* @param uint8_t__paramId 166..168 /
* @param uint32_t__value Requested value /
* @param uint32_t__appliedValue Clamped value out /
* @return bool true when the id belongs here /
 */
bool func__Charger_SetThermalParam(uint8_t uint8_t__paramId,
                                   uint32_t uint32_t__value,
                                   uint32_t *uint32_t__appliedValue);
/**
 * @brief  [EN] Read one thermal-rest parameter (ids 166..168).
* @param uint8_t__paramId 166..168 /
* @param uint32_t__value Live value out /
* @return bool true when the id belongs here /
 */
bool func__Charger_GetThermalParam(uint8_t uint8_t__paramId,
                                   uint32_t *uint32_t__value);
/**
 * @brief  [EN] Latched dead-battery verdict, bit 0 = charger 1, bit 1 = charger 2.
* @return uint8_t 0..3 /
 */
uint8_t func__Charger_DeadMask(void);
/**
 * @brief  [EN] Continuous charge time of ONE channel right now, in seconds -
 *              what the panel shows as that battery's progress toward the
 *              24 h verdict. v1.76 (user order: "this timer has to be counted
 *              separately for each battery"): the clock was always per channel
 *              inside this module, but only the larger of the two left the
 *              board, so the panel could not tell the two batteries apart.
 *              Each channel is now reported on its own.
* @param uint8_t__channelIndex 0 = charger 1, 1 = charger 2 /
* @return uint32_t seconds, 0 when the index is out of range /
 */
uint32_t func__Charger_DeadElapsedSeconds(uint8_t uint8_t__channelIndex);
/**
 * @brief  [EN] True when a latched dead verdict must also keep the battery
 *              off the output (param 127).
* @return bool true = keep battery off the output /
 */
bool func__Charger_DeadBlocksOutput(void);
/**
 * @brief  [EN] v1.80: the scenario-6 lamp and buzzer shape, read by the UI
 *              module so the face is drawn from the user's own numbers
 *              instead of borrowed ones. Any pointer may be NULL.
* @param uint32_t__beepPeriodMs out, 0 = silent /
* @param uint32_t__beepLenMs out /
* @param uint32_t__blinkPeriodMs out, 0 = solid red /
* @param uint32_t__blinkDutyPct out /
 */
void func__Charger_DeadFaceShape(uint32_t *uint32_t__beepPeriodMs,
                                 uint32_t *uint32_t__beepLenMs,
                                 uint32_t *uint32_t__blinkPeriodMs,
                                 uint32_t *uint32_t__blinkDutyPct);
/**
* [EN] Read scenario 6's independent beep count and inter-beep gap.
 */
void func__Charger_DeadBeepPattern(uint32_t *uint32_t__beepCount,
                                   uint32_t *uint32_t__beepGapMs);
/**
 * @brief  [EN] Take-and-clear flag: a persisted verdict changed, so the
 *              caller should mark NVM slot 203 dirty.
* @return bool true = slot 203 needs saving /
 */
bool func__Charger_DeadTakePersistFlag(void);
/**
 * @brief  [EN] Reset active charger channels to OFF so the next control pass
 *              restarts charging from the beginning (BULK).
 */
void func__Charger_RestartCycle(void);
#endif /* CHARGER_H */
