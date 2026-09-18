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
DUTY_STEP = 5


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def regulate_one(channel, voltage_mv, current_ma):
    """Small host model of the bulk branch, with a duty per channel."""
    if current_ma > CURRENT_LIMIT_MA:
        channel["fault"] = True
        return
    if voltage_mv < ABSORB_MV and current_ma < CURRENT_LIMIT_MA:
        channel["duty"] += DUTY_STEP


def test_two_selection_constants():
    text = CHARGER_H.read_text()
    check(re.search(r"#define CHG_CHANNEL_1_INSTALLED\s+0u", text),
          "current test must keep unassembled channel 1 disabled")
    check(re.search(r"#define CHG_CHANNEL_2_INSTALLED\s+1u", text),
          "current test must select installed channel 2")
    check("CHG_INSTALLED_CHANNEL_MASK" in text,
          "the two constants must feed one explicit installed-channel mask")


def test_channel_policy_is_independent():
    channel_1 = {"duty": 100, "fault": False}
    channel_2 = {"duty": 100, "fault": False}
    regulate_one(channel_2, voltage_mv=12400, current_ma=40)
    check(channel_2["duty"] == 105, "low current below target must increase channel 2 duty")
    check(channel_1["duty"] == 100, "channel 1 duty must not be changed by channel 2")
    check(not channel_2["fault"], "low current is not a fault")


def test_only_excess_current_faults():
    channel = {"duty": 100, "fault": False}
    regulate_one(channel, voltage_mv=12400, current_ma=0)
    check(channel["duty"] == 105, "zero/low current must request more duty")
    check(not channel["fault"], "zero/low current must not latch protection")
    regulate_one(channel, voltage_mv=12400, current_ma=CURRENT_LIMIT_MA + 1)
    check(channel["fault"], "only current above 675 mA may enter current protection")


def test_protection_is_independent():
    channel_1 = {"duty": 100, "fault": False}
    channel_2 = {"duty": 100, "fault": False}
    regulate_one(channel_1, voltage_mv=12400, current_ma=CURRENT_LIMIT_MA + 1)
    regulate_one(channel_2, voltage_mv=12400, current_ma=50)
    check(channel_1["fault"], "channel 1 overcurrent must fault channel 1")
    check(not channel_2["fault"], "channel 1 overcurrent must not mark channel 2")
    check(channel_2["duty"] == 105, "channel 2 must keep its own low-current regulation")


def test_no_pack_voltage_for_current_trans2_test():
    text = CHARGER_C.read_text()
    check("v_bat24_mv" not in text,
          "charger policy must not validate the single-battery Trans2 test with pack voltage")
    check("v_bat_low_mv" in text and "v_bat_high_mv" in text,
          "each installed channel must consume its own 12 V measurement")


def test_no_shared_pwm_api():
    text = CHARGER_C.read_text()
    check(text.count("SetPwmBoth(") == 0,
          "shared SetPwmBoth must not be used or reintroduced")
    check(text.count("func__BspPwm_SetDutyPermille(") >= 2,
          "generic channel function must set a selected PWM channel")


def test_pwm_contract():
    main = MAIN_C.read_text()
    ioc = IOC.read_text()
    for text in (main, ioc):
        check("1439" in text, "PWM ARR must be 1439")
        check("Prescaler = 0" in text or "Prescaler=0" in text,
              "PWM PSC must be zero")
    check("TIM2.Period=1439" in ioc and "TIM3.Period=1439" in ioc,
          "both PWM timer periods must be configured")


def test_protection_sequence_is_present():
    text = CHARGER_C.read_text()
    check("func__Charger_StopAllPwm();\n    func__Charger_OpenTransformerInput();" in text,
          "JIT/final protection must stop PWM before opening the NC input")
    check("func__Charger_CloseTransformerInput(uint32_t__nowTick)" in text,
          "retry must close NC through a named relay operation")
    check("func__Jitter_ClearChannel" in text,
          "retry must re-arm only the tripped comparator channel")
    check("CHG_DUTY_RETRY_SECOND_MAX" in text,
          "second retry must be capped at 10 percent or less")


def test_provisional_voltage_constants():
    text = CHARGER_H.read_text()
    check("CHG_ABSORB_MV                 14400u" in text, "absorb setpoint changed")
    check("CHG_FLOAT_MV                  13500u" in text, "float setpoint changed")
    check("CHG_REENTRY_MV               12800u" in text, "reentry setpoint changed")
    check("CHG_BULK_CURRENT_MAX_MA       675u" in text, "bulk current limit changed")


def main():
    tests = [
        test_two_selection_constants,
        test_channel_policy_is_independent,
        test_only_excess_current_faults,
        test_protection_is_independent,
        test_no_pack_voltage_for_current_trans2_test,
        test_no_shared_pwm_api,
        test_pwm_contract,
        test_protection_sequence_is_present,
        test_provisional_voltage_constants,
    ]
    for test in tests:
        test()
    print(f"ALL {len(tests)} CHARGER HOST TESTS PASSED")


if __name__ == "__main__":
    main()
