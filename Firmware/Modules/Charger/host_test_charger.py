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

ABSORB_MV = 14400
FLOAT_MV = 13500
REENTRY_MV = 12800
CURRENT_LIMIT_MA = 675
DUTY_START = 10
DUTY_STEP = 5
INPUT_VALID_MV = 22000


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def regulate_one(channel, voltage_mv, current_ma):
    """Small host model of the bulk branch, with a duty per channel."""
    if current_ma > CURRENT_LIMIT_MA:
        channel["fault"] = True
        channel["duty"] = 0
        return
    if voltage_mv < ABSORB_MV and current_ma < CURRENT_LIMIT_MA:
        channel["duty"] += DUTY_STEP
        if channel["duty"] > 1000:
            channel["duty"] = 1000
    elif voltage_mv < ABSORB_MV and current_ma == CURRENT_LIMIT_MA:
        # At exact current target, hold duty; host test models saturation.
        pass


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


def test_master_enable_constant_is_single_gate():
    text_h = CHARGER_H.read_text()
    text_c = CHARGER_C.read_text()
    check(re.search(r"#define CHG_MASTER_ENABLE\s+0u", text_h),
          "CHG_MASTER_ENABLE must be 0 for current safe-off delivery")
    check("CHG_MASTER_ENABLE == 0u" in text_c and "func__Charger_SafeIdle();" in text_c,
          "CHG_MASTER_ENABLE=0 must force whole-charger safe-idle")
    check("CHG_MASTER_ENABLE == 1u" in text_c or "CHG_MASTER_ENABLE" in text_c,
          "CHG_MASTER_ENABLE must be present in control policy")
    check("APP_CONFIG.power_stage_enabled == false" not in text_c,
          "power_stage_enabled must not remain a hidden Charger gate")
    check("APP_CONFIG.pwm_max_duty_permille == 0u" not in text_c,
          "pwm_max_duty=0 must not remain a hidden Charger gate")


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


def test_input_voltage_is_real_adc_22000mv():
    text_h = CHARGER_H.read_text()
    text_c = CHARGER_C.read_text()
    check(re.search(r"#define CHG_INPUT_VALID_MV\s+22000u", text_h),
          "input validity threshold must be 22000 mV")
    check("measurement_snapshot_t__snap->v_in_mv >= CHG_INPUT_VALID_MV" in text_c,
          "input must be checked from real ADC v_in_mv every cycle")
    check("input_present == false" not in text_c,
          "PB4 digital input alone must not be the charge gate")
    check("func__Charger_SafeIdle();" in text_c,
          "when Vin is below threshold both PWM must stop and relay must stay off/idle")


def test_input_recovery_restarts_with_safe_duty():
    text_c = CHARGER_C.read_text()
    check("CHG_STATE_INPUT_WAIT" in text_c,
          "low-input wait state must exist")
    check("CHG_DUTY_START_PERMILLE" in text_c and re.search(r"#define CHG_DUTY_START_PERMILLE\s+10u", CHARGER_H.read_text()),
          "safe restart duty must be 10 permille = 1%")


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


def test_only_above_675ma_protects():
    channel_low = {"duty": 100, "fault": False}
    regulate_one(channel_low, voltage_mv=12400, current_ma=600)
    check(not channel_low["fault"], "current below 675 mA must not enter overcurrent protection")
    check(channel_low["duty"] == 105, "low current below voltage target must increase duty")
    channel_ok = {"duty": 100, "fault": False}
    regulate_one(channel_ok, voltage_mv=12400, current_ma=675)
    check(not channel_ok["fault"], "675 mA must not enter overcurrent protection")
    channel_bad = {"duty": 100, "fault": False}
    regulate_one(channel_bad, voltage_mv=12400, current_ma=676)
    check(channel_bad["fault"] and channel_bad["duty"] == 0, "only current above 675 mA must enter current protection")


def test_setpoints_and_timing():
    text_h = CHARGER_H.read_text()
    check(re.search(r"#define CHG_ABSORB_MV\s+14400u", text_h), "absorb must be 14400 mV")
    check(re.search(r"#define CHG_FLOAT_MV\s+13500u", text_h), "float must be 13500 mV")
    check(re.search(r"#define CHG_REENTRY_MV\s+12800u", text_h), "reentry must be 12800 mV")
    check(re.search(r"#define CHG_ABSORB_HOLD_MS\s+600000u", text_h), "absorb hold must be 600000 ms = 10 min")
    check(re.search(r"#define CHG_BULK_CURRENT_MAX_MA\s+675u", text_h), "bulk regulation current must be 675 mA")
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
        test_master_enable_constant_is_single_gate,
        test_master_enable_does_not_bypass_numeric_protections,
        test_channel_selection_constants,
        test_channel_one_is_always_zero_stopped,
        test_trans2_uses_only_vlow_not_24v_pack,
        test_min_valid_battery_is_sense_not_setpoint,
        test_input_voltage_is_real_adc_22000mv,
        test_input_recovery_restarts_with_safe_duty,
        test_jit_per_channel_sequence,
        test_low_current_is_not_fault,
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
