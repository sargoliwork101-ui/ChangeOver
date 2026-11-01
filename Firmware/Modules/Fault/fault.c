/**
 * @file    fault.c
 * @brief   [EN] Implementation of the latched-fault store: one static bit
 *               mask with set/clear/query access, the battery-lost detector
 *               that watches a pumping charger channel whose pack voltage
 *               does not answer, the runtime alarm thresholds (parameter
 *               ids 27..34) with their hard per-field clamps, and the
 *               supervision hook that re-applies those clamps after a change.
 * @note    [EN] Full-program audit 2026-10-05: the stale "placeholder"
 *               label of the old header line was removed (see fault.h).
 */
#include "fault.h"
/*
 * [EN] Install map + active query come from the charger header (one concept,
 * one constant: which half is wired/used lives ONLY there). No HAL inside.
 */
#include "charger.h"
#include "cmsis_os2.h"
#include "rtos_time.h"
#include <stddef.h>
#include <stdbool.h>
static volatile fault_mask_t FAULT_MASK_T__G__Mask = FAULT_NONE;
/*
 * [EN] Battery-lost debounce timers: start tick of the current sustained
 * condition, 0 = condition not running.
 */
static uint32_t UINT32_T__G__BatOverSinceTick    = 0u;
static uint32_t UINT32_T__G__BatAbsentSinceTick  = 0u;
static uint32_t UINT32_T__G__BatHealthySinceTick = 0u;
/*
 * [EN] Live reset-only signal: never use the shared, latched alarm bit for
 */
static bool BOOL__G__BatteryAbsentForReset = false;
static bool BOOL__G__BatteryAbsentCh0      = false;
static bool BOOL__G__BatteryAbsentCh1      = false;
/* ==================== Runtime alarm thresholds (v1.15) ==================== */
/*
 * [EN] The eight FAULT_* numbers as one live struct - ids 27..34,
 * flash-persisted, clamped as a set on every write; boot = macro defaults.
 */
/*
 * [EN] volatile: written by the EspLink task, read by the control task
 */
static volatile fault_alarm_t FAULT_ALARM_T__G__Alarm =
{
    FAULT_BAT_DISCONNECT_MV,
    FAULT_BAT_DISCONNECT_DEBOUNCE_MS,
    FAULT_BAT_ABSENT_MV,
    FAULT_BATTERY_BACK_MV,
    FAULT_BAT_ABSENT_DEBOUNCE_MS,
    FAULT_BAT_RECOVER_MS,
    FAULT_INPUT_PRESENT_MIN_MV,
    FAULT_INPUT_PRESENT_MAX_MV
};
/*
 * [EN] Shared input Schmitt margins (ids 169/170) live outside the dense
 * 27..34 alarm word block so the indexed Set/Get contract and its layout
 * asserts stay unchanged. They are flash-persisted through EspLink like the
 * rest of the alarm set.
 */
static volatile uint32_t UINT32_T__G__InputHystLowMv  = FAULT_INPUT_HYST_LOW_DEFAULT_MV;
static volatile uint32_t UINT32_T__G__InputHystHighMv = FAULT_INPUT_HYST_HIGH_DEFAULT_MV;
/*
 * [EN] One published input permission for every consumer. Fault_Evaluate
 * refreshes it before any manual-mode / invalid-snapshot early return, so
 * charger, MCU power path and UI never freeze on a stale input decision.
 */
static volatile bool BOOL__G__InputSnapshotValid = false;
static volatile bool BOOL__G__InputPermitted     = false;
/* ==================== Fault_ClampAlarms (internal) ==================== */
/**
 * @brief  [EN] Re-impose the hard per-parameter windows on the runtime
 *              alarm set. The ESP/Firmware contract owns the raw range of
 *              every field; the battery-disconnect threshold is independently
 *              writable at 14000..15000 mV. It is intentionally not derived
 *              from absorbOver or OV cutoff: those are separate controls and
 *              an operator may choose a disconnect level that the OV path will
 *              reach first. The applied/read-back value is therefore the
 *              firmware's own 14000..15000 clamp. The remaining fault fields
 *              keep their real 500 mV absence/back hysteresis and 1000 mV
 *              input-window clamps. It is safe to call this function repeatedly.
 */
static void func__Fault_ClampAlarms(void)
{
    if (FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv < 14000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv = 14000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv > 15000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv = 15000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__disconnectDebMs < 50u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__disconnectDebMs = 50u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__disconnectDebMs > 1000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__disconnectDebMs = 1000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__absentMv < 3000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__absentMv = 3000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__absentMv > 8000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__absentMv = 8000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__backMv < 4000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__backMv = 4000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__backMv > 9000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__backMv = 9000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__absentMv >
        (FAULT_ALARM_T__G__Alarm.uint32_t__backMv - 500u))
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__absentMv =
            FAULT_ALARM_T__G__Alarm.uint32_t__backMv - 500u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__backMv <
        (FAULT_ALARM_T__G__Alarm.uint32_t__absentMv + 500u))
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__backMv =
            FAULT_ALARM_T__G__Alarm.uint32_t__absentMv + 500u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__absentDebMs < 100u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__absentDebMs = 100u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__absentDebMs > 5000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__absentDebMs = 5000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__recoverMs < 100u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__recoverMs = 100u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__recoverMs > 5000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__recoverMs = 5000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv < 18000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv = 18000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv > 24000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv = 24000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv < 24000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv = 24000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv > 30000u)
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv = 30000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv >
        (FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv - 1000u))
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv =
            FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv - 1000u;
    }
    if (FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv <
        (FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv + 1000u))
    {
        FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv =
            FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv + 1000u;
    }
    /*
 * [EN] The shared Schmitt margins follow the live window: each is
 * clamped to 0..3000 mV and to one quarter of the q33/q34 span, so the
 * inner re-arm band can never invert even at the minimum 1000 mV span.
 */
    {
        uint32_t uint32_t__spanMv =
            FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv -
            FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv;
        uint32_t uint32_t__capMv = uint32_t__spanMv / 4u;
        if (uint32_t__capMv > FAULT_INPUT_HYST_MAX_MV)
        {
            uint32_t__capMv = FAULT_INPUT_HYST_MAX_MV;
        }
        if (UINT32_T__G__InputHystLowMv > uint32_t__capMv)
        {
            UINT32_T__G__InputHystLowMv = uint32_t__capMv;
        }
        if (UINT32_T__G__InputHystHighMv > uint32_t__capMv)
        {
            UINT32_T__G__InputHystHighMv = uint32_t__capMv;
        }
    }
}
/*
 * [EN] Layout contract for the indexed Set/GetAlarmParam below (flash diet
 * 2026-09-27): wire ids 27..34 dense, one packed uint32_t per id in the
 * same order (host test pins every wire id).
 */
_Static_assert(FAULT_ALARM_PARAM_DISCONNECT_MV == 27u,
               "fault alarm id base must be 27");
_Static_assert(FAULT_ALARM_PARAM_INPUT_MAX_MV == 34u,
               "fault alarm id top must be 34");
_Static_assert(sizeof(fault_alarm_t) == (8u * sizeof(uint32_t)),
               "fault_alarm_t must pack exactly 8 words");
_Static_assert(offsetof(fault_alarm_t, uint32_t__disconnectMv) == 0u,
               "first field must be the id-27 word");
_Static_assert(offsetof(fault_alarm_t, uint32_t__inputMaxMv) ==
                   (7u * sizeof(uint32_t)),
               "last field must be the id-34 word");
/* ==================== Fault_SetAlarmParam ==================== */
/**
 * @brief  [EN] Write one runtime alarm threshold by dense parameter id and
 *              report back the value that actually took effect. The ids map
 *              one-to-one onto the words of fault_alarm_t (the static
 *              asserts above nail that layout down), so the write is a
 *              single indexed store rather than a switch that could drift
 *              out of sync. The whole update runs under a scheduler lock:
 *              the comm task writes while the control task evaluates
 *              faults, and without the lock the evaluator could preempt
 *              mid-clamp and read a torn threshold set for one pass. After
 *              the store the interdependency clamps run, which is why the
 *              applied value can legitimately differ from the requested
 *              one - the caller is told the clamped value, never a lie.
 * @param  uint8_t__paramId       [EN] Dense alarm id, valid range
 *                                    FAULT_ALARM_PARAM_DISCONNECT_MV (27)
 *                                    .. FAULT_ALARM_PARAM_INPUT_MAX_MV (34) /
 * @param  uint32_t__value        [EN] Requested value; mV for the voltage
 *                                    ids, ms for the debounce ids /
 * @param  uint32_t__appliedValue [EN] Out: the value in force after the
 *                                    clamps; untouched when the id is
 *                                    rejected /
 * @return bool [EN] true when the id was in range and the write landed,
 *                   false for an unknown id /
 */
bool func__Fault_SetAlarmParam(uint8_t uint8_t__paramId,
                               uint32_t uint32_t__value,
                               uint32_t *uint32_t__appliedValue)
{
    int32_t int32_t__savedKernelLock;
    if (uint32_t__appliedValue == NULL)
    {
        return false;
    }
    /*
 * [EN] Writer-side scheduler lock (v1.16 audit C11): the comm task
 * writes, the control task (fault eval) preempts mid-clamp and would
 * read a torn threshold set for one pass. Pre-kernel the plain path
 * runs (NVM replay).
 */
    int32_t__savedKernelLock = osKernelLock();
    /*
 * [EN] Indexed store (flash diet): ids 27..34 are dense and
 * fault_alarm_t packs the same fields in the same order (asserts above).
 */
    if ((uint8_t__paramId < FAULT_ALARM_PARAM_DISCONNECT_MV) ||
        (uint8_t__paramId > FAULT_ALARM_PARAM_INPUT_MAX_MV))
    {
        if (int32_t__savedKernelLock >= 0)
        {
            (void)osKernelRestoreLock(int32_t__savedKernelLock);
        }
        return false;
    }
    ((volatile uint32_t *)&FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv)
        [uint8_t__paramId - FAULT_ALARM_PARAM_DISCONNECT_MV] = uint32_t__value;
    func__Fault_ClampAlarms();
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
    return func__Fault_GetAlarmParam(uint8_t__paramId, uint32_t__appliedValue);
}
/* ==================== Fault_GetAlarmParam ==================== */
/**
 * @brief  [EN] Read one runtime alarm threshold by dense parameter id. It
 *              is the exact mirror of the setter and shares the same id to
 *              struct-word contract, so a panel read-back always reports
 *              the value the firmware is really using, including whatever
 *              the interdependency clamps changed.
 * @param  uint8_t__paramId [EN] Dense alarm id, valid range
 *                              FAULT_ALARM_PARAM_DISCONNECT_MV (27)
 *                              .. FAULT_ALARM_PARAM_INPUT_MAX_MV (34) /
 * @param  uint32_t__value  [EN] Out: current value, mV for the voltage ids
 *                              and ms for the debounce ids; untouched when
 *                              the id is rejected /
* @return bool true when the id was in range / true
 */
bool func__Fault_GetAlarmParam(uint8_t uint8_t__paramId,
                               uint32_t *uint32_t__value)
{
    if (uint32_t__value == NULL)
    {
        return false;
    }
    /*
 * [EN] Indexed read: same dense-id/struct contract as the setter.
 */
    if ((uint8_t__paramId < FAULT_ALARM_PARAM_DISCONNECT_MV) ||
        (uint8_t__paramId > FAULT_ALARM_PARAM_INPUT_MAX_MV))
    {
        return false;
    }
    *uint32_t__value =
        ((volatile uint32_t *)&FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv)
        [uint8_t__paramId - FAULT_ALARM_PARAM_DISCONNECT_MV];
    return true;
}
/* ==================== Fault_GetInputWindow ==================== */
/**
 * @brief  [EN] Copy the shared healthy-input window (ids 33/34) under one
 *              scheduler lock so a consumer never sees a new lower bound with
 *              an old upper bound while the panel is writing the pair.
* @param uint32_t__minMv Out: lower bound, mV / :
* @param uint32_t__maxMv Out: upper bound, mV / :
* @return bool true when both outputs were written / true
 */
bool func__Fault_GetInputWindow(uint32_t *uint32_t__minMv,
                                uint32_t *uint32_t__maxMv)
{
    int32_t int32_t__savedKernelLock;
    if ((uint32_t__minMv == NULL) || (uint32_t__maxMv == NULL))
    {
        return false;
    }
    int32_t__savedKernelLock = osKernelLock();
    *uint32_t__minMv = FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv;
    *uint32_t__maxMv = FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv;
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
    return true;
}
/* ==================== Fault_SetInputHystParam ==================== */
/**
 * @brief  [EN] Write one shared input Schmitt margin (ids 169/170). The raw
 *              request is clamped to 0..FAULT_INPUT_HYST_MAX_MV, then the
 *              whole alarm set is re-clamped so the live q33/q34 span also
 *              caps each margin at span/4. The applied value is read back.
 */
bool func__Fault_SetInputHystParam(uint8_t uint8_t__paramId,
                                   uint32_t uint32_t__value,
                                   uint32_t *uint32_t__appliedValue)
{
    int32_t int32_t__savedKernelLock;
    if (uint32_t__appliedValue == NULL)
    {
        return false;
    }
    if ((uint8_t__paramId != FAULT_ALARM_PARAM_INPUT_HYST_LOW_MV) &&
        (uint8_t__paramId != FAULT_ALARM_PARAM_INPUT_HYST_HIGH_MV))
    {
        return false;
    }
    int32_t__savedKernelLock = osKernelLock();
    if (uint32_t__value > FAULT_INPUT_HYST_MAX_MV)
    {
        uint32_t__value = FAULT_INPUT_HYST_MAX_MV;
    }
    if (uint8_t__paramId == FAULT_ALARM_PARAM_INPUT_HYST_LOW_MV)
    {
        UINT32_T__G__InputHystLowMv = uint32_t__value;
    }
    else
    {
        UINT32_T__G__InputHystHighMv = uint32_t__value;
    }
    func__Fault_ClampAlarms();
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
    return func__Fault_GetInputHystParam(uint8_t__paramId, uint32_t__appliedValue);
}
/* ==================== Fault_GetInputHystParam ==================== */
/**
 * @brief  [EN] Read one live shared input Schmitt margin (ids 169/170).
 */
bool func__Fault_GetInputHystParam(uint8_t uint8_t__paramId,
                                   uint32_t *uint32_t__value)
{
    int32_t int32_t__savedKernelLock;
    if (uint32_t__value == NULL)
    {
        return false;
    }
    if ((uint8_t__paramId != FAULT_ALARM_PARAM_INPUT_HYST_LOW_MV) &&
        (uint8_t__paramId != FAULT_ALARM_PARAM_INPUT_HYST_HIGH_MV))
    {
        return false;
    }
    int32_t__savedKernelLock = osKernelLock();
    *uint32_t__value = (uint8_t__paramId == FAULT_ALARM_PARAM_INPUT_HYST_LOW_MV)
                           ? UINT32_T__G__InputHystLowMv
                           : UINT32_T__G__InputHystHighMv;
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
    return true;
}
/* ==================== Fault_GetInputPermission ==================== */
/**
 * @brief  [EN] Copy the last published shared input decision under one lock.
 */
bool func__Fault_GetInputPermission(bool *bool__snapshotValid,
                                    bool *bool__permitted)
{
    int32_t int32_t__savedKernelLock;
    if ((bool__snapshotValid == NULL) || (bool__permitted == NULL))
    {
        return false;
    }
    int32_t__savedKernelLock = osKernelLock();
    *bool__snapshotValid = BOOL__G__InputSnapshotValid;
    *bool__permitted     = BOOL__G__InputPermitted;
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
    return true;
}
/* ==================== Fault_UpdateInputPermission (internal) ==================== */
/**
 * @brief  [EN] Refresh the one shared Schmitt permission from the latest
 *              measurement snapshot. Outside [q33,q34] permission drops
 *              immediately; from the released state it re-arms only inside
 *              [q33+H_low, q34-H_high]. Invalid snapshots publish
 *              valid=false / permitted=false so every consumer fails closed.
 */
static void func__Fault_UpdateInputPermission(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    int32_t  int32_t__savedKernelLock;
    uint32_t uint32_t__minMv;
    uint32_t uint32_t__maxMv;
    uint32_t uint32_t__hystLowMv;
    uint32_t uint32_t__hystHighMv;
    uint32_t uint32_t__inputMv;
    bool     bool__permitted;
    if ((measurement_snapshot_t__snap == NULL) ||
        (measurement_snapshot_t__snap->valid == false))
    {
        int32_t__savedKernelLock = osKernelLock();
        BOOL__G__InputSnapshotValid = false;
        BOOL__G__InputPermitted     = false;
        if (int32_t__savedKernelLock >= 0)
        {
            (void)osKernelRestoreLock(int32_t__savedKernelLock);
        }
        return;
    }
    int32_t__savedKernelLock = osKernelLock();
    uint32_t__minMv      = FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv;
    uint32_t__maxMv      = FAULT_ALARM_T__G__Alarm.uint32_t__inputMaxMv;
    uint32_t__hystLowMv  = UINT32_T__G__InputHystLowMv;
    uint32_t__hystHighMv = UINT32_T__G__InputHystHighMv;
    uint32_t__inputMv    = measurement_snapshot_t__snap->v_in_mv;
    bool__permitted      = BOOL__G__InputPermitted;
    if ((uint32_t__inputMv < uint32_t__minMv) ||
        (uint32_t__inputMv > uint32_t__maxMv))
    {
        bool__permitted = false;
    }
    else if (bool__permitted == false)
    {
        bool__permitted =
            ((uint32_t__inputMv >= (uint32_t__minMv + uint32_t__hystLowMv)) &&
             (uint32_t__inputMv <= (uint32_t__maxMv - uint32_t__hystHighMv)));
    }
    else
    {
        /*
 * [EN] Already armed: stay armed anywhere inside the raw window.
 */
    }
    BOOL__G__InputSnapshotValid = true;
    BOOL__G__InputPermitted     = bool__permitted;
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
}
/* ==================== Fault_OnSupervisionChange ==================== */
/**
 * @brief  [EN] Supervision hook the charger calls after it moves one of its
 *              own thresholds. The fault fields have their own hard windows;
 *              this hook re-applies those windows after a profile or alarm
 *              replay without coupling q27 to absorbOver or OV cutoff.
 *              Keeping it named rather than exporting the clamp means the
 *              charger never reaches into this module's internals.
 */
void func__Fault_OnSupervisionChange(void)
{
    func__Fault_ClampAlarms();
}
/* ==================== Fault_DebounceDone (internal) ==================== */
/**
 * @brief  [EN] Shared debounce helper: returns true once the condition has
 *         been continuously true for uint32_t__milliseconds. Call it only
 *         while the condition is true; reset the start tick to 0 when the
 *         condition is false.
* @param uint32_t_ptr__sinceTick Start tick storage (0 = not running) /
* @param uint32_t__nowTick Current kernel tick /
* @param uint32_t__milliseconds Required duration /
* @return bool true when the duration elapsed / true
 */
static bool func__Fault_DebounceDone(uint32_t *uint32_t_ptr__sinceTick,
                                     uint32_t uint32_t__nowTick,
                                     uint32_t uint32_t__milliseconds)
{
    uint32_t uint32_t__durationTicks;
    uint32_t__durationTicks = func__Rtos_MillisecondsToTicks(uint32_t__milliseconds);
    if (uint32_t__durationTicks == 0u)
    {
        /*
 * [EN] Zero-tick conversion (tick frequency 0): never fire instantly.
 */
        return false;
    }
    if (*uint32_t_ptr__sinceTick == 0u)
    {
        *uint32_t_ptr__sinceTick = uint32_t__nowTick;
        return false;
    }
    return ((uint32_t)(uint32_t__nowTick - *uint32_t_ptr__sinceTick) >=
            uint32_t__durationTicks);
}
/**
 * @brief  [EN] Clear all fault bits.
 */
/* ==================== Fault_Init ==================== */
void func__Fault_Init(void)
{
    FAULT_MASK_T__G__Mask = FAULT_NONE;
    UINT32_T__G__BatOverSinceTick    = 0u;
    UINT32_T__G__BatAbsentSinceTick  = 0u;
    UINT32_T__G__BatHealthySinceTick = 0u;
    BOOL__G__BatteryAbsentForReset   = false;
    BOOL__G__BatteryAbsentCh0        = false;
    BOOL__G__BatteryAbsentCh1        = false;
    BOOL__G__InputSnapshotValid      = false;
    BOOL__G__InputPermitted          = false;
}
/**
 * @brief  [EN] Latch bits (OR).
* @param fault_mask_t__bits Bits to set /
 */
/* ==================== Fault_Set ==================== */
void func__Fault_Set(fault_mask_t fault_mask_t__bits)
{
    /*
 * [EN] Scheduler lock (v1.16 audit F4): the RMW is shared by the
 * control and protection tasks - a Set racing a Clear loses one
 * update. Pre-kernel the lock call fails and the plain RMW runs
 * single-threaded (same pattern as measurement.c).
 */
    int32_t int32_t__savedKernelLock = osKernelLock();
    FAULT_MASK_T__G__Mask |= fault_mask_t__bits;
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
}
/**
 * @brief  [EN] Clear bits (AND NOT).
* @param fault_mask_t__bits Bits to clear /
 */
/* ==================== Fault_Clear ==================== */
void func__Fault_Clear(fault_mask_t fault_mask_t__bits)
{
    /*
 * [EN] Scheduler lock: same lost-update closure as Fault_Set.
 */
    int32_t int32_t__savedKernelLock = osKernelLock();
    FAULT_MASK_T__G__Mask &= (fault_mask_t)~fault_mask_t__bits;
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
}
/**
 * @brief  [EN] Return current mask.
* @return fault_mask_t Current fault mask /
 */
/* ==================== Fault_Get ==================== */
fault_mask_t func__Fault_Get(void)
{
    return FAULT_MASK_T__G__Mask;
}
/**
 * @brief  [EN] True if any bit is set.
* @return bool true if any fault latched / true
 */
/* ==================== Fault_Any ==================== */
bool func__Fault_Any(void)
{
    return (FAULT_MASK_T__G__Mask != FAULT_NONE);
}
/**
 * @brief  [EN] Central battery-lost evaluation (see fault.h for the two
 *         cases). Sets FAULT_CHARGER_BAT_LOST after the debounce of either
 *         rule and clears it after the recovery settle; only this bit is
 *         touched. Called every control pass before func__Fault_Get().
 */
/* ==================== Fault_Evaluate ==================== */
void func__Fault_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    uint32_t uint32_t__nowTick;
    uint32_t uint32_t__lowMv;
    uint32_t uint32_t__highMv;
    bool     bool__inputOk;
    bool     bool__highOver;
    bool     bool__lowOver;
    bool     bool__anyOver;
    bool     bool__highHalfLow;
    bool     bool__lowHalfLow;
    bool     bool__anyHalfLow;
    bool     bool__highHalfHardAbsent;
    bool     bool__lowHalfHardAbsent;
    bool     bool__batteryTrulyPresent;
    bool     bool__healthy;
    bool     bool__highHalfInstalled;
    bool     bool__lowHalfInstalled;
    uint32_t__nowTick = osKernelGetTickCount();
    /*
 * [EN] Refresh the shared input Schmitt BEFORE the manual-mode and
 * invalid-snapshot early returns, so charger / MCU / UI never keep a
 * stale input permission while battery debounce is frozen.
 */
    func__Fault_UpdateInputPermission(measurement_snapshot_t__snap);
    /*
 * [EN] Manual test mode: battery conditions suspended - freeze every
 * debounce (nothing new latches, nothing clears); the charger clears
 * BAT_LOST on entering the mode and detectors restart on exit.
 */
    if (func__Charger_IsManualTestModeActive() != false)
    {
        UINT32_T__G__BatOverSinceTick    = 0u;
        UINT32_T__G__BatAbsentSinceTick  = 0u;
        UINT32_T__G__BatHealthySinceTick = 0u;
        BOOL__G__BatteryAbsentForReset   = false;
        BOOL__G__BatteryAbsentCh0        = false;
        BOOL__G__BatteryAbsentCh1        = false;
        return;
    }
    /*
 * [EN] No trustworthy snapshot: freeze every progress (no set, no clear,
 * and debounce restarts from zero next valid pass).
 */
    if ((measurement_snapshot_t__snap == NULL) ||
        (measurement_snapshot_t__snap->valid == false))
    {
        UINT32_T__G__BatOverSinceTick    = 0u;
        UINT32_T__G__BatAbsentSinceTick  = 0u;
        UINT32_T__G__BatHealthySinceTick = 0u;
        BOOL__G__BatteryAbsentForReset   = false;
        BOOL__G__BatteryAbsentCh0        = false;
        BOOL__G__BatteryAbsentCh1        = false;
        return;
    }
    uint32_t__lowMv  = measurement_snapshot_t__snap->v_bat_low_mv;
    uint32_t__highMv = measurement_snapshot_t__snap->v_bat_high_mv;
    /*
 * [EN] Per-half participation follows the channel install map
 * (channel 0 = high half, channel 1 = low half); an uninstalled half
 * is simply not wired and can never count as "disconnected".
 */
    bool__highHalfInstalled = ((CHG_INSTALLED_CHANNEL_MASK & (1u << 0u)) != 0u);
    bool__lowHalfInstalled  = ((CHG_INSTALLED_CHANNEL_MASK & (1u << 1u)) != 0u);
    /*
 * [EN] Case-2 gate: the same shared Schmitt permission every other
 * input consumer uses (already refreshed above from this snapshot).
 */
    bool__inputOk = BOOL__G__InputPermitted;
    /*
 * [EN] Case 1: a half pumped above the disconnect threshold (flyback
 * signature of a cut battery wire while charging). v1.21 (user order
 * 2026-09-28, kills the repeating false 3-beep cycle during charge):
 * armed PER HALF by THAT half's own pumping channel - a parked
 * channel has no pump, so its half cannot fly up to the threshold
 * and must not be judged. The derived vhigh = V24 - V12 in
 * particular moves with the OTHER channel's load current, so judging
 * it while only ch2 pumps latched phantom battery-lost alarms.
 */
    bool__highOver =
        ((bool__highHalfInstalled == true) &&
         ((func__Charger_IsChannelActive(0u) == true) ||
          ((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != FAULT_NONE)) &&
         (uint32_t__highMv > FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv));
    bool__lowOver =
        ((bool__lowHalfInstalled  == true) &&
         ((func__Charger_IsChannelActive(1u) == true) ||
          ((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != FAULT_NONE)) &&
         (uint32_t__lowMv  > FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv));
    bool__anyOver = ((bool__lowOver == true) || (bool__highOver == true));
    /*
 * [EN] Case 2: EITHER half below the absent threshold (6 V) with a
 * valid input = battery disconnected. Recovery keeps 7 V (1 V
 * hysteresis: a deeply discharged-but-connected battery dips near 6 V
 * without tripping).
 */
    bool__highHalfLow =
        ((bool__highHalfInstalled == true) &&
         (uint32_t__highMv < FAULT_ALARM_T__G__Alarm.uint32_t__absentMv));
    bool__lowHalfLow =
        ((bool__lowHalfInstalled  == true) &&
         (uint32_t__lowMv  < FAULT_ALARM_T__G__Alarm.uint32_t__absentMv));
    bool__anyHalfLow = ((bool__lowHalfLow == true) || (bool__highHalfLow == true));
    /*
 * [EN] Reset-only physical-open signature. A fitted half below 3 V is a
 * much stronger removal clue than the adjustable 6 V alarm threshold;
 * it also works when the mains input is absent. Do not add it to the
 * shared FAULT_CHARGER_BAT_LOST bit.
 */
    bool__highHalfHardAbsent =
        ((bool__highHalfInstalled == true) &&
         (uint32_t__highMv < FAULT_BAT_HARD_ABSENT_MV));
    bool__lowHalfHardAbsent =
        ((bool__lowHalfInstalled  == true) &&
         (uint32_t__lowMv  < FAULT_BAT_HARD_ABSENT_MV));
    /*
 * [EN] Publish per-channel and combined live reset signals, not the
 * shared latched fault bit.
 */
    BOOL__G__BatteryAbsentCh0 =
        ((bool__highOver == true) ||
         ((bool__inputOk == true) && (bool__highHalfLow == true)) ||
         (bool__highHalfHardAbsent == true));
    BOOL__G__BatteryAbsentCh1 =
        ((bool__lowOver == true) ||
         ((bool__inputOk == true) && (bool__lowHalfLow == true)) ||
         (bool__lowHalfHardAbsent == true));
    BOOL__G__BatteryAbsentForReset =
        ((BOOL__G__BatteryAbsentCh0 == true) ||
         (BOOL__G__BatteryAbsentCh1 == true));
    /* ---------- Rule 1: pumped overvoltage => latch ---------- */
    if ((bool__anyOver == true) &&
        (func__Fault_DebounceDone(&UINT32_T__G__BatOverSinceTick,
                                  uint32_t__nowTick,
                                  FAULT_ALARM_T__G__Alarm.uint32_t__disconnectDebMs) == true))
    {
        func__Fault_Set(FAULT_CHARGER_BAT_LOST);
    }
    else if (bool__anyOver == false)
    {
        UINT32_T__G__BatOverSinceTick = 0u;
    }
    else
    {
    }
    /* ---------- Rule 2: EITHER half below 6 V with valid input => latch ---------- */
    if ((bool__inputOk == true) && (bool__anyHalfLow == true) &&
        (func__Fault_DebounceDone(&UINT32_T__G__BatAbsentSinceTick,
                                  uint32_t__nowTick,
                                  FAULT_ALARM_T__G__Alarm.uint32_t__absentDebMs) == true))
    {
        func__Fault_Set(FAULT_CHARGER_BAT_LOST);
    }
    else if ((bool__inputOk == false) || (bool__anyHalfLow == false))
    {
        UINT32_T__G__BatAbsentSinceTick = 0u;
    }
    else
    {
    }
    /*
 * ---------- Shared recovery: healthy window held => release ----------
 * [EN] Healthy = no half pumped AND both installed halves >= back
 * threshold (7 V, not 6 V); held for the recover time => clear the bit,
 * otherwise the reminder keeps repeating.
 */
    bool__batteryTrulyPresent =
        (((bool__lowHalfInstalled  == false) ||
          (uint32_t__lowMv  >= FAULT_ALARM_T__G__Alarm.uint32_t__backMv)) &&
         ((bool__highHalfInstalled == false) ||
          (uint32_t__highMv >= FAULT_ALARM_T__G__Alarm.uint32_t__backMv)));
    bool__healthy = ((bool__anyOver == false) &&
                     (bool__batteryTrulyPresent == true));
    if (bool__healthy == false)
    {
        UINT32_T__G__BatHealthySinceTick = 0u;
    }
    else if (func__Fault_DebounceDone(&UINT32_T__G__BatHealthySinceTick,
                                      uint32_t__nowTick,
                                      FAULT_ALARM_T__G__Alarm.uint32_t__recoverMs) == true)
    {
        /*
 * [EN] Battery really reconnected; the charger mirrors the cleared
 * bit to OFF and its own 15 s settle still gates the bulk start.
 */
        func__Fault_Clear(FAULT_CHARGER_BAT_LOST);
        UINT32_T__G__BatHealthySinceTick = 0u;
    }
    else
    {
    }
}
/**
 * @brief  [EN] Return the live battery-removal signal prepared by the latest
 *              valid Fault_Evaluate() call. It is intentionally independent
 *              of FAULT_CHARGER_BAT_LOST, whose latch also serves charger
 *              safety and may have a different recovery window.
* @return bool true while a removal signature is present / true
 */
bool func__Fault_IsBatteryAbsent(void)
{
    return BOOL__G__BatteryAbsentForReset;
}
/* ==================== Fault_IsChannelBatteryAbsent ==================== */
/**
 * @brief  [EN] Return the live per-channel battery-removal signal (channel 0
 *              = high half, channel 1 = low half) prepared by the latest valid
 *              Fault_Evaluate() call.
* @param uint8_t__channelIndex Channel index (0 or 1) / ( )
* @return bool true while that channel's removal signature is present / true
 */
bool func__Fault_IsChannelBatteryAbsent(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return BOOL__G__BatteryAbsentCh0;
    }
    if (uint8_t__channelIndex == 1u)
    {
        return BOOL__G__BatteryAbsentCh1;
    }
    return false;
}
