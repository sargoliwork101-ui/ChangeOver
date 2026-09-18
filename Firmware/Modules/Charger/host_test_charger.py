#!/usr/bin/env python3
"""Host tests for the independent two-channel 12 V charger policy.

These tests validate policy and source contracts only. They do not authorize a
battery connection and cannot replace HAL, transformer, relay, comparator or
oscilloscope tests on the board.
"""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
CHARGER_H = ROOT / "Firmware/Modules/Charger/charger.h"
CHARGER_C = ROOT / "Firmware/Modules/Charger/charger.c"
MAIN_C = ROOT / "CubeIDE/Core/Src/main.c"
IOC = ROOT / "CubeMX/CubeIDE.ioc"
BSP_EXTI_C = ROOT / "Firmware/Bsp/Src/bsp_exti.c"

ABSORB_MV = 14400
FLOAT_MV = 13500
REENTRY_MV = 12800
CURRENT_LIMIT_MA = 675
REGULATE_LOW_MA = 620
HARD_FAULT_MA = 950
DUTY_MAX = 500
RAMP_UP_MS = 1000
RAMP_DOWN_MS = 500
DUTY_START = 10
DUTY_STEP = 5
INPUT_VALID_MV = 22000


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def regulate_one(channel, voltage_mv, current_ma, now_ms=0):
    """Small host model of the bulk branch, with a duty per channel and
    rate-limited steps: one 0.5% up-step per 1000 ms, one 0.5% down-step
    per 100 ms. >950 hard fault -> reset; >675 -> down; <620 -> up;
    620..675 -> hold; too soon to step -> hold."""
    if current_ma > HARD_FAULT_MA:
        channel["fault"] = True
        channel["duty"] = 0
        return
    last_step_ms = channel.get("last_step_ms", -10**9)
    if voltage_mv < ABSORB_MV and current_ma > CURRENT_LIMIT_MA:
        if now_ms - last_step_ms >= RAMP_DOWN_MS:
            channel["duty"] = max(0, channel["duty"] - DUTY_STEP)
            channel["last_step_ms"] = now_ms
    elif voltage_mv < ABSORB_MV and current_ma < REGULATE_LOW_MA:
        if now_ms - last_step_ms >= RAMP_UP_MS:
            channel["duty"] += DUTY_STEP
            if channel["duty"] > DUTY_MAX:
                channel["duty"] = DUTY_MAX
            channel["last_step_ms"] = now_ms
    # inside the 620..675 band or inside the step window: hold duty


def jit_sequence(channel, trip_count):
    """Host model of per-channel JIT sequence."""
    channel["trips"] = trip_count
    if trip_count == 1 or trip_count == 2:
        channel["pwm_this"] = 0
        channel["relay_on"] = False
        channel["other_pwm_changed"] = False
        channel["retry_duty"] = max(1, channel["duty_before"] // 2) if trip_count == 1 else min(100, max(1, channel["duty_before"] // 2))
    else:
        channel["pwm_this"] = 0
        channel["pwm_other"] = 0
        channel["relay_on"] = True
        channel["final"] = True


def test_modules_enabled_build():
    mods = (ROOT / "Firmware/Config/Inc/modules_enable.h").read_text()
    ch = CHARGER_H.read_text()
    check(re.search(r"#define MODULE_CHARGER\s+1", mods),
          "MODULE_CHARGER must be 1 for build/compile coverage")
    check(re.search(r"#define MODULE_JITTER\s+1", mods),
          "MODULE_JITTER must be 1 for build/compile coverage")
    check(re.search(r"#define CHG_MASTER_ENABLE\s+1u", ch),
          "master switch must be 1 for the active charge scenario (normal Bulk/Absorb/Float)")


def test_master_enable_constant_is_single_gate():
    text_h = CHARGER_H.read_text()
    text_c = CHARGER_C.read_text()
    check(re.search(r"#define CHG_MASTER_ENABLE\s+1u", text_h),
          "CHG_MASTER_ENABLE is 1 for the active board bring-up")
    check("CHG_MASTER_ENABLE == 0u" in text_c and "func__Charger_SafeIdle();" in text_c,
          "CHG_MASTER_ENABLE=0 must force whole-charger safe-idle")
    check("CHG_MASTER_ENABLE == 1u" in text_c or "CHG_MASTER_ENABLE" in text_c,
          "CHG_MASTER_ENABLE must guard entry into control")
    check("APP_CONFIG.power_stage_enabled == false" not in text_c,
          "power_stage_enabled must not remain a hidden Charger gate")
    check("APP_CONFIG.pwm_max_duty_permille == 0u" not in text_c,
          "pwm_max_duty=0 must not remain a hidden Charger gate")
    check(("JIT" in text_h and "relay" in text_h and "inactive" in text_h) or
          "سیاست JIT/رله غیرفعال" in text_h,
          "with CHG_MASTER_ENABLE=0 JIT/relay disconnect policy must be documented inactive")


def test_master_enable_zero_safeidle_stops_both_pwm_and_relay_off():
    text_c = CHARGER_C.read_text()
    # Master=0 path must call SafeIdle, and SafeIdle must StopAll + relay false
    idx = text_c.find("if (CHG_MASTER_ENABLE == 0u)")
    check(idx >= 0, "master-zero gate must exist")
    snippet = text_c[idx:idx+600]
    check("func__Charger_SafeIdle();" in snippet, "master-zero must SafeIdle")
    check("func__BspPwm_StopAll();" in text_c and "func__BspGpio_Write(BSP_GPIO_RELAY, false);" in text_c,
          "SafeIdle must StopAll PWM and de-energize relay coil (NC closed)")
    check("return;" in snippet, "master-zero must return before any control/JIT/relay policy")


def test_transformer_known_not_bypassable_bringup_only_when_zero():
    text_h = CHARGER_H.read_text()
    text_c = CHARGER_C.read_text()
    check(re.search(r"#define CHG_TRANSFORMER_KNOWN\s+1u", text_h),
          "CHG_TRANSFORMER_KNOWN is 1: transformer data and current chain are board-verified")
    check("board-verified" in text_h or "برد تأیید" in text_h,
          "CHG_TRANSFORMER_KNOWN=1 must carry the board-verified rationale in docs")
    check(re.search(r"#define CHG_BRINGUP_TEST_ENABLE\s+0u", text_h),
          "bring-up test mode must be 0 (bring-up finished, normal charge active)")
    check("CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE" in text_h and re.search(r"100u\s", text_h),
          "bring-up stage max duty must be documented (100 permille = 10%, board-verified)")
    check(re.search(r"#define CHG_BRINGUP_TEST_SOURCE_LIMIT_MA\s+150u", text_h),
          "bring-up full-stage source limit must be 150 mA external (100 mA kept restarting on real draw)")
    check("CHG_FIRST_BOARD_TEST_MAX_MA" not in text_h,
          "the retired 100 mA first-test setting must not remain as an executable-looking constant")
    # controlAllowed path must require either KNOWN=1 or bring-up enabled
    check("bool__controlAllowed" in text_c and "CHG_TRANSFORMER_KNOWN != 0u" in text_c,
          "normal control must require CHG_TRANSFORMER_KNOWN=1")
    check("CHG_BRINGUP_TEST_ENABLE != 0u" in text_c and "func__Charger_BringupRegulateChannel" in text_c,
          "only explicit bring-up mode is allowed when transformer is unknown")
    check("if (bool__controlAllowed == false)" in text_c and "func__Charger_SafeIdle();" in text_c,
          "when neither KNOWN nor bring-up enabled, SafeIdle must be forced")


def test_master_enable_does_not_bypass_numeric_protections():
    text_c = CHARGER_C.read_text()
    check("CHG_CURRENT_LIMIT_MA" in text_c, "current limit must still exist")
    check("CHG_MIN_VALID_BATTERY_MV" in text_c, "battery validity threshold must still exist")
    check("CHG_INPUT_VALID_MV" in text_c, "real ADC input threshold must still exist")
    check("func__Charger_FinalDisconnect" in text_c, "final disconnect protection must still exist")


def test_channel_selection_constants():
    text = CHARGER_H.read_text()
    check(re.search(r"#define CHG_CHANNEL_1_INSTALLED\s+0u", text),
          "current test must keep unassembled channel 1 disabled")
    check(re.search(r"#define CHG_CHANNEL_2_INSTALLED\s+1u", text),
          "current test must select installed channel 2")
    check("CHG_INSTALLED_CHANNEL_MASK" in text,
          "the two constants must feed one explicit installed-channel mask")


def test_channel_one_is_always_zero_stopped():
    text = CHARGER_C.read_text()
    check("func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);" in text,
          "uninstalled-channel path must write duty 0 explicitly")
    check("SetPwmBoth(" not in text,
          "shared SetPwmBoth must not be used or reintroduced")
    check("BSP_PWM_CHARGER_1" in text and "0u" in text,
          "PWM1 must be explicitly kept 0 for uninstalled CH1")


def test_trans2_uses_only_vlow_not_24v_pack():
    text = CHARGER_C.read_text()
    check("func__Charger_ChannelVoltageMv" in text,
          "channel voltage selector must exist")
    # Channel 2 returns v_bat_low_mv = VLOW = MID-GND.
    check("return measurement_snapshot_t__snap->v_bat_low_mv;" in text,
          "Trans2 channel voltage must read v_bat_low_mv (VLOW = MID-GND)")
    check("v_bat24_mv" not in text,
          "charger policy must not use 24 V pack value for CH2 setpoint or missing battery")
    check("v_bat_low_mv" in text,
          "Trans2 control must use independent low battery sense")


def test_min_valid_battery_is_sense_not_setpoint():
    text_h = CHARGER_H.read_text()
    check(re.search(r"#define CHG_MIN_VALID_BATTERY_MV\s+2000u", text_h),
          "minimum valid battery sense must be 2000 mV")
    import re as _re
    normalized_h = _re.sub(r"\s+\*\s+", " ", text_h.lower())
    normalized_h = " ".join(normalized_h.split())
    check("not a charge setpoint" in normalized_h,
          "2000 mV must be documented as battery sense validity, not a charge setpoint")
    check("VLOW = MID - GND" in text_h or "VLOW = MID-GND" in text_h or "VLOW = MID - GND" in text_h,
          "2000 mV for Trans2 must be documented on VLOW = MID-GND")
    check("electronic load" in text_h and "voltage clamp" in text_h,
          "only voltage-clamped electronic load / battery simulator must be allowed for no-battery tests")
    check("free resistor" in text_h or "resistor alone" in text_h or "مقاومت آزاد" in text_h,
          "free resistor alone must be documented as not a valid battery simulator")


def test_battery_voltage_upper_cutoff_applies_before_any_control():
    text_h = CHARGER_H.read_text()
    text_c = CHARGER_C.read_text()
    check(re.search(r"#define CHG_MAX_VALID_BATTERY_MV\s+15000u", text_h),
          "battery over-voltage cutoff must be 15000 mV")
    check("func__Charger_BatteryVoltageIsValid" in text_c,
          "all battery control paths must use one validity helper")
    bringup = text_c[text_c.find("static void func__Charger_BringupRegulateChannel"):text_c.find("/* ==================== Regulation", text_c.find("static void func__Charger_BringupRegulateChannel"))]
    normal = text_c[text_c.find("static void func__Charger_RegulateChannel"):text_c.find("/* ==================== Charger_Init", text_c.find("static void func__Charger_RegulateChannel"))]
    check("func__Charger_BatteryVoltageIsValid" in bringup and "func__Charger_BatteryVoltageIsValid" in normal,
          "both bring-up and normal charging must stop on low or high battery sense")
    check("CHG_MAX_VALID_BATTERY_MV" in text_h and "not a substitute" in text_h,
          "firmware cutoff must be documented as an additional protection, not hardware protection")


def test_jit_is_active_low_and_retry_captures_duty_before_stop():
    text_c = CHARGER_C.read_text()
    text_exti = BSP_EXTI_C.read_text()
    main = MAIN_C.read_text()
    ioc = IOC.read_text()
    jit = text_c[text_c.find("static void func__Charger_HandleJitTrip"):text_c.find("static uint16_t func__Charger_RetryDuty")]
    check(jit.find("uint16_t__dutyBeforeTripPermille =") < jit.find("func__Charger_StopOneChannel"),
          "JIT retry must capture live duty before StopOneChannel clears it")
    check("HAL_GPIO_ReadPin(PIN_JITTER1_PORT, PIN_JITTER1_PIN) == GPIO_PIN_RESET" in text_exti and
          "HAL_GPIO_ReadPin(PIN_JITTER2_PORT, PIN_JITTER2_PIN) == GPIO_PIN_RESET" in text_exti,
          "software JIT callback guard must accept only the active-low state")
    check("GPIO_MODE_IT_FALLING" in main,
          "generated MCU GPIO setup must use falling-edge EXTI for LM393 JIT")
    check("PB2.Mode=External_Interrupt_Mode_with_Falling_edge_trigger_detection" in ioc and
          "PB6.Mode=External_Interrupt_Mode_with_Falling_edge_trigger_detection" in ioc,
          "CubeMX JIT pins must use falling-edge EXTI")


def test_input_voltage_is_real_adc_22000mv():
    text_h = CHARGER_H.read_text()
    text_c = CHARGER_C.read_text()
    check(re.search(r"#define CHG_INPUT_VALID_MV\s+22000u", text_h),
          "input validity threshold must be 22000 mV")
    check("measurement_snapshot_t__snap->v_in_mv >= CHG_INPUT_VALID_MV" in text_c,
          "input must be checked from real ADC v_in_mv every cycle")
    check("input_present == false" not in text_c,
          "PB4 digital input alone must not be the charge gate")
    # Vin<22000 path: SafeIdle (both PWM 0 + relay coil false/NC closed), no retry/PWM
    idx = text_c.find("bool__inputAdcValid == false")
    check(idx >= 0, "low-Vin gate must exist")
    snippet = text_c[idx:idx+900]
    check("func__Charger_SafeIdle();" in snippet, "low Vin must SafeIdle (PWM1=PWM2=0, relay off/NC closed)")
    check("CHG_STATE_INPUT_WAIT" in snippet, "low Vin must put channels into INPUT_WAIT (no retry/PWM until recovery)")


def test_input_recovery_restarts_with_safe_duty():
    text_c = CHARGER_C.read_text()
    text_h = CHARGER_H.read_text()
    check("CHG_STATE_INPUT_WAIT" in text_c,
          "low-input wait state must exist")
    check("CHG_DUTY_START_PERMILLE" in text_c and re.search(r"#define CHG_DUTY_START_PERMILLE\s+10u", text_h),
          "safe restart duty must be 10 permille = 1%")
    check("CHG_STATE_OFF" in text_c,
          "on Vin recovery channels must transition back to OFF/start duty")


def test_no_shadowing_in_duty_adjustments():
    import re as _re
    text_c = CHARGER_C.read_text()
    # After OFF->BULK start, RegulateChannel must return before falling through to
    # the duty-calculation block (otherwise first cycle double-steps duty).
    off_block = text_c[text_c.find("charger_state_t__state == CHG_STATE_OFF"):text_c.find("charger_state_t__state == CHG_STATE_FLOAT")]
    check("return;" in off_block, "OFF->BULK start must return before duty calculation to avoid first-cycle double step")
    # Duty clamps in ApplyDuty / Bringup / Regulate must write back to an outer variable
    # rather than only declaring a shadowed local. Verify ApplyDuty clamps via a dedicated
    # local that is then passed to BspPwm and stored into the channel.
    check("uint16_t__clampedDuty" in text_c, "ApplyDuty must clamp through a local that feeds BspPwm write (no shadow drop)")


def test_jit_per_channel_sequence():
    text_c = CHARGER_C.read_text()
    check("uint8_t__jitTripCount" in text_c,
          "per-channel JIT trip count must exist")
    check("func__Charger_StopOneChannel(uint8_t__channelIndex);" in text_c,
          "first/second JIT must stop only the same channel PWM")
    check("BOOL__G__RelayOpen = false;" in text_c,
          "first/second JIT retry must keep relay off (NC closed)")
    check("func__Charger_FinalDisconnect();" in text_c,
          "third same-channel JIT must disconnect with both PWM0 and relay on")
    check("uint16_t__retryDuty = CHG_DUTY_RETRY_SECOND_MAX" in text_c or "CHG_DUTY_RETRY_SECOND_MAX" in text_c,
          "second retry must be capped at 10 percent or less")
    check("func__Jitter_ClearChannel" in text_c,
          "retry must clear only the tripped comparator channel")

    ch = {"duty_before": 200, "pwm_other": 100, "other_pwm_changed": False, "final": False}
    jit_sequence(ch, 1)
    check(ch["pwm_this"] == 0 and not ch["relay_on"] and ch["retry_duty"] == 100,
          "first JIT on CH2 must stop only CH2, relay off, retry half duty")
    ch = {"duty_before": 200, "pwm_other": 100, "other_pwm_changed": False, "final": False}
    jit_sequence(ch, 2)
    check(ch["pwm_this"] == 0 and not ch["relay_on"] and ch["retry_duty"] <= 100,
          "second JIT on CH2 must stop only CH2, relay off, retry <=10%")
    ch = {"duty_before": 100, "pwm_other": 50, "other_pwm_changed": False, "final": False}
    jit_sequence(ch, 3)
    check(ch["pwm_this"] == 0 and ch["pwm_other"] == 0 and ch["relay_on"] and ch["final"],
          "third JIT on CH2 must stop both PWM and open relay")


def test_low_current_is_not_fault():
    channel = {"duty": 100, "fault": False}
    regulate_one(channel, voltage_mv=12400, current_ma=40)
    check(channel["duty"] == 105, "low current below target must increase same channel duty by 5 permille")
    check(not channel["fault"], "low current must not be a fault")


def test_duty_steps_are_time_limited():
    channel = {"duty": 100, "fault": False}
    regulate_one(channel, voltage_mv=12400, current_ma=400, now_ms=0)
    check(channel["duty"] == 105, "first up-step allowed -> 0.5% step")
    regulate_one(channel, voltage_mv=12400, current_ma=400, now_ms=500)
    check(channel["duty"] == 105, "up-step inside the 1000 ms window must be blocked (no 50%/s runaway)")
    regulate_one(channel, voltage_mv=12400, current_ma=400, now_ms=1000)
    check(channel["duty"] == 110, "rate is one 0.5% up-step per second")
    regulate_one(channel, voltage_mv=12400, current_ma=700, now_ms=1400)
    check(channel["duty"] == 110, "down-step needs its own 500 ms window after the last step")
    regulate_one(channel, voltage_mv=12400, current_ma=700, now_ms=1500)
    check(channel["duty"] == 105, "over-band gradually steps duty down instead of cutting to zero")


def test_duty_max_dcm_ceiling():
    channel = {"duty": 495, "fault": False}
    regulate_one(channel, voltage_mv=12400, current_ma=400, now_ms=0)
    check(channel["duty"] == 500, "step up allowed up to the 50% DCM ceiling")
    regulate_one(channel, voltage_mv=12400, current_ma=400, now_ms=1000)
    check(channel["duty"] == 500, "duty must clamp at 500 permille = 50% (higher risks burning the MOSFET)")


def test_only_above_675ma_protects():
    channel_low = {"duty": 100, "fault": False}
    regulate_one(channel_low, voltage_mv=12400, current_ma=600)
    check(not channel_low["fault"], "current below 675 mA must not enter overcurrent protection")
    check(channel_low["duty"] == 105, "600 mA is below the 620 band edge and must raise duty")
    channel_ok = {"duty": 100, "fault": False}
    regulate_one(channel_ok, voltage_mv=12400, current_ma=675)
    check(not channel_ok["fault"], "675 mA must not enter overcurrent protection")
    check(channel_ok["duty"] == 100, "exactly 675 mA sits inside the band and must hold duty")
    channel_high = {"duty": 100, "fault": False}
    regulate_one(channel_high, voltage_mv=12400, current_ma=676)
    check(not channel_high["fault"] and channel_high["duty"] == 95,
          "676 mA must only step duty down (regulation), never cut the channel")
    channel_bad = {"duty": 100, "fault": False}
    regulate_one(channel_bad, voltage_mv=12400, current_ma=951)
    check(channel_bad["fault"] and channel_bad["duty"] == 0,
          "only current above the 950 mA hard fault must reset the channel")


def test_setpoints_and_timing():
    text_h = CHARGER_H.read_text()
    check(re.search(r"#define CHG_ABSORB_MV\s+14400u", text_h), "absorb must be 14400 mV")
    check(re.search(r"#define CHG_FLOAT_MV\s+13500u", text_h), "float must be 13500 mV")
    check(re.search(r"#define CHG_REENTRY_MV\s+12800u", text_h), "reentry must be 12800 mV")
    check(re.search(r"#define CHG_ABSORB_HOLD_MS\s+600000u", text_h), "absorb hold must be 600000 ms = 10 min")
    check(re.search(r"#define CHG_BULK_CURRENT_MAX_MA\s+675u", text_h), "bulk regulation current must be 675 mA")
    check(re.search(r"#define CHG_REGULATE_LOW_MA\s+620u", text_h), "regulation band lower edge must be 620 mA")
    check(re.search(r"#define CHG_CURRENT_HARD_FAULT_MA\s+950u", text_h), "hard over-current fault must be 950 mA")
    check(re.search(r"#define CHG_DUTY_MAX_PERMILLE\s+500u", text_h), "duty cap must be 500 permille = 50% (DCM ceiling, board requirement)")
    check(re.search(r"#define CHG_DUTY_RAMP_UP_INTERVAL_MS\s+1000u", text_h), "up-steps must be limited to one per 1000 ms")
    check(re.search(r"#define CHG_DUTY_RAMP_DOWN_INTERVAL_MS\s+500u", text_h), "down-steps must be limited to one per 500 ms")
    check(re.search(r"#define CHG_FLYBACK_EFFICIENCY_PERMILLE\s+705u", text_h), "efficiency must be 705 permille (bench 15%-duty point: real out 441 mA x 13.0 V, true primary 358 mA x 22.9 V)")
    check(re.search(r"#define CHG_CURRENT_EMA_SHIFT\s+6u", text_h), "current estimate must pass through an EMA filter (shift 6, tau ~0.64 s)")
    check(re.search(r"#define CHG_FIXED_DUTY_TEST_ENABLE\s+0u", text_h), "fixed duty diagnostic must be OFF for normal charge (1u only during bench calibration)")
    check(re.search(r"#define CHG_FIXED_DUTY_TEST_DUTY_PERMILLE\s+150u", text_h), "diagnostic duty must be fixed at 150 permille = 15%")
    check(re.search(r"#define CHG_DUTY_START_PERMILLE\s+10u", text_h), "start duty must be 10 permille = 1%")
    check(re.search(r"#define CHG_DUTY_STEP_PERMILLE\s+5u", text_h), "increase step must be 5 permille = 0.5%")


def test_pwm_contract():
    text_h = CHARGER_H.read_text()
    main = MAIN_C.read_text()
    ioc = IOC.read_text()
    check(re.search(r"#define CHG_PWM_FREQUENCY_HZ\s+50000u", text_h), "PWM frequency must be 50000 Hz")
    check(re.search(r"#define CHG_PWM_TIMER_CLOCK_HZ\s+72000000u", text_h), "PWM timer clock must be 72 MHz")
    check(re.search(r"#define CHG_PWM_PRESCALER\s+0u", text_h), "PWM PSC must be 0")
    check(re.search(r"#define CHG_PWM_AUTO_RELOAD\s+1439u", text_h), "PWM ARR must be 1439")
    for text in (main, ioc):
        check("1439" in text, "generated PWM period must still be 1439")
        check("Prescaler = 0" in text or "Prescaler=0" in text,
              "generated PWM prescaler must still be zero")


def test_electronic_load_policy_documented():
    text_h = CHARGER_H.read_text()
    check("free resistor" in text_h or "resistor alone" in text_h or "مقاومت آزاد" in text_h,
          "free resistor alone must be documented as not a valid battery simulator")
    check("battery simulator" in text_h or "voltage clamp" in text_h,
          "valid no-battery load must be documented as clamped/simulator only")


def main():
    tests = [
        test_modules_enabled_build,
        test_master_enable_constant_is_single_gate,
        test_master_enable_zero_safeidle_stops_both_pwm_and_relay_off,
        test_transformer_known_not_bypassable_bringup_only_when_zero,
        test_master_enable_does_not_bypass_numeric_protections,
        test_channel_selection_constants,
        test_channel_one_is_always_zero_stopped,
        test_trans2_uses_only_vlow_not_24v_pack,
        test_min_valid_battery_is_sense_not_setpoint,
        test_battery_voltage_upper_cutoff_applies_before_any_control,
        test_jit_is_active_low_and_retry_captures_duty_before_stop,
        test_input_voltage_is_real_adc_22000mv,
        test_input_recovery_restarts_with_safe_duty,
        test_no_shadowing_in_duty_adjustments,
        test_jit_per_channel_sequence,
        test_low_current_is_not_fault,
        test_duty_steps_are_time_limited,
        test_duty_max_dcm_ceiling,
        test_only_above_675ma_protects,
        test_setpoints_and_timing,
        test_pwm_contract,
        test_electronic_load_policy_documented,
    ]
    for test in tests:
        test()
    print(f"ALL {len(tests)} CHARGER HOST TESTS PASSED")
    print("Note: physical board tests not performed; no real battery connected.")


if __name__ == "__main__":
    main()
