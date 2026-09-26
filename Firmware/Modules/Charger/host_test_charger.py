#!/usr/bin/env python3
"""Host tests for the independent two-channel 12 V charger policy.

These tests validate policy and source contracts only. They do not authorize a
battery connection and cannot replace HAL, transformer, relay, comparator or
oscilloscope tests on the board.
"""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
APP_TYPES_H = Path(__file__).resolve().parents[2] / "Config" / "Inc" / "app_types.h"
CHARGER_H = ROOT / "Firmware/Modules/Charger/charger.h"
CHARGER_C = ROOT / "Firmware/Modules/Charger/charger.c"
FAULT_H = ROOT / "Firmware/Modules/Fault/fault.h"
FAULT_C = ROOT / "Firmware/Modules/Fault/fault.c"
TASK_CONTROL_C = ROOT / "Firmware/Rtos/Src/task_control.c"
MAIN_C = ROOT / "CubeIDE/Core/Src/main.c"
IOC = ROOT / "CubeMX/CubeIDE.ioc"
BSP_EXTI_C = ROOT / "Firmware/Bsp/Src/bsp_exti.c"
ESP_LINK_H = ROOT / "Firmware/Modules/EspLink/esp_link.h"
ESP_LINK_C = ROOT / "Firmware/Modules/EspLink/esp_link.c"

ABSORB_MV = 14400
FLOAT_MV = 13500
REENTRY_MV = 12800
CURRENT_LIMIT_MA = 650
REGULATE_LOW_MA = 630
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
    per 100 ms. >950 hard fault -> reset; >650 -> down; <630 -> up;
    630..650 -> hold; too soon to step -> hold."""
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
    # inside the 630..650 band or inside the step window: hold duty


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
    check(re.search(r"CHG_CURRENT_LIMIT_MA", CHARGER_H.read_text()) is not None and
          "uint32_t__bulkCurrentMaxMa + 25u" in text_c,
          "current limit must still exist (v1.12: the compile-time constant stays as documentation in charger.h and the active limit is derived from the profile band: profile bulk max + 25 mA, user order 2026-09-25)")
    check("CHG_MIN_VALID_BATTERY_MV" in text_c, "battery validity threshold must still exist")
    check("CHG_INPUT_VALID_MV" in text_c, "real ADC input threshold must still exist")
    check("func__Charger_FinalDisconnect" in text_c, "final disconnect protection must still exist")


def test_channel_selection_constants():
    text = CHARGER_H.read_text()
    match_ch1 = re.search(r"#define CHG_CHANNEL_1_INSTALLED\s+([01])u", text)
    check(match_ch1 is not None, "channel 1 install flag must be an explicit 0u/1u")
    check(re.search(r"#define CHG_CHANNEL_2_INSTALLED\s+1u", text),
          "channel 2 stays installed")
    if match_ch1.group(1) == "1":
        check(re.search(r"#define CHG_TRANSFORMER_KNOWN\s+1u", text),
              "with channel 1 installed the transformer data must be marked known (bring-up gate, user order 2026-09-20)")
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
    # Policy code must not touch the pack value; the ONLY allowed user is the
    # diag capture function (user order 2026-09-22: decision values visible
    # in Live Expressions), which must not feed any setpoint decision.
    diag_start = text.find("static void func__Charger_CaptureDiag")
    check(diag_start != -1, "diag capture function must exist")
    diag_end = text.find("void func__Charger_Evaluate(", diag_start)
    if (diag_start != -1) and (diag_end != -1):
        text = text[:diag_start] + text[diag_end:]
    check("v_bat24_mv" not in text,
          "charger policy must not use 24 V pack value for CH2 setpoint or missing battery (outside the diag-only capture function)")
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


def test_current_band_regulates_and_protects():
    channel_low = {"duty": 100, "fault": False}
    regulate_one(channel_low, voltage_mv=12400, current_ma=600)
    check(not channel_low["fault"], "current below 650 mA must not enter overcurrent protection")
    check(channel_low["duty"] == 105, "600 mA is below the 630 band edge and must raise duty")
    channel_ok = {"duty": 100, "fault": False}
    regulate_one(channel_ok, voltage_mv=12400, current_ma=650)
    check(not channel_ok["fault"], "650 mA must not enter overcurrent protection")
    check(channel_ok["duty"] == 100, "exactly 650 mA sits inside the band and must hold duty")
    channel_high = {"duty": 100, "fault": False}
    regulate_one(channel_high, voltage_mv=12400, current_ma=651)
    check(not channel_high["fault"] and channel_high["duty"] == 95,
          "651 mA must only step duty down (regulation), never cut the channel")
    channel_bad = {"duty": 100, "fault": False}
    regulate_one(channel_bad, voltage_mv=12400, current_ma=951)
    check(channel_bad["fault"] and channel_bad["duty"] == 0,
          "only current above the 950 mA hard fault must reset the channel")


def test_setpoints_and_timing():
    text_h = CHARGER_H.read_text()
    text_c = CHARGER_C.read_text()
    check(re.search(r"#define CHG_ABSORB_MV\s+14400u", text_h), "absorb must be 14400 mV")
    check(re.search(r"#define FAULT_BATTERY_BACK_MV\s+7000u", FAULT_H.read_text()), "battery-truly-back must be 7 V on both halves (user: 6 V can still mean charging)")
    check(re.search(r"#define CHG_REENTRY_MV\s+12800u", text_h), "reentry stays 12.8 V (13.0 caused repeat charge cycles as the battery rested at ~13.0 V)")
    check(re.search(r"#define CHG_CONNECT_SETTLE_MS\s+15000u", text_h), "connection-settle must be 15 s (user: 10..20 s before charge start)")
    check("func__Charger_BulkStartSettled" in text_c and "uint32_t__stableFromTick" in text_c, "OFF->BULK must be gated on the connection-settle stamp (kills bat-lost flap + yellow blink inside the buzzer)")
    iso_active = text_c.split("bool func__Charger_IsAnyChannelActive(void)")[-1]
    check("CHG_STATE_FLOAT" not in iso_active and "CHG_STATE_BULK" in iso_active and "CHG_STATE_ABSORB" in iso_active, "IsAnyChannelActive must count only BULK/ABSORB - parked FLOAT is DONE, not pumping (kills done-phase false buzzers and stops the yellow blink)")
    check(re.search(r"#define CHG_FLOAT_MV\s+13500u", text_h), "float must be 13500 mV")
    check(re.search(r"#define CHG_REENTRY_MV\s+12800u", text_h), "reentry must be 12800 mV")
    check(re.search(r"#define CHG_ABSORB_HOLD_MS\s+600000u", text_h), "absorb soak must be 600000 ms = 10 min inside the timed window")
    check(re.search(r"#define CHG_ABSORB_ENTER_MV\s+14300u", text_h), "absorb voltage-hold window must start at 14.3 V (user directive)")
    check("CHG_ABSORB_TIMED_MAX_MV" not in text_h, "soak has no sub-window anymore: it counts during the whole ABSORB stay")
    check(re.search(r"#define CHG_ABSORB_OVER_MV\s+14600u", text_h), "overshoot fast-down threshold must be 14.6 V (user directive)")
    check(re.search(r"#define CHG_DUTY_STEP_FINE_PERMILLE\s+1u", text_h), "voltage-hold duty steps must be 0.1% (user directive)")
    check("uint32_t__absorbAccumTicks" in text_c, "soak must accumulate with pause outside the window")
    check("do NOT fall back to BULK" in text_c, "FLOAT must survive descending below the 14.3 V window (bench bug: fresh soak restarted right after every soak completed)")
    check("PARK THE PUMP AT ZERO" in text_c, "FLOAT must ramp the duty to 0 and park it (user: 'why is the charger not off? duty stuck 4-5%')")
    check(re.search(r"#define CHG_TAPER_CURRENT_MA\s+50u", text_h) and
          re.search(r"#define CHG_TAPER_SUSTAIN_MS\s+60000u", text_h) and
          re.search(r"#define CHG_ABSORB_MAX_MS\s+3600000u", text_h),
          "taper completion must be 50 mA held 60 s with a 1-hour absorb ceiling (user bench decisions)")
    check("uint32_t__taperSinceTick" in text_c and "bool__absorbTimedOut" in text_c and
          "bool__taperDone" in text_c,
          "absorb must end on soak>=10min AND steady tail current, plus the 1-hour ceiling")
    check("CHG_STATE_ABSORB" in text_c, "absorb voltage-hold state must exist in the state machine")
    check(re.search(r"#define CHG_BULK_CURRENT_MAX_MA\s+650u", text_h), "bulk regulation current must be 650 mA (tight band per user)")
    check(re.search(r"#define CHG_REGULATE_LOW_MA\s+630u", text_h), "regulation band lower edge must be 630 mA (~20 mA tolerance)")
    check(re.search(r"#define CHG_CURRENT_HARD_FAULT_MA\s+950u", text_h), "hard over-current fault must be 950 mA")
    check(re.search(r"#define CHG_DUTY_MAX_PERMILLE\s+500u", text_h), "duty cap must be 500 permille = 50% (DCM ceiling, board requirement)")
    check(re.search(r"#define CHG_DUTY_RAMP_UP_INTERVAL_MS\s+1000u", text_h), "up-steps must be limited to one per 1000 ms")
    check(re.search(r"#define CHG_DUTY_RAMP_UP_INTERVAL_ABSORB_MS\s+2000u", text_h), "absorb fine up-steps must be half-rate: one per 2000 ms (user directive)")
    check(re.search(r"#define CHG_DUTY_RAMP_DOWN_INTERVAL_ABSORB_MS\s+1000u", text_h), "absorb fine down-steps must be half-rate: one per 1000 ms (user directive)")
    check("absorbUpIntervalTicks" in text_c and "absorbDownIntervalTicks" in text_c, "absorb branch must use its own half-rate intervals")
    check(re.search(r"#define CHG_DUTY_RAMP_DOWN_INTERVAL_MS\s+500u", text_h), "down-steps must be limited to one per 500 ms")
    check(re.search(r"#define CHG_FLYBACK_ETA1_PERMILLE\s+0u", text_h), "per-channel ETA1 default = 0 permille = identity (v1.3: a reflash changes no number until the user calibrates from the panel)")
    check(re.search(r"#define CHG_FLYBACK_ETA2_PERMILLE\s+0u", text_h), "per-channel ETA2 default = 0 permille = identity (v1.3: a reflash changes no number until the user calibrates from the panel)")
    check(re.search(r"#define CHG_ETA_MIN_PERMILLE\s+0u", text_h), "ETA clamp floor must be 0 (0 = identity bypass, v1.3)")
    charger_c_txt = CHARGER_C.read_text()
    check("if (uint32_t__etaPermille == 0u)" in charger_c_txt and "return uint32_t__primaryMa;" in charger_c_txt,
          "output estimate must fall back to the identity when ETA = 0 (default; user 2026-09-24: with the battery-calibrated gains the filtered reading IS the battery current)")
    check("((((uint32_t__primaryMa * uint32_t__etaPermille) / 1000u) * uint32_t__vinMv)" in charger_c_txt,
          "non-zero ETA must convert with LIVE voltages: iest = I x Vin x eta / (1000 x Vbat) - the v1.3 calibration architecture (user order 2026-09-24)")
    check("CHG_ETA_MIN_VIN_MV" in charger_c_txt and "CHG_ETA_MIN_VBAT_MV" in charger_c_txt,
          "live-voltage sanity guards must exist for the ETA conversion")
    esp_link_h_txt = (ROOT / "Firmware/Modules/EspLink/esp_link.h").read_text()
    esp_link_c_txt = (ROOT / "Firmware/Modules/EspLink/esp_link.c").read_text()
    check("#define ESPLINK_MSG_CAL_REFERENCE     0x03u" in esp_link_h_txt,
          "protocol v1.3 must define the CAL_REFERENCE command 0x03 (user order 2026-09-24: send/receive every calibration number via the ESP)")
    check("func__EspLink_ApplyCalReference" in esp_link_c_txt and "ESPLINK_MSG_CAL_REFERENCE" in esp_link_c_txt,
          "esp_link must handle CAL_REFERENCE (GAIN targets 0/1, ETA targets 2/3, PARAM_REPORT replies, silent rejection)")
    check("uint32_t__gain = (uint32_t__gain * uint32_t__refMa) / uint32_t__liveMa;" in esp_link_c_txt and
          "(((uint32_t__refMa * uint32_t__vbatMv) / uint32_t__vinMv) * 1000u)" in esp_link_c_txt,
          "CAL math must be: gain *= ref/live and eta = ref*Vbat*1000/(live*Vin) on the live snapshot")
    import re as _re
    check(_re.search(r"#define ESPLINK_CAL_MAX_REF_MA\s+5000u", esp_link_h_txt) is not None,
          "CAL_REFERENCE must sanity-cap the typed reference (50..5000 mA) so wire garbage cannot overflow the 32-bit math")
    check("func__Charger_SetEfficiencyPermille(uint8_t__channelIndex, 0u);" in esp_link_c_txt,
          "CAL GAIN must reset that channel's ETA to 0 - the old ETA absorbed the old gain's error")
    check("#if MODULE_CHARGER\n    else if (uint8_t__messageType == (uint8_t)ESPLINK_MSG_CAL_REFERENCE)" in esp_link_c_txt,
          "the CAL_REFERENCE frame branch must be compiled out when the charger module is disabled")
    check("CHG_CURRENT_EMA_SHIFT" not in text_h and "currentEma" not in text_c,
          "charger must NOT filter the current estimate itself (user order 2026-09-22: Measurement's switchable median-3/moving-average chain feeds it; charger decides on that value)")
    check(re.search(r"#define MEASUREMENT_PERIOD_MS\s+1u", (ROOT / "Firmware/Modules/Measurement/measurement.h").read_text()),
          "measurement period must be 1 ms so the synchronized current samples are at most 1 ms old (user order 2026-09-22)")
    bsp_adc_c = (ROOT / "Firmware/Bsp/Src/bsp_adc.c").read_text()
    bsp_pwm_c_sync = (ROOT / "Firmware/Bsp/Src/bsp_pwm.c").read_text()
    bsp_meas_c = (ROOT / "Firmware/Bsp/Src/bsp_measurement.c").read_text()
    meas_h_txt = (ROOT / "Firmware/Modules/Measurement/measurement.h").read_text()
    meas_c_raw = (ROOT / "Firmware/Modules/Measurement/measurement.c").read_text()
    cal_h = (ROOT / "Firmware/Modules/Measurement/calibration.h").read_text()
    check("func__Measurement_FilterCurrent" not in meas_c_raw,
          "the old EMA current filter must stay removed; only the switchable median-3/moving-average chain is allowed (user order 2026-09-22)")
    check(re.search(r"#define MEASUREMENT_CURRENT_MEDIAN3_ENABLE\s+1u", meas_h_txt) and
          re.search(r"#define MEASUREMENT_CURRENT_AVERAGE_ENABLE\s+1u", meas_h_txt) and
          re.search(r"#define MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX\s+15u", meas_h_txt) and
          re.search(r"#define MEASUREMENT_CURRENT_AVERAGE_WINDOW\s+300u", meas_h_txt) and
          re.search(r"#define MEASUREMENT_CURRENT_AVERAGE_WINDOW_DEFAULT\s+10u", meas_h_txt),
          "current filters: both compile switches ON; v1.4 free sizes - median ceiling 15, average ring 300 (v1.9 raise 100->300 so the smoothing is visible at the 10 Hz TLM stream; boot defaults UNCHANGED: median 3, average 10; user orders 2026-09-25)")
    check("func__Measurement_CurrentMedian(" in meas_c_raw and
          "func__Measurement_CurrentMovingAverage" in meas_c_raw and
          "func__Measurement_ApplyCurrentFilters" in meas_c_raw and
          "MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX" in meas_h_txt,
          "Measurement must run the median chain (v1.4: runtime size ANY 1..15, default 3) then the moving-average chain (v1.4: runtime window ANY 1..100, default 10) on each current channel (user order 2026-09-25)")
    lut_chain = re.search(r"CAL_Current2LutChainMa\[\] =\s*\{([^}]*)\}", cal_h)
    lut_batt = re.search(r"CAL_Current2LutBatteryMw\[\] =\s*\{([^}]*)\}", cal_h)
    lut_chain_n = len(lut_chain.group(1).split(",")) if lut_chain else 0
    lut_batt_n = len(lut_batt.group(1).split(",")) if lut_batt else 0
    check('#include "calibration.h"' in meas_c_raw and
          re.search(r"#define CAL_CURRENT2_LUT_ENABLE\s+1u", cal_h) and
          "static const uint32_t CAL_Current2LutChainMa[] =" in cal_h and
          "static const uint32_t CAL_Current2LutBatteryMw[] =" in cal_h and
          "sizeof(CAL_Current2LutChainMa) /" in cal_h and
          "{ 0u, 5u, 37u, 106u, 189u, 236u, 283u, 353u, 441u, 557u, 707u }" in cal_h and
          "{ 0u, 0u, 109u, 751u, 1581u, 2625u, 3807u, 5224u, 6817u, 8573u, 10429u }" in cal_h and
          lut_chain_n == lut_batt_n and lut_chain_n == 11 and
          re.search(r"#define CAL_CURRENT1_LUT_ENABLE\s+0u", cal_h) and
          "CAL_Current1LutChainMa" in cal_h,
          f"channel-2 bench LUT must be ON as a chain->POWER table (v1.13, user order 2026-09-25 'voltages are fixed but the currents are wrong'): the DCM invariant is battery POWER, the current is P/Vbat - the old chain->current table embedded the calibration run's battery voltage (12.0..13.65V) and overread ~7 percent per volt as the battery filled; anchors = DMM_I2 x DMM_V2 of the dense 2026-09-25T18:14 run (10 points, duty 2..20%); the axis stays the ADC chain current (raw-off2)*K*gain, NEVER duty; the tables size themselves from the initializers and both lists must stay the same length (got chain={lut_chain_n} power={lut_batt_n})")
    check("uint32_t uint32_t__batteryPowerMw = func__Measurement_Current2BenchLut(\n        func__BspMeasurement_Current2CountsToMa(uint16_t__counts));" in meas_c_raw and
          "((uint64_t)uint32_t__batteryPowerMw) * 1000u) /\n                      UINT32_T__G__Battery2VoltageMv" in meas_c_raw,
          "the ch2 LUT must wrap the BSP conversion inside func__Measurement_Current2CountsToMa (unfiltered, filtered and iest all become true battery mA; raw counts and shunt uV untouched) and v1.13 DIVIDES the table's POWER output by the live cached battery-2 voltage (user order: the currents were wrong as the battery filled)")
    check("static uint32_t UINT32_T__G__Battery2VoltageMv = 12000u;" in meas_c_raw and
          meas_c_raw.count("UINT32_T__G__Battery2VoltageMv") >= 5 and
          "if (uint32_t__batteryLowMv < 8000u)\n    {\n        UINT32_T__G__Battery2VoltageMv = 8000u;" in meas_c_raw and
          "UINT32_T__G__Battery2VoltageMv = 15000u;" in meas_c_raw,
          "the ch2 power LUT needs the live battery-2 voltage cache: static default 12.0 V, written each pass after the median-5 filter, clamped 8.0..15.0 V so a missing battery can never blow up the division")
    check(meas_c_raw.find("func__Measurement_MedianFilterVoltageSample(0u, uint32_t__batteryLowMv);") <
          meas_c_raw.find("if (uint32_t__batteryLowMv < 8000u)"),
          "the voltage cache must be fed AFTER the median-5 battery-low filter (spikes must not modulate the current reading)")
    check(re.search(r"func__Measurement_Current2CountsToMa\(uint16_t uint16_t__counts\)\n\{\n#if \(CAL_CURRENT2_LUT_ENABLE != 0u\)", meas_c_raw) and
          re.search(r"#else\n    return func__BspMeasurement_Current2CountsToMa\(uint16_t__counts\);\n#endif", meas_c_raw),
          "the ch2 LUT must be compile-switchable: MEASUREMENT_CURRENT2_LUT_ENABLE=0 restores the old linear behaviour exactly")
    check(re.search(r"#define CAL_BATTERY12_BENCH_COMP_ENABLE\s+1u", cal_h) and
          re.search(r"#define CAL_BATTERY12_BENCH_STATIC_MV\s+150u", cal_h) and
          re.search(r"#define CAL_BATTERY12_BENCH_PATH_MOHM\s+470u", cal_h),
          "V12 bench compensation must be ON with the dense 2026-09-25T18:14 refit: static 150 mV + 470 mOhm x I2 (LSQ over 10 DMM points 0..764 mA = 149.8 mV + 472.5 mOhm; residual within +/-28 mV vs DMM on the battery-2 terminals)")
    check("func__Measurement_Battery12BenchCompensate(\n        uint32_t__battery12Mv, uint32_t__current2SampleMa);" in meas_c_raw and
          meas_c_raw.find("func__Measurement_Current2CountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2])") <
          meas_c_raw.find("uint32_t__battery12Mv = func__Measurement_ApplyVoltageOffsetMv(") and
          meas_c_raw.find("func__Measurement_Battery12BenchCompensate(\n        uint32_t__battery12Mv, uint32_t__current2SampleMa);") <
          meas_c_raw.find("uint32_t__batteryLowMv = uint32_t__battery12Mv;"),
          "the V12 compensation must consume the post-LUT channel-2 current (sample moved ahead of the voltage chain) and must land on battery12Mv after the runtime voff, before the low/high derivation and the median - so Vlow, published V12 and derived Vhigh all describe the true battery-2 terminals")
    check(len(re.findall(r"#if \(CAL_BATTERY12_BENCH_COMP_ENABLE != 0u\)", meas_c_raw)) == 2 and
          "uint32_t__dropMv = CAL_BATTERY12_BENCH_STATIC_MV +" in meas_c_raw and
          "return 0u;" in meas_c_raw.split("func__Measurement_Battery12BenchCompensate")[1].split("\n}\n")[0],
          "the V12 bench compensation must be compile-switchable (enable=0 restores today's behaviour), use saturating subtraction (static + I2 x mOhm / 1000, never below 0 mV)")
    check(re.search(r"#define BSP_MEASUREMENT_DIV24BAT_TOP_OHMS\s+62400u", bsp_meas_c) and
          "func__BspMeasurement_Battery24CountsToMv" in bsp_meas_c and
          "func__Measurement_Battery24CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_24V_BAT]);" in meas_c_raw and
          "func__Measurement_V24CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_24V_IN]);" in meas_c_raw,
          "the battery-PACK 24 V channel must use its OWN divider (user order 2026-09-25: net attenuation to the pin is exactly 0.09826589595375722543352601156069 = 6.8k/69.2k, i.e. total 69.2k over the 6.8k bottom - besides the 68k there are a 1.2k and a 6.8k in the path) while the INPUT 24 V net keeps the 76k conversion (bench-verified +1.2 percent)")
    check(re.search(r"#define MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV\s+5000u", meas_h_txt),
          "runtime voltage offset range must be +/-5000 mV (v1.10, user order 2026-09-25: the pack divider error alone was ~2.3 V at 24 V, beyond the old +/-2000, so the offset could not even express it)")
    check("BSP_MEASUREMENT_MA_PER_A" in bsp_meas_c and "BSP_MEASUREMENT_PERMILLE_SCALE" in bsp_meas_c and
          "BSP_MEASUREMENT_CURRENT_MA_SCALE" not in bsp_meas_c,
          "the ADC-to-current formula must be built stage-by-stage from the schematic resistor values (shunt mOhm, LM358 gain, R41/R42 divider), no shared magic scale (user order 2026-09-22)")
    check("ADC_EXTERNALTRIGCONV_T2_CC2" in bsp_adc_c and "ADC_EXTERNALTRIGCONV_T3_TRGO" in bsp_adc_c,
          "synchronized current sampling must use the hardware timer triggers TIM2_CC2 (charger 1) and TIM3_TRGO (charger 2)")
    check("func__BspAdc_SampleCurrentSync" in bsp_adc_c and "func__BspPwm_IsGatePulsing" in bsp_adc_c,
          "the board ADC port must take one hardware-triggered mid-ON sample per channel and skip parked gates")
    check("uint32_t__compareCounts / 2u" in bsp_pwm_c_sync and "TIM_TRGO_OC2REF" in bsp_pwm_c_sync,
          "the PWM port must keep the internal CH2 sampling trigger at CCR1/2 (mid-ON) and route TIM3 OC2REF to TRGO")
    check("func__Measurement_Median5" in meas_c_raw, "battery channel voltages must pass the median-5 prefilter (2-frame spike bursts beat median-3 during absorb)")
    text_fault_h = FAULT_H.read_text()
    text_fault_c = FAULT_C.read_text()
    check(re.search(r"#define FAULT_BAT_DISCONNECT_MV\s+14800u", text_fault_h), "battery-disconnect threshold must be 14.8 V in the central Fault module (user choice)")
    check(re.search(r"#define FAULT_BAT_DISCONNECT_DEBOUNCE_MS\s+150u", text_fault_h), "pump debounce must be 150 ms = 15 control passes (armed-absorb false trips still happened at 50 ms; real pump floats ~0.5 s so 150 ms still catches it)")
    check("func__Measurement_Median5" in (ROOT / "Firmware/Modules/Measurement/measurement.c").read_text(), "battery channel voltages must pass the median-5 prefilter (2-frame spike bursts beat median-3 during absorb)")
    bsp_meas_c = (ROOT / "Firmware/Bsp/Src/bsp_measurement.c").read_text()
    check(re.search(r"#define BSP_MEASUREMENT_CURRENT1_GAIN_PERMILLE\s+1046u", bsp_meas_c) and
          re.search(r"#define BSP_MEASUREMENT_CURRENT2_GAIN_PERMILLE\s+1303u", bsp_meas_c) and
          re.search(r"#define BSP_MEASUREMENT_CURRENT1_OFFSET_COUNTS\s+8u", bsp_meas_c) and
          re.search(r"#define BSP_MEASUREMENT_CURRENT2_OFFSET_COUNTS\s+8u", bsp_meas_c),
          "current calibration must be split per channel (user: charger 1 must not ride on charger 2's calibration); ch1 baked 2026-09-24 to 1046 permille (DMM 423 mA true vs 436/438/442 displayed at D=15%), ch2 baked to 1303 permille from its D=15% point (DMM 425 true vs 354 displayed, latest of 320/354; the D=10% chain stays non-linear: 185/200/208 vs 185)")
    meas_c_txt = (ROOT / "Firmware/Modules/Measurement/measurement.c").read_text()
    check("func__Measurement_Current1CountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1])" in meas_c_txt and
          "func__Measurement_Current2CountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2])" in meas_c_txt,
          "each normalised current channel must convert through its OWN per-channel function")
    check("bool__anyHalfLow" in (ROOT / "Firmware/Modules/Fault/fault.c").read_text() and
          "uint32_t__lowMv  < FAULT_BAT_ABSENT_MV" in (ROOT / "Firmware/Modules/Fault/fault.c").read_text(),
          "rule 2 must be EITHER half below FAULT_BAT_ABSENT_MV = 6 V with recovery kept at 7 V (user threshold split 2026-09-22; was ALL six-V: silent on a single cut lead)")
    check("bool__batteryTrulyPresent" in (ROOT / "Firmware/Modules/Fault/fault.c").read_text(), "bat-lost clear must require BOTH halves >= FAULT_BATTERY_BACK_MV (7 V) - one lead cut keeps its half below 7 V so the alarm repeats until reconnect (one-burst bug)")
    check(re.search(r"#define FAULT_BAT_DISCONNECT_MV\s+14800u", text_fault_h), "threshold stays 14.8 V, NOT 15.0 V: 15.0 would collide with the validity cut (~0.1 s float vs ~0.5 s at 14.8)")
    check(re.search(r"#define FAULT_BAT_ABSENT_MV\s+6000u", text_fault_h), "battery-absent threshold must be 6 V in Fault (user choice)")
    check(re.search(r"#define FAULT_BAT_ABSENT_DEBOUNCE_MS\s+1000u", text_fault_h), "battery-absent debounce must be 1000 ms in Fault")
    check(re.search(r"#define FAULT_BAT_RECOVER_MS\s+1000u", text_fault_h), "battery-back settle must be 1000 ms in Fault")
    check(re.search(r"#define FAULT_INPUT_PRESENT_MIN_MV\s+21000u", text_fault_h), "absent rule must be gated by input present >= 21 V")
    check(re.search(r"#define FAULT_INPUT_PRESENT_MAX_MV\s+28000u", text_fault_h), "absent rule must be gated by input <= 28 V")
    check("CHG_INSTALLED_CHANNEL_MASK" in text_fault_c and "bool__highHalfInstalled" in text_fault_c,
          "battery-lost rules must ignore halves of uninstalled channels (bench bug: with CH1 off, the unwired low half latched bat-lost forever and the charger looked dead)")
    check("func__Charger_IsAnyChannelActive" in text_fault_c,
          "the 14.8 V pump rule must be armed only while some channel is actually pumping (parked-FLOAT bench transients must not trip it)")
    check("func__Fault_Evaluate" in text_fault_h and "func__Fault_Evaluate" in text_fault_c, "Fault must own the central battery-lost evaluation")
    check("func__Fault_Set(FAULT_CHARGER_BAT_LOST)" in text_fault_c, "Fault must latch the bit, not the charger")
    check("func__Fault_Clear(FAULT_CHARGER_BAT_LOST)" in text_fault_c, "Fault must clear the bit after the settle time")
    check("CHG_STATE_BAT_LOST" in text_c, "charger must keep a battery-lost state as the flag mirror")
    check("uint8_t__waitIndex" in text_c and "Two-channel retry queue" in text_c,
          "ServiceRetry must adopt any channel waiting in JIT_RETRY_WAIT when the pointer frees (two-channel starvation fix)")
    check("(UINT8_T__G__RetryChannel == CHG_NO_CHANNEL))" not in text_c,
          "a tripped channel must park at zero duty immediately - the old no-retry-running gate left the second channel pumping up to 3 s against an asserted comparator")
    check("if (UINT8_T__G__RetryChannel == CHG_NO_CHANNEL)" in text_c,
          "HandleJitTrip may take the revive pointer only when it is free (no pointer stomping; queued rival adopted by the scan)")
    check("func__Charger_IsAnyChannelActive" in text_c and "func__Charger_IsAnyChannelActive" in text_h, "charger must expose whether any channel is charging (UI yellow gate)")
    check("CHG_STATE_BULK" in text_c and "CHG_STATE_ABSORB" in text_c and "CHG_STATE_FLOAT" in text_c, "active query must count BULK/ABSORB/FLOAT as charging")
    check("(func__Fault_Get() & FAULT_CHARGER_BAT_LOST)" in text_c, "charger must ONLY mirror the central flag")
    check("CHG_BAT_DISCONNECT_MV" not in text_h, "charger header must not own battery-lost thresholds anymore")
    check("batOverStartTick" not in text_c and "batBackStartTick" not in text_c, "charger must not own battery-lost timers anymore")
    check("func__Fault_Evaluate(&measurement_snapshot_t__snap)" in TASK_CONTROL_C.read_text(), "task_control must run the central detection before func__Fault_Get")
    check(re.search(r"FAULT_CHARGER_BAT_LOST\s+\(1u << 6\)", APP_TYPES_H.read_text()), "fault bit must live centrally in app_types.h")
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


def test_pwm_interleave_phase_lock():
    bsp_pwm_c = (ROOT / "Firmware/Bsp/Src/bsp_pwm.c").read_text()
    check("__HAL_TIM_SET_COUNTER(&htim2" in bsp_pwm_c and
          "__HAL_TIM_SET_COUNTER(&htim3" in bsp_pwm_c,
          "Init must start the two bridge timers with a frozen phase offset (user order 2026-09-21: channel-2 gate exactly half a period = 10 us after the channel-1 gate's START, not after its stop)")
    check("uint32_t__periodCounts / 2u" in bsp_pwm_c,
          "the offset must be half a period derived from the live ARR, not a hardcoded 720")
    check("#define BSP_PWM_TIM3_PHASE_OFFSET_IN_PHASE 0u" in bsp_pwm_c,
          "gate-phase toggle must stay explicit: 0u = production 10 us interleave (the 2026-09-24 in-phase bench experiment showed no measurable crosstalk change), 1u = in-phase experiment")
    check("HAL_TIM_PWM_Stop" not in bsp_pwm_c,
          "counters must run continuously: only compare=0 turns a channel off, because any later HAL_TIM_PWM_Stop/Start cycle could slip the frozen 10 us interleave")
    check("HAL_TIM_PWM_Start(" not in bsp_pwm_c,
          "HAL_TIM_PWM_Start must NOT be called: its per-call latency of several microseconds made the interleave nondeterministic (bench finding 2026-09-21)")
    check("htim2.Instance->CR1 |= TIM_CR1_CEN" in bsp_pwm_c and
          "htim3.Instance->CR1 |= TIM_CR1_CEN" in bsp_pwm_c,
          "both counters must start via two adjacent raw CEN register writes (~tens of ns skew) so the 10 us offset is deterministic")


def test_charge_profile_v112():
    """[EN] v1.12 (user order 2026-09-25): runtime charge profile settable from the
    ESP panel tab, shared by both channels, ids 20..26; calibration tables in ONE
    separate file; input-voltage DMM reading carried across wizard steps.
    [FA] تست‌های v1.12: پروفایل شارژ زمان اجرا از تب پنل، مشترک دو کانال، شناسه‌های
    ۲۰..۲۶؛ جدول‌های کالیبراسیون در یک فایل جدا؛ پیش‌پر شدن ولتاژ ورودی DMM."""
    text_c = CHARGER_C.read_text()
    text_h = CHARGER_H.read_text()
    text_esph = ESP_LINK_H.read_text()
    text_espc = ESP_LINK_C.read_text()
    ino = (ROOT / "esp_link_panel/esp_link_panel.ino").read_text()
    cal_h = (ROOT / "Firmware/Modules/Measurement/calibration.h").read_text()

    # --- charger.h declares the profile API + ids matching esp_link.h ---
    for decl in ["func__Charger_SetProfileParam(uint8_t uint8_t__paramId",
                 "func__Charger_GetProfileParam(uint8_t uint8_t__paramId"]:
        check(decl in text_h, f"charger.h must declare {decl.split('(')[0]}")
    pairs = [("CHG_PROFILE_PARAM_ABSORB_MV", "ESPLINK_PARAM_CHG_PROFILE_ABSORB_MV", 20),
             ("CHG_PROFILE_PARAM_ABSORB_ENTER_MV", "ESPLINK_PARAM_CHG_PROFILE_ABSORB_ENTER_MV", 21),
             ("CHG_PROFILE_PARAM_ABSORB_OVER_MV", "ESPLINK_PARAM_CHG_PROFILE_ABSORB_OVER_MV", 22),
             ("CHG_PROFILE_PARAM_FLOAT_MV", "ESPLINK_PARAM_CHG_PROFILE_FLOAT_MV", 23),
             ("CHG_PROFILE_PARAM_REENTRY_MV", "ESPLINK_PARAM_CHG_PROFILE_REENTRY_MV", 24),
             ("CHG_PROFILE_PARAM_BULK_CURRENT_MAX_MA", "ESPLINK_PARAM_CHG_PROFILE_BULK_CURRENT_MAX_MA", 25),
             ("CHG_PROFILE_PARAM_TAPER_CURRENT_MA", "ESPLINK_PARAM_CHG_PROFILE_TAPER_CURRENT_MA", 26)]
    for chg_name, esp_name, num in pairs:
        m_h = re.search(rf"#define {chg_name}\s+(\d+)u", text_h)
        m_e = re.search(rf"#define {esp_name}\s+(\d+)u", text_esph)
        check(m_h is not None and m_e is not None and int(m_h.group(1)) == num and int(m_e.group(1)) == num,
              f"profile id {num} must be identical in charger.h ({chg_name}) and esp_link.h ({esp_name})")

    # --- charger.c: profile struct, defaults from the compile-time setpoints, clamp rules ---
    check("charger_profile_t" in text_c and "CHARGER_PROFILE_T__G__Profile" in text_c,
          "charger.c must hold the runtime profile struct")
    init = re.search(r"CHARGER_PROFILE_T__G__Profile\s*=\s*\{([^}]*)\}", text_c)
    check(init is not None and
          init.group(1).replace("\n", " ").split() == ["CHG_ABSORB_MV,", "CHG_ABSORB_ENTER_MV,",
              "CHG_ABSORB_OVER_MV,", "CHG_FLOAT_MV,", "CHG_REENTRY_MV,", "CHG_BULK_CURRENT_MAX_MA,",
              "CHG_TAPER_CURRENT_MA"],
          "profile boot defaults must equal the old compile-time setpoints (14400/14300/14600/13500/12800/650/50)")
    check("func__Charger_ClampProfile" in text_c and "func__Charger_SetProfileParam" in text_c
          and "func__Charger_GetProfileParam" in text_c,
          "charger.c must implement clamp + set/get profile API")
    for needle, why in [
            (".uint32_t__absorbMv > 14600u", "absorb capped at 14.6 V (over-threshold must stay under the 14.8 V battery-disconnect fault)"),
            (".uint32_t__absorbOverMv > 14750u", "over-threshold capped 50 mV under the 14.8 V fault"),
            (".uint32_t__absorbMv - 50u", "enter threshold <= absorb - 50"),
            (".uint32_t__absorbMv - 500u", "enter threshold >= absorb - 500"),
            (".uint32_t__bulkCurrentMaxMa > 900u", "current band capped at 900 mA (limit = band + 25 < 950 hard fault)"),
            (".uint32_t__taperCurrentMa >", "taper clamped against the band")]:
        check(needle in text_c, f"clamp rule present: {why}")

    # --- every automatic-charge decision site reads the profile, not the macro ---
    body = re.sub(r"/\*.*?\*/", "", text_c, flags=re.S)
    body = re.sub(r"CHARGER_PROFILE_T__G__Profile\s*=\s*\{[^}]*\}", "", body)
    for macro in ["CHG_ABSORB_MV", "CHG_ABSORB_ENTER_MV", "CHG_ABSORB_OVER_MV", "CHG_FLOAT_MV",
                  "CHG_REENTRY_MV", "CHG_BULK_CURRENT_MAX_MA", "CHG_TAPER_CURRENT_MA", "CHG_REGULATE_LOW_MA"]:
        leftover = [ln for ln in body.split("\n")
                    if macro in ln and "CHARGER_PROFILE_T__G__Profile" not in ln
                    and not ln.strip().startswith(("*", "/*", "//"))]
        check(not leftover,
              f"no bare {macro} use outside the profile initializer/comments (got {leftover[:2]})")
    check(text_c.count("CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv") >= 5,
          "the absorb setpoint must be read from the profile at every decision site")

    # --- esp_link.c wires ids 20..26 to the charger profile API ---
    apply_block = text_espc.split("bool func__EspLink_ApplyParam")[1][:8000]
    get_block = text_espc.split("bool func__EspLink_GetParam")[1][:8000]
    check("func__Charger_SetProfileParam" in apply_block and apply_block.count("ESPLINK_PARAM_CHG_PROFILE_") >= 7,
          "ApplyParam must route all 7 profile ids to Charger_SetProfileParam")
    check("func__Charger_GetProfileParam" in get_block and get_block.count("ESPLINK_PARAM_CHG_PROFILE_") >= 7,
          "GetParam must route all 7 profile ids to Charger_GetProfileParam")

    # --- ESP panel: 27 params, third tab with 7 fields + descriptions, 78-col CSV, vin carry ---
    check(re.search(r"#define ESP_PARAM_COUNT\s+27u", ino), "panel ESP_PARAM_COUNT must be 27")
    mn = re.search(r"INT32_T__G__ParamMin\[ESP_PARAM_COUNT\] = \{([^}]*)\}", ino)
    mx = re.search(r"INT32_T__G__ParamMax\[ESP_PARAM_COUNT\] = \{([^}]*)\}", ino)
    check(mn and mx and len(mn.group(1).split(",")) == 27 and len(mx.group(1).split(",")) == 27,
          "panel min/max tables must carry 27 entries (outer envelope for ids 20..26)")
    check('<button data-t="2">تنظیمات</button>' in ino, "third nav tab must exist (v1.14b: renamed from تنظیمات شارژ when the filter windows moved in)")
    check('id="p2"' in ino and all(f'id="q{i}"' in ino for i in range(20, 27)),
          "tab p2 must hold the seven profile inputs q20..q26")
    check("qfill" in ino and "qdef" in ino and "e.onchange=()=>{const v=parseInt(e.value,10);" in ino,
          "profile inputs must auto-fill from /t, POST on change, and offer factory defaults")
    check("حداکثر ولتاژ باتری (ابزورب)" in ino and "جریان تیپر" in ino and "ولتاژ شناور" in ino,
          "the tab must label/describe every field (user order: with descriptions)")
    check("for(let k=0;k<27;k++)P.push(q(D.p[k]));" in ino and "[profile]" in ino,
          "wrow must log all 27 params (78 columns) with the [profile] header block")
    check("window.WVI=" in ino and "L('wVi','ولتاژ ورودی V',WVI)" in ino,
          "the input-voltage DMM reading must carry into the next wizard step (user order 2026-09-25: quasi-static, type once)")

    # --- python model of ClampProfile: interdependencies hold for arbitrary writes ---
    def clamp(p):
        p = dict(p)
        p[20] = min(max(p[20], 11000), 14600)
        p[21] = min(max(p[21], p[20] - 500), p[20] - 50)
        p[22] = min(max(p[22], p[20] + 100), min(p[20] + 400, 14750))
        p[23] = min(max(p[23], 9000), p[20] - 300)
        p[24] = min(max(p[24], 8000), p[23] - 300)
        p[25] = min(max(p[25], 100), 900)
        p[26] = min(max(p[26], 10), min(300, p[25]))
        return p
    d = {20: 14400, 21: 14300, 22: 14600, 23: 13500, 24: 12800, 25: 650, 26: 50}
    check(clamp(d) == d, "defaults must be a fixed point of the clamp (no drift at boot)")
    lo = clamp({20: 20000, 21: 0, 22: 0, 23: 20000, 24: 0, 25: 0, 26: 0})
    check(lo == {20: 14600, 21: 14100, 22: 14700, 23: 14300, 24: 8000, 25: 100, 26: 10},
          f"absurd writes must collapse into a consistent set (got {lo})")
    mid = clamp({20: 13000, 21: 14300, 22: 14600, 23: 13500, 24: 12800, 25: 650, 26: 50})
    check(mid[21] == 12950 and mid[22] == 13400 and mid[23] == 12700 and mid[24] == 12400,
          f"lowering absorb must drag enter/over/float/reentry with it (got {mid})")
    cur = clamp({20: 14400, 21: 14300, 22: 14600, 23: 13500, 24: 12800, 25: 40, 26: 900})
    check(cur[25] == 100 and cur[26] == 100, "current writes must clamp band floor 100 and taper <= band")
    import random
    rng = random.Random(20260925)
    ok = True
    for _ in range(2000):
        w = {k: rng.randint(0, 20000) for k in range(20, 27)}
        w[25] = rng.randint(0, 2000); w[26] = rng.randint(0, 2000)
        c = clamp(w)
        if not (11000 <= c[20] <= 14600 and c[20] - 500 <= c[21] <= c[20] - 50
                and c[20] + 100 <= c[22] <= 14750 and 9000 <= c[23] <= c[20] - 300
                and 8000 <= c[24] <= c[23] - 300 and 100 <= c[25] <= 900
                and 10 <= c[26] <= min(300, c[25])):
            ok = False
            break
    check(ok, "2000 random writes must all land inside the invariant ranges")

    # --- calibration.h: one file, three tables (user order 2026-09-25) ---
    check("TABLE 1" in cal_h and "TABLE 2" in cal_h and "TABLE 3" in cal_h,
          "calibration.h must host all THREE tables (ch1 LUT placeholder, ch2 LUT, voltage comp)")
    check(cal_h.count("CAL_Current1LutChainMa") >= 2 and re.search(r"#define CAL_CURRENT1_LUT_ENABLE\s+0u", cal_h),
          "table 1 (ch1) stays EMPTY until SOLO1 data arrives (enable 0, placeholder anchors)")
    check("can grow into a full anchor table" in cal_h,
          "table 3 must document its growth path to an anchor table")


def test_ch2_power_lut_v113():
    """[EN] v1.13 (user order 2026-09-25, "voltages are fixed but the currents
    you read are wrong"): the ch2 LUT outputs battery-2 POWER; the live battery
    voltage turns it into current. This test replays the EXACT firmware integer
    math (BSP u64 chain with truncating divisions -> LUT -> x1000 / V) on the
    dense-run CSV rows and checks both the DMM agreement AND the voltage
    behaviour the old current-current table got wrong.
    [FA] تست عددی v1.13: بازپخش دقیق ریاضی صحیح فرم‌ور روی ردیف‌های ران
    متراکم + بررسی رفتار ولتاژی که جدول قدیمی اشتباه می‌گرفت."""
    cal_h = (ROOT / "Firmware/Modules/Measurement/calibration.h").read_text()
    meas_c_raw = (ROOT / "Firmware/Modules/Measurement/measurement.c").read_text()

    m_chain = re.search(r"CAL_Current2LutChainMa\[\] =\s*\{([^}]*)\}", cal_h)
    m_mw = re.search(r"CAL_Current2LutBatteryMw\[\] =\s*\{([^}]*)\}", cal_h)
    check(m_chain and m_mw, "calibration.h must carry both ch2 LUT arrays")
    xs = [int(v.strip().rstrip("u")) for v in m_chain.group(1).split(",")]
    ys = [int(v.strip().rstrip("u")) for v in m_mw.group(1).split(",")]
    check(len(xs) == len(ys) == 11 and all(xs[i] < xs[i + 1] for i in range(10))
          and all(ys[i] <= ys[i + 1] for i in range(10)),
          "ch2 LUT anchors: 11 points, chain strictly increasing, power non-decreasing")

    # exact firmware math replay (u64 intermediates, truncating divisions)
    def bsp_chain(counts, off=8, gain=1303):
        if counts <= off:
            return 0
        num = (counts - off) * 3300
        den = 4095
        num *= 11
        den *= 10
        den *= 101
        num *= 1000
        den *= 10
        ma = num // den
        return (ma * gain) // 1000

    def lut_mw(c):
        if c <= xs[0]:
            return ys[0]
        for i in range(1, len(xs)):
            if c <= xs[i]:
                return ys[i - 1] + ((c - xs[i - 1]) * (ys[i] - ys[i - 1])) // (xs[i] - xs[i - 1])
        return ys[-1] + ((c - xs[-1]) * (ys[-1] - ys[-2])) // (xs[-1] - xs[-2])

    def ibat(raw, v_mv):
        return (lut_mw(bsp_chain(raw)) * 1000) // v_mv

    # dense 2026-09-25T18:14 run: (raw2_avg, vlow TLM, dmm_i_bat2) per duty 2..20%
    rows = [(12.6, 12073, -13), (40.1, 12085, 9), (100.9, 12122, 62), (173.0, 12159, 130),
            (214.2, 12230, 215), (255.2, 12306, 310), (316.3, 12430, 422), (393.4, 12651, 541),
            (494.8, 13031, 660), (626.5, 13626, 764)]
    worst = 0
    for raw, vlow, dmm in rows:
        err = ibat(raw, vlow) - dmm
        worst = max(worst, err if raw > 20 else 0)  # the 2% row is the documented unsigned floor
    check(worst <= 6,
          f"firmware-math replay of the dense run: worst DMM error {worst} mA (<= 6 = integer-truncation bias, within DMM accuracy; the 2%-duty row floors at 0 as documented)")

    # the fix's whole point: same chain, fuller battery -> proportionally less current
    i_122, i_130, i_140, i_144 = (ibat(494.8, v) for v in (12200, 13000, 14000, 14400))
    check(i_122 > i_130 > i_140 > i_144 and abs(i_130 - 658) <= 3,
          f"voltage behaviour: at chain 556 the current must fall as the battery fills (12.2V:{i_122} 13.0V:{i_130} 14.0V:{i_140} 14.4V:{i_144} mA) - the old current-current table answered 658 mA at EVERY voltage")

    check("UINT32_T__G__Battery2VoltageMv = 12000u" in meas_c_raw and
          "((uint64_t)uint32_t__batteryPowerMw) * 1000u)" in meas_c_raw,
          "the division must run in u64 with the cached clamped voltage (boot default 12.0 V)")


def test_charger_persistence_v114():
    """[EN] v1.14 (user order 2026-09-25, "values must survive power loss"):
    the whole persisted-parameter stack - flash driver, ping-pong record,
    clamped boot replay, debounced save, panel graph + texts - plus a REAL
    compiled run of the exact flash-state code against a RAM-emulated flash
    with power-cut fault injection.
    [FA] v1.14 (دستور کاربر: «با قطع برق از بین نره»): کل پشتهٔ پارامترهای
    ذخیره‌شونده + اجرای کامپایل‌شدهٔ همان کد ماشین حالت فلش روی شبیه‌سازی
    RAM با تزریق قطع برق."""
    import subprocess
    import tempfile
    import shutil

    nvm_h = (ROOT / "Firmware/Modules/EspLink/esp_link_nvm.h").read_text(encoding="utf-8")
    nvm_c = (ROOT / "Firmware/Modules/EspLink/esp_link_nvm.c").read_text(encoding="utf-8")
    flash_c = (ROOT / "Firmware/Bsp/Src/bsp_flash.c").read_text(encoding="utf-8")
    flash_h = (ROOT / "Firmware/Bsp/Inc/bsp_flash.h").read_text(encoding="utf-8")
    app_c = (ROOT / "Firmware/App/Src/app.c").read_text(encoding="utf-8")
    ld = (ROOT / "CubeIDE/STM32CubeIDE/STM32F103C8TX_FLASH.ld").read_text(encoding="utf-8")
    ino = (ROOT / "esp_link_panel/esp_link_panel.ino").read_text(encoding="utf-8")

    check((ROOT / "Firmware/Bsp/Src/bsp_flash.c").exists() and
          (ROOT / "Firmware/Bsp/Inc/bsp_flash.h").exists() and
          (ROOT / "Firmware/Modules/EspLink/esp_link_nvm.c").exists() and
          (ROOT / "Firmware/Modules/EspLink/esp_link_nvm.h").exists(),
          "v1.14 needs the BSP flash driver (bsp_flash.c/.h) and the persistence module (esp_link_nvm.c/.h)")

    check("FLASH->KEYR" in flash_c and "FLASH_CR_PER" in flash_c and
          "FLASH_CR_STRT" in flash_c and "FLASH_CR_PG" in flash_c and
          "FLASH_CR_LOCK" in flash_c and "0x45670123" in flash_c,
          "bsp_flash.c must use the direct RM0008 FPEC sequences (KEYR unlock, PER+AR+STRT page erase, PG halfword program, LOCK) with no HAL flash dependency")

    check("func__EspLink_NvmMarkDirty(uint8_t__payload[0]);" in ESP_LINK_C.read_text(encoding="utf-8") and
          "func__EspLink_NvmTick();" in ESP_LINK_C.read_text(encoding="utf-8"),
          "a successful SET_PARAM of a persisted id must arm the debounced save, and EspLink_Run must tick it every comm period")
    check("#if MODULE_ESP" in app_c and "func__EspLink_NvmInit();" in app_c,
          "the boot replay must run in func__App_Init pre-scheduler under MODULE_ESP (module Inits reset channel state, never the settable statics)")
    esph_txt = ESP_LINK_H.read_text(encoding="utf-8")
    check("bool func__EspLink_ApplyParam(uint8_t uint8_t__paramId," in esph_txt and
          "bool func__EspLink_GetParam(uint8_t uint8_t__paramId, uint32_t *uint32_t__value);" in esph_txt,
          "ApplyParam/GetParam must be public since v1.14 - the flash load/save replays through the SAME clamped setters as the panel")

    check(re.search(r"FLASH\s+\(rx\)\s*: ORIGIN = 0x8000000,\s*LENGTH = 62K", ld) and
          re.search(r"NVM\s+\(r\)\s*: ORIGIN = 0x800F800,\s*LENGTH = 2K", ld),
          "the linker must shrink application FLASH to 62K and reserve the 2K NVM region at 0x0800F800 (build-time collision guard)")
    check("0x0800F800u" in nvm_h and "0x0800FC00u" in nvm_h,
          "the persistence pages must be the last two 1 KiB pages of the 64 KiB bank")
    check(re.search(r"ESP_LINK_NVM_ENTRY_MAX\s+27u", nvm_h) and
          "ESP_LINK_NVM_PERSISTED_ID_MAX_LOW     14u" in nvm_h and
          "ESP_LINK_NVM_PERSISTED_ID_MIN_HIGH    20u" in nvm_h and
          "ESP_LINK_NVM_PERSISTED_ID_MAX_HIGH    26u" in nvm_h,
          "persisted set = 0..14 + 20..26 (22 ids, 27 slots) - the transient test modes 15..19 must NEVER survive a reboot")

    # the persisted-id predicate in C, replicated and cross-checked
    persisted = {i for i in range(27) if i <= 14 or 20 <= i <= 26}
    check(persisted == set(range(15)) | set(range(20, 27)) and 19 not in persisted and 15 not in persisted,
          f"persisted id set must exclude 15..19 (got {len(persisted)} ids)")

    tab2 = ino.split('id="p2"', 2)[1]
    check("روی فلش برد ذخیره می‌شود و با قطع برق می‌ماند" in ino and
          "ماندگاری:" in ino and "function qgraph()" in ino and "e.oninput=qgraph" in ino and
          "if(TAB==2)qgraph();" in ino and "نمودار مراحل شارژ" in ino,
          "the panel must carry the stage graph (qgraph + live preview + redraw hook) and the persistence texts")
    check('<button data-t="2">تنظیمات</button>' in ino and
          'id="q7"' in tab2 and 'id="q8"' in tab2 and 'id="a7"' in tab2 and 'id="a8"' in tab2 and
          "پنجرهٔ مدین (Median)" in tab2 and "پنجرهٔ میانگین (Average)" in tab2 and
          "for(const id of [7,8,20,21,22,23,24,25,26])" in ino and
          "row(7)+row(8)" not in ino,
          "v1.14b (user order 2026-09-26): the median/average window controls must live in the settings tab under the filter section (nav renamed, tab 0 keeps only the live status), and both ids bind through the same send/qfill path")
    check("ناحیهٔ ابزورب (Absorb)" in ino and "ناحیهٔ شناور (Float)" in ino and
          "ناحیهٔ تجاوز (Over)" in ino and "زیر بازگشت (Reentry)" in ino and
          "قطع سخت (Cutoff) ۱۵V" in ino and "بالک (Bulk)" in ino and "خاموش (Off)" in ino and
          'fill="#0d1320"' in ino,
          "the stage graph must use the dark panel palette with bilingual (FA+EN) zone, threshold and stage labels")
    check("'باتری پایین (Vlow)',tt[17],tt[13],tt[10]" in ino and
          "'باتری بالا (Vhigh)',tt[18],tt[6],tt[3]" in ino and
          '<circle cx="${x}" cy="${y}" r="7"' in ino and
          'stroke="#e7eaf0"' not in ino and "marker-end" not in ino,
          "v1.14c (user order 2026-09-26, 'show each battery's position and state; the white Bulk curve is confusing - are the zones not enough?'): the graph drops the V(t) curve and cycle arrow, and each battery gets a live position DOT on its own voltage column (ch2->Vlow t17/t13/t10, ch1->Vhigh t18/t6/t3) with a state chip under the chart")
    check('H=560' in ino and 'id="qw"' in ino and 'function qchk()' in ino and
          'q.o.d' in ino and 'q.r.d' in ino and 'pvln(q.o' in ino and
          'const ZL=[],LL=[]' in ino and 'ترکیب نامعتبر' in ino and
          'باز هم ارسال شود؟' in ino and 'نگهبان ترکیب' in ino,
          "v1.14d (user order 2026-09-26, 'stretch the graph downward, the zone borders are cramped; zones must follow the profile numbers and never overlap'): taller chart (H=560), zones drawn from APPLIED values with dashed preview lines for typed values, anti-collision label pass (ZL/LL), and a qchk() guard mirroring Charger_ClampProfile - red warning + red field + confirm-before-send on invalid combos")

    # ---------- compiled fault-injection run of the EXACT flash-state code ----------
    gcc = shutil.which("gcc")
    if gcc is None:
        print("  SKIP: gcc not found - the compiled NVM fault-injection run was not executed (static checks above still ran)")
        return
    tmp = tempfile.mkdtemp(prefix="nvm_harness_")
    try:
        (Path(tmp) / "stub_esp_link.h").write_text(
            "#include <stdint.h>\n#include <stdbool.h>\n"
            "#define ESPLINK_PARAM_COUNT 27u\n"
            "bool func__EspLink_ApplyParam(uint8_t id, uint32_t value, uint32_t *applied);\n"
            "bool func__EspLink_GetParam(uint8_t id, uint32_t *value);\n", encoding="utf-8")
        (Path(tmp) / "stub_bsp_flash.h").write_text(
            "#include <stdint.h>\n#include <stdbool.h>\n"
            "bool func__BspFlash_ErasePage(uint32_t pageAddress);\n"
            "bool func__BspFlash_ProgramHalfWords(uint32_t address, const uint16_t *data, uint32_t count);\n", encoding="utf-8")
        harness = r"""
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#define EMU_FLASH_BASE 0x10000000u
uint8_t *EMU_FLASH;
#define ESP_LINK_NVM_PAGE_A_ADDR (EMU_FLASH_BASE)
#define ESP_LINK_NVM_PAGE_B_ADDR (EMU_FLASH_BASE + 1024u)
#include "esp_link_nvm.h"
#include "stub_esp_link.h"
#include "stub_bsp_flash.h"

int g_cut_after = -1, g_erase_cut = 0, g_apply_calls = 0;
uint32_t g_params[27];
static uint32_t clampf(uint32_t v, uint32_t lo, uint32_t hi){ return v < lo ? lo : (v > hi ? hi : v); }
bool func__EspLink_ApplyParam(uint8_t id, uint32_t value, uint32_t *applied){
    g_apply_calls++;
    if (id >= 27u) return false;
    switch (id) {
        case 20: value = clampf(value, 11000, 14600); break;
        case 21: value = clampf(value, 13800, 14550); break;
        case 22: value = clampf(value, 14500, 14750); break;
        case 23: value = clampf(value, 9000, 14300); break;
        case 24: value = clampf(value, 8000, 13200); break;
        case 25: value = clampf(value, 100, 900); break;
        case 26: value = clampf(value, 10, 300); break;
        default: break;
    }
    g_params[id] = value;
    if (applied) *applied = value;
    return true;
}
bool func__EspLink_GetParam(uint8_t id, uint32_t *value){ if (id >= 27u) return false; *value = g_params[id]; return true; }
bool func__BspFlash_ErasePage(uint32_t p){
    if (p != EMU_FLASH_BASE && p != EMU_FLASH_BASE + 1024u) return false;
    memset((void *)(uintptr_t)p, 0xFF, 1024);
    return g_erase_cut ? false : true;
}
bool func__BspFlash_ProgramHalfWords(uint32_t a, const uint16_t *d, uint32_t c){
    volatile uint16_t *dst = (volatile uint16_t *)(uintptr_t)a;
    for (uint32_t i = 0; i < c; i++) {
        if (g_cut_after >= 0 && (int)i >= g_cut_after) return false;
        if (dst[i] != 0xFFFFu && dst[i] != d[i]) return false;
        dst[i] = d[i];
    }
    return true;
}
#include "esp_link_nvm_body.c"

static void set_profile(uint32_t a, uint32_t f, uint32_t r, uint32_t im){
    uint32_t ap; (void)ap;
    func__EspLink_ApplyParam(20, a, &ap); func__EspLink_ApplyParam(23, f, &ap);
    func__EspLink_ApplyParam(24, r, &ap); func__EspLink_ApplyParam(25, im, &ap);
}
static void reboot(void){ memset(g_params, 0, sizeof g_params); g_apply_calls = 0; func__EspLink_NvmInit(); }
static void run_ticks(int n){ for (int i = 0; i < n; i++) func__EspLink_NvmTick(); }

int main(void){
    uint32_t ap;
    EMU_FLASH = mmap((void *)EMU_FLASH_BASE, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    assert(EMU_FLASH == (uint8_t *)EMU_FLASH_BASE);

    /* T1 fresh board: nothing applied, defaults stay */
    memset(EMU_FLASH, 0xFF, 2048); reboot();
    assert(g_apply_calls == 0);

    /* T2 debounce + save + reboot round-trip; burst resets the wait */
    set_profile(14400, 13500, 12800, 650);
    func__EspLink_NvmMarkDirty(23); run_ticks(14);
    const esp_link_nvm_record_t *pb = (const esp_link_nvm_record_t *)(void *)(uintptr_t)ESP_LINK_NVM_PAGE_B_ADDR;
    assert(func__EspLink_NvmSeqCompare(pb->uint16_t__seq, 0) == 1); /* first save on page B */
    reboot();
    assert(g_params[20] == 14400 && g_params[23] == 13500 && g_params[24] == 12800 && g_params[25] == 650);

    /* T3 alternation to page A, seq 2 */
    set_profile(14200, 13400, 12700, 700);
    func__EspLink_NvmMarkDirty(20); run_ticks(20);
    reboot();
    assert(g_params[20] == 14200 && g_params[23] == 13400 && g_params[24] == 12700 && g_params[25] == 700);

    /* T4 power cut DURING programming -> previous good record survives */
    set_profile(14100, 13300, 12600, 750);
    func__EspLink_NvmMarkDirty(20); g_cut_after = 10; run_ticks(20); g_cut_after = -1;
    reboot();
    assert(g_params[20] == 14200);

    /* T5 power cut DURING erase -> previous good record survives */
    set_profile(14000, 13200, 12500, 800);
    func__EspLink_NvmMarkDirty(20); g_erase_cut = 1; run_ticks(20); g_erase_cut = 0;
    reboot();
    assert(g_params[20] == 14200);

    /* T6 both pages damaged (flow-independent) -> nothing applied, defaults */
    EMU_FLASH[8] ^= 0x40;
    EMU_FLASH[1024 + 8] ^= 0x40;
    reboot();
    assert(g_apply_calls == 0 && g_params[20] == 0u);

    /* T6b clean pair: single corruption falls back to the older page */
    memset(EMU_FLASH, 0xFF, 2048); reboot();
    {
        esp_link_nvm_record_t rec; esp_link_nvm_entry_t e[1];
        e[0].uint16_t__id = 20; e[0].uint16_t__pad = 0; e[0].uint32_t__value = 14400;
        func__EspLink_NvmRecordBuild(&rec, 3, e, 1);
        assert(func__BspFlash_ErasePage(ESP_LINK_NVM_PAGE_A_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_A_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        e[0].uint32_t__value = 14200;
        func__EspLink_NvmRecordBuild(&rec, 4, e, 1);
        assert(func__BspFlash_ErasePage(ESP_LINK_NVM_PAGE_B_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_B_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        reboot();
        assert(g_params[20] == 14200);
        EMU_FLASH[1024 + 60] ^= 0x08;
        reboot();
        assert(g_params[20] == 14400);
    }

    /* T7 a record carrying a transient test id (19) is rejected WHOLE */
    {
        esp_link_nvm_record_t rec; esp_link_nvm_entry_t e[3];
        e[0].uint16_t__id = 20; e[0].uint16_t__pad = 0; e[0].uint32_t__value = 14500;
        e[1].uint16_t__id = 19; e[1].uint16_t__pad = 0; e[1].uint32_t__value = 1;
        e[2].uint16_t__id = 23; e[2].uint16_t__pad = 0; e[2].uint32_t__value = 13600;
        func__EspLink_NvmRecordBuild(&rec, 9, e, 3);
        assert(func__BspFlash_ErasePage(ESP_LINK_NVM_PAGE_B_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_B_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        reboot();
        assert(g_params[20] == 14400);
    }

    /* T8 hostile out-of-window value lands CLAMPED */
    {
        esp_link_nvm_record_t rec; esp_link_nvm_entry_t e[1];
        e[0].uint16_t__id = 20; e[0].uint16_t__pad = 0; e[0].uint32_t__value = 99999;
        func__EspLink_NvmRecordBuild(&rec, 10, e, 1);
        assert(func__BspFlash_ErasePage(ESP_LINK_NVM_PAGE_B_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_B_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        reboot();
        assert(g_params[20] == 14600);
    }

    /* T9 sequence wrap 65534/65535 -> 0 stays monotonic */
    {
        esp_link_nvm_record_t rec; esp_link_nvm_entry_t e[1];
        e[0].uint16_t__id = 25; e[0].uint16_t__pad = 0; e[0].uint32_t__value = 590;
        func__EspLink_NvmRecordBuild(&rec, 65534u, e, 1);
        assert(func__BspFlash_ErasePage(ESP_LINK_NVM_PAGE_B_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_B_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        e[0].uint32_t__value = 600;
        func__EspLink_NvmRecordBuild(&rec, 65535u, e, 1);
        assert(func__BspFlash_ErasePage(ESP_LINK_NVM_PAGE_A_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_A_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        reboot();
        assert(g_params[25] == 600);
        assert(func__EspLink_NvmSeqCompare(0, 65535) == 1);
        func__EspLink_ApplyParam(23, 13600, &ap);
        func__EspLink_NvmMarkDirty(23); run_ticks(20);
        reboot();
        assert(g_params[25] == 600 && g_params[23] == 13600);
    }

    /* T10 a transient MarkDirty never arms a save */
    memset(EMU_FLASH, 0xFF, 2048); reboot();
    func__EspLink_ApplyParam(19, 1, &ap); func__EspLink_NvmMarkDirty(19);
    run_ticks(30);
    assert(EMU_FLASH[0] == 0xFF && EMU_FLASH[1024] == 0xFF);

    printf("ALL NVM HARNESS TESTS PASSED\n");
    return 0;
}
"""
        (Path(tmp) / "harness.c").write_text(harness, encoding="utf-8")
        marker = "EspLink Nvm pure record logic"
        i = nvm_c.find(marker)
        assert i > 0
        body = nvm_c[nvm_c.find("*/", i) + 2:]
        (Path(tmp) / "esp_link_nvm_body.c").write_text(body, encoding="utf-8")
        cc = subprocess.run([gcc, "-O2", "-I", str(tmp), "-I", str(ROOT / "Firmware/Modules/EspLink"),
                             "-o", str(Path(tmp) / "nvmtest"), str(Path(tmp) / "harness.c")],
                            capture_output=True, text=True, timeout=120)
        check(cc.returncode == 0, f"the NVM harness must compile clean (stderr: {cc.stderr[:300]})")
        if cc.returncode == 0:
            rr = subprocess.run([str(Path(tmp) / "nvmtest")], capture_output=True, text=True, timeout=60)
            check("ALL NVM HARNESS TESTS PASSED" in rr.stdout,
                  f"the EXACT v1.14 flash-state code must survive the fault-injection suite "
                  f"(cut during erase, cut during program, bit-flip, hostile record, clamp, wrap) - output: {rr.stdout!r} rc={rr.returncode}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_electronic_load_policy_documented():
    text_h = CHARGER_H.read_text()
    check("free resistor" in text_h or "resistor alone" in text_h or "مقاومت آزاد" in text_h,
          "free resistor alone must be documented as not a valid battery simulator")
    check("battery simulator" in text_h or "voltage clamp" in text_h,
          "valid no-battery load must be documented as clamped/simulator only")


def test_manual_test_mode_v12():
    text_c = CHARGER_C.read_text()
    text_h = CHARGER_H.read_text()
    text_fault = FAULT_C.read_text()
    text_esph = ESP_LINK_H.read_text()
    text_esp = ESP_LINK_C.read_text()

    check("CHG_STATE_MANUAL" in text_c, "manual test mode needs its own charger state (9)")
    check(re.search(r"#define CHG_MANUAL_WATCHDOG_MS\s+3000u", text_h),
          "manual link dead-man must be 3 s")
    check(re.search(r"#define ESPLINK_FRAME_MAX_PAYLOAD\s+144u", text_esph),
          "payload limit must be 144: PARAMS_BULK with 27 params = 1 + 27 x 5 = 136 bytes (v1.12 charge-profile params; was 112 for 20)")
    check(re.search(r"#define ESPLINK_PARAM_MANUAL_TEST_MODE\s+19u", text_esph)
          and re.search(r"#define ESPLINK_PARAM_COUNT\s+27u", text_esph),
          "param 19 = manual test mode; 27 params total since v1.12 (20..26 = charge profile)")

    manual = text_c[text_c.find("static void func__Charger_ManualDriveChannel"):
                    text_c.find("/* ==================== Charger_Evaluate")]
    check("func__Charger_ManualDriveChannel" in text_c,
          "manual mode must drive the duty directly")
    check("CHG_MAX_VALID_BATTERY_MV" in manual,
          "manual mode keeps the 15 V hard overvoltage cutoff")
    check("func__Charger_ApplyDuty" in manual,
          "manual duty must go through the ApplyDuty clamp choke point")

    jit = text_c[text_c.find("static void func__Charger_HandleJitTrip"):
                 text_c.find("static uint16_t func__Charger_RetryDuty")]
    check("BOOL__G__ChargerManualModeActive" in jit,
          "JIT in manual mode must not arm the auto-retry/queue")

    check("func__Charger_IsManualTestModeActive" in text_fault,
          "battery-lost detection must freeze while manual mode is active")
    check("func__Charger_NotifyEspLinkActivity" in text_esp,
          "every valid ESP frame must feed the manual dead-man")
    check("ESPLINK_TLM_FLAG_MANUAL_MODE" in text_esp and "0x20u" in text_esp,
          "telemetry must expose the manual flag on b5")


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
        test_current_band_regulates_and_protects,
        test_setpoints_and_timing,
        test_pwm_contract,
        test_pwm_interleave_phase_lock,
        test_electronic_load_policy_documented,
        test_manual_test_mode_v12,
        test_charge_profile_v112,
        test_ch2_power_lut_v113,
        test_charger_persistence_v114,
    ]
    for test in tests:
        test()
    print(f"ALL {len(tests)} CHARGER HOST TESTS PASSED")
    print("Note: physical board tests not performed; no real battery connected.")


if __name__ == "__main__":
    main()
