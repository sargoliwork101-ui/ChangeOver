/**
 * @file    fault.h
 * @brief   [EN] Public interface of the latched-fault bit mask: the bit
 *               definitions themselves, the set/clear/query API, the
 *               battery-lost detection constants, the runtime alarm
 *               threshold contract (parameter ids 27..34) and the
 *               supervision hook the charger calls when one of its own
 *               thresholds moves. A latched bit forces the application into
 *               FAULT and keeps it there until something clears it.
 * @note    [EN] Full-program audit 2026-10-05: the old header line called
 *               this file a placeholder/skeleton. It has not been one for a
 *               long time - the module carries the battery-lost detector,
 *               the alarm set and the supervision cascade - so the stale
 *               label was removed.
 */
#ifndef FAULT_H
#define FAULT_H
/* ==================== Includes ==================== */
#include "app_types.h"
/* ==================== Battery-lost detection constants ==================== */
/**
 * @brief  [EN] Central battery-lost detection (Charger only mirrors the
 *         bit). Two cases, one shared bit FAULT_CHARGER_BAT_LOST:
 *         1) charging active + battery wire cut: a half-battery above
 *            FAULT_BAT_DISCONNECT_MV for FAULT_BAT_DISCONNECT_DEBOUNCE_MS
 *            (150 ms - the 15.0 V validity cut kills the pump within
 *            milliseconds, so a longer debounce can never fill; the node
 *            still floats ~0.5 s above 14.8 V). v1.21: armed PER HALF -
 *            each half is judged only while ITS OWN charger channel is
 *            pumping (a parked channel cannot fly its half up).
 *         2) input present and in range but EITHER half below
 *            FAULT_BAT_ABSENT_MV (6 V) for FAULT_BAT_ABSENT_DEBOUNCE_MS.
 *         Recovery: BOTH halves >= FAULT_BATTERY_BACK_MV (7 V, not 6 V - a
 *         half can sit near 6 V while charging) and no pump, for
 *         FAULT_BAT_RECOVER_MS => clear the bit.
 * @note   [EN] Case 2 runs only with the input inside
 *         [FAULT_INPUT_PRESENT_MIN_MV, FAULT_INPUT_PRESENT_MAX_MV].
 */
/*
 * [EN] The macros below are BOOT DEFAULTS only: the live values live in
 *      FAULT_ALARM_T__G__Alarm (fault.c) as panel ids 27..34, flash-persisted
 *      and clamped as a set on every write (func__Fault_ClampAlarms).
 */
#define FAULT_BAT_DISCONNECT_MV           14800u
#define FAULT_BAT_DISCONNECT_DEBOUNCE_MS    150u
/*
 * [EN] Absence threshold, case 2. Recovery stays at the higher
 *      FAULT_BATTERY_BACK_MV: the 1 V gap is the set/clear hysteresis.
 */
#define FAULT_BAT_ABSENT_MV                6000u
/*
 * [EN] Reset-only hard-open signature: a fitted 12 V half below 3 V is
 *      treated as physically removed even when the mains input is absent.
 *      This does not set the shared charger fault bit; Imbalance still
 *      requires its own 3 s continuous qualifier.
 */
#define FAULT_BAT_HARD_ABSENT_MV           3000u
/*
 * [EN] Recovery threshold: >= 7 V on BOTH halves, not 6 V (a half can
 *      sit near 6 V while charging).
 */
#define FAULT_BATTERY_BACK_MV              7000u
#define FAULT_BAT_ABSENT_DEBOUNCE_MS      1000u
#define FAULT_BAT_RECOVER_MS              1000u
#define FAULT_INPUT_PRESENT_MIN_MV        21000u
#define FAULT_INPUT_PRESENT_MAX_MV        28000u
/*
 * [EN] Shared input Schmitt margins, panel ids 169/170. Boot defaults are
 *      500 mV on each edge; the live values are clamped to 0..3000 mV and
 *      to one quarter of the live q33/q34 span so the re-arm band never
 *      inverts. All input consumers use the same published permission.
 */
#define FAULT_INPUT_HYST_LOW_DEFAULT_MV     500u
#define FAULT_INPUT_HYST_HIGH_DEFAULT_MV    500u
#define FAULT_INPUT_HYST_MAX_MV            3000u
#define FAULT_ALARM_PARAM_INPUT_HYST_LOW_MV  169u
#define FAULT_ALARM_PARAM_INPUT_HYST_HIGH_MV 170u
/**
 * @brief  [EN] Clear all fault bits.
 */
/* ==================== Functions ==================== */
void func__Fault_Init(void);
/**
 * @brief  [EN] Latch bits (OR).
* @param fault_mask_t__bits Bits to set /
 */
void func__Fault_Set(fault_mask_t fault_mask_t__bits);
/**
 * @brief  [EN] Clear bits (AND NOT).
* @param fault_mask_t__bits Bits to clear /
 */
void func__Fault_Clear(fault_mask_t fault_mask_t__bits);
/**
 * @brief  [EN] Return current mask.
* @return fault_mask_t Current mask /
 */
fault_mask_t func__Fault_Get(void);
/**
 * @brief  [EN] True if any bit is set.
* @return bool true if any fault / true
 */
bool func__Fault_Any(void);
/**
 * @brief  [EN] Evaluate battery-lost detection from the latest snapshot
 *         (once per control pass, BEFORE Fault_Get). Only installed halves
 *         participate; the pump rule is armed only while a channel really
 *         pumps. Sets/clears FAULT_CHARGER_BAT_LOST only.
* @param measurement_snapshot_t__snap Snapshot pointer, may be NULL / snapshot
 */
void func__Fault_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap);
/**
 * @brief  [EN] Return the live battery-removal signal for the imbalance
 *         battery-swap reset. It is separate from the latched shared fault
 *         bit: pumped-overvoltage, the existing low-half/input signature, or
 *         a fitted half below FAULT_BAT_HARD_ABSENT_MV. False after an invalid
 *         snapshot or during manual charger testing. Call after Evaluate().
* @return bool true while a removal signature is present / true
 */
bool func__Fault_IsBatteryAbsent(void);
/**
 * @brief  [EN] Return the live per-channel battery-removal signal (channel 0
 *         = high half, channel 1 = low half) prepared by the latest valid
 *         Fault_Evaluate() call. Used by Scenario 6 dead-battery supervision
 *         so removing one battery only clears that channel's latch/timer.
* @param uint8_t__channelIndex Channel index (0 or 1) / ( )
* @return bool true while that channel's removal signature is present / true
 */
bool func__Fault_IsChannelBatteryAbsent(uint8_t uint8_t__channelIndex);
/* ==================== Runtime alarm thresholds (v1.15) ==================== */
/*
 * [EN] The eight FAULT_* numbers above are runtime panel ids 27..34,
 *      flash-persisted and clamped as a set on every write; the macros
 *      stay boot defaults only.
 */
#define FAULT_ALARM_PARAM_DISCONNECT_MV        27u  /* [EN] mV, 14000..15000, independent of absorbOver and OV cutoff / mV */
#define FAULT_ALARM_PARAM_DISCONNECT_DEB_MS    28u  /* [EN] ms, 50..1000 / ms */
#define FAULT_ALARM_PARAM_ABSENT_MV            29u  /* [EN] mV, 3000..8000, < back-500 / mV */
#define FAULT_ALARM_PARAM_BACK_MV              30u  /* [EN] mV, 4000..9000, > absent+500 / mV */
#define FAULT_ALARM_PARAM_ABSENT_DEB_MS        31u  /* [EN] ms, 100..5000 / ms */
#define FAULT_ALARM_PARAM_RECOVER_MS           32u  /* [EN] ms, 100..5000 / ms */
#define FAULT_ALARM_PARAM_INPUT_MIN_MV         33u  /* [EN] mV, 18000..24000, < max-1000 / mV */
#define FAULT_ALARM_PARAM_INPUT_MAX_MV         34u  /* [EN] mV, 24000..30000, > min+1000 / mV */
/**
 * @brief  [EN] Live alarm-threshold set (one struct, like the charger
 *              profile). Evaluate() reads these, never the macros.
 */
typedef struct
{
    uint32_t uint32_t__disconnectMv;
    uint32_t uint32_t__disconnectDebMs;
    uint32_t uint32_t__absentMv;
    uint32_t uint32_t__backMv;
    uint32_t uint32_t__absentDebMs;
    uint32_t uint32_t__recoverMs;
    uint32_t uint32_t__inputMinMv;
    uint32_t uint32_t__inputMaxMv;
} fault_alarm_t;
/**
 * @brief  [EN] Write one alarm threshold (27..34): store, re-clamp the whole
 *              set, report the applied value.
* @param uint8_t__paramId 27..34 /
* @param uint32_t__value Requested value /
* @param uint32_t__appliedValue Required applied value out; NULL rejects the write / NULL
* @return bool true when the id is 27..34 /
 */
bool func__Fault_SetAlarmParam(uint8_t uint8_t__paramId,
                               uint32_t uint32_t__value,
                               uint32_t *uint32_t__appliedValue);
/**
 * @brief  [EN] Read one live alarm threshold (27..34).
* @param uint8_t__paramId 27..34 /
* @param uint32_t__value Value out /
* @return bool true when the id is 27..34 /
 */
bool func__Fault_GetAlarmParam(uint8_t uint8_t__paramId,
                               uint32_t *uint32_t__value);
/**
 * @brief  [EN] Read the shared healthy-input window (panel ids 33/34) as one
 *              atomic pair. UI input indication, the MCU self-supply path and
 *              the charger permission all use this same lower/upper reference;
 *              each consumer keeps its own internal hysteresis and timing.
* @param uint32_t__minMv Out: lower healthy input bound, mV / :
* @param uint32_t__maxMv Out: upper healthy input bound, mV / :
* @return bool true when both outputs were written / true
 */
bool func__Fault_GetInputWindow(uint32_t *uint32_t__minMv,
                                uint32_t *uint32_t__maxMv);
/**
 * @brief  [EN] Write one shared input Schmitt margin (panel ids 169/170).
 *              The value is clamped to 0..FAULT_INPUT_HYST_MAX_MV and to one
 *              quarter of the live q33/q34 span; the applied value is read
 *              back. Changing q33/q34 re-clamps both margins as well.
* @param uint8_t__paramId 169 or 170 /
* @param uint32_t__value Requested margin, mV /
* @param uint32_t__appliedValue Required applied value out /
* @return bool true when the id was accepted /
 */
bool func__Fault_SetInputHystParam(uint8_t uint8_t__paramId,
                                   uint32_t uint32_t__value,
                                   uint32_t *uint32_t__appliedValue);
/**
 * @brief  [EN] Read one live shared input Schmitt margin (169/170).
* @param uint8_t__paramId 169 or 170 /
* @param uint32_t__value Value out, mV /
* @return bool true when the id was accepted /
 */
bool func__Fault_GetInputHystParam(uint8_t uint8_t__paramId,
                                   uint32_t *uint32_t__value);
/**
 * @brief  [EN] Read the shared input permission published by the last
 *              Fault_Evaluate() pass. The rule is one Schmitt trigger for
 *              every consumer: permission drops immediately outside
 *              [q33,q34] and re-arms only inside
 *              [q33+H_low, q34-H_high]. snapshotValid=false means the ADC
 *              snapshot was not trustworthy and permission is false.
* @param bool__snapshotValid Out: last snapshot was valid / : snapshot
* @param bool__permitted Out: shared Schmitt permission / :
* @return bool true when both outputs were written / true
 */
bool func__Fault_GetInputPermission(bool *bool__snapshotValid,
                                    bool *bool__permitted);
/**
 * @brief  [EN] Re-apply the fault module's own hard windows after a
 *         profile/OV replay. q27 remains an independent 14000..15000 mV
 *         setting; this hook does not impose a charger-side relationship.
 */
void func__Fault_OnSupervisionChange(void);
#endif /* FAULT_H */
