/**
 * @file    ui_led.c
 * @brief   [EN] UI LED scenarios - green/red/yellow, battery percent, InputOk/Charging/BatteryRun.
 *          Split from UI into LED and BUZZER per user request. Constants for LED in ui_led.h.
 *          CMSIS-RTOS2 simple readable, non-linear formulas, markers above each function and variable in h and c.
 * @note    [EN] ui_led.h provides defaults; since v1.16 the live cadence
 *          comes from the persisted UI_ALARM_T__G__Alarm struct (ids
 *          38..82), not from const APP_CONFIG (which now feeds only the
 *          one-shot BoardTest timings). Naming __ after type, func__ prefix.
 *          CMSIS-RTOS2: osDelay allowed, HAL_Delay forbidden. Formulas non-linear broken into steps.
 */
#include "ui_led.h"
#include "ui_buzzer.h"
#include "app_config.h"
#include "bsp_gpio.h"
#include "bsp_iwdg.h"
#include "cmsis_os2.h"
#include "rtos_time.h"
#include "modules_enable.h"
#include <stddef.h>
#if MODULE_FAULT
#include "fault.h"
#endif
#if MODULE_CHARGER
#include "charger.h"
#endif
#if MODULE_IMBALANCE
#include "imbalance.h"
#endif
#if MODULE_CHANGEOVER
#include "changeover.h"
#endif
#include <stdbool.h>
/* ==================== UI Global Battery Alarm Flag / UI ==================== */
/**
 * @brief  [EN] Global flag owned by the UI: true while a valid low-battery alarm is active.
 *         It is continuous (level), not a pulse. Set in Init to false, cleared on invalid
 *         snapshot, set when v_bat24_mv < 21000 and cleared when >=21200.
 */
/* ==================== Runtime UI cadence (v1.16) ==================== */
/*
 * [EN] v1.16 (user order 2026-09-26): the 39 UI_* numbers as one live
 *      struct - ids 38..76 (76 = panel-session mute, RAM-only), STM32-flash
 *      persisted, clamped as a set on
 *      every write. Boot = the macro defaults, so a reflash with an
 *      unreadable NVM record changes no behaviour.
 *      v1.17 (user order 2026-09-27): 45 numbers, ids 38..82 (77..82 =
 *      full latch + stable hysteresis + 0/1 exits, all flash-persisted).
 */
/*
 * [EN] volatile: written by the EspLink task (panel edits), read by the UI
 * task (scenarios) with no lock - single-word members stay atomic and no
 * reader may cache a half-applied set across one pass (full-program audit
 */
static volatile ui_alarm_t UI_ALARM_T__G__Alarm =
{
    UI_INPUT_OVERVOLTAGE_LED_PERIOD_MS,
    UI_INPUT_OVERVOLTAGE_LED_DUTY_PERCENT,
    UI_INPUT_OVERVOLTAGE_BEEP_PERIOD_MS,
    UI_INPUT_OVERVOLTAGE_BEEP_DURATION_MS,
    UI_INPUT_OVERVOLTAGE_BEEP_COUNT,
    UI_INPUT_OVERVOLTAGE_BEEP_GAP_MS,
    UI_BAT_LOST_LED_PERIOD_MS,
    UI_BAT_LOST_LED_DUTY_PERCENT,
    UI_BAT_LOST_BEEP_PERIOD_MS,
    /*
 * [EN] v1.16: durations are uniformly PER-BEEP; the legacy 900 ms
 * BatLost window was shared by 3 beeps + 2 gaps, i.e. (900-200)/3 =
 * 233 ms per beep - this keeps the exact boot sound (duty 30 ->
 * [233,233,234], see host_test_ui).
 */
    233u,
    UI_BAT_LOST_BEEP_COUNT,
    UI_BAT_LOST_BEEP_GAP_MS,
    UI_BATTERY_RUN_BEEP_START_PERCENT,
    UI_BATTERY_RUN_BEEP_DOUBLE_PERCENT,
    UI_BATTERY_RUN_BEEP_TRIPLE_PERCENT,
    UI_BATTERY_RUN_BEEP_CRITICAL_PERCENT,
    UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS,
    UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS,
    UI_BATTERY_RUN_BEEP_CRITICAL_PERIOD_MS,
    UI_BATTERY_RUN_BEEP_CRITICAL_BEEP_DURATION_MS,
    UI_BATTERY_RUN_BEEP_CRITICAL_COUNT,
    UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS,
    UI_BATTERY_RUN_BEEP_TRIPLE_DURATION_MS,
    UI_BATTERY_RUN_BEEP_CRITICAL_DURATION_MS,
    UI_BATTERY_RUN_BEEP_STANDARD_COUNT,
    UI_BATTERY_RUN_BEEP_DOUBLE_COUNT,
    UI_BATTERY_RUN_BEEP_TRIPLE_COUNT,
    UI_BATTERY_RUN_BEEP_GAP_MS,
    UI_BLINK_PERIOD_MS,
    UI_GREEN_MIN_OFF_MS,
    UI_CHARGING_BLINK_PERIOD_MS,
    UI_CHARGING_YELLOW_MIN_ON_MS,
    UI_INPUT_OVERVOLTAGE_THRESHOLD_MV,
    UI_INPUT_OVERVOLTAGE_HYSTERESIS_MV,
    0u,
    0u,
    UI_BAT_V_MIN_MV,
    UI_BAT_V_MAX_MV,
    0u,
    UI_CHARGING_FULL_ENTER_PERCENT,
    UI_CHARGING_FULL_EXIT_PERCENT,
    UI_CHARGING_PERCENT_HYSTERESIS_PERCENT,
    UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT,
    UI_BATTERY_ZERO_EXIT_THRESHOLD,
    UI_BATTERY_ONE_EXIT_THRESHOLD,
    /*
 * [EN] v1.49: the charge-side percent map (ids 119/120) ships with the
 * same factory numbers as the discharge map, so an untouched board
 * behaves exactly as it did before the split.
 */
    UI_BAT_V_MIN_MV,
    UI_BAT_V_MAX_MV,
    /*
 * [EN] v1.50/v1.71: band 2 boots with exactly what it used to borrow
 * from band 1 - its own per-beep duration and its own repeat interval -
 * so the split is inaudible until somebody shapes it on purpose.
 */
    UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS,
    UI_BATTERY_RUN_BEEP_DOUBLE_INTERVAL_MS,
    UI_TECH_FAULT_BEEP_PERIOD_MS,
    UI_TECH_FAULT_BEEP_DURATION_MS,
    UI_TECH_FAULT_BEEP_COUNT,
    UI_TECH_FAULT_BEEP_GAP_MS,
    UI_TECH_FAULT_LED_PERIOD_MS,
    UI_TECH_FAULT_LED_DUTY_PERCENT
};
/**
 * @brief  [EN] Clamp one value into an outer window.
 */
static uint32_t func__Ui_ClampWindow(uint32_t uint32_t__value,
                                     uint32_t uint32_t__low,
                                     uint32_t uint32_t__high)
{
    if (uint32_t__value < uint32_t__low)
    {
        return uint32_t__low;
    }
    if (uint32_t__value > uint32_t__high)
    {
        return uint32_t__high;
    }
    return uint32_t__value;
}
/* ==================== Ui_ClampPeriod ==================== */
/**
 * @brief  [EN] Clamp one beep period: 0 disables the pattern, otherwise
 *              1000..600000 (below 1000 the buzzer service would go
 *              INVALID and silent anyway).
 */
static uint32_t func__Ui_ClampPeriod(uint32_t uint32_t__value)
{
    if (uint32_t__value == 0u)
    {
        return 0u;
    }
    return func__Ui_ClampWindow(uint32_t__value, 1000u, 600000u);
}
/* ==================== Ui_ClampAlarms ==================== */
/**
 * @brief  [EN] Re-clamp the whole UI set, single pass, dependency order
 *              inside each group (period -> count -> gap -> duration, then
 *              the band/threshold orderings). A duration is left stale
 *              while its period is 0 (pattern off - the duty helper
 *              guards the divide-by-zero and the service stays silent).
 */
static void func__Ui_ClampAlarms(void)
{
    typedef struct
    {
        uint32_t uint32_t__hi;
        uint16_t uint16_t__lo;
        uint8_t  uint8_t__isPeriod;
        uint8_t  uint8_t__skip;
    } ui_clamp_rule_t;
    /*
 * [EN] v1.56 + Flash diet: per-field min/max rules indexed over the 55
 * contiguous words of ui_alarm_t (same indexing contract as Set/Get).
 * Cross-field rules remain solely in the panel.
 */
    static const ui_clamp_rule_t UI_CLAMP_RULE_T__A__Rules[] = {
        {  10000u,   100u, 0u, 0u },
        {    100u,     0u, 0u, 0u },
        { 600000u,  1000u, 1u, 0u },
        { 600000u,     0u, 0u, 0u },
        {     10u,     0u, 0u, 0u },
        {   5000u,     0u, 0u, 0u },
        { 120000u,     0u, 0u, 0u },
        {  10000u,     0u, 0u, 0u },
        {      0u,     0u, 0u, 1u },
        {   2000u,     0u, 0u, 0u },
        {      0u,     0u, 0u, 1u },
        {  25000u, 15000u, 0u, 0u },
        {  32000u, 25000u, 0u, 0u },
        {      1u,     0u, 0u, 0u },
        {    100u,     1u, 0u, 0u },
        {     50u,     0u, 0u, 0u }
    };
    static const uint8_t UINT8_T__A__ClampFieldRule[sizeof(ui_alarm_t) / sizeof(uint32_t)] = {
        0u, 1u, 2u, 3u, 4u, 5u,     /* ovLedPeriod..ovBeepGap (38..43) */
        0u, 1u, 2u, 3u, 4u, 5u,     /* blLedPeriod..blBeepGap (44..49) */
        1u, 1u, 1u, 1u,             /* runBeepStart..CritPct (50..53) */
        2u, 2u, 2u,                 /* runStd/Tri/CritInterval (54..56) */
        3u, 4u, 3u, 3u, 6u,         /* runCritBeepDur..runCritDur (57..61) */
        4u, 4u, 4u, 5u,             /* runStd/Double/TriCount, runGap (62..65) */
        0u, 7u, 0u, 7u,             /* green/yellow period & min (66..69) */
        8u, 9u,                     /* ovThreshMv, ovHystMv (70..71) */
        10u, 10u,                   /* retired low-bat slots (72..73) */
        11u, 12u,                   /* pctVminMv, pctVmaxMv (74..75) */
        13u,                        /* buzzerMute (76) */
        14u, 1u, 15u, 15u, 14u, 1u, /* chgFullEnter..runOneExit (77..82) */
        11u, 12u, 3u, 2u,           /* chgPctVmin..runDoubleInterval (119..122) */
        2u, 3u, 4u, 5u, 0u, 1u      /* techBeepPeriod..techLedDuty (137..142) */
    };
    _Static_assert((sizeof(ui_alarm_t) / sizeof(uint32_t)) == 55u,
                   "UINT8_T__A__ClampFieldRule must match ui_alarm_t word count");
    volatile uint32_t *uint32_t__words = &UI_ALARM_T__G__Alarm.uint32_t__ovLedPeriodMs;
    uint32_t uint32_t__idx;
    for (uint32_t__idx = 0u;
         uint32_t__idx < (uint32_t)(sizeof(ui_alarm_t) / sizeof(uint32_t));
         uint32_t__idx++)
    {
        const ui_clamp_rule_t *ui_clamp_rule_t__rule =
            &UI_CLAMP_RULE_T__A__Rules[UINT8_T__A__ClampFieldRule[uint32_t__idx]];
        if (ui_clamp_rule_t__rule->uint8_t__skip != 0u)
        {
            continue;
        }
        if (ui_clamp_rule_t__rule->uint8_t__isPeriod != 0u)
        {
            uint32_t__words[uint32_t__idx] =
                func__Ui_ClampPeriod(uint32_t__words[uint32_t__idx]);
        }
        else
        {
            uint32_t__words[uint32_t__idx] =
                func__Ui_ClampWindow(uint32_t__words[uint32_t__idx],
                                     (uint32_t)ui_clamp_rule_t__rule->uint16_t__lo,
                                     ui_clamp_rule_t__rule->uint32_t__hi);
        }
    }
}
/**
 * @brief  [EN] Duty percent of one beep pattern from its live window:
 *              ceil((dur*count + gap*(count-1)) * 100 / period). The ceil
 *              matches the legacy DUTY_PERCENT macros exactly on defaults
 *              (exact divisions are unaffected). A zero period yields 0
 *              (the caller passes period 0 = buzzer off). Overflow-safe:
 *              the clamped window never exceeds the period (<= 600000),
 *              x100 stays far below 2^32, and the result never exceeds
 *              100. Formula broken into steps (beeps, gaps+window, duty).
 */
static uint8_t func__Ui_BeepDutyPercent(uint32_t uint32_t__periodMs,
                                        uint32_t uint32_t__durMs,
                                        uint32_t uint32_t__count,
                                        uint32_t uint32_t__gapMs)
{
    uint32_t uint32_t__beepsTotalMs;
    uint32_t uint32_t__gapsTotalMs;
    uint32_t uint32_t__windowMs;
    uint32_t uint32_t__scaledWindow;
    if ((uint32_t__periodMs == 0u) || (uint32_t__durMs == 0u) ||
        (uint32_t__count == 0u))
    {
        func__Ui_Buzzer_SetDutyWindowMs(0u);
        return 0u;
    }
    /*
 * [EN] Step 1: beeps window = dur * count
 */
    uint32_t__beepsTotalMs = uint32_t__durMs * uint32_t__count;
    /*
 * [EN] Step 2: gaps window + full duty window
 */
    uint32_t__gapsTotalMs = 0u;
    if (uint32_t__count > 1u)
    {
        uint32_t__gapsTotalMs = uint32_t__gapMs * (uint32_t__count - 1u);
    }
    uint32_t__windowMs = uint32_t__beepsTotalMs + uint32_t__gapsTotalMs;
    /*
 * [EN] A window wider than the period is infeasible (gaps alone can
 * overflow it): report 101 so the service goes INVALID = deterministic
 * silence instead of a uint8-truncated phantom duty.
 */
    if (uint32_t__windowMs > uint32_t__periodMs)
    {
        func__Ui_Buzzer_SetDutyWindowMs(0u);
        return (uint8_t)(UI_BUZZER_DUTY_MAX_PERCENT + 1u);
    }
    /*
 * [EN] Step 3: queue the exact ms window and compute ceil(window * 100 / period)
 */
    func__Ui_Buzzer_SetDutyWindowMs(uint32_t__windowMs);
    uint32_t__scaledWindow = uint32_t__windowMs * UI_PERCENT_SCALE;
    return (uint8_t)(((uint32_t__scaledWindow - 1u) / uint32_t__periodMs) + 1u);
}
/**
 * @brief  [EN] Buzzer service with the panel-session mute: while id 76 is set,
 *              every SCENARIO pattern is replaced by an explicit off (the
 *              one-shot BoardTest wiring beep calls the service directly
 *              and still sounds, so a muted board still proves its buzzer
 *              works at boot).
 */
static int32_t func__Ui_Buzzer_Gated(uint32_t uint32_t__periodMs,
                                     uint8_t uint8_t__dutyPercent,
                                     uint8_t uint8_t__beepCount,
                                     uint32_t uint32_t__gapMs)
{
    if (UI_ALARM_T__G__Alarm.uint32_t__buzzerMute != 0u)
    {
        return func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
    }
    return func__Ui_Buzzer_Tick(uint32_t__periodMs,
                                uint8_t__dutyPercent,
                                uint8_t__beepCount,
                                uint32_t__gapMs);
}
/*
 * [EN] Layout contract for the indexed Set/GetAlarmParam below (flash diet
 * 2026-09-27): the wire ids MIN..MAX are dense and ui_alarm_t packs one
 * uint32_t per id in the SAME order (no padding possible between u32
 * words); the host test pins every wire id, these pins the struct side.
 */
_Static_assert(sizeof(ui_alarm_t) ==
                   (((UI_ALARM_PARAM_MAX_ID - UI_ALARM_PARAM_MIN_ID + 1u) +
                     (UI_ALARM_PARAM_EXT_MAX_ID - UI_ALARM_PARAM_EXT_MIN_ID + 1u) +
                     (UI_ALARM_PARAM_TECH_EXT_MAX_ID - UI_ALARM_PARAM_TECH_EXT_MIN_ID + 1u)) *
                    sizeof(uint32_t)),
               "ui_alarm_t must pack exactly one word per wire id (dense block + ext pair)");
_Static_assert(offsetof(ui_alarm_t, uint32_t__ovLedPeriodMs) == 0u,
               "first field must be the MIN-id word");
_Static_assert(offsetof(ui_alarm_t, uint32_t__runOneExit) ==
                   ((UI_ALARM_PARAM_MAX_ID - UI_ALARM_PARAM_MIN_ID) *
                    sizeof(uint32_t)),
               "last field must be the MAX-id word");
_Static_assert(offsetof(ui_alarm_t, uint32_t__chgPctVminMv) ==
                   ((UI_ALARM_PARAM_MAX_ID - UI_ALARM_PARAM_MIN_ID + 1u) *
                    sizeof(uint32_t)),
               "the ext pair must start right after the dense block");
/* ==================== Alarm Param Index ==================== */
/*
 * [EN] v1.49: wire id -> word index inside ui_alarm_t. Two ranges now: the
 * dense 38..82 block and the charge-side map 119/120 appended after it.
 * Returning the index in one place keeps Set and Get from ever disagreeing.
 */
#define UI_ALARM_PARAM_INDEX_INVALID   0xFFu
/* ==================== Ui_AlarmParamIndex ==================== */
static uint8_t func__Ui_AlarmParamIndex(uint8_t uint8_t__paramId)
{
    uint8_t uint8_t__denseCount;
    /*
 * [EN] v1.74: ids 72/73 are retired (the low-battery window moved into
 * Changeover as constants). They are rejected here, so a stale panel or
 * an old backup file cannot write into the reserved words.
 */
    if ((uint8_t__paramId >= UI_ALARM_PARAM_RETIRED_LOWBAT_FIRST) &&
        (uint8_t__paramId <= UI_ALARM_PARAM_RETIRED_LOWBAT_LAST))
    {
        return UI_ALARM_PARAM_INDEX_INVALID;
    }
    if ((uint8_t__paramId >= UI_ALARM_PARAM_MIN_ID) &&
        (uint8_t__paramId <= UI_ALARM_PARAM_MAX_ID))
    {
        return (uint8_t)(uint8_t__paramId - UI_ALARM_PARAM_MIN_ID);
    }
    if ((uint8_t__paramId >= UI_ALARM_PARAM_EXT_MIN_ID) &&
        (uint8_t__paramId <= UI_ALARM_PARAM_EXT_MAX_ID))
    {
        uint8_t__denseCount =
            (uint8_t)(UI_ALARM_PARAM_MAX_ID - UI_ALARM_PARAM_MIN_ID + 1u);
        return (uint8_t)(uint8_t__denseCount +
                         (uint8_t__paramId - UI_ALARM_PARAM_EXT_MIN_ID));
    }
    if ((uint8_t__paramId >= UI_ALARM_PARAM_TECH_EXT_MIN_ID) &&
        (uint8_t__paramId <= UI_ALARM_PARAM_TECH_EXT_MAX_ID))
    {
        uint8_t__denseCount =
            (uint8_t)(UI_ALARM_PARAM_MAX_ID - UI_ALARM_PARAM_MIN_ID + 1u);
        return (uint8_t)(uint8_t__denseCount +
                         (UI_ALARM_PARAM_EXT_MAX_ID - UI_ALARM_PARAM_EXT_MIN_ID + 1u) +
                         (uint8_t__paramId - UI_ALARM_PARAM_TECH_EXT_MIN_ID));
    }
    return UI_ALARM_PARAM_INDEX_INVALID;
}
bool func__Ui_SetAlarmParam(uint8_t uint8_t__paramId,
                            uint32_t uint32_t__value,
                            uint32_t *uint32_t__appliedValue)
{
    int32_t int32_t__savedKernelLock;
    uint8_t uint8_t__wordIndex;
    if (uint32_t__appliedValue == NULL)
    {
        /*
 * [EN] The API promises the applied value on success; reject a write
 * that cannot report that value rather than applying it silently.
 */
        return false;
    }
    /*
 * [EN] Writer-side scheduler lock (v1.16 audit C11): strictly redundant
 * today (the UI reader runs BELOW the comm writer, so no preemption),
 * but it closes the 46-field set against future priority moves for a
 * few microseconds. Pre-kernel the plain path runs (NVM replay).
 */
    /*
 * [EN] v1.104: id 70 is a read-only mirror of the shared q34 upper input
 * limit. Writes are rejected so the threshold cannot diverge from q34.
 */
    if (uint8_t__paramId == UI_ALARM_PARAM_OV_THRESH_MV)
    {
        return false;
    }
    int32_t__savedKernelLock = osKernelLock();
    /*
 * [EN] Indexed store (flash diet 2026-09-27: the 39-case switch cost
 * 1.4 KiB the F103C8 no longer has). Wire ids are dense MIN..MAX and
 * the struct packs the same fields in the same order (layout asserts
 * above, wire-id density pinned by the host test), so id-MIN indexes
 * the target word directly: identical store, identical clamp call,
 * identical lock/unlock paths.
 */
    uint8_t__wordIndex = func__Ui_AlarmParamIndex(uint8_t__paramId);
    if (uint8_t__wordIndex == UI_ALARM_PARAM_INDEX_INVALID)
    {
        if (int32_t__savedKernelLock >= 0)
        {
            (void)osKernelRestoreLock(int32_t__savedKernelLock);
        }
        return false;
    }
    ((volatile uint32_t *)&UI_ALARM_T__G__Alarm.uint32_t__ovLedPeriodMs)
        [uint8_t__wordIndex] = uint32_t__value;
    func__Ui_ClampAlarms();
    if (int32_t__savedKernelLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedKernelLock);
    }
    return func__Ui_GetAlarmParam(uint8_t__paramId, uint32_t__appliedValue);
}
bool func__Ui_GetAlarmParam(uint8_t uint8_t__paramId,
                            uint32_t *uint32_t__value)
{
    uint8_t uint8_t__wordIndex;
    if (uint32_t__value == NULL)
    {
        return false;
    }
    /*
 * [EN] v1.104: id 70 mirrors the live shared upper input limit (q34).
 */
    if (uint8_t__paramId == UI_ALARM_PARAM_OV_THRESH_MV)
    {
#if MODULE_FAULT
        uint32_t uint32_t__minMv = 0u;
        uint32_t uint32_t__maxMv = 0u;
        if (func__Fault_GetInputWindow(&uint32_t__minMv, &uint32_t__maxMv) == true)
        {
            *uint32_t__value = uint32_t__maxMv;
        }
        else
        {
            *uint32_t__value = UI_INPUT_WINDOW_MAX_MV;
        }
#else
        *uint32_t__value = UI_INPUT_WINDOW_MAX_MV;
#endif
        return true;
    }
    /*
 * [EN] Indexed read: same dense-id/struct contract as the setter.
 */
    uint8_t__wordIndex = func__Ui_AlarmParamIndex(uint8_t__paramId);
    if (uint8_t__wordIndex == UI_ALARM_PARAM_INDEX_INVALID)
    {
        return false;
    }
    *uint32_t__value =
        ((volatile uint32_t *)&UI_ALARM_T__G__Alarm.uint32_t__ovLedPeriodMs)
        [uint8_t__wordIndex];
    return true;
}
/* ==================== Scenario Alarm Switches ==================== */
/*
 * [EN] One flag per scenario 1..7 (index 0 = scenario 1), all ON at boot. The
 *      panel's old "enabled" tick never left the browser; this array is the
 *      board side of it. ONLY the LED/beep face reads it - detection,
 *      latching and every output gate (Charger/Imbalance/Changeover) keep
 *      working untouched, which is why silencing a face can never silence a
 *      protection.
 */
static bool BOOL__G__ScenarioAlarmOn[7] =
{
    true, true, true, true, true, true, true
};
bool func__Ui_SetScenarioAlarm(uint8_t uint8_t__paramId,
                               uint32_t uint32_t__value,
                               uint32_t *uint32_t__appliedValue)
{
    uint8_t uint8_t__index;
    if ((uint32_t__appliedValue == NULL) ||
        (uint8_t__paramId < UI_SCENARIO_ALARM_ID_FIRST) ||
        (uint8_t__paramId > UI_SCENARIO_ALARM_ID_LAST))
    {
        return false;
    }
    uint8_t__index = (uint8_t)(uint8_t__paramId - UI_SCENARIO_ALARM_ID_FIRST);
    BOOL__G__ScenarioAlarmOn[uint8_t__index] = (uint32_t__value != 0u);
    *uint32_t__appliedValue =
        (BOOL__G__ScenarioAlarmOn[uint8_t__index] == true) ? 1u : 0u;
    return true;
}
bool func__Ui_GetScenarioAlarm(uint8_t uint8_t__paramId,
                               uint32_t *uint32_t__value)
{
    uint8_t uint8_t__index;
    if ((uint32_t__value == NULL) ||
        (uint8_t__paramId < UI_SCENARIO_ALARM_ID_FIRST) ||
        (uint8_t__paramId > UI_SCENARIO_ALARM_ID_LAST))
    {
        return false;
    }
    uint8_t__index = (uint8_t)(uint8_t__paramId - UI_SCENARIO_ALARM_ID_FIRST);
    *uint32_t__value = (BOOL__G__ScenarioAlarmOn[uint8_t__index] == true) ? 1u : 0u;
    return true;
}
bool func__Ui_ScenarioAlarmOn(uint8_t uint8_t__scenario)
{
    if ((uint8_t__scenario < 1u) || (uint8_t__scenario > 7u))
    {
        return true;
    }
    return BOOL__G__ScenarioAlarmOn[uint8_t__scenario - 1u];
}
/* ==================== Battery Voltage To Percent ==================== */
/**
 * @brief  [EN] Battery voltage to percent 0..100. Non-linear formula broken into 4 steps: range, offset, scaled, percent.
* @param uint32_t__batteryMv Battery voltage in mV, 0..40000mV, 21000=0% 29000=100% /
* @return uint8_t Percent 0..100 /
 */
/**
 * @brief  [EN] One voltage-to-percent map, used by both pairs. Non-linear
 *              formula broken into 4 steps: range, offset, scaled, percent.
 *              v1.49: the body used to read the 74/75 globals directly; the
 *              two maps (discharge 74/75, charge 119/120) now pass their own
 *              numbers in, so the maths exists exactly once.
* @param uint32_t__batteryMv Battery voltage in mV /
* @param uint32_t__vminMv Voltage of 0% /
* @param uint32_t__vmaxMv Voltage of 100% /
* @return uint8_t Percent 0..100 /
 */
static uint8_t func__Ui_VoltageToPercentMap(uint32_t uint32_t__batteryMv,
                                            uint32_t uint32_t__vminMv,
                                            uint32_t uint32_t__vmaxMv)
{
    uint32_t uint32_t__voltageRangeMv;
    uint32_t uint32_t__voltageOffsetMv;
    uint32_t uint32_t__scaledOffset;
    uint8_t uint8_t__batteryPercent;
    if (uint32_t__batteryMv <= uint32_t__vminMv)
    {
        return 0u;
    }
    if (uint32_t__batteryMv >= uint32_t__vmaxMv)
    {
        return UI_PERCENT_FULL;
    }
    /*
 * [EN] Step 1: range = Vmax - Vmin
 */
    uint32_t__voltageRangeMv = uint32_t__vmaxMv - uint32_t__vminMv;
    /*
 * [EN] Step 2: offset = Vbat - Vmin
 */
    uint32_t__voltageOffsetMv = uint32_t__batteryMv - uint32_t__vminMv;
    if (uint32_t__voltageRangeMv == 0u)
    {
        return 0u;
    }
    /*
 * [EN] Step 3: scaled = offset * 100
 */
    uint32_t__scaledOffset = uint32_t__voltageOffsetMv * UI_PERCENT_SCALE;
    /*
 * [EN] Step 4: percent = scaled / range
 */
    uint8_t__batteryPercent = (uint8_t)(uint32_t__scaledOffset / uint32_t__voltageRangeMv);
    if (uint8_t__batteryPercent > UI_PERCENT_FULL)
    {
        uint8_t__batteryPercent = UI_PERCENT_FULL;
    }
    return uint8_t__batteryPercent;
}
/* ==================== Ui_BatteryVoltageToPercent ==================== */
/**
 * @brief  [EN] DISCHARGE voltage to percent, from the runtime 74/75 map.
 *              Since v1.49 this pair belongs to the discharge side only.
* @param uint32_t__batteryMv Battery voltage in mV /
* @return uint8_t Percent 0..100 /
 */
uint8_t func__Ui_BatteryVoltageToPercent(uint32_t uint32_t__batteryMv)
{
    return func__Ui_VoltageToPercentMap(uint32_t__batteryMv,
                                        UI_ALARM_T__G__Alarm.uint32_t__pctVminMv,
                                        UI_ALARM_T__G__Alarm.uint32_t__pctVmaxMv);
}
/* ==================== Ui_ChargeVoltageToPercent ==================== */
/**
 * @brief  [EN] CHARGE voltage to percent, from the runtime 119/120 map
 *              (v1.49 user order: the charge side must not move when the
 *              discharge limits move).
* @param uint32_t__batteryMv Battery voltage in mV /
* @return uint8_t Percent 0..100 /
 */
uint8_t func__Ui_ChargeVoltageToPercent(uint32_t uint32_t__batteryMv)
{
    return func__Ui_VoltageToPercentMap(uint32_t__batteryMv,
                                        UI_ALARM_T__G__Alarm.uint32_t__chgPctVminMv,
                                        UI_ALARM_T__G__Alarm.uint32_t__chgPctVmaxMv);
}
/* ==================== Green LED / LED ==================== */
/**
 * @brief  [EN] Drive green LED on/off. Low-level wrapper around BSP GPIO.
* @param bool__greenOn true=on, false=off /
 */
static void func__green(bool bool__greenOn)
{
    func__BspGpio_Write(BSP_GPIO_LED_GREEN, bool__greenOn);
}
/* ==================== Red LED / LED ==================== */
/**
 * @brief  [EN] Drive red LED on/off. Low-level.
* @param bool__redOn true=on, false=off /
 */
static void func__red(bool bool__redOn)
{
    func__BspGpio_Write(BSP_GPIO_LED_RED, bool__redOn);
}
/* ==================== Yellow LED / LED ==================== */
/**
 * @brief  [EN] Drive yellow LED on/off. Low-level.
* @param bool__yellowOn true=on, false=off /
 */
static void func__yellow(bool bool__yellowOn)
{
    func__BspGpio_Write(BSP_GPIO_LED_YELLOW, bool__yellowOn);
}
/* ==================== All Off Safe ==================== */
/**
 * @brief  [EN] Drive all LEDs off and request the buzzer service to enter its safe-off state.
 */
static void func__all_off(void)
{
    func__green(false);
    func__red(false);
    func__yellow(false);
    (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
}
/* ==================== Input connection state ==================== */
/**
 * @brief  [EN] Stateful healthy-input result used by the shared q33/q34 window.
 *         The lower edge keeps a 1 V release hysteresis; the upper edge releases
 *         immediately. It starts disconnected as the safe default.
 */
static bool BOOL__G__UiInputPresent = false;
/* ==================== Input overvoltage state ==================== */
/**
 * @brief  [EN] Stateful input overvoltage error flag.
 *         The flag remains active between 27V and 28V after an overvoltage event.
 */
static bool BOOL__G__UiInputOverVoltage = false;
/**
 * @brief  [EN] CMSIS-RTOS2 tick at which the current input overvoltage display started.
 */
static uint32_t UINT32_T__G__UiInputOverVoltageStartTick = 0;
/* ==================== BatteryRun critical beep state / BatteryRun ==================== */
/**
 * @brief  [EN] TRUE while the one-time critical BatteryRun beep is active.
 */
static bool BOOL__G__UiBatteryCriticalBeepActive = false;
/**
 * @brief  [EN] TRUE after the one-time critical BatteryRun beep has completed.
 */
static bool BOOL__G__UiBatteryCriticalBeepCompleted = false;
/**
 * @brief  [EN] RTOS tick at which the critical BatteryRun beep started.
 */
static uint32_t UINT32_T__G__UiBatteryCriticalBeepStartTick = 0;
/* ==================== BatteryRun green blink state / BatteryRun ==================== */
/**
 * @brief  [EN] Current green LED phase in the non-blocking BatteryRun blink.
 */
static bool BOOL__G__UiBatteryGreenOn = false;
/**
 * @brief  [EN] TRUE after the BatteryRun green blink phase has been initialized.
 */
static bool BOOL__G__UiBatteryGreenBlinkInitialized = false;
/**
 * @brief  [EN] RTOS tick at which the current BatteryRun green phase started.
 */
static uint32_t UINT32_T__G__UiBatteryGreenPhaseStartTick = 0;
/**
 * @brief  [EN] Stored BatteryRun green ON duration used to restart phase timing when percentage changes.
 */
static uint32_t UINT32_T__G__UiBatteryGreenOnMs = 0u;
/**
 * @brief  [EN] Stored BatteryRun green OFF duration used to restart phase timing when percentage changes.
 */
static uint32_t UINT32_T__G__UiBatteryGreenOffMs = 0u;
/* ==================== Battery percent hysteresis state ==================== */
/**
 * @brief  [EN] Stable battery percent after hysteresis (0..100). Used for BatteryRun blink timing and buzzer bands.
 *         Raw percent is from func__Ui_BatteryVoltageToPercent; stable changes only with 2% hysteresis and special 0/1 handling.
 */
static uint8_t UINT8_T__G__UiBatteryStablePercent = 0u;
/**
 * @brief  [EN] TRUE after stable percent has been initialized from the first valid raw percent.
 */
static bool BOOL__G__UiBatteryStableInitialized = false;
/* ==================== BatteryRun stable percent detailed / BatteryRun ==================== */
/**
 * @brief  [EN] Alias stable percent for BatteryRun (raw/hysteresis 2% + 0/1).
 */
/* ==================== Charging stable percent ==================== */
/**
 * @brief  [EN] Stable battery percent for Charging yellow timing (hysteresis 5%).
 *         Raw only updates this when |raw-stable|>=5.
 */
static uint8_t UINT8_T__G__UiChargingStablePercent = 0u;
/**
 * @brief  [EN] TRUE after Charging stable percent initialized.
 */
static bool BOOL__G__UiChargingStableInitialized = false;
/* ==================== Charging full hysteresis state ==================== */
/**
 * @brief  [EN] TRUE while Charging has entered full (InputOk) and stays until raw<95.
 */
static bool BOOL__G__UiChargingFullActive = false;
/* ==================== Charging yellow blink state ==================== */
/**
 * @brief  [EN] Current yellow LED phase in the non-blocking Charging blink.
 */
static bool BOOL__G__UiChargingYellowOn = false;
/**
 * @brief  [EN] TRUE after the Charging yellow blink phase has been initialized.
 */
static bool BOOL__G__UiChargingYellowBlinkInitialized = false;
/**
 * @brief  [EN] RTOS tick at which the current Charging yellow phase started.
 */
static uint32_t UINT32_T__G__UiChargingYellowPhaseStartTick = 0u;
/**
 * @brief  [EN] Stored Charging yellow ON duration.
 */
static uint32_t UINT32_T__G__UiChargingYellowOnMs = 0u;
/**
 * @brief  [EN] Stored Charging yellow OFF duration.
 */
static uint32_t UINT32_T__G__UiChargingYellowOffMs = 0u;
/* ==================== BatteryRun critical beep reset / BatteryRun ==================== */
/**
 * @brief  [EN] Reset the one-time critical BatteryRun beep state.
 */
static void func__Ui_ResetBatteryCriticalBeep(void)
{
    BOOL__G__UiBatteryCriticalBeepActive = false;
    BOOL__G__UiBatteryCriticalBeepCompleted = false;
    UINT32_T__G__UiBatteryCriticalBeepStartTick = 0;
}
/* ==================== BatteryRun green blink reset / BatteryRun ==================== */
/**
 * @brief  [EN] Reset non-blocking BatteryRun green blink timing.
 */
static void func__Ui_ResetBatteryRunGreenBlink(void)
{
    BOOL__G__UiBatteryGreenOn = false;
    BOOL__G__UiBatteryGreenBlinkInitialized = false;
    UINT32_T__G__UiBatteryGreenPhaseStartTick = 0;
    UINT32_T__G__UiBatteryGreenOnMs = 0u;
    UINT32_T__G__UiBatteryGreenOffMs = 0u;
}
/* ==================== Battery stable percent helpers ==================== */
/**
 * @brief  [EN] Reset the BatteryRun stable percent hysteresis state. Called on UI init, invalid snapshot, or explicit reset.
 */
static void func__Ui_ResetBatteryStablePercent(void)
{
    UINT8_T__G__UiBatteryStablePercent = 0u;
    BOOL__G__UiBatteryStableInitialized = false;
}
/* ==================== Ui_UpdateBatteryStablePercent ==================== */
/**
 * @brief  [EN] Update stable battery percent from raw percent with 2% hysteresis and special 0/1 handling.
 *         General: |raw-stable| <2 → keep stable; >=2 → stable = raw. Special: stable 0 stays 0 until raw>=2 then →1; stable 1: raw==0→0, raw>=3→2, else keep 1.
* @param uint8_t__rawPercent Raw percent 0..100 /
* @return uint8_t Stable percent after hysteresis /
 */
static uint8_t func__Ui_UpdateBatteryStablePercent(uint8_t uint8_t__rawPercent)
{
    uint8_t uint8_t__stablePercent;
    uint8_t uint8_t__diffPercent;
    if (BOOL__G__UiBatteryStableInitialized == false)
    {
        UINT8_T__G__UiBatteryStablePercent = uint8_t__rawPercent;
        BOOL__G__UiBatteryStableInitialized = true;
        return UINT8_T__G__UiBatteryStablePercent;
    }
    uint8_t__stablePercent = UINT8_T__G__UiBatteryStablePercent;
    if (uint8_t__stablePercent == 0u)
    {
        if ((uint32_t)uint8_t__rawPercent >=
            UI_ALARM_T__G__Alarm.uint32_t__runZeroExit)
        {
            uint8_t__stablePercent = 1u;
        }
        else
        {
            /*
 * [EN] Keep 0 until raw reaches 2.
 */
        }
    }
    else if (uint8_t__stablePercent == 1u)
    {
        if (uint8_t__rawPercent == 0u)
        {
            uint8_t__stablePercent = 0u;
        }
        else if ((uint32_t)uint8_t__rawPercent >=
                 UI_ALARM_T__G__Alarm.uint32_t__runOneExit)
        {
            uint8_t__stablePercent = 2u;
        }
        else
        {
            /*
 * [EN] Keep 1 for raw 1..2.
 */
        }
    }
    else
    {
        if (uint8_t__rawPercent > uint8_t__stablePercent)
        {
            uint8_t__diffPercent = uint8_t__rawPercent - uint8_t__stablePercent;
        }
        else
        {
            uint8_t__diffPercent = uint8_t__stablePercent - uint8_t__rawPercent;
        }
        if ((uint32_t)uint8_t__diffPercent >=
            UI_ALARM_T__G__Alarm.uint32_t__runHystPct)
        {
            uint8_t__stablePercent = uint8_t__rawPercent;
        }
        else
        {
            /*
 * [EN] Jitter <2% keeps BatteryRun stable, e.g., 56↔57, 57↔58 preserves 57.
 */
        }
    }
    UINT8_T__G__UiBatteryStablePercent = uint8_t__stablePercent;
    return uint8_t__stablePercent;
}
/* ==================== Ui_ResetChargingStablePercent ==================== */
/**
 * @brief  [EN] Reset Charging stable percent (5% hysteresis).
 */
static void func__Ui_ResetChargingStablePercent(void)
{
    UINT8_T__G__UiChargingStablePercent = 0u;
    BOOL__G__UiChargingStableInitialized = false;
}
/* ==================== Ui_UpdateChargingStablePercent ==================== */
/**
 * @brief  [EN] Update Charging stable percent with 5% hysteresis. |raw-stable| <5 keeps stable, >=5 updates.
 * @param  uint8_t__rawPercent [EN] Raw percent 0..100
 * @return uint8_t stable
 */
static uint8_t func__Ui_UpdateChargingStablePercent(uint8_t uint8_t__rawPercent)
{
    uint8_t uint8_t__stablePercent;
    uint8_t uint8_t__diffPercent;
    if (BOOL__G__UiChargingStableInitialized == false)
    {
        UINT8_T__G__UiChargingStablePercent = uint8_t__rawPercent;
        BOOL__G__UiChargingStableInitialized = true;
        return UINT8_T__G__UiChargingStablePercent;
    }
    uint8_t__stablePercent = UINT8_T__G__UiChargingStablePercent;
    if (uint8_t__rawPercent > uint8_t__stablePercent)
    {
        uint8_t__diffPercent = uint8_t__rawPercent - uint8_t__stablePercent;
    }
    else
    {
        uint8_t__diffPercent = uint8_t__stablePercent - uint8_t__rawPercent;
    }
    if ((uint32_t)uint8_t__diffPercent >=
        UI_ALARM_T__G__Alarm.uint32_t__chgHystPct)
    {
        uint8_t__stablePercent = uint8_t__rawPercent;
    }
    else
    {
        /*
 * [EN] Jitter <5% keeps charging stable, e.g., 57 with 53..61 stays 57.
 */
    }
    UINT8_T__G__UiChargingStablePercent = uint8_t__stablePercent;
    return uint8_t__stablePercent;
}
/* ==================== Ui_ResetChargingFullHysteresis ==================== */
/**
 * @brief  [EN] Reset Charging full (InputOk) hysteresis state.
 */
static void func__Ui_ResetChargingFullHysteresis(void)
{
    BOOL__G__UiChargingFullActive = false;
}
/* ==================== Ui_UpdateChargingFullHysteresis ==================== */
/**
 * @brief  [EN] Update Charging full hysteresis. Enter InputOk only at raw 100, stay until raw<95.
 * @param  uint8_t__rawPercent [EN] Raw percent
 * @return bool true = InputOk (full), false = Charging
 */
static bool func__Ui_UpdateChargingFullHysteresis(uint8_t uint8_t__rawPercent)
{
    if (BOOL__G__UiChargingFullActive == true)
    {
        if ((uint32_t)uint8_t__rawPercent <
            UI_ALARM_T__G__Alarm.uint32_t__chgFullExitPct)
        {
            BOOL__G__UiChargingFullActive = false;
        }
        else
        {
            /*
 * [EN] Keep InputOk until <95.
 */
        }
    }
    else
    {
        if ((uint32_t)uint8_t__rawPercent >=
            UI_ALARM_T__G__Alarm.uint32_t__chgFullEnterPct)
        {
            BOOL__G__UiChargingFullActive = true;
        }
        else
        {
            /*
 * [EN] Stay Charging until 100.
 */
        }
    }
    return BOOL__G__UiChargingFullActive;
}
/* ==================== Charging yellow blink helpers ==================== */
/**
 * @brief  [EN] Reset Charging yellow blink phase.
 */
static void func__Ui_ResetChargingYellowBlink(void)
{
    BOOL__G__UiChargingYellowOn = false;
    BOOL__G__UiChargingYellowBlinkInitialized = false;
    UINT32_T__G__UiChargingYellowPhaseStartTick = 0u;
    UINT32_T__G__UiChargingYellowOnMs = 0u;
    UINT32_T__G__UiChargingYellowOffMs = 0u;
}
/* ==================== Ui_ResetForeignScenarioState ==================== */
/**
 * @brief  [EN] Reset non-blocking BatteryRun/Charging blink and critical beep
 *         phases together via one non-inlined helper so 14 scenario entry/exit
 *         paths share a single code copy instead of duplicating inline stores.
 */
static __attribute__((noinline)) void func__Ui_ResetForeignScenarioState(void)
{
    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();
    func__Ui_ResetChargingYellowBlink();
}
/* ==================== Ui_BlinkOnFromTick ==================== */
/**
 * @brief  [EN] Compute absolute-tick blink state: first duty% of each period is
 *         ON; period 0 returns true (documented solid-lamp opt-out).
* @param uint32_t__startTick Phase anchor tick (0 for absolute tick) /
* @param uint32_t__periodMs Blink period in ms (0 = solid ON) /
* @param uint32_t__dutyPct ON duty percentage 0..100 /
* @return true when the lamp should be ON / true
 */
static __attribute__((noinline)) bool func__Ui_BlinkOnFromTick(uint32_t uint32_t__startTick,
                                                               uint32_t uint32_t__periodMs,
                                                               uint32_t uint32_t__dutyPct)
{
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__phaseMs;
    uint32_t uint32_t__dutyProduct;
    uint32_t uint32_t__onMs;
    if (uint32_t__periodMs == 0u)
    {
        return true;
    }
    uint32_t__elapsedMs =
        func__Rtos_TicksToMilliseconds(osKernelGetTickCount() - uint32_t__startTick);
    uint32_t__phaseMs = uint32_t__elapsedMs % uint32_t__periodMs;
    uint32_t__dutyProduct = uint32_t__periodMs * uint32_t__dutyPct;
    uint32_t__onMs = uint32_t__dutyProduct / UI_PERCENT_SCALE;
    return (uint32_t__phaseMs < uint32_t__onMs);
}
/* ==================== Ui_UpdateChargingYellowBlink ==================== */
/**
 * @brief  [EN] Update non-blocking Charging yellow blink. Preserves phase on duration change.
* @param uint32_t__yellowOnMs Yellow ON duration /
* @param uint32_t__yellowOffMs Yellow OFF duration /
 */
static void func__Ui_UpdateChargingYellowBlink(uint32_t uint32_t__yellowOnMs, uint32_t uint32_t__yellowOffMs)
{
    uint32_t uint32_t__nowTick;
    uint32_t uint32_t__currentPhaseMs;
    uint32_t__nowTick = osKernelGetTickCount();
    if (BOOL__G__UiChargingYellowBlinkInitialized == false)
    {
        BOOL__G__UiChargingYellowBlinkInitialized = true;
        BOOL__G__UiChargingYellowOn = true;
        UINT32_T__G__UiChargingYellowPhaseStartTick = uint32_t__nowTick;
        UINT32_T__G__UiChargingYellowOnMs = uint32_t__yellowOnMs;
        UINT32_T__G__UiChargingYellowOffMs = uint32_t__yellowOffMs;
    }
    else if ((UINT32_T__G__UiChargingYellowOnMs != uint32_t__yellowOnMs) ||
             (UINT32_T__G__UiChargingYellowOffMs != uint32_t__yellowOffMs))
    {
        /*
 * [EN] Duration changed: keep current phase and start tick, only update stored durations.
 * New timing takes effect from next phase boundary.
 */
        UINT32_T__G__UiChargingYellowOnMs = uint32_t__yellowOnMs;
        UINT32_T__G__UiChargingYellowOffMs = uint32_t__yellowOffMs;
    }
    else
    {
        uint32_t__currentPhaseMs = func__Rtos_TicksToMilliseconds(uint32_t__nowTick - UINT32_T__G__UiChargingYellowPhaseStartTick);
        if ((BOOL__G__UiChargingYellowOn == true) &&
            (uint32_t__currentPhaseMs >= uint32_t__yellowOnMs))
        {
            BOOL__G__UiChargingYellowOn = false;
            UINT32_T__G__UiChargingYellowPhaseStartTick = uint32_t__nowTick;
        }
        else if ((BOOL__G__UiChargingYellowOn == false) &&
                 (uint32_t__currentPhaseMs >= uint32_t__yellowOffMs))
        {
            BOOL__G__UiChargingYellowOn = true;
            UINT32_T__G__UiChargingYellowPhaseStartTick = uint32_t__nowTick;
        }
        else
        {
            /*
 * [EN] Keep current yellow phase until duration expires.
 */
        }
    }
    func__yellow(BOOL__G__UiChargingYellowOn);
}
/* ==================== BatteryRun green blink update / BatteryRun ==================== */
/**
 * @brief  [EN] Update the non-blocking BatteryRun green blink and service its phase timing.
* @param uint32_t__greenOnMs Green ON duration /
* @param uint32_t__greenOffMs Green OFF duration /
 */
static void func__Ui_UpdateBatteryRunGreenBlink(uint32_t uint32_t__greenOnMs, uint32_t uint32_t__greenOffMs)
{
    uint32_t uint32_t__nowTick;
    uint32_t uint32_t__currentPhaseMs;
    uint32_t__nowTick = osKernelGetTickCount();
    if (BOOL__G__UiBatteryGreenBlinkInitialized == false)
    {
        BOOL__G__UiBatteryGreenBlinkInitialized = true;
        BOOL__G__UiBatteryGreenOn = true;
        UINT32_T__G__UiBatteryGreenPhaseStartTick = uint32_t__nowTick;
        UINT32_T__G__UiBatteryGreenOnMs = uint32_t__greenOnMs;
        UINT32_T__G__UiBatteryGreenOffMs = uint32_t__greenOffMs;
    }
    else if ((UINT32_T__G__UiBatteryGreenOnMs != uint32_t__greenOnMs) ||
             (UINT32_T__G__UiBatteryGreenOffMs != uint32_t__greenOffMs))
    {
        /*
 * [EN] Duration changed due to stable percent jitter: keep current phase and start tick, only update stored durations.
 * New timing applies from next phase boundary, so a 10ms jitter does not restart the green ON.
 */
        UINT32_T__G__UiBatteryGreenOnMs = uint32_t__greenOnMs;
        UINT32_T__G__UiBatteryGreenOffMs = uint32_t__greenOffMs;
    }
    else
    {
        uint32_t__currentPhaseMs = func__Rtos_TicksToMilliseconds(uint32_t__nowTick - UINT32_T__G__UiBatteryGreenPhaseStartTick);
        if ((BOOL__G__UiBatteryGreenOn == true) &&
            (uint32_t__currentPhaseMs >= uint32_t__greenOnMs))
        {
            BOOL__G__UiBatteryGreenOn = false;
            UINT32_T__G__UiBatteryGreenPhaseStartTick = uint32_t__nowTick;
        }
        else if ((BOOL__G__UiBatteryGreenOn == false) &&
                 (uint32_t__currentPhaseMs >= uint32_t__greenOffMs))
        {
            BOOL__G__UiBatteryGreenOn = true;
            UINT32_T__G__UiBatteryGreenPhaseStartTick = uint32_t__nowTick;
        }
        else
        {
            /*
 * [EN] Keep the current LED phase until its configured duration expires.
 */
        }
    }
    func__green(BOOL__G__UiBatteryGreenOn);
}
/* ==================== Input state update ==================== */
/**
 * @brief  [EN] Update input presence and input overvoltage state.
 *         Presence uses the shared healthy-input window q33/q34 (Fault):
 *         enter inside [q33,q34]; release below q33-1V or above q34 immediately.
 *         Scenario-1 OV threshold = live q34; only hysteresis q71 is adjustable.
* @param uint32_t__inputVoltageMv Input voltage in mV /
 */
static void func__Ui_UpdateInputState(uint32_t uint32_t__inputVoltageMv)
{
    /*
 * [EN] v1.104 (user order): the OV threshold IS the shared upper input
 * limit (q34). Only the hysteresis (q71) remains independently adjustable.
 * clear = q34 - q71; no underflow because q34 >= 24000 and q71 <= 2000.
 */
    uint32_t uint32_t__ovThreshMv;
#if MODULE_FAULT
    {
        uint32_t uint32_t__minMv = 0u;
        uint32_t uint32_t__maxMv = 0u;
        if (func__Fault_GetInputWindow(&uint32_t__minMv, &uint32_t__maxMv) == true)
        {
            uint32_t__ovThreshMv = uint32_t__maxMv;
        }
        else
        {
            uint32_t__ovThreshMv = UI_INPUT_WINDOW_MAX_MV;
        }
    }
#else
    uint32_t__ovThreshMv = UI_INPUT_WINDOW_MAX_MV;
#endif
    if (BOOL__G__UiInputOverVoltage == false)
    {
        if (uint32_t__inputVoltageMv > uint32_t__ovThreshMv)
        {
            BOOL__G__UiInputOverVoltage = true;
            UINT32_T__G__UiInputOverVoltageStartTick = osKernelGetTickCount();
        }
    }
    else if (uint32_t__inputVoltageMv <=
             (uint32_t__ovThreshMv -
              UI_ALARM_T__G__Alarm.uint32_t__ovHystMv))
    {
        BOOL__G__UiInputOverVoltage = false;
        (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
    }
    else
    {
        /*
 * [EN] Keep the OV alarm in its q34/q71 hysteresis band.
 */
    }
    /*
 * [EN] Normal presence faces mirror the one shared Fault Schmitt flag.
 * The OV alarm above now uses the same q34 as its threshold (v1.104).
 */
#if MODULE_FAULT
    {
        bool bool__permissionValid = false;
        bool bool__permitted = false;
        if ((func__Fault_GetInputPermission(&bool__permissionValid, &bool__permitted) == true) &&
            (bool__permissionValid != false))
        {
            BOOL__G__UiInputPresent = bool__permitted;
        }
        else
        {
            BOOL__G__UiInputPresent = false;
        }
    }
#else
    {
        static bool BOOL__S__InputArmed = false;
        uint32_t uint32_t__inputMinMv = UI_INPUT_CONNECTED_THRESHOLD_MV;
        uint32_t uint32_t__inputMaxMv = UI_INPUT_WINDOW_MAX_MV;
        uint32_t uint32_t__spanMv = uint32_t__inputMaxMv - uint32_t__inputMinMv;
        uint32_t uint32_t__marginMv = UI_INPUT_HYSTERESIS_MV;
        if (uint32_t__marginMv > (uint32_t__spanMv / 4u))
        {
            uint32_t__marginMv = uint32_t__spanMv / 4u;
        }
        if ((uint32_t__inputVoltageMv < uint32_t__inputMinMv) ||
            (uint32_t__inputVoltageMv > uint32_t__inputMaxMv))
        {
            BOOL__S__InputArmed = false;
        }
        else if (BOOL__S__InputArmed == false)
        {
            BOOL__S__InputArmed =
                ((uint32_t__inputVoltageMv >= (uint32_t__inputMinMv + uint32_t__marginMv)) &&
                 (uint32_t__inputVoltageMv <= (uint32_t__inputMaxMv - uint32_t__marginMv)));
        }
        else
        {
        }
        BOOL__G__UiInputPresent = BOOL__S__InputArmed;
    }
#endif
}
/* ==================== Scenario Input Overvoltage ==================== */
/**
 * @brief  [EN] Display input overvoltage: green steady, yellow off, red blink (38/39)
 *         and the periodic beep pattern (40..43). Defaults: red 50%, one 1-second
 *         pulse every 10 seconds. This tick is non-blocking.
 */
static void func__Ui_ScenarioInputOverVoltage_Tick(void)
{
    bool bool__redOn;
    func__Ui_ResetForeignScenarioState();
    bool__redOn = func__Ui_BlinkOnFromTick(UINT32_T__G__UiInputOverVoltageStartTick,
                                           UI_ALARM_T__G__Alarm.uint32_t__ovLedPeriodMs,
                                           UI_ALARM_T__G__Alarm.uint32_t__ovLedDutyPct);
    func__green(true);
    func__yellow(false);
    func__red(bool__redOn);
    (void)func__Ui_Buzzer_Gated(
        UI_ALARM_T__G__Alarm.uint32_t__ovBeepPeriodMs,
        func__Ui_BeepDutyPercent(UI_ALARM_T__G__Alarm.uint32_t__ovBeepPeriodMs,
                                 UI_ALARM_T__G__Alarm.uint32_t__ovBeepDurMs,
                                 UI_ALARM_T__G__Alarm.uint32_t__ovBeepCount,
                                 UI_ALARM_T__G__Alarm.uint32_t__ovBeepGapMs),
        (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__ovBeepCount,
        UI_ALARM_T__G__Alarm.uint32_t__ovBeepGapMs);
}
/* ==================== Scenario BatLost Tick ==================== */
/**
 * @brief  [EN] Battery-lost announcement: red fast blink (defaults 50% of a 1 s period,
 *         distinctly unlike the slow overvoltage pulse; ids 44/45), green steady since
 *         the input is present in both detection cases, and the periodic
 *         buzzer pattern (defaults: three short beeps plus a pause; ids 46..49). The pattern phase uses
 *         the absolute kernel tick so no state needs remembering here; the
 *         fault bit itself is owned (set and cleared) only by the Fault
 *         module. Must be called every Ui pass while active so the buzzer
 *         pattern advances.
 */
void func__Ui_ScenarioBatLost_Tick(void)
{
    bool bool__redOn;
    /*
 * [EN] Foreign blink/beep states belong to other scenarios; stop them so
 * nothing from BatteryRun/Charging bleeds into this pattern.
 */
    func__Ui_ResetForeignScenarioState();
    bool__redOn = func__Ui_BlinkOnFromTick(0u,
                                           UI_ALARM_T__G__Alarm.uint32_t__blLedPeriodMs,
                                           UI_ALARM_T__G__Alarm.uint32_t__blLedDutyPct);
    /*
 * [EN] Green reflects whether mains input is still present; if mains drops
 * while FAULT_CHARGER_BAT_LOST remains latched, turn green off so the face
 * never falsely indicates input power.
 */
    func__green(BOOL__G__UiInputPresent);
    func__yellow(false);
    func__red(bool__redOn);
    (void)func__Ui_Buzzer_Gated(
        UI_ALARM_T__G__Alarm.uint32_t__blBeepPeriodMs,
        func__Ui_BeepDutyPercent(UI_ALARM_T__G__Alarm.uint32_t__blBeepPeriodMs,
                                 UI_ALARM_T__G__Alarm.uint32_t__blBeepDurMs,
                                 UI_ALARM_T__G__Alarm.uint32_t__blBeepCount,
                                 UI_ALARM_T__G__Alarm.uint32_t__blBeepGapMs),
        (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__blBeepCount,
        UI_ALARM_T__G__Alarm.uint32_t__blBeepGapMs);
}
/* ==================== Scenario 7: technical board fault ==================== */
#if MODULE_FAULT
/* ==================== Ui_ScenarioTechnicalFault_Tick ==================== */
/**
 * @brief [EN] Technical board fault face: all three LEDs use one absolute
 *        phase and therefore blink together. The buzzer reads only scenario
 *        7's six live words; it never uses q132..q135.
 */
static void func__Ui_ScenarioTechnicalFault_Tick(void)
{
    bool bool__on;
    func__Ui_ResetForeignScenarioState();
    bool__on = func__Ui_BlinkOnFromTick(0u,
                                        UI_ALARM_T__G__Alarm.uint32_t__techLedPeriodMs,
                                        UI_ALARM_T__G__Alarm.uint32_t__techLedDutyPct);
    /*
 * [EN] One boolean drives all outputs: simultaneous LED blink is a
 */
    func__red(bool__on);
    func__green(bool__on);
    func__yellow(bool__on);
    (void)func__Ui_Buzzer_Gated(
        UI_ALARM_T__G__Alarm.uint32_t__techBeepPeriodMs,
        func__Ui_BeepDutyPercent(UI_ALARM_T__G__Alarm.uint32_t__techBeepPeriodMs,
                                 UI_ALARM_T__G__Alarm.uint32_t__techBeepDurMs,
                                 UI_ALARM_T__G__Alarm.uint32_t__techBeepCount,
                                 UI_ALARM_T__G__Alarm.uint32_t__techBeepGapMs),
        (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__techBeepCount,
        UI_ALARM_T__G__Alarm.uint32_t__techBeepGapMs);
}
#endif
/* ==================== Scenario Imbalance Latch ==================== */
#if MODULE_IMBALANCE
/* ==================== Ui_ScenarioImbalance_Tick ==================== */
/**
 * @brief  [EN] Imbalance-latched verdict (scenario 5, user order
 *         2026-10-04, cadence changed by user order 2026-10-05): RED
 *         BLINKING - the user asked for a blink, not the solid lamp the
 *         first version used, because a steady red reads as "a lamp that
 *         was left on" while a blink reads as "something is wrong now".
 *         Period and duty come from ids 123/124 (default 1000 ms, 50%), so
 *         it is still distinct from the BatLost blink (ids 44/45, which also
 *         keeps GREEN on) and from the overvoltage pulse. Green/yellow off,
 *         plus the periodic one-short-beep bore by
 *         the buzzer's absolute-tick pattern machinery: one beep of id 116
 *         (default 200 ms) repeating every id 115 (default 1 h), starting at
 *         the latch moment because the pattern anchors on the first call.
 *         Must be called every Ui pass while the latch holds.
 */
void func__Ui_ScenarioImbalance_Tick(void)
{
    uint32_t uint32_t__beepPeriodMs;
    uint32_t uint32_t__beepCount = IMBAL_DEF_LATCH_BEEP_COUNT;
    uint32_t uint32_t__beepGapMs = IMBAL_DEF_LATCH_BEEP_GAP_MS;
    uint32_t uint32_t__blinkPeriodMs = IMBAL_DEF_LATCH_BLINK_PERIOD_MS;
    uint32_t uint32_t__blinkDutyPct = IMBAL_DEF_LATCH_BLINK_DUTY_PCT;
    bool     bool__redOn;
    /*
 * [EN] Stop foreign blink/beep states first (same hygiene as BatLost).
 */
    func__Ui_ResetForeignScenarioState();
    /*
 * [EN] Blink the red lamp on the absolute tick, exactly like every other
 * scenario cadence: the first duty% of each period is ON. Period 0 is
 * the documented opt-out and means solid red.
 */
    func__Imbalance_GetParam(IMBAL_PARAM_LATCH_BLINK_PERIOD_MS, &uint32_t__blinkPeriodMs);
    func__Imbalance_GetParam(IMBAL_PARAM_LATCH_BLINK_DUTY_PCT, &uint32_t__blinkDutyPct);
    bool__redOn = func__Ui_BlinkOnFromTick(0u, uint32_t__blinkPeriodMs, uint32_t__blinkDutyPct);
    func__green(false);
    func__yellow(false);
    func__red(bool__redOn);
    func__Imbalance_GetParam(IMBAL_PARAM_LATCH_BEEP_PERIOD_MS, &uint32_t__beepPeriodMs);
    if (uint32_t__beepPeriodMs != 0u)
    {
        uint32_t uint32_t__beepLenMs = IMBAL_DEF_BEEP_LEN_MS;
        (void)func__Imbalance_GetParam(IMBAL_PARAM_LATCH_BEEP_LEN_MS, &uint32_t__beepLenMs);
        (void)func__Imbalance_GetParam(IMBAL_PARAM_LATCH_BEEP_COUNT, &uint32_t__beepCount);
        (void)func__Imbalance_GetParam(IMBAL_PARAM_LATCH_BEEP_GAP_MS, &uint32_t__beepGapMs);
        (void)func__Ui_Buzzer_Gated(
            uint32_t__beepPeriodMs,
            func__Ui_BeepDutyPercent(uint32_t__beepPeriodMs, uint32_t__beepLenMs,
                                     (uint8_t)uint32_t__beepCount, uint32_t__beepGapMs),
            (uint8_t)uint32_t__beepCount,
            uint32_t__beepGapMs);
    }
    else
    {
        (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
    }
}
#endif
/* ==================== Scenario DeadBattery ==================== */
#if MODULE_CHARGER
/* ==================== Ui_ScenarioDeadBattery_Tick ==================== */
/**
 * @brief  [EN] v1.72 scenario 6 face (user order: "if it has not charged
 *              after 24 continuous hours we must raise an error that the
 *              battery is faulty, with a red LED, and charge it no more
 *              until the battery is replaced"). SOLID red - unlike the
 *              imbalance latch this verdict is deliberately not blinking,
 *              so the two condemned-battery faces stay distinguishable on
 *              the same single red lamp. Its beep shape is independently
 *              configurable through the scenario-6 parameters.
 */
void func__Ui_ScenarioDeadBattery_Tick(void)
{
    uint32_t uint32_t__beepPeriodMs  = 0u;
    uint32_t uint32_t__beepLenMs     = 0u;
    uint32_t uint32_t__beepCount     = CHG_DEAD_DEF_BEEP_COUNT;
    uint32_t uint32_t__beepGapMs     = CHG_DEAD_DEF_BEEP_GAP_MS;
    uint32_t uint32_t__blinkPeriodMs = 0u;
    uint32_t uint32_t__blinkDutyPct  = 0u;
    func__Ui_ResetForeignScenarioState();
    /*
 * [EN] Scenario 6 uses its own six shape parameters; period 0 keeps the
 * historical solid-red option.
 */
    func__green(false);
    func__yellow(false);
    func__Charger_DeadFaceShape(&uint32_t__beepPeriodMs,
                                &uint32_t__beepLenMs,
                                &uint32_t__blinkPeriodMs,
                                &uint32_t__blinkDutyPct);
    func__Charger_DeadBeepPattern(&uint32_t__beepCount, &uint32_t__beepGapMs);
    func__red(func__Ui_BlinkOnFromTick(0u, uint32_t__blinkPeriodMs, uint32_t__blinkDutyPct));
    if (uint32_t__beepPeriodMs != 0u)
    {
        (void)func__Ui_Buzzer_Gated(
            uint32_t__beepPeriodMs,
            func__Ui_BeepDutyPercent(uint32_t__beepPeriodMs, uint32_t__beepLenMs,
                                     (uint8_t)uint32_t__beepCount, uint32_t__beepGapMs),
            (uint8_t)uint32_t__beepCount,
            uint32_t__beepGapMs);
    }
    else
    {
        (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
    }
}
#endif
/* ==================== Scenario WeakBattery ==================== */
#if MODULE_CHANGEOVER
/* ==================== Ui_ScenarioWeakBattery_Tick ==================== */
/**
 * @brief  [EN] Scenario-8 face (user order 2026-10-23: "the weak pack that
 *         empties fast and comes back when the load is removed must be
 *         detectable - red blinking lamp and a beep"). RED BLINKING with its
 *         own period/duty (ids 151/152, default 500 ms / 50%) so it is not
 *         confused with the imbalance latch (1000 ms) or the solid-red
 *         dead-battery verdict, and its own beep shape (ids 153..156: 3
 *         short beeps every 3 s by default). Green and yellow stay off. The
 *         verdict itself belongs to Changeover; this function only draws it.
 */
void func__Ui_ScenarioWeakBattery_Tick(void)
{
    uint32_t uint32_t__blinkPeriodMs = CHANGEOVER_DEF_WEAK_LED_PERIOD_MS;
    uint32_t uint32_t__blinkDutyPct  = CHANGEOVER_DEF_WEAK_LED_DUTY_PCT;
    uint32_t uint32_t__beepPeriodMs  = CHANGEOVER_DEF_WEAK_BEEP_PERIOD_MS;
    uint32_t uint32_t__beepLenMs     = CHANGEOVER_DEF_WEAK_BEEP_LEN_MS;
    uint32_t uint32_t__beepCount     = CHANGEOVER_DEF_WEAK_BEEP_COUNT;
    uint32_t uint32_t__beepGapMs     = CHANGEOVER_DEF_WEAK_BEEP_GAP_MS;
    /*
 * [EN] Stop the other scenarios' blink/beep phases first (same hygiene as
 * every other face), so nothing leaks in from a previous scenario.
 */
    func__Ui_ResetForeignScenarioState();
    (void)func__Changeover_GetParam(CHANGEOVER_PARAM_WEAK_LED_PERIOD_MS, &uint32_t__blinkPeriodMs);
    (void)func__Changeover_GetParam(CHANGEOVER_PARAM_WEAK_LED_DUTY_PCT,  &uint32_t__blinkDutyPct);
    func__green(false);
    func__yellow(false);
    func__red(func__Ui_BlinkOnFromTick(0u, uint32_t__blinkPeriodMs, uint32_t__blinkDutyPct));
    (void)func__Changeover_GetParam(CHANGEOVER_PARAM_WEAK_BEEP_PERIOD_MS, &uint32_t__beepPeriodMs);
    if (uint32_t__beepPeriodMs != 0u)
    {
        (void)func__Changeover_GetParam(CHANGEOVER_PARAM_WEAK_BEEP_LEN_MS, &uint32_t__beepLenMs);
        (void)func__Changeover_GetParam(CHANGEOVER_PARAM_WEAK_BEEP_COUNT,  &uint32_t__beepCount);
        (void)func__Changeover_GetParam(CHANGEOVER_PARAM_WEAK_BEEP_GAP_MS, &uint32_t__beepGapMs);
        (void)func__Ui_Buzzer_Gated(
            uint32_t__beepPeriodMs,
            func__Ui_BeepDutyPercent(uint32_t__beepPeriodMs, uint32_t__beepLenMs,
                                     (uint8_t)uint32_t__beepCount, uint32_t__beepGapMs),
            (uint8_t)uint32_t__beepCount,
            uint32_t__beepGapMs);
    }
    else
    {
        (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
    }
}
#endif
/* ==================== Scenario InputOk ==================== */
/**
 * @brief  [EN] InputOk scenario: green steady, red/yellow off, and buzzer off.
 */
void func__Ui_ScenarioInputOk(void)
{
    func__Ui_ResetForeignScenarioState();
    func__green(true);
    func__red(false);
    func__yellow(false);
    (void)func__Ui_Buzzer_Gated(0u, 0u, 0u, 0u);
    /*
 * [EN] InputOk is steady green, no blink — return immediately. The UI task's 10ms loop provides the poll period,
 * so a 500ms blocking delay inside the scenario is not needed and would slow UI reaction.
 */
}
/* ==================== Scenario Charging Tick ==================== */
/**
 * @brief  [EN] Charging scenario tick: green steady, yellow shows remaining to full non-linear.
 *         Formula: remainingPercent = 100-pct, periodPerPercent = period/100, yellowOnMs = remaining*periodPer, yellowOffMs = period-yellowOn.
 *         v1.20 (user order 2026-09-28): the remaining is floored at
 *         UI_CHARGING_YELLOW_MIN_REMAINING_PERCENT and a 100% reading no
 *         longer darks the yellow - this scenario only runs while a
 *         charger channel is pumping, so the LED keeps one visible blink
 *         per period until the charger is cut (full face / no channel).
* @param uint32_t__batteryMv Battery voltage mV, charge percent map 119/120 (v1.49) /
 */
void func__Ui_ScenarioCharging_Tick(uint32_t uint32_t__batteryMv)
{
    uint8_t uint8_t__rawPercent;
    uint8_t uint8_t__stablePercent;
    uint32_t uint32_t__remainingPercent;
    uint32_t uint32_t__periodPerPercent;
    uint32_t uint32_t__yellowOnMs;
    uint32_t uint32_t__yellowOffMs;
    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();
    (void)func__Ui_Buzzer_Gated(0u, 0u, 0u, 0u);
    /*
 * [EN] Charging yellow timing uses hysteresis 5%: stable only moves when |raw-stable|>=5. v_bat is PA3 / v_bat24_mv.
 */
    /*
 * [EN] v1.49: the charging face reads the CHARGE map (119/120).
 */
    uint8_t__rawPercent = func__Ui_ChargeVoltageToPercent(uint32_t__batteryMv);
    uint8_t__stablePercent = func__Ui_UpdateChargingStablePercent(uint8_t__rawPercent);
    func__green(true);
    func__red(false);
    if (uint8_t__stablePercent == 0u)
    {
        func__Ui_ResetChargingYellowBlink();
        func__yellow(true);
        return;
    }
    /*
 * [EN] v1.20 (user order 2026-09-28): a 100% reading while this
 * scenario runs means the percent map is at its ceiling but the
 * charger is still pumping (ABSORB soak up to 1 h) - keep the minimum
 * blink instead of going dark. Fully dark happens only outside this
 * scenario (full face / no active channel).
 */
    if (uint8_t__stablePercent >= UI_PERCENT_FULL)
    {
        uint32_t__remainingPercent = 0u;
    }
    else
    {
        uint32_t__remainingPercent = UI_PERCENT_FULL - uint8_t__stablePercent;
    }
    if (uint32_t__remainingPercent < UI_CHARGING_YELLOW_MIN_REMAINING_PERCENT)
    {
        uint32_t__remainingPercent = UI_CHARGING_YELLOW_MIN_REMAINING_PERCENT;
    }
    uint32_t__periodPerPercent = UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs / UI_PERCENT_SCALE;
    uint32_t__yellowOnMs = uint32_t__remainingPercent * uint32_t__periodPerPercent;
    if (uint32_t__yellowOnMs < UI_ALARM_T__G__Alarm.uint32_t__yellowMinOnMs)
    {
        uint32_t__yellowOnMs = UI_ALARM_T__G__Alarm.uint32_t__yellowMinOnMs;
    }
    if (uint32_t__yellowOnMs > UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs)
    {
        uint32_t__yellowOnMs = UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs;
    }
    uint32_t__yellowOffMs = UI_ALARM_T__G__Alarm.uint32_t__yellowPeriodMs - uint32_t__yellowOnMs;
    func__Ui_UpdateChargingYellowBlink(uint32_t__yellowOnMs, uint32_t__yellowOffMs);
}
#if MODULE_CHARGER
/* ==================== Scenario ThermalRest Tick ==================== */
/**
 * @brief  [EN] Thermal rest face (v1.88, user order 2026-10-12): yellow blinks
 *              1s 50% while charger rests 10min after 1h at max current.
 *              Green steady, red off, buzzer off. Uses absolute tick phase.
 * @param  uint32_t__batteryMv [EN] Battery voltage mV (not used, kept for uniform signature)
 */
void func__Ui_ScenarioThermalRest_Tick(uint32_t uint32_t__batteryMv)
{
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__phaseMs;
    uint32_t uint32_t__onMs;
    bool bool__yellowOn;
    (void)uint32_t__batteryMv;
    func__Ui_ResetBatteryCriticalBeep();
    func__Ui_ResetBatteryRunGreenBlink();
    (void)func__Ui_Buzzer_Gated(0u, 0u, 0u, 0u);
    func__green(true);
    func__red(false);
    uint32_t__elapsedMs = func__Rtos_TicksToMilliseconds(osKernelGetTickCount());
    uint32_t__phaseMs = uint32_t__elapsedMs % CHG_THERMAL_YELLOW_PERIOD_MS;
    uint32_t__onMs = (CHG_THERMAL_YELLOW_PERIOD_MS * CHG_THERMAL_YELLOW_DUTY_PCT) / UI_PERCENT_SCALE;
    bool__yellowOn = (uint32_t__phaseMs < uint32_t__onMs);
    func__yellow(bool__yellowOn);
}
#endif
/* ==================== Scenario BatteryRun Tick ==================== */
/**
 * @brief  [EN] BatteryRun scenario: green blink follows the runtime 74/75 percent map;
 *         buzzer warnings use the four percentage bands (50..53).
 *         Below the crit band (53) all LEDs turn off and the critical pattern (56/57/58/65)
 *         plays once for the latch length (61), then silence until the battery recovers.
* @param uint32_t__batteryMv Battery voltage mV, percent map 74/75 /
 */
void func__Ui_ScenarioBatteryRun_Tick(uint32_t uint32_t__batteryMv)
{
    uint8_t uint8_t__rawPercent;
    uint8_t uint8_t__stablePercent;
    uint32_t uint32_t__remainingPercent;
    uint32_t uint32_t__periodPerPercent;
    uint32_t uint32_t__greenOffMs;
    uint32_t uint32_t__greenOnMs;
    uint32_t uint32_t__nowTick;
    uint32_t uint32_t__criticalElapsedMs;
#if MODULE_FAULT
    /*
 * [EN] Defensive priority guard: if a battery-lost fault reaches this
 * helper directly, it must replace BatteryRun before the one-shot empty
 * battery beep can start. Fault clear is the battery-return boundary.
 */
    if (((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != FAULT_NONE) &&
        (func__Ui_ScenarioAlarmOn(2u) != false))
    {
        func__Ui_ScenarioBatLost_Tick();
        return;
    }
#endif
    func__Ui_ResetChargingYellowBlink();
    /*
 * [EN] Step 1: raw percent from PA3 / ADC1_IN3 / v_bat24_mv.
 */
    uint8_t__rawPercent = func__Ui_BatteryVoltageToPercent(uint32_t__batteryMv);
    /*
 * [EN] Step 2: stable percent with 2% hysteresis and special 0/1 handling.
 * This hysteresis is only for BatteryRun display/timing, not for input/overvoltage/Low Alarm.
 */
    uint8_t__stablePercent = func__Ui_UpdateBatteryStablePercent(uint8_t__rawPercent);
    /*
 * [EN] Critical 0% uses stable percent: one 10s beep, then LEDs off until battery exits critical area.
 * Noise 0<->1 does not restart the beep because stable 0 stays 0 until raw>=2.
 */
    if (uint8_t__stablePercent < (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runBeepCritPct)
    {
        func__Ui_ResetBatteryRunGreenBlink();
        func__green(false);
        func__red(false);
        func__yellow(false);
        if (BOOL__G__UiBatteryCriticalBeepCompleted == true)
        {
            (void)func__Ui_Buzzer_Gated(0u, 0u, 0u, 0u);
            return;
        }
        if (BOOL__G__UiBatteryCriticalBeepActive == false)
        {
            BOOL__G__UiBatteryCriticalBeepActive = true;
            UINT32_T__G__UiBatteryCriticalBeepStartTick = osKernelGetTickCount();
        }
        uint32_t__nowTick = osKernelGetTickCount();
        uint32_t__criticalElapsedMs = func__Rtos_TicksToMilliseconds(uint32_t__nowTick - UINT32_T__G__UiBatteryCriticalBeepStartTick);
        if (uint32_t__criticalElapsedMs >= UI_ALARM_T__G__Alarm.uint32_t__runCritDurMs)
        {
            (void)func__Ui_Buzzer_Gated(0u, 0u, 0u, 0u);
            BOOL__G__UiBatteryCriticalBeepActive = false;
            BOOL__G__UiBatteryCriticalBeepCompleted = true;
            return;
        }
        /*
 * [EN] v1.71: the critical band is shaped like every other band -
 * period, per-beep duration, count, shared gap - and the duty handed
 * to the buzzer is derived, not typed.
 */
        (void)func__Ui_Buzzer_Gated(
            UI_ALARM_T__G__Alarm.uint32_t__runCritPeriodMs,
            func__Ui_BeepDutyPercent(UI_ALARM_T__G__Alarm.uint32_t__runCritPeriodMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runCritBeepDurMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runCritCount,
                                     UI_ALARM_T__G__Alarm.uint32_t__runGapMs),
            (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runCritCount,
            UI_ALARM_T__G__Alarm.uint32_t__runGapMs);
        return;
    }
    func__Ui_ResetBatteryCriticalBeep();
    /*
 * [EN] Non-linear: green blink OFF = remaining * period/100, with min. Input is stablePercent.
 */
    uint32_t__remainingPercent = UI_PERCENT_FULL - uint8_t__stablePercent;
    uint32_t__periodPerPercent = UI_ALARM_T__G__Alarm.uint32_t__greenPeriodMs / UI_PERCENT_SCALE;
    uint32_t__greenOffMs = uint32_t__remainingPercent * uint32_t__periodPerPercent;
    if (uint32_t__greenOffMs < UI_ALARM_T__G__Alarm.uint32_t__greenMinOffMs)
    {
        uint32_t__greenOffMs = UI_ALARM_T__G__Alarm.uint32_t__greenMinOffMs;
    }
    /*
 * [EN] Full-program audit: the min-off floor (id 67) and the period
 * (id 66) are clamped independently, so min-off can exceed the whole
 * period; without this clamp the subtraction below underflows and the
 * green phase would stick on for ~49 days (display only, but wrong).
 */
    if (uint32_t__greenOffMs > UI_ALARM_T__G__Alarm.uint32_t__greenPeriodMs)
    {
        uint32_t__greenOffMs = UI_ALARM_T__G__Alarm.uint32_t__greenPeriodMs;
    }
    uint32_t__greenOnMs = UI_ALARM_T__G__Alarm.uint32_t__greenPeriodMs - uint32_t__greenOffMs;
    if (uint8_t__stablePercent >= (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runBeepStartPct)
    {
        (void)func__Ui_Buzzer_Gated(0u, 0u, 0u, 0u);
    }
    else if (uint8_t__stablePercent >= (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runBeepDoublePct)
    {
        /*
 * [EN] v1.16: duty from the live window (dur/count/gap), shared
 * standard duration (id 59) + band count (id 62).
 */
        (void)func__Ui_Buzzer_Gated(
            UI_ALARM_T__G__Alarm.uint32_t__runStdIntervalMs,
            func__Ui_BeepDutyPercent(UI_ALARM_T__G__Alarm.uint32_t__runStdIntervalMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runStdDurMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runStdCount,
                                     UI_ALARM_T__G__Alarm.uint32_t__runGapMs),
            (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runStdCount,
            UI_ALARM_T__G__Alarm.uint32_t__runGapMs);
    }
    else if (uint8_t__stablePercent >= (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runBeepTriplePct)
    {
        /*
 * [EN] v1.71: band 2 plays its OWN duration (121), its OWN repeat
 * interval (122) and the ONE shared gap (65).
 */
        (void)func__Ui_Buzzer_Gated(
            UI_ALARM_T__G__Alarm.uint32_t__runDoubleIntervalMs,
            func__Ui_BeepDutyPercent(UI_ALARM_T__G__Alarm.uint32_t__runDoubleIntervalMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runDoubleDurMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runDoubleCount,
                                     UI_ALARM_T__G__Alarm.uint32_t__runGapMs),
            (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runDoubleCount,
            UI_ALARM_T__G__Alarm.uint32_t__runGapMs);
    }
    else
    {
        (void)func__Ui_Buzzer_Gated(
            UI_ALARM_T__G__Alarm.uint32_t__runTriIntervalMs,
            func__Ui_BeepDutyPercent(UI_ALARM_T__G__Alarm.uint32_t__runTriIntervalMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runTriDurMs,
                                     UI_ALARM_T__G__Alarm.uint32_t__runTriCount,
                                     UI_ALARM_T__G__Alarm.uint32_t__runGapMs),
            (uint8_t)UI_ALARM_T__G__Alarm.uint32_t__runTriCount,
            UI_ALARM_T__G__Alarm.uint32_t__runGapMs);
    }
    func__red(false);
    func__yellow(false);
    func__Ui_UpdateBatteryRunGreenBlink(uint32_t__greenOnMs, uint32_t__greenOffMs);
}
/* ==================== Ui Tick / UI ==================== */
/**
 * @brief  [EN] Ui main tick - decides which scenario from the real Measurement snapshot.
 *         The snapshot is obtained via func__Measurement_GetSnapshot(); valid is checked
 *         before any decision. If invalid, UI enters safe-off, alarm flag is cleared and
 *         no stale/manual values are used. Battery production source is snapshot.v_bat24_mv only.
* @param measurement_snapshot_t__snap Snapshot pointer, may be NULL / snapshot
 */
void func__Ui_Tick(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    uint32_t uint32_t__inputVoltageMv;
    uint32_t uint32_t__batteryVoltageMv;
    uint32_t uint32_t__batteryClampedMv;
    uint8_t uint8_t__rawPercent;
    bool bool__snapshotValid;
    bool bool__isFull;
    if (measurement_snapshot_t__snap == NULL)
    {
        func__all_off();
        BOOL__G__UiInputPresent = false;
        BOOL__G__UiInputOverVoltage = false;
        UINT32_T__G__UiInputOverVoltageStartTick = 0u;
        func__Ui_ResetForeignScenarioState();
        func__Ui_ResetBatteryStablePercent();
        func__Ui_ResetChargingStablePercent();
        func__Ui_ResetChargingFullHysteresis();
        return;
    }
    bool__snapshotValid = measurement_snapshot_t__snap->valid;
    if (bool__snapshotValid == false)
    {
        BOOL__G__UiInputPresent = false;
        BOOL__G__UiInputOverVoltage = false;
        UINT32_T__G__UiInputOverVoltageStartTick = 0u;
        func__Ui_ResetForeignScenarioState();
#if MODULE_FAULT
        /*
 * [EN] When persistent ADC failure latches FAULT_ADC (or charger
 * technical fault is latched), show the Scenario 7 technical-fault
 * alarm even while the snapshot is invalid instead of sitting dark.
 */
        if (((func__Fault_Get() & (FAULT_CHARGER_TECHNICAL | FAULT_ADC)) != FAULT_NONE) &&
            (func__Ui_ScenarioAlarmOn(7u) != false))
        {
            func__Ui_ScenarioTechnicalFault_Tick();
            return;
        }
#endif
        func__all_off();
        /*
 * [EN] BatteryRun and Charging stable percents keep previous values on invalid but phases reset; full hysteresis keeps state to avoid flicker on noisy valid->invalid.
 * Battery voltage source is PA3 / v_bat24_mv only; do not use v_in here.
 */
        return;
    }
    uint32_t__inputVoltageMv = measurement_snapshot_t__snap->v_in_mv;
    uint32_t__batteryVoltageMv = measurement_snapshot_t__snap->v_bat24_mv;
    /*
 * [EN] v1.74 (user order): the low-battery alarm is no longer a UI
 * scenario. It never had an LED face of its own - its only real consumer
 * was the changeover soft cut - so the whole decision, window and
 * hysteresis moved into the Changeover module and nothing is computed or
 * published here any more.
 */
    func__Ui_UpdateInputState(uint32_t__inputVoltageMv);
#if MODULE_FAULT
    /*
 * [EN] Scenario 7 is the highest-priority visible fault face: when the
 * technical latch or persistent FAULT_ADC is active, all three LEDs must
 * show its synchronized phase even if input overvoltage is also present.
 */
    if (((func__Fault_Get() & (FAULT_CHARGER_TECHNICAL | FAULT_ADC)) != FAULT_NONE) &&
        (func__Ui_ScenarioAlarmOn(7u) != false))
    {
        func__Ui_ScenarioTechnicalFault_Tick();
        return;
    }
#endif
    if ((BOOL__G__UiInputOverVoltage == true) &&
        (func__Ui_ScenarioAlarmOn(1u) != false))
    {
        func__Ui_ScenarioInputOverVoltage_Tick();
        return;
    }
#if MODULE_FAULT
    /*
 * [EN] Battery-lost, after technical fault and overvoltage, before normal
 * scenarios. The Fault module latches and clears the bit; while it is set
 * we show this scenario and return, so the BatteryRun critical beep
 * (input-absent world) cannot overlap with this pattern (input-present
 */
    if (((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != FAULT_NONE) &&
        (func__Ui_ScenarioAlarmOn(2u) != false))
    {
        func__Ui_ScenarioBatLost_Tick();
        return;
    }
#endif
#if MODULE_CHARGER
    /*
 * [EN] v1.72 dead-battery latch, priority 3 (just above the imbalance
 * latch: a pack that never takes charge is the harder verdict and
 * must own the lamp). Solid red until the battery is swapped.
 */
    if ((func__Charger_DeadMask() != 0u) &&
        (func__Ui_ScenarioAlarmOn(6u) != false))
    {
        func__Ui_ScenarioDeadBattery_Tick();
        return;
    }
#endif
#if MODULE_IMBALANCE
    /*
 * [EN] Imbalance latch, priority 4 (after overvoltage, battery-lost
 * and the dead-battery verdict, before every normal scenario): while the verdict
 * holds, the condemned-battery face (solid red + periodic one-short
 * beep, ids 115/116) is the ONLY face shown. It ends only with the
 * automatic reset on battery replacement.
 */
    {
        imbalance_outputs_t imbalance_outputs_t__imbalance;
        func__Imbalance_GetOutputs(&imbalance_outputs_t__imbalance);
        if ((imbalance_outputs_t__imbalance.bool__latched == true) &&
            (func__Ui_ScenarioAlarmOn(5u) != false))
        {
            func__Ui_ScenarioImbalance_Tick();
            return;
        }
    }
#endif
#if MODULE_CHANGEOVER
    /*
 * [EN] Scenario 8 (priority 5, after the two condemned-battery verdicts
 * and before every normal scenario): the pack could not carry the
 * load and is now resting. Red blink + its own beep pattern, and the
 * face stays as long as Changeover keeps the verdict latched.
 */
    /*
 * [EN] v1.88 (user order 2026-10-23): the verdict is about running on the
 * BATTERY. While the mains is back and the pack is charging, the panel
 * shows the normal charge face; the verdict, the quiet-rest timer and
 * the blocked battery path all stay exactly as they are, and the face
 * returns the moment the mains drops again.
 */
    if ((func__Changeover_WeakVerdictActive() != false) &&
        (measurement_snapshot_t__snap->input_present == false))
    {
        func__Ui_ScenarioWeakBattery_Tick();
        return;
    }
#endif
    uint32_t__batteryClampedMv = uint32_t__batteryVoltageMv;
    /*
 * [EN] v1.49: the full-charge latch (77/78) is a CHARGE-side decision, so
 * it clamps against and converts with the charge map (119/120). Before
 * the split it borrowed the discharge pair, which is exactly why moving
 * a discharge limit used to move the full-charge point.
 */
    if (uint32_t__batteryClampedMv > UI_ALARM_T__G__Alarm.uint32_t__chgPctVmaxMv)
    {
        uint32_t__batteryClampedMv = UI_ALARM_T__G__Alarm.uint32_t__chgPctVmaxMv;
    }
    uint8_t__rawPercent = func__Ui_ChargeVoltageToPercent(uint32_t__batteryClampedMv);
    bool__isFull = func__Ui_UpdateChargingFullHysteresis(uint8_t__rawPercent);
#if MODULE_CHARGER
    /*
 * [EN] v1.17b (user order 2026-09-27: "after a full charge the blinking
 * must be gone"): the charger itself declares completion - every
 * relevant channel through ABSORB (taper or the 1 h ceiling). The
 * voltage latch above stays as an independent second path (pack at
 * the 119/120 ceiling also means full, even mid-pump).
 * v1.20 (user order 2026-09-28: "the yellow may go FULLY dark only
 * when the charger is cut"): while any channel is still pumping, the
 * full face yields to the Charging face - the pack can legitimately
 * sit at the percent ceiling for the whole ABSORB soak (up to 1 h)
 * and the yellow keeps its minimum visible blink instead of dying
 * mid-charge. The voltage latch keeps deciding when nothing pumps
 * (FLOAT parked / done) and IsChargeComplete stays the primary full
 * path (the v1.17b order stays intact).
 */
    if (func__Charger_IsChargeComplete() == true)
    {
        bool__isFull = true;
    }
    else if (func__Charger_IsAnyChannelActive() == true)
    {
        bool__isFull = false;
    }
    else if (func__Charger_IsAnyChannelInThermalRest() == true)
    {
        bool__isFull = false;
    }
#endif
#if MODULE_CHANGEOVER
    /*
 * [EN] v1.89 (user order 2026-10-23): while the battery path is CUT and no
 * mains is present there is nothing to show - the load is off, the pack
 * is not feeding anything, and blinking "running on battery" beside a
 * dark output is a lie. All three LEDs and the buzzer stay off; the
 * charging face returns by itself as soon as the mains is back, because
 * this gate only speaks for the no-mains case.
 */
    if ((BOOL__G__UiInputPresent == false) &&
        (func__Changeover_PathBlocked() != false))
    {
        func__all_off();
        func__Ui_ResetForeignScenarioState();
        return;
    }
#endif
    if (BOOL__G__UiInputPresent == true)
    {
        if (bool__isFull == true)
        {
            /*
 * [EN] Full: charge complete (v1.17b) or the 100/<95 voltage
 * hysteresis - steady green, the yellow blink is gone.
 */
            func__Ui_ScenarioInputOk();
        }
#if MODULE_CHARGER
        else if (func__Charger_IsAnyChannelInThermalRest() == true)
        {
            /*
 * [EN] Thermal rest: 1h max -> 10min rest, yellow blinks 1s 50%
 * so user sees rest, green steady, red off.
 */
            func__Ui_ScenarioThermalRest_Tick(uint32_t__batteryClampedMv);
        }
        else if (func__Charger_IsAnyChannelActive() == false)
        {
            /*
 * [EN] Not full and NO channel charging (OFF / JIT retry /
 * input wait / final fault / battery-lost): the yellow charge
 * blink must NOT run - the user wants it only while the charger
 * module really works (channel 1, channel 2 or both). Green
 * steady stays as the honest "input ok" face instead.
 */
            func__Ui_ScenarioInputOk();
        }
#endif
        else if (func__Ui_ScenarioAlarmOn(4u) != false)
        {
            func__Ui_ScenarioCharging_Tick(uint32_t__batteryClampedMv);
        }
        else
        {
            /*
 * [EN] Scenario 4's face is switched off in the panel: the LEDs
 * and the buzzer stay dark while the charger charges normally.
 */
            func__all_off();
            func__Ui_ResetForeignScenarioState();
        }
    }
    else if (func__Ui_ScenarioAlarmOn(3u) != false)
    {
        /*
 * [EN] Input disconnected → BatteryRun uses 2% hysteresis + 0/1.
 */
        func__Ui_ScenarioBatteryRun_Tick(uint32_t__batteryVoltageMv);
    }
    else
    {
        /*
 * [EN] Scenario 3's face is switched off in the panel.
 */
        func__all_off();
        func__Ui_ResetForeignScenarioState();
    }
}
/* ==================== Ui Init / UI ==================== */
/**
 * @brief  [EN] Drive all UI outputs low (safe state).
 */
void func__Ui_Init(void)
{
    func__all_off();
    BOOL__G__UiInputPresent = false;
    BOOL__G__UiInputOverVoltage = false;
    UINT32_T__G__UiInputOverVoltageStartTick = 0u;
    func__Ui_ResetForeignScenarioState();
    func__Ui_ResetBatteryStablePercent();
    func__Ui_ResetChargingStablePercent();
    func__Ui_ResetChargingFullHysteresis();
}
/* ==================== Board Test Delay With Watchdog ==================== */
/**
 * @brief  [EN] Delay helper for BoardTest that refreshes BSP_IWDG_SLOT_UI between slices
 *         without changing total LED or buzzer duration.
* @param uint32_t__delayMs Requested delay in milliseconds. / .
 */
static void func__Ui_BoardTest_DelayWithWatchdog(uint32_t uint32_t__delayMs)
{
    uint32_t uint32_t__remainingMs;
    uint32_t uint32_t__sliceMs;
    uint32_t__remainingMs = uint32_t__delayMs;
#if MODULE_UI
    func__BspIwdg_CheckIn(BSP_IWDG_SLOT_UI,
                          func__Rtos_TicksToMilliseconds(osKernelGetTickCount()));
#endif
    while (uint32_t__remainingMs > 0u)
    {
        uint32_t__sliceMs = uint32_t__remainingMs;
        if (uint32_t__sliceMs > UI_BOARDTEST_IWDG_SLICE_MS)
        {
            uint32_t__sliceMs = UI_BOARDTEST_IWDG_SLICE_MS;
        }
        func__Rtos_DelayMilliseconds(uint32_t__sliceMs);
        uint32_t__remainingMs -= uint32_t__sliceMs;
#if MODULE_UI
        func__BspIwdg_CheckIn(BSP_IWDG_SLOT_UI,
                              func__Rtos_TicksToMilliseconds(osKernelGetTickCount()));
#endif
    }
}
/* ==================== Board Test Start ==================== */
/**
 * @brief  [EN] One-shot wiring check: red, yellow, green and the previous 150ms-style buzzer check.
 *         The buzzer uses the new periodic API with a safe period and then is explicitly turned off.
 *         Each delay step refreshes BSP_IWDG_SLOT_UI so the 1650 ms sequence stays inside BSP_IWDG_STALE_MS.
 */
void func__Ui_BoardTest_Start(void)
{
    uint32_t uint32_t__beepPeriodMs;
    uint32_t uint32_t__beepDutyPercent;
    uint32_t uint32_t__beepDutyProduct;
    int32_t int32_t__buzzerResult;
    func__all_off();
    func__Ui_ResetForeignScenarioState();
    func__Ui_ResetBatteryStablePercent();
    func__Ui_ResetChargingStablePercent();
    func__Ui_ResetChargingFullHysteresis();
    func__red(true);
    func__Ui_BoardTest_DelayWithWatchdog(APP_CONFIG.ui_selftest_led_ms);
    func__red(false);
    func__yellow(true);
    func__Ui_BoardTest_DelayWithWatchdog(APP_CONFIG.ui_selftest_led_ms);
    func__yellow(false);
    func__green(true);
    func__Ui_BoardTest_DelayWithWatchdog(APP_CONFIG.ui_selftest_led_ms);
    func__green(false);
    if (APP_CONFIG.ui_boot_beep_ms == 0u)
    {
        return;
    }
    uint32_t__beepPeriodMs = APP_CONFIG.ui_boot_beep_ms;
    if (uint32_t__beepPeriodMs < UI_BUZZER_MIN_PERIOD_MS)
    {
        uint32_t__beepPeriodMs = UI_BUZZER_MIN_PERIOD_MS;
    }
    uint32_t__beepDutyPercent = UI_BUZZER_DUTY_MAX_PERCENT;
    if (APP_CONFIG.ui_boot_beep_ms < uint32_t__beepPeriodMs)
    {
        /*
 * [EN] Flash diet 2026-09-27: u32 is exact - boot beep is the
 * 150 ms compile default x 100 = 15,000 (never written at
 * runtime). No uldivmod.
 */
        uint32_t__beepDutyProduct = APP_CONFIG.ui_boot_beep_ms * UI_BUZZER_PERCENT_SCALE;
        uint32_t__beepDutyPercent = uint32_t__beepDutyProduct / uint32_t__beepPeriodMs;
        if ((uint32_t__beepDutyProduct % uint32_t__beepPeriodMs) != 0u)
        {
            uint32_t__beepDutyPercent++;
        }
        if (uint32_t__beepDutyPercent == 0u)
        {
            uint32_t__beepDutyPercent = 1u;
        }
    }
    int32_t__buzzerResult = func__Ui_Buzzer_Tick(
        uint32_t__beepPeriodMs,
        (uint8_t)uint32_t__beepDutyPercent,
        1u,
        0u);
    if (int32_t__buzzerResult != UI_BUZZER_INVALID_RESULT)
    {
        func__Ui_BoardTest_DelayWithWatchdog(APP_CONFIG.ui_boot_beep_ms);
    }
    (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
}
