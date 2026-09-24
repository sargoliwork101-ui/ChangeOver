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
    check("CHG_CURRENT_LIMIT_MA" in text_c, "current limit must still exist")
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
    check(re.search(r"#define CHG_FLYBACK_EFFICIENCY_UP_PERMILLE\s+786u", text_h), "per-channel efficiency UP = 786 permille (INERT since 2026-09-24: estimate = identity, chain is battery-side; kept only for ESP param 9 read-back)")
    check(re.search(r"#define CHG_FLYBACK_EFFICIENCY_DN_PERMILLE\s+786u", text_h), "per-channel efficiency DOWN = 786 permille (INERT since 2026-09-24: estimate = identity, chain is battery-side; kept only for ESP param 10 read-back)")
    charger_c_txt = CHARGER_C.read_text()
    check("return uint32_t__primaryMa;" in charger_c_txt and "(void)measurement_snapshot_t__snap;" in charger_c_txt,
          "output estimate must be the identity (user 2026-09-24: the sense chain is battery-side - the measured voltage IS the battery current; the Vin*eta/Vbat conversion is retired)")
    check("CHG_CURRENT_EMA_SHIFT" not in text_h and "currentEma" not in text_c,
          "charger must NOT filter the current estimate itself (user order 2026-09-22: Measurement's switchable median-3/moving-average chain feeds it; charger decides on that value)")
    check(re.search(r"#define MEASUREMENT_PERIOD_MS\s+1u", (ROOT / "Firmware/Modules/Measurement/measurement.h").read_text()),
          "measurement period must be 1 ms so the synchronized current samples are at most 1 ms old (user order 2026-09-22)")
    bsp_adc_c = (ROOT / "Firmware/Bsp/Src/bsp_adc.c").read_text()
    bsp_pwm_c_sync = (ROOT / "Firmware/Bsp/Src/bsp_pwm.c").read_text()
    bsp_meas_c = (ROOT / "Firmware/Bsp/Src/bsp_measurement.c").read_text()
    meas_h_txt = (ROOT / "Firmware/Modules/Measurement/measurement.h").read_text()
    meas_c_raw = (ROOT / "Firmware/Modules/Measurement/measurement.c").read_text()
    check("func__Measurement_FilterCurrent" not in meas_c_raw,
          "the old EMA current filter must stay removed; only the switchable median-3/moving-average chain is allowed (user order 2026-09-22)")
    check(re.search(r"#define MEASUREMENT_CURRENT_MEDIAN3_ENABLE\s+1u", meas_h_txt) and
          re.search(r"#define MEASUREMENT_CURRENT_AVERAGE_ENABLE\s+1u", meas_h_txt) and
          re.search(r"#define MEASUREMENT_CURRENT_AVERAGE_WINDOW\s+10u", meas_h_txt),
          "current filters must exist as compile-time switches: median-3 + moving-average over the last 10 samples, both ON by default (user order 2026-09-22)")
    check("func__Measurement_CurrentMedian(" in meas_c_raw and
          "func__Measurement_CurrentMovingAverage" in meas_c_raw and
          "func__Measurement_ApplyCurrentFilters" in meas_c_raw and
          "MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX" in meas_h_txt,
          "Measurement must run the median chain (runtime size 1/3/5, default 3) then the moving-average chain (runtime window 1..10, default 10) on each current channel (user order 2026-09-22)")
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
    check(re.search(r"#define ESPLINK_FRAME_MAX_PAYLOAD\s+112u", text_esph),
          "payload limit must be 112 for the 20-param bulk")
    check(re.search(r"#define ESPLINK_PARAM_MANUAL_TEST_MODE\s+19u", text_esph)
          and re.search(r"#define ESPLINK_PARAM_COUNT\s+20u", text_esph),
          "param 19 = manual test mode, 20 params total")

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
    ]
    for test in tests:
        test()
    print(f"ALL {len(tests)} CHARGER HOST TESTS PASSED")
    print("Note: physical board tests not performed; no real battery connected.")


if __name__ == "__main__":
    main()
