/**
 * @file    ui_led.h
 * @brief   [EN] UI LED scenarios - green/red/yellow, battery percent, InputOk/Charging/BatteryRun.
 *          Split from UI into LED and BUZZER per user request. Constants for LED in its own header.
 *          RTOS simple readable, non-linear formulas, markers above each function in h and c.
 * @note    [EN] LED defaults live in this header; since v1.16 the live
 *          cadence is the persisted ui_alarm_t (ids 38..82) and runtime
 *          logic reads it - APP_CONFIG keeps only the one-shot BoardTest
 *          timings. Naming __ after type, func__ prefix.
 *          CMSIS-RTOS2: osDelay allowed, HAL_Delay forbidden. Formulas broken into steps.
 */
#ifndef UI_LED_H
#define UI_LED_H
/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"
#include "modules_enable.h"
/* ==================== Battery voltage mapping constants ==================== */
/**
 * @brief  [EN] Battery voltage mapped to zero percent, in millivolts.
 */
#define UI_BAT_V_MIN_MV                 21000u
/**
 * @brief  [EN] Battery voltage mapped to one hundred percent, in millivolts.
 *              Raised 28000 -> 29000 by user order 2026-09-20: the Charging
 *              yellow blink keeps pacing until the battery truly reaches 29 V.
 */
#define UI_BAT_V_MAX_MV                 29000u
/* ==================== Input voltage thresholds and hysteresis ==================== */
/**
 * @brief  [EN] Boot fallback for the shared healthy-input lower bound (panel q33).
 *              With MODULE_FAULT=1 the live q33/q34 pair is read atomically from
 *              Fault; this macro only mirrors the factory default for Fault-off.
 */
#define UI_INPUT_CONNECTED_THRESHOLD_MV 21000u
/**
 * @brief  [EN] Boot fallback for the shared healthy-input upper bound (panel q34).
 */
#define UI_INPUT_WINDOW_MAX_MV          28000u
/**
 * @brief  [EN] Boot fallback Schmitt margin for input presence in Fault-off
 *              builds, in millivolts. With MODULE_FAULT=1 the live shared
 *              permission (panel q169/q170 margins) is used instead. The
 *              scenario-1 OV alarm uses the shared q34 as threshold and q71 as hysteresis.
 */
#define UI_INPUT_HYSTERESIS_MV          500u
/**
 * @brief  [EN] Lower release threshold derived from the fallback connected bound.
 */
#define UI_INPUT_DISCONNECTED_THRESHOLD_MV \
    (UI_INPUT_CONNECTED_THRESHOLD_MV - UI_INPUT_HYSTERESIS_MV)
/* ==================== Input overvoltage error constants ==================== */
/**
 * @brief  [EN] Boot fallback for the OV alarm threshold in Fault-off builds.
 *              With MODULE_FAULT=1 the live threshold is the shared upper input
 *              limit (panel q34) read atomically from Fault; this macro is only
 *              the compile-time default for builds without Fault.
 */
#define UI_INPUT_OVERVOLTAGE_THRESHOLD_MV 28000u
/**
 * @brief  [EN] Hysteresis band used to clear the input overvoltage error, in millivolts.
 */
#define UI_INPUT_OVERVOLTAGE_HYSTERESIS_MV 1000u
/**
 * @brief  [EN] Boot fallback clear point (Fault-off builds only). At runtime
 *              clear = live q34 - q71 hysteresis.
 */
#define UI_INPUT_OVERVOLTAGE_CLEAR_THRESHOLD_MV \
    (UI_INPUT_OVERVOLTAGE_THRESHOLD_MV - UI_INPUT_OVERVOLTAGE_HYSTERESIS_MV)
/* ==================== Scenario timing constants ==================== */
/**
 * @brief  [EN] Red LED period while the input overvoltage error is displayed, in milliseconds.
 */
#define UI_INPUT_OVERVOLTAGE_LED_PERIOD_MS 1000u
/**
 * @brief  [EN] Red LED duty while the input overvoltage error is displayed, in percent.
 */
#define UI_INPUT_OVERVOLTAGE_LED_DUTY_PERCENT 50u
/**
 * @brief  [EN] Buzzer pattern period for the input overvoltage error, in milliseconds.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_PERIOD_MS 10000u
/**
 * @brief  [EN] Buzzer ON duration for one input overvoltage warning, in milliseconds.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_DURATION_MS 1000u
/**
 * @brief  [EN] Buzzer duty derived from the overvoltage warning ON duration and period.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_DUTY_PERCENT \
    ((UI_INPUT_OVERVOLTAGE_BEEP_DURATION_MS * UI_PERCENT_SCALE) / UI_INPUT_OVERVOLTAGE_BEEP_PERIOD_MS)
/**
 * @brief  [EN] Number of buzzer pulses in one input overvoltage warning pattern.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_COUNT 1u
/**
 * @brief  [EN] Gap between adjacent overvoltage pulses; one pulse does not use a gap.
 */
#define UI_INPUT_OVERVOLTAGE_BEEP_GAP_MS 0u
/* ==================== Battery-lost error constants ==================== */
/**
 * @brief  [EN] Battery-lost scenario (2026-09-19): shown only while the
 *         central fault bit FAULT_CHARGER_BAT_LOST is latched by the Fault
 *         module. Design chosen with the user: red fast blink (defaults 500 ms ON / 500 ms OFF - clearly
 *         different from the 1 s input-overvoltage pulse; ids 44/45),
 *         green steady (input is present in both detection cases), and a
 *         repeating buzzer pattern (defaults: THREE short beeps then a silence; ids 46..49). It can
 *         never mix with the BatteryRun critical beep: that one runs only with
 *         the input ABSENT, and in func__Ui_Tick this scenario is checked
 *         (after overvoltage) before every normal scenario and returns.
 */
#define UI_BAT_LOST_LED_PERIOD_MS 1000u
/**
 * @brief  [EN] Red LED duty while the battery-lost error is displayed, percent.
 */
#define UI_BAT_LOST_LED_DUTY_PERCENT 50u
/**
 * @brief  [EN] Battery-lost buzzer pattern: one full cycle (beeps + silence),
 *         in milliseconds.
 */
#define UI_BAT_LOST_BEEP_PERIOD_MS 3000u
/**
 * @brief  [EN] Total beep window inside one battery-lost cycle (the three
 *         beeps share it), remainder is silence, in milliseconds.
 */
#define UI_BAT_LOST_BEEP_DURATION_MS 900u
/**
 * @brief  [EN] Buzzer duty derived from the beep window and the full period.
 */
#define UI_BAT_LOST_BEEP_DUTY_PERCENT \
    ((UI_BAT_LOST_BEEP_DURATION_MS * UI_PERCENT_SCALE) / UI_BAT_LOST_BEEP_PERIOD_MS)
/**
 * @brief  [EN] Number of buzzer pulses per battery-lost cycle (three beeps).
 */
#define UI_BAT_LOST_BEEP_COUNT 3u
/**
 * @brief  [EN] Silence between adjacent battery-lost beeps, in milliseconds.
 */
#define UI_BAT_LOST_BEEP_GAP_MS 100u
/* ==================== Technical board fault / (scenario 7) ==================== */
/*
 * [EN] Scenario 7 is deliberately its own alarm family. It never borrows
 *      the count/gap of scenario 5 (132/133) or scenario 6 (134/135).
 *      All three LEDs share one phase, so they turn on and off together.
 */
#define UI_TECH_FAULT_LED_PERIOD_MS       1000u
#define UI_TECH_FAULT_LED_DUTY_PERCENT    50u
#define UI_TECH_FAULT_BEEP_PERIOD_MS      3000u
#define UI_TECH_FAULT_BEEP_DURATION_MS    200u
#define UI_TECH_FAULT_BEEP_COUNT          3u
#define UI_TECH_FAULT_BEEP_GAP_MS         100u
/**
 * @brief  [EN] Delay used while InputOk holds the green LED steady.
 */
#define UI_INPUT_OK_POLL_MS             500u
/**
 * @brief  [EN] Duration of each LED step in the one-shot BoardTest.
 */
#define UI_SELFTEST_LED_MS              500u
/**
 * @brief  [EN] Maximum delay slice between UI watchdog check-ins during BoardTest.
 */
#define UI_BOARDTEST_IWDG_SLICE_MS      500u
/**
 * @brief  [EN] Complete green blink period used by BatteryRun, in milliseconds.
 */
#define UI_BLINK_PERIOD_MS              1000u
/**
 * @brief  [EN] Minimum green LED OFF time used to keep the full-battery blink visible.
 */
#define UI_GREEN_MIN_OFF_MS             10u
/**
 * @brief  [EN] Complete yellow blink period used by Charging, in milliseconds.
 */
#define UI_CHARGING_BLINK_PERIOD_MS     1000u
/**
 * @brief  [EN] Minimum yellow LED ON blip near full charge, in milliseconds
 *              (misnamed MIN_OFF until the 2026-09-26 audit: the code floors
 *              the ON time, keeping the nearly-full blink visible).
 *              Raised 10 -> 150 by user order 2026-09-27 (v1.17): at 28.8 V
 *              the remaining-based blink was only ~30-50 ms/s and looked
 *              dead - the floor keeps it clearly visible until full/cutoff.
 */
#define UI_CHARGING_YELLOW_MIN_ON_MS   150u
/**
 * @brief  [EN] Floor for the remaining-to-full percent that drives the
 *              Charging yellow ON time (user order 2026-09-28: "even at
 *              1 percent the yellow must still blink once - say it never
 *              goes below 2 percent"). With it the yellow never goes
 *              fully dark INSIDE the Charging scenario: while a charger
 *              channel is pumping, the LED always gives at least this
 *              blink level, and UI_CHARGING_YELLOW_MIN_ON_MS (id 69)
 *              then keeps that blip visible. Fully dark is reserved for
 *              the charger being cut (charge complete / no channel
 *              active).
 */
#define UI_CHARGING_YELLOW_MIN_REMAINING_PERCENT 2u
/* ==================== BatteryRun warning constants / BatteryRun ==================== */
/**
 * @brief  [EN] Battery percentage below which the standard BatteryRun warning beep is enabled.
 */
#define UI_BATTERY_RUN_BEEP_START_PERCENT 40u
/**
 * @brief  [EN] Battery percentage below which the standard warning uses two beeps.
 */
#define UI_BATTERY_RUN_BEEP_DOUBLE_PERCENT 20u
/**
 * @brief  [EN] Battery percentage below which the warning uses three beeps every 20 seconds.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_PERCENT 10u
/**
 * @brief  [EN] Battery percentage below which the critical ten-second beep is used.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_PERCENT 1u
/**
 * @brief  [EN] Interval shared by the one-beep and two-beep BatteryRun warnings, in milliseconds.
 */
#define UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS 60000u
/**
 * @brief  [EN] Interval of the three-beep BatteryRun warning, in milliseconds.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS 20000u
/**
 * @brief  [EN] Complete period used for the single critical ten-second beep, in milliseconds.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_PERIOD_MS 10000u
/**
 * @brief  [EN] v1.71 (user order: every discharge band must be shaped the
 *         same way): the critical band no longer takes a duty percent. It
 *         takes a per-beep duration in ms, exactly like bands 1..3, and the
 *         duty handed to the buzzer is derived from it. The default keeps the
 *         old sound: one 10000 ms beep inside a 10000 ms period, i.e. the
 *         former 100 percent duty.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_BEEP_DURATION_MS 10000u
/**
 * @brief  [EN] Repeat interval of the two-beep BatteryRun band, in ms.
 *         v1.71 gave band 2 its own interval; before that it borrowed the
 *         band-1 interval, so the default is that same number.
 */
#define UI_BATTERY_RUN_BEEP_DOUBLE_INTERVAL_MS UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS
/**
 * @brief  [EN] Pulse count of the critical BatteryRun beep.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_COUNT 1u
/**
 * @brief  [EN] Duration of each one-beep or two-beep BatteryRun pulse, in milliseconds.
 */
#define UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS 1000u
/**
 * @brief  [EN] Approximate duty for one standard beep, derived from duration and interval.
 */
#define UI_BATTERY_RUN_BEEP_STANDARD_DUTY_PERCENT \
    ((UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS * UI_PERCENT_SCALE + UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS - 1u) / UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS)
/**
 * @brief  [EN] Approximate duty for two standard-duration beeps and their gap.
 */
#define UI_BATTERY_RUN_BEEP_DOUBLE_DUTY_PERCENT \
    ((((UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS * UI_BATTERY_RUN_BEEP_DOUBLE_COUNT) + \
       (UI_BATTERY_RUN_BEEP_GAP_MS * (UI_BATTERY_RUN_BEEP_DOUBLE_COUNT - 1u))) * \
      UI_PERCENT_SCALE + UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS - 1u) / \
     UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS)
/**
 * @brief  [EN] Approximate duty for three triple-duration beeps and their gaps.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_DUTY_PERCENT \
    ((((UI_BATTERY_RUN_BEEP_TRIPLE_DURATION_MS * UI_BATTERY_RUN_BEEP_TRIPLE_COUNT) + \
       (UI_BATTERY_RUN_BEEP_GAP_MS * (UI_BATTERY_RUN_BEEP_TRIPLE_COUNT - 1u))) * \
      UI_PERCENT_SCALE + UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS - 1u) / \
     UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS)
/**
 * @brief  [EN] Duration of each pulse in the three-beep BatteryRun warning, in milliseconds.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_DURATION_MS 2000u
/**
 * @brief  [EN] Duration of the single critical BatteryRun beep, in milliseconds.
 */
#define UI_BATTERY_RUN_BEEP_CRITICAL_DURATION_MS 10000u
/**
 * @brief  [EN] Pulse count for the standard BatteryRun warning.
 */
#define UI_BATTERY_RUN_BEEP_STANDARD_COUNT 1u
/**
 * @brief  [EN] Pulse count for the low-battery BatteryRun warning.
 */
#define UI_BATTERY_RUN_BEEP_DOUBLE_COUNT 2u
/**
 * @brief  [EN] Pulse count for the critical BatteryRun warning above the empty threshold.
 */
#define UI_BATTERY_RUN_BEEP_TRIPLE_COUNT 3u
/**
 * @brief  [EN] Low gap between adjacent BatteryRun warning pulses, in milliseconds.
 */
#define UI_BATTERY_RUN_BEEP_GAP_MS 100u
/* ==================== Battery percent hysteresis ==================== */
/**
 * @brief  [EN] Hysteresis for BatteryRun display/timing percent, in percent points.
 *         Stable percent changes only when raw percent differs by at least 2.
 *         Jitter 56↔57 or 57↔58 does not change blink timing; 57→55 or 57→59 does.
 *         This hysteresis applies only to BatteryRun green blink and buzzer timing, not to input, overvoltage or Low Battery Alarm.
 */
#define UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT 2u
/**
 * @brief  [EN] Legacy alias kept for compatibility. Use UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT.
 */
#define UI_BATTERY_PERCENT_HYSTERESIS_PERCENT UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT
/**
 * @brief  [EN] Hysteresis for Charging yellow blink timing, in percent points.
 *         Charging stable percent changes only when raw differs by at least 5.
 *         Example: stable 57, raw 53..61 keeps 57; outside range moves to new raw.
 */
#define UI_CHARGING_PERCENT_HYSTERESIS_PERCENT 5u
/**
 * @brief  [EN] Raw percent threshold to exit the critical 0% state.
 *         While stable is 0, it stays 0 until raw reaches at least 2; then it moves to 1 first, not directly to 2.
 */
#define UI_BATTERY_ZERO_EXIT_THRESHOLD        2u
/**
 * @brief  [EN] Raw percent threshold to exit the 1% state upward.
 *         While stable is 1: raw==0 → 0, raw>=3 → 2, otherwise keep 1. Prevents chatter between 0 and 1.
 */
#define UI_BATTERY_ONE_EXIT_THRESHOLD         3u
/**
 * @brief  [EN] Raw battery percent at which Charging may enter InputOk (full) state.
 *         InputOk is entered only when raw reaches 100%.
 */
#define UI_CHARGING_FULL_ENTER_PERCENT        100u
/**
 * @brief  [EN] Raw battery percent below which InputOk exits back to Charging.
 *         While InputOk is active it stays until raw falls below 95%.
 */
#define UI_CHARGING_FULL_EXIT_PERCENT         95u
/*
 * [EN] v1.74 (user order): the low-battery alarm used to live here as a UI
 * scenario (threshold/clear constants, two link params and the global flag
 * BOOL__G__UiBatteryAlarmIssued). It had no LED face and its only consumer
 * was the changeover soft cut, so the whole thing moved to
 * Firmware/Modules/Changeover/changeover.h as fixed constants.
 */
/* ==================== Percentage constants ==================== */
/**
 * @brief  [EN] Full battery percentage and upper bound of percentage calculations.
 */
#define UI_PERCENT_FULL                 100u
/**
 * @brief  [EN] Scale used to convert a ratio into a percentage.
 */
#define UI_PERCENT_SCALE                100u
/* ==================== RTOS tick / RTOS ==================== */
/**
 * @brief  [EN] Base UI task delay in milliseconds.
 */
#define UI_TICK_MS                      10u
/* ==================== Battery Voltage To Percent ==================== */
/**
 * @brief  [EN] Convert battery voltage to percent 0..100. Non-linear broken into steps: range, offset, scaled, percent.
* @param uint32_t__batteryMv Battery voltage mV, range 0..40000mV, clamped /
* @return uint8_t Percent 0..100 /
 */
uint8_t func__Ui_BatteryVoltageToPercent(uint32_t uint32_t__batteryMv);
/**
 * @brief  [EN] Charge-side voltage to percent, from the 119/120 map (v1.49).
 *              Same maths as the discharge map, different numbers.
* @param uint32_t__batteryMv Battery voltage in mV /
* @return uint8_t Percent 0..100 /
 */
uint8_t func__Ui_ChargeVoltageToPercent(uint32_t uint32_t__batteryMv);
/* ==================== Ui Init / UI ==================== */
/**
 * @brief  [EN] Drive all UI outputs low (safe state).
 */
void func__Ui_Init(void);
/* ==================== Board Test Start ==================== */
/**
 * @brief  [EN] One-shot wiring check: red, yellow, green and the previous short buzzer check via the independent buzzer service.
 */
void func__Ui_BoardTest_Start(void);
/* ==================== Scenario WeakBattery ==================== */
/**
 * @brief  [EN] Scenario-8 face: red blink (ids 151/152) plus its own beep
 *              pattern (ids 153..156), drawn only while the Changeover
 *              weak-battery verdict is latched. Must be called every Ui pass
 *              while the verdict holds.
 */
void func__Ui_ScenarioWeakBattery_Tick(void);
/* ==================== Scenario InputOk ==================== */
/**
 * @brief  [EN] InputOk: green steady, red/yellow/buzzer off. CMSIS-RTOS2 delay, MCU not locked.
 */
void func__Ui_ScenarioInputOk(void);
#if MODULE_IMBALANCE
/*
 * [EN] Scenario 5 latched face: solid red + periodic one-short beep (ids
 *      115/116). See ui_led.c for the full behaviour contract.
 */
void func__Ui_ScenarioImbalance_Tick(void);
#endif
#if MODULE_CHARGER
/*
 * [EN] v1.72 scenario 6: condemned-battery face owned by Charger /
 */
void func__Ui_ScenarioDeadBattery_Tick(void);
/*
 * [EN] v1.88 thermal rest face: yellow blink during 10min rest after 1h max.
 */
void func__Ui_ScenarioThermalRest_Tick(uint32_t uint32_t__batteryMv);
#endif
/* ==================== Scenario Charging Tick ==================== */
/**
 * @brief  [EN] Charging: green steady, yellow remaining to full non-linear (remainingPercent, periodPerPercent, yellowOnMs/offMs).
 *         CMSIS-RTOS2 delay.
* @param uint32_t__batteryMv Battery voltage mV /
 */
void func__Ui_ScenarioCharging_Tick(uint32_t uint32_t__batteryMv);
/* ==================== Scenario BatLost Tick ==================== */
/**
 * @brief  [EN] BatLost: red fast blink + green steady + periodic beeps
 *         (defaults: three short beeps and a pause, UI_BAT_LOST_*; ids 44..49).
 *         Called every Ui pass while FAULT_CHARGER_BAT_LOST is latched; no latch lives in the UI.
 */
void func__Ui_ScenarioBatLost_Tick(void);
/* ==================== Scenario BatteryRun Tick ==================== */
/**
 * @brief  [EN] BatteryRun: green blink from the runtime 74/75 percent map and four buzzer bands (50..53).
 *         Below the crit band (53) all LEDs turn off and the critical pattern (56/57/58/65)
 *         plays once for the latch length (61), then silence until the battery recovers.
* @param uint32_t__batteryMv Battery voltage mV, 21000=0% 29000=100% /
 */
void func__Ui_ScenarioBatteryRun_Tick(uint32_t uint32_t__batteryMv);
/* ==================== Ui Tick / UI ==================== */
/**
 * @brief  [EN] Ui main tick - decides which scenario from the real Measurement snapshot.
 *         The snapshot is obtained via func__Measurement_GetSnapshot(); valid is checked
 *         before any decision. If invalid, UI enters safe-off, alarm flag is cleared and
 *         no stale/manual values are used. Battery production source is snapshot.v_bat24_mv only.
* @param measurement_snapshot_t__snap Pointer to snapshot, may be NULL / snapshot
 */
void func__Ui_Tick(const measurement_snapshot_t *measurement_snapshot_t__snap);
/* ==================== Runtime UI cadence (v1.16) ==================== */
/*
 * [EN] v1.16 (user order 2026-09-26): the 39 UI_* numbers below became
 *      runtime ids 38..76, editable from the ESP panel, flash-persisted
 *      like the charge profile (~1.5 s debounce) and clamped as a set on
 *      every write; the macros stay BOOT DEFAULTS only. Id 76 (mute) is panel-session only
 *      since v1.16b (RAM, cleared on reboot - only the one-shot BoardTest
 *      wiring beep ignores the mute). v1.17 (user
 *      order 2026-09-27: charge-scenario numbers panel-editable too):
 *      ids 77..82 add the full latch (enter/exit), the charge/discharge
 *      stable hysteresis and the 0/1 exits - 45 numbers, ids 38..82.
 */
#define UI_ALARM_PARAM_OV_LED_PERIOD_MS    38u  /* ms, 100..10000 */
#define UI_ALARM_PARAM_OV_LED_DUTY_PCT     39u  /* %, 0..100 */
#define UI_ALARM_PARAM_OV_BEEP_PERIOD_MS   40u  /* ms, 0=off else 1000..600000 */
#define UI_ALARM_PARAM_OV_BEEP_DUR_MS      41u  /* ms per beep, 0..window-fit vs 40/42/43 */
#define UI_ALARM_PARAM_OV_BEEP_COUNT       42u  /* n, 0..10, 0=off */
#define UI_ALARM_PARAM_OV_BEEP_GAP_MS      43u  /* ms, 0..5000, >=100 when 42>1 */
#define UI_ALARM_PARAM_BL_LED_PERIOD_MS    44u  /* ms, 100..10000 */
#define UI_ALARM_PARAM_BL_LED_DUTY_PCT     45u  /* %, 0..100 */
#define UI_ALARM_PARAM_BL_BEEP_PERIOD_MS   46u  /* ms, 0=off else 1000..600000 */
#define UI_ALARM_PARAM_BL_BEEP_DUR_MS      47u  /* ms per beep, def 233 (legacy 900 window shared), 0..fit vs 46/48/49 */
#define UI_ALARM_PARAM_BL_BEEP_COUNT       48u  /* n, 0..10, 0=off */
#define UI_ALARM_PARAM_BL_BEEP_GAP_MS      49u  /* ms, 0..5000, >=100 when 48>1 */
#define UI_ALARM_PARAM_RUN_BEEP_START_PCT  50u  /* %, 0..100, >= 51 */
#define UI_ALARM_PARAM_RUN_BEEP_DOUBLE_PCT 51u  /* %, 0..100, <= 50, >= 52 */
#define UI_ALARM_PARAM_RUN_BEEP_TRIPLE_PCT 52u  /* %, 0..100, <= 51, >= 53 */
#define UI_ALARM_PARAM_RUN_BEEP_CRIT_PCT   53u  /* %, 0..100, <= 52 */
#define UI_ALARM_PARAM_RUN_STD_INTERVAL_MS 54u  /* ms, 0=off else 1000..600000 */
#define UI_ALARM_PARAM_RUN_TRI_INTERVAL_MS 55u  /* ms, 0=off else 1000..600000 */
#define UI_ALARM_PARAM_RUN_CRIT_PERIOD_MS  56u  /* ms, 0=off else 1000..600000 */
#define UI_ALARM_PARAM_RUN_CRIT_BEEP_DUR_MS 57u /* ms per beep, 0..600000 (v1.71) */
#define UI_ALARM_PARAM_RUN_CRIT_COUNT      58u  /* n, 0..10 */
#define UI_ALARM_PARAM_RUN_STD_DUR_MS      59u  /* ms per beep, 0..fit vs 54/62/63/65 */
#define UI_ALARM_PARAM_RUN_TRI_DUR_MS      60u  /* ms per beep, 0..fit vs 55/64/65 */
#define UI_ALARM_PARAM_RUN_CRIT_DUR_MS     61u  /* ms, 0..120000 one-shot latch length */
#define UI_ALARM_PARAM_RUN_STD_COUNT       62u  /* n, 0..10 */
#define UI_ALARM_PARAM_RUN_DOUBLE_COUNT    63u  /* n, 0..10 */
#define UI_ALARM_PARAM_RUN_TRI_COUNT       64u  /* n, 0..10 */
#define UI_ALARM_PARAM_RUN_GAP_MS          65u  /* ms, 0..5000, >=100 when any band count>1 */
#define UI_ALARM_PARAM_GREEN_PERIOD_MS     66u  /* ms, 100..10000 */
#define UI_ALARM_PARAM_GREEN_MIN_OFF_MS    67u  /* ms, 0..66 */
#define UI_ALARM_PARAM_YELLOW_PERIOD_MS    68u  /* ms, 100..10000 */
#define UI_ALARM_PARAM_YELLOW_MIN_ON_MS   69u  /* ms, 0..68 */
#define UI_ALARM_PARAM_OV_THRESH_MV        70u  /* read-only mirror of shared q34; writes rejected */
#define UI_ALARM_PARAM_OV_HYST_MV          71u  /* mV, 0..2000 */
/*
 * [EN] v1.74 (user order: "this must not sit among the LED scenarios - it
 * belongs to changeover, that is where we need it, and we do not need it in
 * the panel either"). Ids 72/73 were the low-battery alarm window. The UI
 * never had a face for them: the only consumer was the changeover soft cut,
 * so the window moved into the Changeover module as fixed constants and
 * these two ids are RETIRED - no owner, no panel field, never persisted.
 * They are deliberately NOT reused and the ids above them are deliberately
 * NOT renumbered: every stored NVM record, backup file and bench log keeps
 * its meaning across this change.
 */
#define UI_ALARM_PARAM_RETIRED_LOWBAT_FIRST 72u
#define UI_ALARM_PARAM_RETIRED_LOWBAT_LAST  73u
#define UI_ALARM_PARAM_PCT_VMIN_MV         74u  /* mV, 15000..25000, <= 75-100, DISCHARGE map only since v1.49 */
#define UI_ALARM_PARAM_PCT_VMAX_MV         75u  /* mV, 25000..32000, >= 74+100, DISCHARGE map only since v1.49 */
#define UI_ALARM_PARAM_BUZZER_MUTE         76u  /* 0/1, panel-session only (RAM); never persisted, cleared on reboot; scenarios only */
#define UI_ALARM_PARAM_CHG_FULL_ENTER_PCT  77u  /* %, 1..100, enter authoritative: exit pulled to enter-1 */
#define UI_ALARM_PARAM_CHG_FULL_EXIT_PCT   78u  /* %, 0..100, < 77 after clamp */
#define UI_ALARM_PARAM_CHG_HYST_PCT        79u  /* %, 0..50, charging stable-percent hysteresis (0 = follow raw) */
#define UI_ALARM_PARAM_RUN_HYST_PCT        80u  /* %, 0..50, BatteryRun stable-percent hysteresis (0 = follow raw) */
#define UI_ALARM_PARAM_RUN_ZERO_EXIT       81u  /* %, 1..100 (v1.92: 0 made the 0% test always true), raw needed to leave the 0% state to 1 */
#define UI_ALARM_PARAM_RUN_ONE_EXIT        82u  /* %, 0..100, raw needed to leave the 1% state to 2 */
#define UI_ALARM_PARAM_MIN_ID              38u
#define UI_ALARM_PARAM_MAX_ID              82u
/* ==================== Charge-Side Percent Map (v1.49) ==================== */
/*
 * [EN] v1.49 (user order 2026-10-05: "the discharge limits must be separate
 * from the full-charge variable - the two sections are completely separate -
 * but the discharge percent must still be computed from this one"):
 * ids 74/75 are now the DISCHARGE percent map only. The charging side
 * (percent shown while charging and the full-charge latch 77/78) reads its
 * own pair, 119/120. Both pairs ship with the same factory defaults, so a
 * board that is never touched behaves exactly as before; the point is that
 * moving one side no longer moves the other.
 * The ids are 119/120 and not 83/84 because the parameter id space is
 * global: 83..118 already belong to the charger PID, the charger limits and
 * the imbalance scenario. They live in their own small range above the dense
 * 38..82 block, and the indexed Set/Get below maps them to the two words
 * appended at the end of ui_alarm_t.
 */
#define UI_ALARM_PARAM_CHG_PCT_VMIN_MV    119u  /* mV, 15000..25000, <= 120-100 */
#define UI_ALARM_PARAM_CHG_PCT_VMAX_MV    120u  /* mV, 25000..32000, >= 119+100 */
/* ==================== Band 2 Own Beep Shape (v1.50) ==================== */
/*
 * [EN] v1.50 (user order 2026-10-05: "band 2 is the only one that repeats
 * band 1 - separate those two as well"): the 2-beep band used to borrow the
 * 1-beep band's per-beep duration (59) and the all-band gap (65). It now has
 * its own pair, so shaping the double beep cannot change the single beep.
 * Still shared on purpose: the repeat interval (54) - the user named only
 * these two. Bands 3 and critical keep the common gap 65.
 */
#define UI_ALARM_PARAM_RUN_DOUBLE_DUR_MS  121u  /* ms per beep, 0..fit vs 54/63/122 */
#define UI_ALARM_PARAM_RUN_DOUBLE_INTERVAL_MS 122u /* ms, repeat interval of band 2 (v1.71) */
#define UI_ALARM_PARAM_EXT_MIN_ID         119u
#define UI_ALARM_PARAM_EXT_MAX_ID         122u
/*
 * [EN] Scenario 7 extension is non-contiguous because ids 123..136 belong
 * to scenarios 5 and 6. Keep a separate range instead of claiming their
 */
#define UI_ALARM_PARAM_TECH_EXT_MIN_ID    137u
#define UI_ALARM_PARAM_TECH_EXT_MAX_ID    142u
#define UI_ALARM_PARAM_TECH_BEEP_PERIOD_MS 137u
#define UI_ALARM_PARAM_TECH_BEEP_LEN_MS    138u
#define UI_ALARM_PARAM_TECH_BEEP_COUNT     139u
#define UI_ALARM_PARAM_TECH_BEEP_GAP_MS    140u
#define UI_ALARM_PARAM_TECH_LED_PERIOD_MS  141u
#define UI_ALARM_PARAM_TECH_LED_DUTY_PCT   142u
/**
 * @brief  [EN] Live UI cadence set (one struct, like the fault alarms).
 *              Scenarios read these, never the macros.
 */
typedef struct
{
    uint32_t uint32_t__ovLedPeriodMs;
    uint32_t uint32_t__ovLedDutyPct;
    uint32_t uint32_t__ovBeepPeriodMs;
    uint32_t uint32_t__ovBeepDurMs;
    uint32_t uint32_t__ovBeepCount;
    uint32_t uint32_t__ovBeepGapMs;
    uint32_t uint32_t__blLedPeriodMs;
    uint32_t uint32_t__blLedDutyPct;
    uint32_t uint32_t__blBeepPeriodMs;
    uint32_t uint32_t__blBeepDurMs;
    uint32_t uint32_t__blBeepCount;
    uint32_t uint32_t__blBeepGapMs;
    uint32_t uint32_t__runBeepStartPct;
    uint32_t uint32_t__runBeepDoublePct;
    uint32_t uint32_t__runBeepTriplePct;
    uint32_t uint32_t__runBeepCritPct;
    uint32_t uint32_t__runStdIntervalMs;
    uint32_t uint32_t__runTriIntervalMs;
    uint32_t uint32_t__runCritPeriodMs;
    uint32_t uint32_t__runCritBeepDurMs;
    uint32_t uint32_t__runCritCount;
    uint32_t uint32_t__runStdDurMs;
    uint32_t uint32_t__runTriDurMs;
    uint32_t uint32_t__runCritDurMs;
    uint32_t uint32_t__runStdCount;
    uint32_t uint32_t__runDoubleCount;
    uint32_t uint32_t__runTriCount;
    uint32_t uint32_t__runGapMs;
    uint32_t uint32_t__greenPeriodMs;
    uint32_t uint32_t__greenMinOffMs;
    uint32_t uint32_t__yellowPeriodMs;
    uint32_t uint32_t__yellowMinOnMs;
    /*
 * [EN] v1.104: reserved padding - the OV threshold is now the live shared
 * q34 read from Fault; this word is never read or written at runtime.
 */
    uint32_t uint32_t__ovThreshMv;
    uint32_t uint32_t__ovHystMv;
    /*
 * [EN] v1.74: the two retired low-battery words stay as reserved padding
 * so the dense id->word index (id - 38) that the whole block relies on
 * does not move. Nothing reads or writes them.
 */
    uint32_t uint32_t__retiredLowBatThreshMv;
    uint32_t uint32_t__retiredLowBatClearMv;
    uint32_t uint32_t__pctVminMv;
    uint32_t uint32_t__pctVmaxMv;
    uint32_t uint32_t__buzzerMute;
    uint32_t uint32_t__chgFullEnterPct;
    uint32_t uint32_t__chgFullExitPct;
    uint32_t uint32_t__chgHystPct;
    uint32_t uint32_t__runHystPct;
    uint32_t uint32_t__runZeroExit;
    uint32_t uint32_t__runOneExit;
    /*
 * [EN] v1.49: the charge-side percent map (ids 119/120). Appended at the
 * END on purpose - Set/Get index the struct as a word array, so the dense
 * 38..82 block must keep its offsets.
 */
    uint32_t uint32_t__chgPctVminMv;
    uint32_t uint32_t__chgPctVmaxMv;
    /*
 * [EN] v1.50 gave band 2 its own per-beep duration (id 121) and gap
 * (id 122); v1.71 (user order: "only the gap is shared, everything else
 * is per band") turns id 122 into band 2's own REPEAT INTERVAL and sends
 * every band back to the one shared gap, id 65. Appended after the
 * charge map for the same indexing reason as v1.49.
 */
    uint32_t uint32_t__runDoubleDurMs;
    uint32_t uint32_t__runDoubleIntervalMs;
    /*
 * [EN] Scenario 7 owns six appended words: beep period, per-beep
 * duration, count, gap, and the synchronized three-LED period/duty.
 */
    uint32_t uint32_t__techBeepPeriodMs;
    uint32_t uint32_t__techBeepDurMs;
    uint32_t uint32_t__techBeepCount;
    uint32_t uint32_t__techBeepGapMs;
    uint32_t uint32_t__techLedPeriodMs;
    uint32_t uint32_t__techLedDutyPct;
} ui_alarm_t;
/**
 * @brief  [EN] Write one UI cadence value (38..82): store, re-clamp the
 *              whole set, report the applied value.
* @param uint8_t__paramId 38..82 /
* @param uint32_t__value Requested value /
* @param uint32_t__appliedValue Required applied value out; NULL rejects the write / NULL
* @return bool true when the id is 38..76 /
 */
bool func__Ui_SetAlarmParam(uint8_t uint8_t__paramId,
                            uint32_t uint32_t__value,
                            uint32_t *uint32_t__appliedValue);
/**
 * @brief  [EN] Read one live UI cadence value (38..82).
* @param uint8_t__paramId 38..82 /
* @param uint32_t__value Value out /
* @return bool true when the id is 38..82 /
 */
bool func__Ui_GetAlarmParam(uint8_t uint8_t__paramId,
                            uint32_t *uint32_t__value);
/* ==================== Scenario alarm switches ==================== */
/*
 * [EN] v1.87 (user order): every scenario card in the panel carries an
 *      "enabled" tick, but until now that tick only muted the card in the
 *      browser and never reached the board. These seven ids are the board
 *      side of the same ticks: 0 switches THAT scenario's LED/beep face off
 *      while its detection, its latch and every output protection stay
 *      exactly as they were; 1 (the default) shows the face again.
 */
/*
 * [EN] Numbering follows the PANEL CARD order (the number the user reads on
 *      the card), which is not the dispatch order of func__Ui_Tick:
 *        1 = input overvoltage, 2 = battery lost, 3 = discharge (no input),
 *        4 = normal charge, 5 = imbalance, 6 = dead battery, 7 = technical
 *        fault. Card 8 (weak battery) has no face switch.
 */
#define UI_SCENARIO_ALARM_ID_FIRST  158u
#define UI_SCENARIO_ALARM_ID_LAST   164u
/**
 * @brief  [EN] Write one scenario-alarm switch (158..164 = scenarios 1..7).
 *              Values clamp to 0/1; the applied value comes back.
* @param uint8_t__paramId 158..164 /
* @param uint32_t__value 0 = face off, 1 = face on /
* @param uint32_t__appliedValue Required out; NULL rejects /
* @return bool true when the id is in range /
 */
bool func__Ui_SetScenarioAlarm(uint8_t uint8_t__paramId,
                               uint32_t uint32_t__value,
                               uint32_t *uint32_t__appliedValue);
/**
 * @brief  [EN] Read one scenario-alarm switch (158..164).
* @param uint8_t__paramId 158..164 /
* @param uint32_t *uint32_t__value Value out /
* @return bool true when the id is in range /
 */
bool func__Ui_GetScenarioAlarm(uint8_t uint8_t__paramId,
                               uint32_t *uint32_t__value);
/**
 * @brief  [EN] Is the LED/beep face of this scenario allowed to run?
 *              Scenario numbers are 1..8; anything outside 1..7 is true, so a
 *              caller can never silence a face by passing a bad number.
* @param uint8_t__scenario 1..7 /
* @return bool true when the face may run /
 */
bool func__Ui_ScenarioAlarmOn(uint8_t uint8_t__scenario);
#endif /* UI_LED_H */
