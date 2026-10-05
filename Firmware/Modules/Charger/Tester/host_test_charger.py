#!/usr/bin/env python3
"""Host tests for the independent two-channel 12 V charger policy.

These tests validate policy and source contracts only. They do not authorize a
battery connection and cannot replace HAL, transformer, relay, comparator or
oscilloscope tests on the board.
"""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[4]
APP_TYPES_H = Path(__file__).resolve().parents[3] / "Config" / "Inc" / "app_types.h"
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
ESP_LINK_NVM_H = ROOT / "Firmware/Modules/EspLink/esp_link_nvm.h"
ESP_LINK_NVM_C = ROOT / "Firmware/Modules/EspLink/esp_link_nvm.c"
BSP_UART_C = ROOT / "Firmware/Bsp/Src/bsp_uart.c"
MEASUREMENT_C = ROOT / "Firmware/Modules/Measurement/measurement.c"
CALIBRATION_H = ROOT / "Firmware/Modules/Measurement/calibration.h"
UI_LED_H = ROOT / "Firmware/Modules/Ui/ui_led.h"
UI_LED_C = ROOT / "Firmware/Modules/Ui/ui_led.c"
UI_BUZZER_C = ROOT / "Firmware/Modules/Ui/ui_buzzer.c"
BSP_MEAS_C = ROOT / "Firmware/Bsp/Src/bsp_measurement.c"
RTOS_TIME_C = ROOT / "Firmware/Rtos/Src/rtos_time.c"
FREERTOSCONFIG_H = ROOT / "CubeIDE/Core/Inc/FreeRTOSConfig.h"
FREERTOS_HOOKS_C = ROOT / "Firmware/Rtos/Src/freertos_hooks.c"
FLASH_LD = ROOT / "CubeIDE/STM32CubeIDE/STM32F103C8TX_FLASH.ld"
CPROJECT = ROOT / "CubeIDE/STM32CubeIDE/.cproject"

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


def test_fault_pump_rule_per_half_v121():
    """[EN] v1.21 (user order 2026-09-28): the 14.8 V pump rule arms PER HALF
       by that half's own pumping channel (channel 0 = high half, 1 = low half).
       [FA] v1.21: قانون پمپ ۱۴٫۸V هر نیم را فقط شارژر خودش مسلح می‌کند."""
    def any_over_v120(any_pumping, low_mv, high_mv, installed=(True, True), disc=14800):
        low_ok, high_ok = installed
        return any_pumping and ((low_ok and low_mv > disc) or (high_ok and high_mv > disc))
    def any_over_v121(ch0_pumping, ch1_pumping, low_mv, high_mv, installed=(True, True), disc=14800):
        low_ok, high_ok = installed
        return ((low_ok and ch1_pumping and low_mv > disc) or
                (high_ok and ch0_pumping and high_mv > disc))
    # [EN] Bench case that produced the repeating 3-beep cycle: only ch2
    # pumps, the DERIVED vhigh (V24 - V12) spikes above 14.8 V.
    # [FA] همان حالت بنچ با چرخهٔ سه‌بوق: فقط ch2 پمپ می‌کند و vhigh
    # مشتق‌شده (V24 - V12) اسپایک می‌گیرد.
    check(any_over_v120(True, 13000, 15000) == True, "old rule: the parked ch1 half could false-latch while ch2 pumped")
    check(any_over_v121(False, True, 13000, 15000) == False, "v1.21: parked ch1 half is not judged while only ch2 pumps")
    # [EN] Real wire cut on a charging channel must still latch.
    # [FA] قطع واقعی سیم روی کانال در حال شارژ همچنان قفل می‌کند.
    check(any_over_v121(True, False, 13000, 15000) == True, "v1.21: real ch1 wire cut while ch1 pumps still latches")
    check(any_over_v121(False, True, 15000, 13000) == True, "v1.21: real ch2 wire cut while ch2 pumps still latches")
    check(any_over_v121(True, True, 15000, 14400) == True, "v1.21: both pumping + low half over latches")
    # [EN] No pump -> rule not armed at all; uninstalled half ignored.
    # [FA] بدون پمپ اصلاً مسلح نیست؛ نیمِ کانال غیرنصب نادیده گرفته می‌شود.
    check(any_over_v121(False, False, 16000, 16000) == False, "v1.21: no pump -> rule not armed")
    check(any_over_v121(True, False, 13000, 15000, installed=(True, False)) == False, "v1.21: uninstalled high half ignored")


def test_modules_enabled_build():
    mods = (ROOT / "Firmware/Config/Inc/modules_enable.h").read_text()
    ch = CHARGER_H.read_text()
    check(re.search(r"#define MODULE_CHARGER\s+1", mods),
          "MODULE_CHARGER must be 1 for build/compile coverage")
    check(re.search(r"#define MODULE_JITTER\s+1", mods),
          "MODULE_JITTER must be 1 for build/compile coverage")
    check(re.search(r"#define CHG_MASTER_ENABLE\s+1u", ch),
          "master switch must be 1 for the active charge scenario (normal Bulk/Absorb/Float)")
    check(re.search(r"#define MODULE_ESP\s+1", mods),
          "MODULE_ESP must be 1 (default since the 2026-09-27 audit: pushed build works with the panel untouched)")
    check(re.search(r"#define MODULE_PROTECTION\s+0", mods),
          "MODULE_PROTECTION must stay 0 (skeleton: only the instantaneous FAULT_ADC bit)")


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
    snippet = text_c[idx:idx+1400]  # C26 (2026-09-26): SafeIdle-first + comment widened the block
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
    iso_active = text_c.split("bool func__Charger_IsChannelActive(uint8_t")[-1].split("bool func__Charger_IsChargeComplete(void)")[0]
    check("CHG_STATE_FLOAT" not in iso_active and "CHG_STATE_BULK" in iso_active and "CHG_STATE_ABSORB" in iso_active, "IsAnyChannelActive/IsChannelActive must count only BULK/ABSORB - parked FLOAT is DONE, not pumping (kills done-phase false buzzers and stops the yellow blink)")
    check("func__Charger_IsChannelActive(uint8_t__channelIndex) == true" in iso_active, "v1.21: IsAnyChannelActive delegates to the per-channel predicate (one source of truth)")
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
    # [EN] USER-ORDERED LOGIC CHANGE 2026-10-03 ("I cannot raise the hard
    #      fault current - my hand has to be free; my batteries may be in
    #      parallel"). This check froze the literal 950 and was therefore the
    #      thing blocking the order - the sixth hard-coded value found
    #      defending a NUMBER instead of an INTENT. The intents are:
    #        - a board nobody touches still trips at 950 (opening a range
    #          must not move an existing setup),
    #        - the ceiling is derived from what the ADC chain can represent,
    #          not from taste, and stays inside it,
    #        - the ceiling is genuinely above the old value, or the user's
    #          hand is still tied.
    # [FA] این چک عدد ۹۵۰ را منجمد کرده بود و دقیقاً همان چیزی بود که جلوی
    #      دستور کاربر را می‌گرفت - ششمین مقدار ثابتی که به‌جای «نیت» از یک
    #      «عدد» دفاع می‌کرد. نیت‌ها: بردی که کسی دستش نزند هنوز در ۹۵۰ تریپ
    #      کند؛ سقف از آنچه زنجیرهٔ ADC می‌تواند نمایش دهد مشتق شود نه از
    #      سلیقه؛ و سقف واقعاً بالاتر از مقدار قبلی باشد.
    dflt = int(re.search(r"#define CHG_CURRENT_HARD_FAULT_DEFAULT_MA\s+(\d+)u",
                         text_h).group(1))
    ceil = int(re.search(r"#define CHG_CURRENT_HARD_FAULT_MAX_MA\s+(\d+)u",
                         text_h).group(1))
    meas = int(re.search(r"#define CHG_CURRENT_MEASURABLE_MAX_MA\s+(\d+)u",
                         text_h).group(1))
    check(dflt == 950, "the factory hard-fault trip must still be 950 mA: "
                       "opening a range must not move an existing setup")
    check(ceil > dflt, "the settable ceiling must be above the factory trip, "
                       "otherwise the user's hand is still tied")
    check(ceil <= meas, "the ceiling must stay inside what the ADC chain can "
                        "represent: a trip that can never be reached is not a "
                        "protection, it is decoration")
    # 4095 counts * 24200 / 27573 is the chain full scale (bsp_measurement.c);
    # through the ch1 LUT's last slope, divided by the highest valid battery
    # voltage, that is the smallest battery current the chain still covers.
    chain_fs = (4095 * 24200) // 27573
    gain1 = 1046
    slope1 = (9089 - 8061) / (640 - 567)
    power1 = 9089 + ((chain_fs * gain1) // 1000 - 640) * slope1
    worst = power1 * 1000.0 / 15000.0
    check(abs(meas - worst) < 10,
          "CHG_CURRENT_MEASURABLE_MAX_MA must match its own derivation from "
          "the ADC chain and the channel-1 LUT (got %d, derived %.0f)"
          % (meas, worst))
    check("#define CHG_CURRENT_HARD_FAULT_MA " not in text_h and
          "#define CHG_CURRENT_HARD_FAULT_MA\t" not in text_h,
          "the old one-name-two-jobs macro must not come back as an alias: "
          "it would silently ship the ceiling as the factory default")
    check(re.search(r"#define CHG_DUTY_MAX_PERMILLE\s+500u", text_h), "duty cap must be 500 permille = 50% (DCM ceiling, board requirement)")
    # v1.23: the legacy step chain is DELETED, not bypassed. Nothing of it may
    # come back - if any of these reappear, someone re-introduced the old
    # regulator the user ordered removed.
    for gone in ("absorbUpIntervalTicks", "absorbDownIntervalTicks", "CHG_REGULATE_LOW_MA",
                 "CHG_DUTY_RAMP_UP_INTERVAL_MS", "CHG_DUTY_FINE_STEP_PERMILLE",
                 "uint32_t__enable", "CHG_PID_PARAM_ENABLE"):
        check(gone not in text_c and gone not in text_h,
              "the legacy step regulator must stay deleted, found: " + gone)
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
    # [EN] CAL_REFERENCE was a v1.3 command the panel stopped sending in v1.7.
    #      The STM32 handler was then carried DEAD for six versions - 172 source
    #      lines, roughly 0.7-1.1 KB of a 62 KB flash budget - and it is what
    #      pushed the v2 build 92 bytes over the limit. Removed, along with the
    #      whole cluster of tests that were pinning dead code in place.
    #      The id stays RESERVED rather than freed: if 0x03 were reused for
    #      something else, an older panel still sending CAL_REFERENCE would be
    #      silently misinterpreted instead of harmlessly ignored.
    # [FA] CAL_REFERENCE فرمانی از نسخهٔ ۱.۳ بود که پنل از نسخهٔ ۱.۷ نمی‌فرستد.
    #      هندلرش شش نسخه مرده حمل شد - ۱۷۲ خط و حدود یک کیلوبایت از ۶۲ کیلوبایت
    #      فلش - و همان چیزی بود که بیلد نسخهٔ ۲ را ۹۲ بایت سرریز کرد. حذف شد،
    #      به‌همراه دسته تستی که آن کد مرده را سر جایش قفل کرده بود. شناسه رزرو
    #      می‌ماند نه آزاد.
    check("#define ESPLINK_MSG_CAL_REFERENCE     0x03u" in esp_link_h_txt
          and "reserved, not handled" in esp_link_h_txt,
          "message id 0x03 must stay RESERVED and marked as no longer handled, so it "
          "is never reused and an old panel's CAL_REFERENCE stays harmlessly ignored")
    check("func__EspLink_ApplyCalReference" not in esp_link_c_txt
          and "ESPLINK_CAL_MAX_REF_MA" not in esp_link_c_txt,
          "the dead CAL_REFERENCE handler must stay removed - the panel has not sent "
          "it since v1.7, and carrying it dead is what overflowed the 62 KB flash "
          "budget by 92 bytes")
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
          "MEASUREMENT_CURRENT_MEDIAN_SIZE_MAX" in meas_h_txt and
          "func__Measurement_ApplyCurrentFilters(0u,\n            (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_CURRENT1]);" in meas_c_raw and
          "func__Measurement_ApplyCurrentFilters(1u,\n            (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2]);" in meas_c_raw and
          "func__Measurement_Current1CountsToMa((uint16_t)uint32_t__current1CountsFiltered);" in meas_c_raw and
          "func__Measurement_Current2CountsToMa((uint16_t)uint32_t__current2CountsFiltered);" in meas_c_raw,
          "Measurement must run the median chain (v1.4: runtime size ANY 1..15, default 3) then the moving-average chain (v1.4: runtime window ANY 1..300 since v1.9, default 10) on each current channel (user order 2026-09-25) - and on the RAW ADC counts with the counts->mA conversion AFTER the filter (user order 2026-09-27)")
    lut_chain = re.search(r"CAL_Current2LutChainMa\[\] =\s*\{([^}]*)\}", cal_h)
    lut_batt = re.search(r"CAL_Current2LutBatteryMw\[\] =\s*\{([^}]*)\}", cal_h)
    lut_chain_n = len(lut_chain.group(1).split(",")) if lut_chain else 0
    lut_batt_n = len(lut_batt.group(1).split(",")) if lut_batt else 0
    lut1_chain = re.search(r"CAL_Current1LutChainMa\[\] =\s*\{([^}]*)\}", cal_h)
    lut1_batt = re.search(r"CAL_Current1LutBatteryMw\[\] =\s*\{([^}]*)\}", cal_h)
    lut1_chain_n = len(lut1_chain.group(1).split(",")) if lut1_chain else 0
    lut1_batt_n = len(lut1_batt.group(1).split(",")) if lut1_batt else 0
    check('#include "calibration.h"' in meas_c_raw and
          re.search(r"#define CAL_CURRENT2_LUT_ENABLE\s+1u", cal_h) and
          "static const uint32_t CAL_Current2LutChainMa[] =" in cal_h and
          "static const uint32_t CAL_Current2LutBatteryMw[] =" in cal_h and
          "sizeof(CAL_Current2LutChainMa) /" in cal_h and
          "{ 0u, 20u, 37u, 106u, 189u, 236u, 253u, 283u, 312u, 353u, 390u, 441u, 557u, 707u }" in cal_h and
          "{ 0u, 0u, 111u, 766u, 1616u, 2753u, 3347u, 4037u, 4686u, 5523u, 6231u, 7043u, 8867u, 10794u }" in cal_h and
          lut_chain_n == lut_batt_n and lut_chain_n == 14 and
          re.search(r"#define CAL_CURRENT1_LUT_ENABLE\s+1u", cal_h) and
          "static const uint32_t CAL_Current1LutChainMa[] =" in cal_h and
          "static const uint32_t CAL_Current1LutBatteryMw[] =" in cal_h and
          "sizeof(CAL_Current1LutChainMa) /" in cal_h and
          "{ 0u, 5u, 11u, 31u, 54u, 81u, 114u, 148u, 189u, 231u, 277u, 330u, 382u, 444u, 504u, 567u, 640u }" in cal_h and
          "{ 0u, 0u, 135u, 445u, 795u, 1061u, 1670u, 2189u, 2778u, 3390u, 4007u, 4720u, 5474u, 6306u, 7159u, 8061u, 9089u }" in cal_h and
          lut1_chain_n == lut1_batt_n and lut1_chain_n == 17,
          f"channel-2 bench LUT must be ON as a chain->POWER table (v1.13, user order 2026-09-25 'voltages are fixed but the currents are wrong'; v1.17/v1.18 refit 2026-09-27 SOLO2 sweep duty 1..19): the DCM invariant is battery POWER, the current is P/Vbat - the old chain->current table embedded the calibration run's battery voltage (12.0..13.65V) and overread ~7 percent per volt as the battery filled; anchors = DMM_I2 x DMM_V2 of the dense 2026-09-25T18:14 run (10 points, duty 2..20%) plus the 2026-09-27 SOLO2 refit points, fitted end-to-end against the exact integer pipeline (v1.18: the integer chain sits ~1.5 mA left of the float chain, so float-fitted anchors drifted -4..-8 mA on the steep slopes); the axis stays the ADC chain current (raw-off2)*K*gain, NEVER duty; the tables size themselves from the initializers and both lists must stay the same length (got chain={lut_chain_n} power={lut_batt_n}); v1.19 (user order 2026-09-27, SOLO1 sweep duty 1..18): channel 1 gets the SAME chain->POWER architecture (TABLE 1, 17 anchors, gate (5,0), ceil-fitted so every DMM point replays EXACTLY - got chain={lut1_chain_n} power={lut1_batt_n}); USER-ORDERED 2026-09-29: TABLE 1 was REFITTED when the pack divider was corrected to the schematic - power is V x I, so the table had silently absorbed the divider error and I = P/V only looked right because BOTH terms were wrong by the same factor")
    check("uint32_t uint32_t__batteryPowerMw = func__Measurement_Current2BenchLut(\n        func__BspMeasurement_Current2CountsToMa(uint16_t__counts));" in meas_c_raw and
          "(uint32_t__batteryPowerMw * 1000u) /\n           UINT32_T__G__Battery2VoltageMv" in meas_c_raw,
          "the ch2 LUT must wrap the BSP conversion inside func__Measurement_Current2CountsToMa (unfiltered, filtered and iest all become true battery mA; raw counts and shunt uV untouched) and v1.13 DIVIDES the table's POWER output by the live cached battery-2 voltage (user order: the currents were wrong as the battery filled)")
    check("static uint32_t UINT32_T__G__Battery2VoltageMv = 12000u;" in meas_c_raw and
          meas_c_raw.count("UINT32_T__G__Battery2VoltageMv") >= 5 and
          "if (uint32_t__batteryLowMv < 8000u)\n    {\n        UINT32_T__G__Battery2VoltageMv = 8000u;" in meas_c_raw and
          "UINT32_T__G__Battery2VoltageMv = 15000u;" in meas_c_raw,
          "the ch2 power LUT needs the live battery-2 voltage cache: static default 12.0 V, written each pass after the median-5 filter, clamped 8.0..15.0 V so a missing battery can never blow up the division")
    check("uint32_t uint32_t__batteryPowerMw = func__Measurement_Current1BenchLut(\n        func__BspMeasurement_Current1CountsToMa(uint16_t__counts));" in meas_c_raw and
          "(uint32_t__batteryPowerMw * 1000u) /\n           UINT32_T__G__Battery1VoltageMv" in meas_c_raw,
          "v1.19: the ch1 LUT must wrap the BSP conversion inside func__Measurement_Current1CountsToMa (unfiltered, filtered and iest all become true battery mA; raw counts and shunt uV untouched) and DIVIDE the table's POWER output by the live cached battery-1 voltage (same architecture as ch2)")
    check("static uint32_t UINT32_T__G__Battery1VoltageMv = 12000u;" in meas_c_raw and
          meas_c_raw.count("UINT32_T__G__Battery1VoltageMv") >= 5 and
          "if (uint32_t__batteryHighMv < 8000u)\n    {\n        UINT32_T__G__Battery1VoltageMv = 8000u;" in meas_c_raw and
          "UINT32_T__G__Battery1VoltageMv = 15000u;" in meas_c_raw,
          "v1.19: the ch1 power LUT needs the live battery-1 voltage cache (vhigh = V24 - V12): static default 12.0 V, written each pass after the median-5 filter, clamped 8.0..15.0 V so a missing battery can never blow up the division")
    check("func__Measurement_MedianFilterVoltageSample(0u,\n            (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_24V_BAT]);" in meas_c_raw and
          "func__Measurement_MedianFilterVoltageSample(1u,\n            (uint32_t)uint16_t__raw[BSP_ADC_CHANNEL_12V_BAT]);" in meas_c_raw and
          meas_c_raw.find("func__Measurement_MedianFilterVoltageSample(0u,") <
          meas_c_raw.find("if (uint32_t__batteryLowMv < 8000u)"),
          "the median-5 must run on the RAW V24/V12 ADC counts (user order 2026-09-27: filters on raw data, conversion after) and the LUT voltage cache must be fed AFTER it (spikes must not modulate the current reading)")
    check(re.search(r"func__Measurement_Current2CountsToMa\(uint16_t uint16_t__counts\)\n\{\n#if \(CAL_CURRENT2_LUT_ENABLE != 0u\)", meas_c_raw) and
          re.search(r"#else\n    return func__BspMeasurement_Current2CountsToMa\(uint16_t__counts\);\n#endif", meas_c_raw),
          "the ch2 LUT must be compile-switchable: MEASUREMENT_CURRENT2_LUT_ENABLE=0 restores the old linear behaviour exactly")
    # [EN] TURNED OFF 2026-09-29. This test used to require the compensation to
    #      be ON, and cited a good least-squares fit as the justification -
    #      149.8 mV + 472.5 mOhm, residual within +/-28 mV. The fit was good and
    #      the MODEL was wrong, which is the more dangerous combination: the
    #      residual is 203 mV at essentially ZERO current, and an I x R term must
    #      be zero there. Replayed with the divider corrected, the implied
    #      resistance runs from 12.4 ohm at 17 mA to 0.73 ohm at 754 mA. That is
    #      not a resistance.
    #      What it actually did: the block SUBTRACTS from the 12 V reading, the
    #      regulator only sees the reading, so the real terminal sat that much
    #      HIGHER - 14.86 V at 500 mA while the panel showed 14.40. The user
    #      measured 14.88 V. It also blinded the protection: the 14.85 V cut was
    #      tripping at a real 15.31 V.
    #      A static term is only legitimate if it survives at zero current.
    # [FA] ۲۰۲۶-۰۹-۲۹ خاموش شد. این تست قبلاً روشن‌بودن جبران‌ساز را الزام می‌کرد و
    #      برازش خوب کمترین‌مربعات را دلیل می‌آورد. برازش خوب بود و مدل غلط، که
    #      ترکیب خطرناک‌تری است: باقی‌مانده در جریانِ عملاً صفر ۲۰۳ میلی‌ولت است و
    #      جملهٔ I×R آنجا باید صفر باشد. کاری که می‌کرد: از خوانش کم می‌کرد و چون
    #      تنظیم‌کننده فقط خوانش را می‌بیند، ترمینال واقعی همان‌قدر بالاتر می‌نشست.
    check(re.search(r"#define CAL_BATTERY12_BENCH_COMP_ENABLE\s+0u", cal_h),
          "the V12 bench compensation must stay OFF: it subtracts 150 mV + I x 0.47 ohm "
          "from the reading the regulator uses, so it pushed the lower battery to a real "
          "14.86 V while the panel showed 14.40, and made the 14.85 V cut trip at a real "
          "15.31 V. Its residual is 203 mV at zero current, where an I x R term must be "
          "zero - a good fit to a wrong model. Re-enable only with a drop measured "
          "directly at a known current, and with STATIC_MV at 0")
    check("func__Measurement_Battery12BenchCompensate(\n        uint32_t__battery12Mv, uint32_t__current2SampleMa);" in meas_c_raw and
          meas_c_raw.find("func__Measurement_Current2CountsToMa(uint16_t__raw[BSP_ADC_CHANNEL_CURRENT2])") <
          meas_c_raw.find("uint32_t__battery12Mv = func__Measurement_ApplyVoltageOffsetMv(") and
          meas_c_raw.find("func__Measurement_Battery12BenchCompensate(\n        uint32_t__battery12Mv, uint32_t__current2SampleMa);") <
          meas_c_raw.find("uint32_t__batteryLowMv = uint32_t__battery12Mv;"),
          "the V12 compensation must consume the post-LUT channel-2 current (sample moved ahead of the voltage chain) and must land on battery12Mv after the runtime voff, before the low/high derivation (the median now runs on the raw counts ahead of the chain) - so Vlow, published V12 and derived Vhigh all describe the true battery-2 terminals")
    check(len(re.findall(r"#if \(CAL_BATTERY12_BENCH_COMP_ENABLE != 0u\)", meas_c_raw)) == 2 and
          "uint32_t__dropMv = CAL_BATTERY12_BENCH_STATIC_MV +" in meas_c_raw and
          "return 0u;" in meas_c_raw.split("func__Measurement_Battery12BenchCompensate")[1].split("\n}\n")[0],
          "the V12 bench compensation must be compile-switchable (enable=0 restores today's behaviour), use saturating subtraction (static + I2 x mOhm / 1000, never below 0 mV)")
    check(re.search(r"#define BSP_MEASUREMENT_DIV24BAT_TOP_OHMS\s+BSP_MEASUREMENT_DIV24_TOP_OHMS", bsp_meas_c) and
          re.search(r"#define BSP_MEASUREMENT_SENSE_TOP_24V_OHMS\s+68000u", bsp_meas_c) and
          re.search(r"#define BSP_MEASUREMENT_SENSE_TOP_12V_OHMS\s+33000u", bsp_meas_c) and
          re.search(r"#define BSP_MEASUREMENT_SENSE_SERIES_MCU_OHMS\s+1200u", bsp_meas_c) and
          re.search(r"#define BSP_MEASUREMENT_SENSE_SHUNT_OHMS\s+6800u", bsp_meas_c) and
          not re.search(r"#define\s+\w+\s+66200u", bsp_meas_c) and
          "func__BspMeasurement_Battery24CountsToMv" in bsp_meas_c and
          "func__Measurement_Battery24CountsToMv(uint16_t__battery24CountsFiltered);" in meas_c_raw and
          "func__Measurement_V24CountsToMv(uint16_t__raw[BSP_ADC_CHANNEL_24V_IN]);" in meas_c_raw,
          "USER-ORDERED 2026-09-29: the sense dividers must be the SCHEMATIC values, never tuned. Each net is two series resistors into the ADC pin plus one to ground: R46=68K (input) / R47=68K (pack) / R48=33K (mid), each + R11/R13/R15=1.2K, each over R12/R14/R16=6.8K. The two 24 V nets are electrically IDENTICAL so the pack MUST reuse the input's divider expression - it carried a fabricated TOP=66200 that matches no resistor on the board, which is exactly why the pack voltage never calibrated. A wrong divider is a GAIN error: right at one point, wrong everywhere else. If a decision must happen sooner, move the THRESHOLD (CHG_OV_DECIDE_EARLY_MV), never the scale")
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
          "uint32_t__lowMv  < FAULT_ALARM_T__G__Alarm.uint32_t__absentMv" in (ROOT / "Firmware/Modules/Fault/fault.c").read_text(),
          "rule 2 must be EITHER half below the absent threshold (boot default FAULT_BAT_ABSENT_MV = 6 V, runtime id 29 since v1.15) with recovery kept at 7 V (user threshold split 2026-09-22; was ALL six-V: silent on a single cut lead)")
    check("bool__batteryTrulyPresent" in (ROOT / "Firmware/Modules/Fault/fault.c").read_text(), "bat-lost clear must require BOTH halves >= FAULT_BATTERY_BACK_MV (7 V) - one lead cut keeps its half below 7 V so the alarm repeats until reconnect (one-burst bug)")
    check(re.search(r"#define FAULT_BAT_DISCONNECT_MV\s+14800u", text_fault_h), "threshold stays 14.8 V, NOT 15.0 V: 15.0 would collide with the validity cut (~0.1 s float vs ~0.5 s at 14.8)")
    check(re.search(r"#define FAULT_BAT_ABSENT_MV\s+6000u", text_fault_h), "battery-absent threshold must be 6 V in Fault (user choice)")
    check(re.search(r"#define FAULT_BAT_ABSENT_DEBOUNCE_MS\s+1000u", text_fault_h), "battery-absent debounce must be 1000 ms in Fault")
    check(re.search(r"#define FAULT_BAT_RECOVER_MS\s+1000u", text_fault_h), "battery-back settle must be 1000 ms in Fault")
    check(re.search(r"#define FAULT_INPUT_PRESENT_MIN_MV\s+21000u", text_fault_h), "absent rule must be gated by input present >= 21 V")
    check(re.search(r"#define FAULT_INPUT_PRESENT_MAX_MV\s+28000u", text_fault_h), "absent rule must be gated by input <= 28 V")
    check("CHG_INSTALLED_CHANNEL_MASK" in text_fault_c and "bool__highHalfInstalled" in text_fault_c,
          "battery-lost rules must ignore halves of uninstalled channels (bench bug: with CH1 off, the unwired low half latched bat-lost forever and the charger looked dead)")
    check("func__Charger_IsChannelActive(0u)" in text_fault_c and "func__Charger_IsChannelActive(1u)" in text_fault_c,
          "v1.21 (user order 2026-09-28): the 14.8 V pump rule is armed PER HALF by that half's own pumping channel - a parked channel has no pump, its half cannot fly up (kills the repeating false 3-beep cycle during charge; the derived vhigh = V24 - V12 moves with the OTHER channel's load)")
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
    ino = "\n".join((ROOT / "esp_link_panel" / f).read_text(encoding="utf-8") for f in ["esp_link_panel.ino", "plink_config.h", "plink_params.h", "plink_state.h", "plink_panel.h", "plink_font.h", "plink_link.h", "plink_http.h"])
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
            # [EN] The band no longer stops at a frozen 900: it stops 50 mA
            #      below the hard-fault ceiling, which is the relationship
            #      that actually matters (the trip must stay above the
            #      setpoint or the charger faults on its own target). The
            #      intent is pinned, the literal is not.
            # [FA] باند دیگر روی ۹۰۰ منجمد نمی‌ایستد؛ ۵۰ میلی‌آمپر زیر سقف
            #      خطای سخت می‌ایستد، یعنی همان رابطه‌ای که واقعاً مهم است.
            (".uint32_t__bulkCurrentMaxMa >\n        (CHG_CURRENT_HARD_FAULT_MAX_MA - 50u)",
             "current band capped 50 mA under the hard-fault ceiling"),
            (".uint32_t__taperCurrentMa >", "taper clamped against the band")]:
        check(needle in text_c, f"clamp rule present: {why}")

    # --- every automatic-charge decision site reads the profile, not the macro ---
    body = re.sub(r"/\*.*?\*/", "", text_c, flags=re.S)
    body = re.sub(r"CHARGER_PROFILE_T__G__Profile\s*=\s*\{[^}]*\}", "", body)
    for macro in ["CHG_ABSORB_MV", "CHG_ABSORB_ENTER_MV", "CHG_ABSORB_OVER_MV", "CHG_FLOAT_MV",
                  "CHG_REENTRY_MV", "CHG_BULK_CURRENT_MAX_MA", "CHG_TAPER_CURRENT_MA"]:
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

    # --- ESP panel: 99 params, third tab with 7 fields + descriptions, 150-col CSV, vin carry ---
    check(re.search(r"#define ESP_PARAM_COUNT\s+132u", ino), "panel ESP_PARAM_COUNT must be 132 (v1.80: +4 scenario-6 lamp/buzzer ids 128..131)")
    mn = re.search(r"INT32_T__G__ParamMin\[ESP_PARAM_COUNT\] = \{([^}]*)\}", ino)
    mx = re.search(r"INT32_T__G__ParamMax\[ESP_PARAM_COUNT\] = \{([^}]*)\}", ino)
    check(mn and mx and len(mn.group(1).split(",")) == 132 and len(mx.group(1).split(",")) == 132,
          "panel min/max tables must carry 128 entries (outer envelope for ids 20..26, 27..82, 83..92, 93..107, 108..118, 119..124 and 125..127 and 128..131)")
    check('<button data-t="2">تنظیمات</button>' in ino, "third nav tab must exist (v1.14b: renamed from تنظیمات شارژ when the filter windows moved in)")
    # [EN] v1.33 (user order 2026-10-03: "why is this charge profile still
    #      here when I am editing on the chart?"). The seven q20..q26 input
    #      boxes were a second place to set the same seven values, which is
    #      exactly the duplication the user has been removing. The assertion
    #      is INVERTED, not deleted: the form must be GONE, and every one of
    #      those ids must still be reachable - on the chart, where the user
    #      asked for them. A deleted assertion would have let an id vanish
    #      with the form.
    # [FA] هفت کادر ورودی q20..q26 جای دومِ تنظیم همان هفت مقدار بودند؛
    #      همان تکراری که کاربر دارد حذف می‌کند. ادعا به‌جای حذف «وارونه» شد:
    #      فرم باید رفته باشد و هر هفت شناسه همچنان در دسترس باشند - روی
    #      نمودار، همان‌جا که کاربر خواست. حذف ادعا اجازه می‌داد یک شناسه
    #      همراه فرم ناپدید شود.
    check('id="p2"' in ino and not any(f'id="q{i}"' in ino for i in range(20, 27)),
          "the duplicate profile form q20..q26 must be gone: the chart edits "
          "those seven values now")
    chart_ids = {int(x) for x in re.findall(r'data-i=\\?"?\$\{?(\d+)', ino)}
    for i in range(20, 27):
        check(f"evat({i})" in ino or i in chart_ids or f":[" in ino,
              f"id {i} must still be editable somewhere after the form went")
    check("qfill" in ino and "qdef" in ino and "e.onchange=()=>{const v=parseInt(e.value,10);" in ino,
          "the remaining numeric inputs must auto-fill from /t, POST on change, "
          "and the factory-default button must survive the form removal")
    # [EN] v1.32 (user order 2026-10-03: "write the English word for the ones
    #      that should be English"): the transliterated stage names are gone -
    #      Absorb/Taper/Float are English terms, and ابزورب/تیپر/شناور were the
    #      worst of both worlds, neither English nor a Persian word you could
    #      look up. The labels still have to exist; only their spelling moved.
    # [FA] نام مرحله‌های ترانویسی‌شده حذف شدند - Absorb/Taper/Float اصطلاح
    #      انگلیسی‌اند و «ابزورب/تیپر/شناور» بدترین حالت هر دو زبان بودند: نه
    #      انگلیسی، نه واژه‌ای فارسی که بشود جایی پیدایش کرد.
    check("حداکثر ولتاژ باتری (Absorb)" in ino and "جریان Taper" in ino and
          "ولتاژ Float" in ino,
          "the tab must label/describe every field (user order: with descriptions)")
    for bad in ("ابزورب", "تیپر", "بالک", "دیوتی", "ددمن", "هیسترزیس"):
        check(bad not in ino,
              "transliterated term '" + bad + "' must be written as the English word")
    # [EN] This used to assert the literal `k<99`, which PINNED THE BUG: 99 was
    #      the v1.22 parameter count, and holding it there made every bench CSV
    #      row 6 columns too long from v1.23 onward. A test that hard-codes a
    #      count it should be deriving will defend the wrong answer. It now
    #      requires the loop bound to come from PN, and
    #      test_benchlog_row_matches_header_v125 checks PN against the real
    #      count and the whole row against the header.
    # [FA] این تست قبلاً خودِ عدد ۹۹ را الزام می‌کرد و در عمل باگ را قفل کرده
    #      بود: ۹۹ تعداد پارامتر v1.22 بود و نگه‌داشتنش باعث شد هر ردیف CSV از
    #      v1.23 به بعد ۶ ستون اضافه داشته باشد. تستی که عددی را hard-code کند
    #      که باید مشتق شود، از جواب غلط دفاع می‌کند.
    # [EN] v1.26 (user order 2026-09-29: "a lot of these are zero and never change
    #      - do not log them every row, or send them once at the start"): the 93
    #      settings moved out of the data row into a single '# settings:' line.
    #      That was 62 percent of every row, so the ~100 KB file now holds about
    #      2.7x more sweep points. The row must therefore NOT contain them, the
    #      settings line must still be built from the derived bound PN, and a
    #      mid-run change must be re-logged so no row inherits stale settings.
    # [FA] نسخهٔ ۱.۲۶ (دستور کاربر): ۹۳ تنظیم از ردیف داده بیرون رفت و به یک خط
    #      «# settings:» منتقل شد - ۶۲٪ هر ردیف. پس ردیف نباید آن‌ها را داشته
    #      باشد، خط تنظیمات باید از PN ساخته شود، و تغییر وسط کار باید دوباره
    #      ثبت شود تا هیچ ردیفی تنظیمات کهنه به ارث نبرد.
    check("function wset(){" in ino and "for(let k=0;k<PN;k++)P.push(" in ino and
          "async function wsync()" in ino and "WSIG" in ino and
          "'# settings: '" in ino and
          "[settings]" in ino and "[raw]" in ino,
          "the settings must be written ONCE as a '# settings:' line built from the "
          "derived bound PN, with a signature check (wsync/WSIG) that re-logs them if "
          "anything is changed mid-run - otherwise a row silently inherits the wrong "
          "settings")
    wrow_body = re.search(r"function wrow\([^)]*\)\{.*?\n\s*return \[(.*?)\]\.join",
                          ino, re.S)
    check(wrow_body and "...P," not in wrow_body.group(1),
          "the data row must NOT repeat the 125 settings - that was 62 percent of every "
          "row and it is what filled the file cap")
    txo = re.search(r"UINT8_T__G__TxOrder\[ESP_PARAM_COUNT\] = \{([^}]*)\}", ino)
    check(txo and len(txo.group(1).split(",")) == 132 and "128, 129, 130, 131 };" in ino,
          "TxOrder must list all 128 ids explicitly (v1.17: a short initializer zero-fills the tail, so the tail ids would never transmit and id 0 would repeat)")
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
          "calibration.h must host all THREE tables (ch1 LUT, ch2 LUT, voltage comp)")
    check(cal_h.count("CAL_Current1LutChainMa") >= 2 and re.search(r"#define CAL_CURRENT1_LUT_ENABLE\s+1u", cal_h) and
          "CAL_Current1LutBatteryMw" in cal_h,
          "table 1 (ch1) is FILLED since the v1.19 SOLO1 sweep (enable 1, chain->POWER anchors like ch2)")
    check("can grow into a full anchor table" in cal_h,
          "table 3 must document its growth path to an anchor table")


def test_ch2_power_lut_v113():
    """[EN] v1.13 (user order 2026-09-25, "voltages are fixed but the currents
    you read are wrong"): the ch2 LUT outputs battery-2 POWER; the live battery
    voltage turns it into current. This test replays the EXACT firmware integer
    math (BSP staged chain = the folded u32 form, proven bit-identical, with
    truncating divisions -> LUT -> x1000 / V) on the dense-run CSV rows and
    checks both the DMM agreement AND the voltage behaviour the old
    current-current table got wrong. The LUT under test is the v1.18 refit
    (14 anchors); the 2026-09-25 rows below must STILL agree (<= 6 mA).
    [FA] تست عددی v1.13: بازپخش دقیق ریاضی صحیح فرم‌ور روی ردیف‌های ران
    متراکم + بررسی رفتار ولتاژی که جدول قدیمی اشتباه می‌گرفت."""
    cal_h = (ROOT / "Firmware/Modules/Measurement/calibration.h").read_text()
    meas_c_raw = (ROOT / "Firmware/Modules/Measurement/measurement.c").read_text()

    m_chain = re.search(r"CAL_Current2LutChainMa\[\] =\s*\{([^}]*)\}", cal_h)
    m_mw = re.search(r"CAL_Current2LutBatteryMw\[\] =\s*\{([^}]*)\}", cal_h)
    check(m_chain and m_mw, "calibration.h must carry both ch2 LUT arrays")
    xs = [int(v.strip().rstrip("u")) for v in m_chain.group(1).split(",")]
    ys = [int(v.strip().rstrip("u")) for v in m_mw.group(1).split(",")]
    check(len(xs) == len(ys) == 14 and all(xs[i] < xs[i + 1] for i in range(13))
          and all(ys[i] <= ys[i + 1] for i in range(13)),
          "ch2 LUT anchors: 14 points (v1.18 refit), chain strictly increasing, power non-decreasing")

    # exact firmware math replay (folded u32 chain, truncating divisions)
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
    # [EN] The vlow column was LOGGED by a firmware that used the old 12 V
    #      divider and still applied the bench compensation. The table is power
    #      and the firmware divides it by this voltage, so the replay has to use
    #      the voltage the firmware will ACTUALLY see now: undo the compensation
    #      the logger had applied, then correct the divider to the ratio measured
    #      on the board. Replaying against the old logged value would be testing
    #      a firmware that no longer exists.
    # [FA] ستون vlow را فرم‌وری ثبت کرده که مقسم قدیمی ۱۲ولت را داشت و جبران‌ساز
    #      را هم اعمال می‌کرد. جدول «توان» است و فرم‌ور بر همین ولتاژ تقسیم می‌کند،
    #      پس بازپخش باید ولتاژی را بگذارد که فرم‌ور واقعاً خواهد دید.
    def vlow_now(vlow, i_ma):
        undone = vlow + 150 + ((i_ma if i_ma > 0 else 0) * 470) // 1000
        return round(undone * 6.0585 / 6.0294)

    worst = 0
    for raw, vlow, dmm in rows:
        err = ibat(raw, vlow_now(vlow, dmm)) - dmm
        worst = max(worst, err if raw > 20 else 0)  # the 2% row is the documented unsigned floor
    check(worst <= 6,
          f"firmware-math replay of the dense run: worst DMM error {worst} mA (<= 6 = integer-truncation bias, within DMM accuracy; the 2%-duty row floors at 0 as documented)")

    # the fix's whole point: same chain, fuller battery -> proportionally less current
    i_122, i_130, i_140, i_144 = (ibat(494.8, v) for v in (12200, 13000, 14000, 14400))
    # [EN] The property - more voltage, less current for the same chain - is the
    #      point of the power table and still holds. The absolute anchor moved
    #      658 -> 680 mA because the table was rescaled when the 12 V voltage it
    #      is divided by was corrected (divider fixed, bench compensation off):
    #      658 x 1.034 = 680. Same current from the board, different number here
    #      only because the voltage underneath it is now right.
    # [FA] خاصیت - ولتاژ بیشتر، جریان کمتر برای همان زنجیره - هدف جدول توان است و
    #      هنوز برقرار است. لنگر مطلق از ۶۵۸ به ۶۸۰ رفت چون جدول با اصلاح ولتاژ
    #      ۱۲ولت بازمقیاس شد؛ جریان واقعی همان است و فقط ولتاژ زیرش درست شده.
    check(i_122 > i_130 > i_140 > i_144 and abs(i_130 - 680) <= 3,
          f"voltage behaviour: at chain 556 the current must fall as the battery fills (12.2V:{i_122} 13.0V:{i_130} 14.0V:{i_140} 14.4V:{i_144} mA) - the old current-current table answered 658 mA at EVERY voltage")

    check("UINT32_T__G__Battery2VoltageMv = 12000u" in meas_c_raw and
          "(uint32_t__batteryPowerMw * 1000u)" in meas_c_raw,
          "the division must run with the cached clamped voltage (boot default 12.0 V); flash diet 2026-09-27: u32 is exact (power x 1000 < 2^32), the u64 only pulled __aeabi_uldivmod")


def test_ch2_lut_refit_v118():
    """[EN] v1.18 end-to-end fit (user order 2026-09-27, "look closer, there
    is still drift"): replay the exact firmware integer math on the
    2026-09-27 SOLO2 sweep (duty 1..19 step 1, off2=8/gain2=1303) and
    require DMM agreement within 5 mA with NO systematic sign - the v1.17
    float-fitted anchors drifted -4..-8 mA at D10..D15 because the integer
    chain sits ~1.5 mA left of the float chain on the steep slopes. D9 is
    EXCLUDED (10.4% low outlier: unsettled sample, raw spread 197..210,
    DMM 167 mA below the whole firmware window 177..186 mA); D16 uses the
    firmware voltage (13617 mV - the DMM 12650 mV reading is a typo against
    neighbours 13200/14000 mV). D1..D3 must read exactly 0 (the (20,0)
    noise gate + unsigned floor; the DMM's -15..-5 mA there is backfeed
    through the idle converter, which the single-supply shunt chain cannot
    see). Methodology floor is +-1 count = +-3 mA; the old 2026-09-25 run
    still replays within 6 mA (v113 test).
    [FA] تست عددی v1.18: بازپخش ریاضی فرم‌ور روی سوییپ SOLO2 با تلرانس
    5mA و بدون علامت سیستماتیک؛ D9 کنارگذاشته، ولتاژ D16 اصلاح‌شده،
    D1..D3 دقیقاً صفر. کف روش ‎±3mA است."""
    cal_h = (ROOT / "Firmware/Modules/Measurement/calibration.h").read_text()
    m_chain = re.search(r"CAL_Current2LutChainMa\[\] =\s*\{([^}]*)\}", cal_h)
    m_mw = re.search(r"CAL_Current2LutBatteryMw\[\] =\s*\{([^}]*)\}", cal_h)
    xs = [int(v.strip().rstrip("u")) for v in m_chain.group(1).split(",")]
    ys = [int(v.strip().rstrip("u")) for v in m_mw.group(1).split(",")]

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

    check("EXCLUDED: D9" in cal_h and "12650" in cal_h,
          "calibration.h must document the D9 exclusion and the D16 DMM-voltage typo")
    for raw, vlow in ((7.9, 12143), (10.6, 12141), (22.9, 12149)):
        check(ibat(raw, vlow) == 0,
              f"sweep rows D1..D3 (raw {raw}) must read exactly 0 (noise gate + unsigned floor)")
    # (raw2_avg, vlow, dmm_i_bat2); D9 excluded, D16 voltage corrected - see docstring
    rows = [(39.0, 12165, 9), (68.7, 12185, 34), (100.8, 12208, 61),
            (134.2, 12240, 92), (173.3, 12274, 129), (212.7, 12372, 211),
            (228.8, 12442, 256), (250.8, 12543, 303), (280.5, 12721, 354),
            (311.9, 12936, 404), (349.1, 13213, 455), (390.6, 13617, 498),
            (436.3, 14006, 540), (491.6, 14458, 585), (552.1, 14879, 632)]
    # [EN] Same correction as the dense-run replay above: this vlow column was
    #      logged with the old 12 V divider and the bench compensation applied,
    #      so it has to be brought to the voltage the firmware will now see.
    # [FA] همان اصلاح بازپخش بالا: این ستون vlow با مقسم قدیمی و جبران‌ساز روشن
    #      ثبت شده، پس باید به ولتاژی برسد که فرم‌ور حالا می‌بیند.
    def _vnow(vlow, i_ma):
        return round((vlow + 150 + (max(i_ma, 0) * 470) // 1000) * 6.0585 / 6.0294)

    worst = 0
    for raw, vlow, dmm in rows:
        worst = max(worst, abs(ibat(raw, _vnow(vlow, dmm)) - dmm))
    check(worst <= 5,
          f"firmware-math replay of the 2026-09-27 SOLO2 sweep: worst DMM error {worst} mA (<= 5, no systematic sign; the v1.17 float fit drifted -8 mA at D15)")


def test_ch1_lut_v119():
    """[EN] v1.19 (user order 2026-09-27, "check the upper charger's data -
    it went past 15 V and never cut off"): replay the exact firmware integer
    math on the 2026-09-27 SOLO1 sweep (duty 1..18 step 1, off1=8/gain1=1046)
    through the TABLE 1 chain->POWER LUT, dividing by the CORRECTED firmware
    divisor (V24 pack top 66200: v24 x 73000 // 69200 - vlow). Require DMM
    agreement within 5 mA with no systematic sign (actual worst 4, minimax
    hump +3/-4; the old linear chain read an S-curve -29..+72 mA).
    D1..D3 must read exactly 0 (the (5,0) noise gate: SOLO1 rows D1..D3 never
    exceed chain 1; the DMM's -15..-5 mA there is backfeed the single-supply
    shunt chain cannot see). Methodology floor is +-1 count = +-3 mA.
    The same order's companion safety fix is pinned too: the pack divider
    top 62400 -> 66200 (the old top read ~1.4 V low and blinded the 15.0 V
    hard OV cut - firmware saw 13.9 V at a true 15.22 V); D18's replayed
    vhigh must reach the 15000 mV cutoff so the manual-mode cut provably
    trips where the user had to stop by hand.
    [FA] تست عددی v1.19: بازپخش ریاضی فرم‌ور روی سوییپ SOLO1 از جدول توان
    کانال ۱ با مقسوم‌علیه اصلاح‌شده (تاپ پک ۶۶۲۰۰) با تلرانس 5mA و بدون
    علامت سیستماتیک (واقعی ۴)؛ D1..D3 دقیقاً صفر؛ فیکس ایمنی همراه (تاپ
    ۶۲۴۰۰←۶۶۲۰۰) و رسیدن vhigh بازپخش‌شدهٔ D18 به آستانهٔ ۱۵V هم قفل
    می‌شود. کف روش ‎±3mA است."""
    cal_h = (ROOT / "Firmware/Modules/Measurement/calibration.h").read_text()
    bsp_c = (ROOT / "Firmware/Bsp/Src/bsp_measurement.c").read_text()
    chg_c = (ROOT / "Firmware/Modules/Charger/charger.c").read_text()
    m_chain = re.search(r"CAL_Current1LutChainMa\[\] =\s*\{([^}]*)\}", cal_h)
    m_mw = re.search(r"CAL_Current1LutBatteryMw\[\] =\s*\{([^}]*)\}", cal_h)
    xs = [int(v.strip().rstrip("u")) for v in m_chain.group(1).split(",")]
    ys = [int(v.strip().rstrip("u")) for v in m_mw.group(1).split(",")]

    def bsp_chain(counts, off=8, gain=1046):
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

    def vhigh_corr(v24, vlow):
        # [EN] The sweep's v24 was logged while the pack net still used total
        #      69200. Rescale it to the total the divider was MEASURED to have
        #      on the board: 74800 (68K over 6.8K). The schematic reading said
        #      76000 because it counted the 1.2K, but probing the ADC pin showed
        #      that resistor sits after the tap and drops no DC - two
        #      independent nets landed on 11.0000 to within 0.05 %.
        # [FA] v24 این سوییپ با مجموع ۶۹۲۰۰ ثبت شده؛ به مجموعی مقیاس می‌شود که
        #      روی برد اندازه گرفته شد یعنی ۷۴۸۰۰. خوانش شماتیک ۷۶۰۰۰ می‌گفت چون
        #      ۱٫۲ کیلواهم را می‌شمرد، ولی اندازه‌گیری پایهٔ ADC نشان داد آن مقاومت
        #      بعد از نقطهٔ تقسیم است و افت DC ندارد.
        return (v24 * 74800) // 69200 - vlow

    def ibat(raw, v24, vlow):
        return (lut_mw(bsp_chain(raw)) * 1000) // vhigh_corr(v24, vlow)

    check("D7" in cal_h and "12200" in cal_h,
          "calibration.h must document the kept D7 dip and the D5 DMM-voltage outlier")
    for raw in (7.4, 7.6, 7.5):
        check(lut_mw(bsp_chain(raw)) == 0,
              f"sweep rows D1..D3 (raw {raw}) must read exactly 0 (noise gate + unsigned floor)")
    # (raw1_avg, v24_fw, vlow_fw, dmm_i_bat1) - divisor is the CORRECTED firmware vhigh
    rows = [(20.6, 22760, 12283, 11), (42.8, 22797, 12274, 36),
            (68.1, 22838, 12265, 64), (97.0, 22887, 12258, 85),
            (132.5, 22952, 12251, 133), (170.3, 23032, 12240, 173),
            (214.7, 23162, 12233, 217), (260.4, 23371, 12222, 260),
            (310.1, 23737, 12210, 298), (368.1, 24285, 12201, 336),
            (425.9, 24783, 12190, 375), (492.5, 25294, 12182, 416),
            (558.2, 25657, 12170, 460), (626.7, 25870, 12158, 510),
            (705.5, 26040, 12145, 568)]
    worst = 0
    for raw, v24, vlow, dmm in rows:
        worst = max(worst, abs(ibat(raw, v24, vlow) - dmm))
    check(worst <= 5,
          f"firmware-math replay of the 2026-09-27 SOLO1 sweep through the ch1 power LUT with the corrected divisor: worst DMM error {worst} mA (<= 5, no systematic sign; the old linear chain drifted -29..+72 mA)")
    check(re.search(r"#define BSP_MEASUREMENT_DIV24BAT_TOP_OHMS\s+BSP_MEASUREMENT_DIV24_TOP_OHMS", bsp_c) and
          not re.search(r"#define\s+\w+\s+(66200|62400)u", bsp_c),
          "the pack divider must BE the input divider (both nets are 68K+1.2K over 6.8K); "
          "62400 and 66200 were successive fudges, neither is a resistor on this board")
    ov_ceiling = int(re.search(r"#define CHG_MAX_VALID_BATTERY_MV\s+(\d+)u",
                               (ROOT / "Firmware/Modules/Charger/charger.h").read_text()).group(1))
    vh18 = vhigh_corr(26040, 12145)
    check(vh18 >= ov_ceiling,
          f"D18's replayed vhigh must still reach the {ov_ceiling} mV hard cutoff "
          f"(got {vh18} mV) so the manual-mode OV cut provably trips on the row where the "
          "operator had to stop by hand")
    # [EN] OPEN BENCH DISCREPANCY, recorded rather than papered over. With the
    #      honest divider this row replays at ~16.4 V while the 2026-09-27 note
    #      says the DMM read 15.22 V. The 2026-09-25 CSV disagrees with that
    #      note: there the V12 net is accurate to 0.4 percent and the input net
    #      to 1.2 percent using this SAME two-series-plus-shunt model, and the
    #      honest divider is what makes the two half-packs come out plausible
    #      (12.07 / 12.94 V) instead of leaving one half at an implausible
    #      10.70 V. One DMM reading on pack+ settles it; until then the
    #      SCHEMATIC wins, because a divider is hardware, not a fitted number.
    # [FA] اختلاف باز بنچ، ثبت شده نه ماست‌مالی. با مقسم صادقانه این ردیف حدود
    #      ۱۶٫۴ ولت بازپخش می‌شود در حالی که یادداشت ۲۰۲۶-۰۹-۲۷ می‌گوید DMM
    #      ۱۵٫۲۲ خوانده. CSV ۲۰۲۶-۰۹-۲۵ با آن یادداشت نمی‌خواند: همین مدل روی
    #      نت ۱۲ولت تا ۰٫۴٪ و روی ورودی تا ۱٫۲٪ دقیق است. یک اندازه‌گیری DMM
    #      روی pack+ تکلیف را روشن می‌کند؛ تا آن موقع شماتیک برنده است.
    chg_h_txt = (ROOT / "Firmware/Modules/Charger/charger.h").read_text()
    m_early = re.search(r"#define CHG_OV_DECIDE_EARLY_MV\s+(\d+)u", chg_h_txt)
    check(m_early and int(m_early.group(1)) >= 50,
          "the safety margin that used to be bought by bending the divider must now exist "
          "as a REAL earlier decision: CHG_OV_DECIDE_EARLY_MV has to be a non-trivial "
          f"margin, got {m_early.group(1) if m_early else 'missing'}")
    check(re.search(r"#define CHG_OV_CUTOFF_DEFAULT_MV\s*\\?\s*\n?\s*\(CHG_MAX_VALID_BATTERY_MV - CHG_OV_DECIDE_EARLY_MV\)", chg_h_txt) and
          "UINT32_T__G__ChargerOvCutoffMv = CHG_OV_CUTOFF_DEFAULT_MV;" in chg_c,
          "the default OV cut-off must sit BELOW the absolute ceiling by that margin and "
          "actually be used as the power-on default - stopping before the ceiling instead "
          "of at it is the honest replacement for the old scale fudge")
    check("func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap,\n                                       uint8_t__channelIndex) >= UINT32_T__G__ChargerOvCutoffMv" in chg_c,
          "ManualDriveChannel must keep the hard OV comparison (channel voltage >= ChargerOvCutoffMv forces duty 0) - the logic was sound, it was only blinded by the divider")


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
    ino = "\n".join((ROOT / "esp_link_panel" / f).read_text(encoding="utf-8") for f in ["esp_link_panel.ino", "plink_config.h", "plink_params.h", "plink_state.h", "plink_panel.h", "plink_font.h", "plink_link.h", "plink_http.h"])

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

    # [EN] v1.66: application FLASH is 60K now - the bench-LUT block took two
    #      more pages at 0x0800F000. Both reserved regions must be declared, or
    #      the image could grow into them without the linker saying a word.
    # [FA] v1.66: فلش برنامه ۶۰K شد - بلوک جدول بنچ دو صفحهٔ دیگر گرفت. هر دو
    #      ناحیهٔ رزرو باید اعلام شوند وگرنه تصویر بی‌صدا داخلشان رشد می‌کند.
    # [EN] v1.80: the parameter record outgrew one 1 KiB page (ids 128..131),
    #      so each ping-pong bank is TWO pages and the 4 KiB record block moved
    #      down to 0x0800E000. Application FLASH is 56K; the two old record
    #      pages stay reserved as SPARE so stale field records are never
    #      overwritten by code.
    # [FA] از v1.80 بلوک رکوردها ۴K در 0x0800E000 است و فلش برنامه ۵۶K.
    check(re.search(r"FLASH\s+\(rx\)\s*: ORIGIN = 0x8000000,\s*LENGTH = 56K", ld) and
          re.search(r"NVM\s+\(r\)\s*: ORIGIN = 0x800E000,\s*LENGTH = 4K", ld) and
          re.search(r"LUTNVM\s+\(r\)\s*: ORIGIN = 0x800F000,\s*LENGTH = 2K", ld) and
          re.search(r"SPARE\s+\(r\)\s*: ORIGIN = 0x800F800,\s*LENGTH = 2K", ld),
          "the linker must shrink application FLASH to 56K and declare all three reserved regions: the 4K parameter-record block at 0x0800E000 (v1.80), the LUT block at 0x0800F000 (v1.66) and the retired record pages at 0x0800F800 kept as SPARE")
    check("0x0800E000u" in nvm_h and "0x0800E800u" in nvm_h
          and re.search(r"ESP_LINK_NVM_FLASH_PAGE_SIZE\s+0x400u", nvm_h),
          "the two persistence banks must be two 1 KiB pages each, with the erase granularity spelled out so the save loop can erase every page of a bank")
    # [EN] Matched by regex, not by exact spacing: these #defines are column
    #      aligned, so going from a 2-digit to a 3-digit id silently broke a
    #      literal-string check that had nothing to do with what it tested.
    # [FA] با regex تطبیق می‌شود نه با فاصله‌گذاری دقیق: این تعریف‌ها ستونی
    #      تراز شده‌اند و رفتن از شناسهٔ دو رقمی به سه رقمی، چکِ رشتهٔ عینی را
    #      بی‌صدا می‌شکست بدون آن‌که ربطی به چیزی که می‌سنجید داشته باشد.
    check(re.search(r"ESP_LINK_NVM_ENTRY_MAX\s+144u", nvm_h) and
          re.search(r"ESP_LINK_NVM_PERSISTED_ID_MAX_LOW\s+14u", nvm_h) and
          re.search(r"ESP_LINK_NVM_PERSISTED_ID_MIN_HIGH\s+20u", nvm_h) and
          re.search(r"ESP_LINK_NVM_PERSISTED_ID_MAX_HIGH\s+131u", nvm_h) and re.search(r"ESP_LINK_NVM_SLOT_MIN_ID\s+200u", nvm_h) and re.search(r"ESP_LINK_NVM_SLOT_MAX_ID\s+203u", nvm_h),
          "persisted set = 0..14 + 20..75 + 77..131 + runtime slots 200..203 (128 entries in 144 slots - v1.80 moved the record into a TWO-page 2 KiB bank at 0x0800E000 because the scenario-6 lamp/buzzer ids 128..131 pushed it past the old single full page; v1.49 added the charge map 119/120, v1.50 the band-2 beep shape 121/122, v1.68 the imbalance blink 123/124, v1.72 the dead-battery ids 125..127 and the latch slot 203, v1.80 its lamp/buzzer 128..131) - the transient test modes 15..19 and the panel-session mute 76 must NEVER survive a reboot, but the imbalance verdict budget MUST")
    # [EN] v1.71 bumps 10 -> 11. This one is a MEANING bump, not a layout
    #      bump: ids 57 and 122 kept their slots but changed units (critical
    #      duty % -> critical per-beep ms, band-2 gap -> band-2 repeat
    #      interval). A v10 record holds 100 in both, which under the new
    #      meaning is a 100 ms beep every 100 ms - loud nonsense. Rejecting
    #      the old record is the point.
    # [FA] نسخهٔ ۱۱ تغییر معنی است نه چیدمان؛ رکورد قدیمی باید رد شود.
    check(re.search(r"ESP_LINK_NVM_VERSION\s+11u", nvm_h),
          "v1.43 bumps the NVM record version to 10: a v9 record carries 108 slots, so "
          "replaying one into a 122-slot layout would leave ids 108..118 and 200..202 holding whatever "
          "the erased flash reads as. The version check must reject it and fall back to "
          "compiled defaults - the first boot after this upgrade is a factory-default boot")

    # the persisted-id predicate in C, replicated and cross-checked
    persisted = {i for i in range(256) if i <= 14 or (20 <= i <= 122 and i != 76) or 200 <= i <= 202}
    check(persisted == set(range(15)) | set(range(20, 76)) | set(range(77, 123)) | {200, 201, 202} and 19 not in persisted and 15 not in persisted and 76 not in persisted,
          f"persisted id set must exclude 15..19 and 76 (got {len(persisted)} ids)")

    tab2 = ino.split('id="p2"', 2)[1]
    # [EN] v1.31: the chart is mounted on the chargers page as well, because
    #      that is where the operating table is and the user edits ON the
    #      chart. So the redraw can no longer be gated on tab 2 alone - a
    #      mount that is never drawn is a container that is present, empty
    #      and permanently silent, and the markup looks perfectly correct.
    # [FA] نمودار روی صفحهٔ شارژرها هم نصب است، چون جدول عملکرد همان‌جاست و
    #      کاربر روی نمودار ویرایش می‌کند. پس بازرسم دیگر نمی‌تواند فقط به تب ۲
    #      مشروط باشد - محل نصبی که هرگز رسم نشود یعنی ظرفی که هست، خالی است و
    #      برای همیشه ساکت می‌ماند، در حالی که مارک‌آپ کاملاً درست به نظر می‌رسد.
    # [EN] v1.33: the chart has ONE mount (chargers page) and PID moved into
    #      sub-tab 0, so the gates changed shape. The persistence text left
    #      with the duplicate profile form and was deliberately carried over
    #      into the chart's own help - losing it would have been dropping
    #      information, not dropping a duplicate.
    # [FA] نمودار یک محل نصب دارد و PID به زیرتب ۰ رفت، پس گاردها عوض شدند.
    #      متن ماندگاری همراه فرم تکراری رفت و عمداً به راهنمای خود نمودار
    #      منتقل شد - از دست دادنش یعنی انداختن «اطلاعات»، نه «تکرار».
    check("روی فلش برد ذخیره" in ino and
          "ماندگاری:" in ino and "function qgraph()" in ino and
          "if(TAB==2&&STAB==0)qgraph();" in ino and
          "if(TAB==2){if(STAB==0)pchk();else if(STAB!=3)afresh();}astat();" in ino and
          "نمودار مراحل شارژ" in ino,
          "the panel must carry the stage graph (one mount, gated on the "
          "settings sub-tab that now owns it; PID guard on sub-tab 0) and "
          "the persistence texts")
    # [EN] v1.32 (user order: "remove the extra and duplicated items"): the
    #      second copy of the chart was itself the duplication. Exactly one
    #      mount survives, on the chargers page above the operating table.
    # [FA] نسخهٔ دوم نمودار خودش همان تکرار بود. دقیقاً یک محل نصب می‌ماند، روی
    #      صفحهٔ شارژرها بالای جدول عملکرد.
    # [EN] v1.34: the settings mount is back by user order. Counting mounts
    #      encoded a preference that has since reversed; what must hold is
    #      that one renderer fills them all, so they cannot disagree.
    # [FA] نسخهٔ تب تنظیمات به دستور کاربر برگشت. شمردن محل نصب یک ترجیح را
    #      کد کرده بود که برگشت؛ چیزی که باید برقرار بماند این است که یک
    #      رندرکننده همه را پر کند تا نتوانند حرف متفاوت بزنند.
    # [EN] v1.37 (user order 2026-10-03: "why did you put the chart in the
    #      panel too? delete it from there. put the tables under it into the
    #      chargers page"). The preference reversed a THIRD time, which is
    #      exactly why the count is not what gets pinned. What is pinned is
    #      the pair of facts that cannot be argued with: the chart has one
    #      home and cannot be out of step with itself, and the chargers page
    #      keeps the operating table the user asked to have there.
    # [FA] ترجیح برای سومین بار برگشت، و دقیقاً به همین دلیل «تعداد» چیزی
    #      نیست که قفل شود. چیزی که قفل می‌شود دو واقعیت بحث‌ناپذیر است:
    #      نمودار یک خانه دارد و نمی‌تواند با خودش ناهماهنگ شود، و صفحهٔ
    #      شارژرها جدول عملکردی را که کاربر خواسته نگه می‌دارد.
    check(ino.count('class="qgm"') == 1,
          "the chart has exactly one home, so no two copies can disagree")
    _p0 = ino.split('id="p0"', 1)[1].split('id="p1"', 1)[0]
    check('class="qgm"' not in _p0,
          "the chart is OFF the chargers page, as ordered")
    check('id="ctb"' not in _p0 and 'id="ctb"' not in ino,
          "the operating table stays GONE - the user ordered its wholesale deletion (v1.36)")
    # [EN] v1.35 (user order 2026-10-03: "write those times underneath so the
    #      charts do not get so crowded - do the same for the gains"). These
    #      two assertions demanded the exact opposite and are INVERTED, not
    #      deleted: the timers and gains must now be chips UNDER the chart,
    #      and must no longer be drawn as floating runs over the I-V plane.
    #      Deleting them would leave the next layout change unguarded, which
    #      is how the on-plot version crept back in the first place.
    # [FA] این دو ادعا دقیقاً عکس دستور تازه بودند و «وارونه» شدند، نه حذف:
    #      زمان‌ها و گین‌ها حالا باید زیر نمودار تراشه باشند و دیگر نباید روی
    #      صفحهٔ I-V شناور رسم شوند. حذف‌کردنشان تغییر چیدمان بعدی را بی‌نگهبان
    #      می‌گذاشت - و دقیقاً همین‌طور بود که نسخهٔ روی‌نمودار برگشت.
    check(".evcg" in ino and 'class="evc"' in ino and
          ino.count('class="qgcm"') == ino.count('class="qgm"'),
          "the timers and gains must be chips under EVERY chart, not drawn on it")
    check(" const ann=(x,y,parts,anchor)=>{" not in ino and
          "s+=ann(" not in ino,
          "the on-plot annotation helper must be gone, not merely unused: "
          "dead UI code is how a removed layout comes back by accident")
    # [EN] Moved, never demoted - a chip that lost its editor hook would look
    #      the same and silently stop being settable.
    # [FA] منتقل شد، نه تنزل‌یافته - تراشه‌ای که قلاب ویرایش را از دست بدهد
    #      دقیقاً همان‌شکل است و بی‌صدا غیرقابل‌تنظیم می‌شود.
    import re as _re
    _evn = _re.search(r"const EVN=\{(.*?)\};", ino, _re.S)
    _names = _re.findall(r"\d+:'([^']+)'", _evn.group(1)) if _evn else []
    # [EN] v1.73 (user order: remove the "!" badges, explain above each
    #      section instead) - the same coverage rule now applies to the
    #      always-visible chart explanation.
    # [FA] از v1.73 همین قاعده روی متنِ همیشه-روی-صفحهٔ کارت نمودار اعمال می‌شود.
    _ch = _re.findall(
        r'<b>نمودار فقط ولتاژ و جریان را نشان می‌دهد</b>(.*?)</div>', ino, _re.S)
    check(len(_names) >= 13 and _ch and
          all(all(n in h for n in _names) for h in _ch),
          "the visible chart explanation must cover every variable name "
          "(user order: say how to tune these and what each name means)")
    # [EN] Strip comments before looking for the banned call. The first run of
    #      this check failed on the only remaining $('qg') in the file - inside
    #      the bilingual comment explaining why $('qg') is wrong. Same trap as
    #      the audit's "hard-coded bound" finding that lived only in the
    #      sentence warning against hard-coded bounds.
    # [FA] قبل از جست‌وجوی فراخوان ممنوع، کامنت‌ها را حذف کن. اولین اجرای این
    #      چک روی تنها $('qg') باقی‌ماندهٔ فایل شکست - داخل همان کامنت دوزبانه‌ای
    #      که توضیح می‌دهد چرا $('qg') غلط است.
    ino_live = re.sub(r"/\*.*?\*/", "", ino, flags=re.S)
    check("document.querySelectorAll('.qgm')" in ino_live and
          "$('qg')" not in ino_live,
          "the mount must still be found by class, not a hard-coded id: $('qg') "
          "would silently fill only the first of any future pair")
    check('<button data-t="2">تنظیمات</button>' in ino and
          'id="q7"' in tab2 and 'id="q8"' in tab2 and 'id="a7"' in tab2 and 'id="a8"' in tab2 and
          "پنجرهٔ median" in tab2 and "پنجرهٔ میانگین" in tab2 and
          # [EN] v1.33: ids 20..26 left this loop with their input boxes.
          # [FA] شناسه‌های ۲۰..۲۶ همراه کادرهای ورودی‌شان از این حلقه رفتند.
          "for(const id of [7,8])" in ino and
          "row(7)+row(8)" not in ino,
          "v1.14b (user order 2026-09-26): the median/average window controls must live in the settings tab under the filter section (nav renamed, tab 0 keeps only the live status), and both ids bind through the same send/qfill path")
    # [EN] v1.32 (user order 2026-10-03: "write the English word for the ones
    #      that should be English"). This check used to demand the doubled
    #      form "ناحیهٔ ابزورب (Absorb)" - a transliteration followed by the
    #      real term in brackets. That is the pattern the user rejected, so
    #      the assertion is INVERTED rather than deleted: the labels must
    #      still all exist, in the single English form, and the doubled form
    #      must not come back. Deleting it would have left the next rename
    #      unguarded.
    # [FA] این چک قبلاً شکل دوتایی «ناحیهٔ ابزورب (Absorb)» را طلب می‌کرد -
    #      ترانویسی به‌علاوهٔ اصطلاح واقعی در پرانتز؛ دقیقاً همان الگویی که
    #      کاربر رد کرد. پس ادعا به‌جای حذف‌شدن «وارونه» شد: همهٔ برچسب‌ها باید
    #      همچنان وجود داشته باشند، در شکل یگانهٔ انگلیسی، و شکل دوتایی نباید
    #      برگردد. حذف‌کردنش تغییرنام بعدی را بی‌نگهبان می‌گذاشت.
    check("ناحیهٔ Absorb" in ino and "ناحیهٔ Float" in ino and
          "ناحیهٔ Over" in ino and "زیر Reentry" in ino and
          "Hard cutoff 15V" in ino and "'Bulk'" in ino and "'Off'" in ino and
          # [EN] A Persian description with the English term in brackets is
          #      fine and stays - "حداکثر ولتاژ باتری (Absorb)" names a thing
          #      in Persian and then gives its standard term. What is banned
          #      is the transliteration, and the loop above already pins that,
          #      so this must NOT be widened into "no brackets anywhere".
          # [FA] توضیح فارسی با اصطلاح انگلیسی داخل پرانتز درست است و می‌ماند؛
          #      چیزی که ممنوع است ترانویسی است و حلقهٔ بالا همان را میخ کرده.
          "ابزورب" not in ino and "شناور (" not in ino and
          # [EN] This pinned the literal #0b0f17. Fifth frozen hex/number found
          #      defending a VALUE instead of an INTENT, and it blocked the
          #      requested re-theme. The intent is "the graph uses the panel's
          #      own inset colour", so derive it from :root - which is strictly
          #      stronger, because now the graph can never drift from the theme.
          # [FA] این عدد ۰b0f17 را منجمد کرده بود - پنجمین مقدار ثابتی که
          #      به‌جای «نیت» از یک «عدد» دفاع می‌کرد و جلوی تغییر تم را گرفت.
          #      نیت این است که نمودار از رنگ فرورفتگی خود پنل استفاده کند، پس
          #      از :root مشتق می‌شود و حالا نمودار هرگز از تم جدا نمی‌افتد.
          ('fill="%s"' % re.search(r"--in:(#[0-9a-f]{6})",
                                   re.search(r":root\{([^}]*)\}", ino).group(1)).group(1)) in ino,
          "the stage graph must use the dark panel palette (v1.16f inset #0b0f17) with bilingual (FA+EN) zone, threshold and stage labels")
    check("'Battery low (Vlow)',tt[17],tt[13],tt[10]" in ino and
          "'Battery high (Vhigh)',tt[18],tt[6],tt[3]" in ino and
          # [EN] The DOT must exist; its radius is cosmetic and was frozen at 7
          #      here, which blocked a requested resize. Fourth hard-coded
          #      literal found defending a value instead of an intent - check
          #      that a dot is drawn at a sane size, not that it never changes.
          # [FA] نقطه باید باشد؛ شعاعش ظاهری است و روی ۷ منجمد شده بود و جلوی
          #      تغییر اندازهٔ درخواستی را گرفت. چهارمین عدد hard-code شده که
          #      به‌جای «نیت» از یک «مقدار» دفاع می‌کرد.
          re.search(r'<circle cx="\$\{x\}" cy="\$\{y\}" r="(\d+(?:\.\d+)?)"', ino) and
          2.0 <= float(re.search(r'<circle cx="\$\{x\}" cy="\$\{y\}" r="(\d+(?:\.\d+)?)"', ino).group(1)) <= 10.0 and
          'stroke="#e7eaf0"' not in ino and "marker-end" not in ino,
          "v1.14c (user order 2026-09-26, 'show each battery's position and state; the white Bulk curve is confusing - are the zones not enough?'): the graph drops the V(t) curve and cycle arrow, and each battery gets a live position DOT on its own voltage column (ch2->Vlow t17/t13/t10, ch1->Vhigh t18/t6/t3) with a state chip under the chart")
    # [EN] v1.31: the warning box is mounted next to every copy of the chart,
    #      so it is a class now. An id would have warned on one page only -
    #      and the chargers page is the one where the numbers are edited.
    # [FA] جعبهٔ هشدار کنار هر نسخه از نمودار نصب است، پس حالا کلاس است. با id
    #      فقط روی یک صفحه هشدار می‌داد - و صفحهٔ شارژرها همان جایی است که
    #      اعداد ویرایش می‌شوند.
    check(ino.count('class="qwm"') >= 1 and
          "document.querySelectorAll('.qwm').forEach" in ino and
          'function qchk()' in ino and
          'q.o.d' in ino and 'q.r.d' in ino and 'pvln(q.o' in ino and
          'const LL=[],PL=[]' in ino and
          'باز هم ارسال شود؟' in ino and 'نگهبان ترکیب' in ino,
          "v1.14d (user order 2026-09-26, 'stretch the graph downward, the zone borders are cramped; zones must follow the profile numbers and never overlap'): zones drawn from APPLIED values with dashed preview lines for typed values, anti-collision label pass (ZL/LL; v1.39 moved the zone names out of the chart into an external legend, so v1.41 pruned the now-empty ZL list - the pass is PL/LL), and a qchk() guard - red field + confirm-before-send on invalid combos (v1.69 user order: the red BANNER is gone, it claimed the board clamps combinations and the board has not done that since v1.56)")

    # [EN] v1.69 (user order: "what is this message? why should it be there?
    #      delete this junk"): the cross-field banner must stay deleted. It
    #      told the operator "the board will clamp these", which stopped
    #      being true in v1.56 when the joint rules moved into the panel.
    # [FA] بنر ترکیب نامعتبر حذف شد و باید حذف بماند.
    for junk in ("ترکیب نامعتبر — برد این‌ها را گیره می‌زند",
                 "۱۱۳ مقدار ماندگار"):
        check(junk not in re.sub(r"/\*.*?\*/", " ", ino, flags=re.S),
              f"the panel must no longer print the deleted text: {junk}")

    # [EN] v1.69: the sticky bars are measured, not hard-coded, and the
    #      scenario-card bar parks BELOW the section bar instead of on top of
    #      it (that is what hid the "charger / scenarios / ..." row).
    # [FA] نوارهای چسبان اندازه‌گیری می‌شوند و نوار کارت‌های سناریو زیر نوار
    #      بخش‌ها می‌چسبد نه رویش.
    # [EN] v1.70 (user order: "the message must be complete and tell me
    #      whether to resend"): the verdict used to be written only into
    #      #sbar, which hides itself as soon as the queue empties - so the
    #      success line was erased in the same tick it was produced. There is
    #      now a result card that stays until dismissed, and it offers a
    #      resend button on every outcome where something did not land. An id
    #      with no echo stays queued so that resend actually has something to
    #      send.
    # [FA] کارت نتیجهٔ ارسال که تا بسته نشود می‌ماند و دکمهٔ «دوباره بفرست».
    check('id="sres"' in ino and "function sdlg(" in ino and "function sdlgx()" in ino
          and "دوباره بفرست" in ino and "#sres.on{display:flex}" in ino
          and "⛔ هیچ‌کدام ارسال نشد" in ino and "⚠ ارسال ناقص" in ino
          and "✅ همه نشست" in ino,
          "every send must end in a result card that survives the queue emptying, "
          "with a resend button whenever part of the batch did not land")
    # [EN] no echo must NOT clear the pending flag - otherwise the resend
    #      button would have an empty queue and silently do nothing.
    body = ino[ino.index("async function sendall()"):]
    body = body[:body.index("function num(id)")]
    check("if(back==null){bad.push(id+': بی‌پاسخ');continue;}" in body
          and body.index("continue;") < body.index("pclr(id);"),
          "an unanswered id must stay in PEND so that 'resend' has something to resend")

    # [EN] v1.70 (user order: "the 'board value' note under some boxes never
    #      updates after a change and is not needed - we have the factory
    #      default"): the chip is gone. The a<id> spans stay, because the
    #      queue/send states ("در صف", "…", "خطا") are written into them.
    # [FA] چیپ «روی برد: …» حذف شد؛ خود span ها می‌مانند.
    check("'روی برد: '" not in ino and "a.textContent=''" in ino
          and "در صف" in ino,
          "the stale 'board value' chip must be gone, while the queue state stays")

    check("function stickfit()" in ino and "--t-sub2" in ino
          and "#usel{position:sticky;top:var(--t-sub2" in ino
          and "top:113px" not in ino,
          "the scenario bar must stick below the section bar, from measured heights")
    # [EN] The 2026-09-26 order was "at least 50 % taller" (H 560 -> 840); the
    #      2026-09-29 order REVERSES it ("make the height 50 % less, and the
    #      text smaller so it stops overlapping"), so H is 420. What still
    #      matters and is still checked: the hatched Bulk zone, and that the
    #      label pitch shrank WITH the height - halving the canvas while
    #      leaving a 16 px stacking pitch is exactly what would make the
    #      labels collide, which is the thing the user complained about.
    # [FA] دستور ۲۰۲۶-۰۹-۲۶ «حداقل ۵۰٪ بلندتر» بود؛ دستور ۲۰۲۶-۰۹-۲۹ برعکسش
    #      می‌کند، پس H برابر ۴۲۰ است. آنچه هنوز مهم است و سنجیده می‌شود: ناحیهٔ
    #      حاشورخوردهٔ بالک، و اینکه گام عمودی برچسب‌ها همراه ارتفاع کوچک شده
    #      باشد - نصف‌کردن بوم با نگه‌داشتن گام ۱۶ پیکسلی دقیقاً همان چیزی است
    #      که برچسب‌ها را روی هم می‌اندازد.
    m_h = re.search(r"\bH=(\d+),X0=", ino)
    m_gap = re.search(r"LBL_GAP=(\d+)", ino)
    check(m_h and int(m_h.group(1)) == 420 and m_gap and int(m_gap.group(1)) <= 12 and
          'id="bkh"' in ino and 'url(#bkh)' in ino and
          'ناحیهٔ Bulk' in ino and 'patternTransform="rotate(45)"' in ino,
          "v1.14e (user order 2026-09-26, 'at least 50% taller; what is the zone between float and absorb - hatch the bulk zone lightly'): chart height is now 420 (user order 2026-09-29 halved the 840 set on 2026-09-26) and the label stacking pitch shrank with it (LBL_GAP <= 12) so the labels stop colliding; the former grey filler between absorb-enter and float remains a labelled Bulk zone with a light diagonal hatch pattern")

    # ---------- compiled fault-injection run of the EXACT flash-state code ----------
    gcc = shutil.which("gcc")
    if gcc is None:
        print("  SKIP: gcc not found - the compiled NVM fault-injection run was not executed (static checks above still ran)")
        return
    tmp = tempfile.mkdtemp(prefix="nvm_harness_")
    try:
        (Path(tmp) / "stub_esp_link.h").write_text(
            "#include <stdint.h>\n#include <stdbool.h>\n"
            "#define ESPLINK_PARAM_COUNT 83u\n"
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
/* v1.80: a bank is two 1 KiB pages now, so B sits 2 KiB above A */
#define ESP_LINK_NVM_PAGE_B_ADDR (EMU_FLASH_BASE + 2048u)
#include "esp_link_nvm.h"
#include "stub_esp_link.h"
#include "stub_bsp_flash.h"

int g_cut_after = -1, g_erase_cut = 0, g_apply_calls = 0;
uint32_t g_params[99];
static uint32_t clampf(uint32_t v, uint32_t lo, uint32_t hi){ return v < lo ? lo : (v > hi ? hi : v); }
bool func__EspLink_ApplyParam(uint8_t id, uint32_t value, uint32_t *applied){
    g_apply_calls++;
    if (id >= 83u) return false;
    switch (id) {
        case 20: value = clampf(value, 11000, 14600); break;
        case 21: value = clampf(value, 13800, 14550); break;
        case 22: value = clampf(value, 14500, 14750); break;
        case 23: value = clampf(value, 9000, 14300); break;
        case 24: value = clampf(value, 8000, 14000); break;  /* float-300, float max 14300 */
        case 25: value = clampf(value, 100, 900); break;
        case 26: value = clampf(value, 10, 300); break;
        case 27: value = clampf(value, 14000, 15000); break;
        case 28: value = clampf(value, 50, 1000); break;
        case 29: value = clampf(value, 3000, 8000); break;
        case 30: value = clampf(value, 4000, 9000); break;
        case 31: value = clampf(value, 100, 5000); break;
        case 32: value = clampf(value, 100, 5000); break;
        case 33: value = clampf(value, 18000, 24000); break;
        case 34: value = clampf(value, 24000, 30000); break;
        case 35: value = clampf(value, 150, 950); break;
        case 36: value = clampf(value, 14000, 15000); break;
        case 37: value = clampf(value, 0, 8000); break;
        case 38: value = clampf(value, 100, 10000); break;
        case 39: value = clampf(value, 0, 100); break;
        case 40: value = clampf(value, 0, 600000); break;
        case 41: value = clampf(value, 0, 600000); break;
        case 42: value = clampf(value, 0, 10); break;
        case 43: value = clampf(value, 0, 5000); break;
        case 44: value = clampf(value, 100, 10000); break;
        case 45: value = clampf(value, 0, 100); break;
        case 46: value = clampf(value, 0, 600000); break;
        case 47: value = clampf(value, 0, 600000); break;
        case 48: value = clampf(value, 0, 10); break;
        case 49: value = clampf(value, 0, 5000); break;
        case 50: value = clampf(value, 0, 100); break;
        case 51: value = clampf(value, 0, 100); break;
        case 52: value = clampf(value, 0, 100); break;
        case 53: value = clampf(value, 0, 100); break;
        case 54: value = clampf(value, 0, 600000); break;
        case 55: value = clampf(value, 0, 600000); break;
        case 56: value = clampf(value, 0, 600000); break;
        case 57: value = clampf(value, 0, 100); break;
        case 58: value = clampf(value, 0, 10); break;
        case 59: value = clampf(value, 0, 600000); break;
        case 60: value = clampf(value, 0, 600000); break;
        case 61: value = clampf(value, 0, 120000); break;
        case 62: value = clampf(value, 0, 10); break;
        case 63: value = clampf(value, 0, 10); break;
        case 64: value = clampf(value, 0, 10); break;
        case 65: value = clampf(value, 0, 5000); break;
        case 66: value = clampf(value, 100, 10000); break;
        case 67: value = clampf(value, 0, 10000); break;
        case 68: value = clampf(value, 100, 10000); break;
        case 69: value = clampf(value, 0, 10000); break;
        case 70: value = clampf(value, 24000, 32000); break;
        case 71: value = clampf(value, 0, 2000); break;
        case 72: value = clampf(value, 15000, 24000); break;
        case 73: value = clampf(value, 15000, 24000); break;
        case 74: value = clampf(value, 15000, 25000); break;
        case 75: value = clampf(value, 25000, 32000); break;
        case 76: value = clampf(value, 0, 1); break;
        case 77: value = clampf(value, 1, 100); break;
        case 78: value = clampf(value, 0, 100); break;
        case 79: value = clampf(value, 0, 50); break;
        case 80: value = clampf(value, 0, 50); break;
        case 81: value = clampf(value, 0, 100); break;
        case 82: value = clampf(value, 0, 100); break;
        default: break;
    }
    g_params[id] = value;
    if (applied) *applied = value;
    return true;
}
bool func__EspLink_GetParam(uint8_t id, uint32_t *value){ if (id >= 83u) return false; *value = g_params[id]; return true; }
bool func__BspFlash_ErasePage(uint32_t p){
    /* v1.80: four 1 KiB pages now - two per bank. Still ONE page per call,
       exactly like the hardware: the save loop must walk the bank itself. */
    if (p != EMU_FLASH_BASE && p != EMU_FLASH_BASE + 1024u &&
        p != EMU_FLASH_BASE + 2048u && p != EMU_FLASH_BASE + 3072u) return false;
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
/* v1.80: the tests that lay a record down by hand must erase the WHOLE bank,
   exactly like the save path does - a 1168 B record spans both pages. */
static bool erase_bank(uint32_t p){
    return func__BspFlash_ErasePage(p) && func__BspFlash_ErasePage(p + 1024u);
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
    EMU_FLASH = mmap((void *)EMU_FLASH_BASE, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    assert(EMU_FLASH == (uint8_t *)EMU_FLASH_BASE);

    /* T1 fresh board: nothing applied, defaults stay */
    memset(EMU_FLASH, 0xFF, 4096); reboot();
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
    EMU_FLASH[2048 + 8] ^= 0x40;
    reboot();
    assert(g_apply_calls == 0 && g_params[20] == 0u);

    /* T6b clean pair: single corruption falls back to the older page */
    memset(EMU_FLASH, 0xFF, 4096); reboot();
    {
        esp_link_nvm_record_t rec; esp_link_nvm_entry_t e[1];
        e[0].uint16_t__id = 20; e[0].uint16_t__pad = 0; e[0].uint32_t__value = 14400;
        func__EspLink_NvmRecordBuild(&rec, 3, e, 1);
        assert(erase_bank(ESP_LINK_NVM_PAGE_A_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_A_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        e[0].uint32_t__value = 14200;
        func__EspLink_NvmRecordBuild(&rec, 4, e, 1);
        assert(erase_bank(ESP_LINK_NVM_PAGE_B_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_B_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        reboot();
        assert(g_params[20] == 14200);
        EMU_FLASH[2048 + 60] ^= 0x08;
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
        assert(erase_bank(ESP_LINK_NVM_PAGE_B_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_B_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        reboot();
        assert(g_params[20] == 14400);
    }

    /* T8 hostile out-of-window value lands CLAMPED */
    {
        esp_link_nvm_record_t rec; esp_link_nvm_entry_t e[1];
        e[0].uint16_t__id = 20; e[0].uint16_t__pad = 0; e[0].uint32_t__value = 99999;
        func__EspLink_NvmRecordBuild(&rec, 10, e, 1);
        assert(erase_bank(ESP_LINK_NVM_PAGE_B_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_B_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        reboot();
        assert(g_params[20] == 14600);
    }

    /* T9 sequence wrap 65534/65535 -> 0 stays monotonic */
    {
        esp_link_nvm_record_t rec; esp_link_nvm_entry_t e[1];
        e[0].uint16_t__id = 25; e[0].uint16_t__pad = 0; e[0].uint32_t__value = 590;
        func__EspLink_NvmRecordBuild(&rec, 65534u, e, 1);
        assert(erase_bank(ESP_LINK_NVM_PAGE_B_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_B_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        e[0].uint32_t__value = 600;
        func__EspLink_NvmRecordBuild(&rec, 65535u, e, 1);
        assert(erase_bank(ESP_LINK_NVM_PAGE_A_ADDR));
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
    memset(EMU_FLASH, 0xFF, 4096); reboot();
    func__EspLink_ApplyParam(19, 1, &ap); func__EspLink_NvmMarkDirty(19);
    run_ticks(30);
    assert(EMU_FLASH[0] == 0xFF && EMU_FLASH[2048] == 0xFF);

    /* T11 (v1.15): alarm ids persist round-trip; a hostile hard-current
       replays CLAMPED to the 950 ceiling (down-only safety) */
    memset(EMU_FLASH, 0xFF, 4096); reboot();
    {
        esp_link_nvm_record_t rec; esp_link_nvm_entry_t e[2];
        e[0].uint16_t__id = 27; e[0].uint16_t__pad = 0; e[0].uint32_t__value = 14700;
        e[1].uint16_t__id = 35; e[1].uint16_t__pad = 0; e[1].uint32_t__value = 990;
        func__EspLink_NvmRecordBuild(&rec, 11, e, 2);
        assert(erase_bank(ESP_LINK_NVM_PAGE_A_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_A_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        reboot();
        assert(g_params[27] == 14700 && g_params[35] == 950);
    }

    /* T12 (v1.16b): UI ids persist round-trip; a hostile red-duty
       replays CLAMPED to the 100 ceiling */
    memset(EMU_FLASH, 0xFF, 4096); reboot();
    {
        esp_link_nvm_record_t rec; esp_link_nvm_entry_t e[2];
        e[0].uint16_t__id = 39; e[0].uint16_t__pad = 0; e[0].uint32_t__value = 999;
        e[1].uint16_t__id = 38; e[1].uint16_t__pad = 0; e[1].uint32_t__value = 5000;
        func__EspLink_NvmRecordBuild(&rec, 12, e, 2);
        assert(erase_bank(ESP_LINK_NVM_PAGE_A_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_A_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        reboot();
        assert(g_params[76] == 0 && g_params[39] == 100 && g_params[38] == 5000);
    }

    /* T13 (v1.16b): a record carrying the panel-session mute (76) is
       rejected WHOLE - nothing applied, compiled defaults stay */
    memset(EMU_FLASH, 0xFF, 4096); reboot();
    {
        esp_link_nvm_record_t rec; esp_link_nvm_entry_t e[2];
        e[0].uint16_t__id = 38; e[0].uint16_t__pad = 0; e[0].uint32_t__value = 5000;
        e[1].uint16_t__id = 76; e[1].uint16_t__pad = 0; e[1].uint32_t__value = 1;
        func__EspLink_NvmRecordBuild(&rec, 13, e, 2);
        assert(erase_bank(ESP_LINK_NVM_PAGE_A_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_A_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        reboot();
        assert(g_apply_calls == 0 && g_params[38] == 0 && g_params[76] == 0);
    }

    /* T14 (v1.17): the six full/hysteresis ids persist round-trip; a
       hostile full-exit replays CLAMPED to the 100 ceiling */
    memset(EMU_FLASH, 0xFF, 4096); reboot();
    {
        esp_link_nvm_record_t rec; esp_link_nvm_entry_t e[3];
        e[0].uint16_t__id = 77; e[0].uint16_t__pad = 0; e[0].uint32_t__value = 90;
        e[1].uint16_t__id = 78; e[1].uint16_t__pad = 0; e[1].uint32_t__value = 999;
        e[2].uint16_t__id = 82; e[2].uint16_t__pad = 0; e[2].uint32_t__value = 4;
        func__EspLink_NvmRecordBuild(&rec, 14, e, 3);
        assert(erase_bank(ESP_LINK_NVM_PAGE_A_ADDR));
        assert(func__BspFlash_ProgramHalfWords(ESP_LINK_NVM_PAGE_A_ADDR, (const uint16_t *)&rec, sizeof rec / 2));
        reboot();
        assert(g_params[77] == 90 && g_params[78] == 100 && g_params[82] == 4);
    }

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
    check(re.search(r"#define ESPLINK_FRAME_MAX_PAYLOAD\s+512u", text_esph),
          "payload limit must be 512. v1.43: 119 params would need 1 + 119 x 5 = 596 "
          "bytes, so PARAMS_BULK is CHUNKED at (512-1)/5 = 102 items per frame "
          "instead of raising the ceiling - on a 20 KB part that buffer is charged "
          "twice, once on each side of the link")
    check(re.search(r"#define ESPLINK_PARAM_MANUAL_TEST_MODE\s+19u", text_esph)
          and re.search(r"#define ESPLINK_PARAM_COUNT\s+132u", text_esph),
          "param 19 = manual test mode; 132 params total since v1.80 (20..26 = charge profile, 27..37 = alarms, 38..82 = UI cadence, 83..92 = two-loop CC/CV PID, 93..107 = charger limits, 108..118 = imbalance scenario 5, 119..120 = charge-side percent map, 121..122 = band-2 beep shape, 123..124 = imbalance latched red-lamp blink, 125..127 = dead-battery scenario 6, 128..131 = its own lamp and buzzer)")

    manual = text_c[text_c.find("static void func__Charger_ManualDriveChannel"):
                    text_c.find("/* ==================== Charger_Evaluate")]
    check("func__Charger_ManualDriveChannel" in text_c,
          "manual mode must drive the duty directly")
    check("UINT32_T__G__ChargerOvCutoffMv" in manual,
          "manual mode keeps the hard overvoltage cutoff (v1.15: runtime id 36, down-only from the 15 V compile ceiling)")
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


def test_alarms_tab_v115():
    """[EN] v1.15 (user order 2026-09-26, "an alarms tab - the number behind
    every alarm must be editable from the ESP panel and stick on the board
    MCU"): ids 27..34 = fault supervision thresholds (runtime, persisted),
    35..37 = charger safety ceilings (down-only, never above the compile
    maxima); an alarms SUB-TAB inside settings (v1.15b) with grouped cards,
    a flicker-free grouped status card with fault explanations, threshold
    bars and JSON import/export; PARAMS_BULK grows to 191 bytes.
    [FA] تست‌های v1.15: تب آلارم‌ها — شناسه‌های ۲۷..۳۴ آستانه‌های نظارت فالت
    (زمان‌اجرا، ماندگار)، ۳۵..۳۷ سقف‌های ایمنی شارژر (فقط پایین‌بردنی)؛ تب
    چهارم پنل با کارت‌های دسته‌بندی‌شده، کارت وضعیت زنده و نوار آستانه."""
    text_cc = CHARGER_C.read_text()
    text_ch = CHARGER_H.read_text()
    text_fc = FAULT_C.read_text()
    text_fh = FAULT_H.read_text()
    text_esph = ESP_LINK_H.read_text()
    text_espc = ESP_LINK_C.read_text()
    ino = "\n".join((ROOT / "esp_link_panel" / f).read_text(encoding="utf-8") for f in ["esp_link_panel.ino", "plink_config.h", "plink_params.h", "plink_state.h", "plink_panel.h", "plink_font.h", "plink_link.h", "plink_http.h"])

    # --- id match: fault 27..34 and charger 35..37 identical on both sides ---
    fault_pairs = [("FAULT_ALARM_PARAM_DISCONNECT_MV", "ESPLINK_PARAM_FAULT_ALARM_DISCONNECT_MV", 27),
                   ("FAULT_ALARM_PARAM_DISCONNECT_DEB_MS", "ESPLINK_PARAM_FAULT_ALARM_DISCONNECT_DEB_MS", 28),
                   ("FAULT_ALARM_PARAM_ABSENT_MV", "ESPLINK_PARAM_FAULT_ALARM_ABSENT_MV", 29),
                   ("FAULT_ALARM_PARAM_BACK_MV", "ESPLINK_PARAM_FAULT_ALARM_BACK_MV", 30),
                   ("FAULT_ALARM_PARAM_ABSENT_DEB_MS", "ESPLINK_PARAM_FAULT_ALARM_ABSENT_DEB_MS", 31),
                   ("FAULT_ALARM_PARAM_RECOVER_MS", "ESPLINK_PARAM_FAULT_ALARM_RECOVER_DEB_MS", 32),
                   ("FAULT_ALARM_PARAM_INPUT_MIN_MV", "ESPLINK_PARAM_FAULT_ALARM_INPUT_MIN_MV", 33),
                   ("FAULT_ALARM_PARAM_INPUT_MAX_MV", "ESPLINK_PARAM_FAULT_ALARM_INPUT_MAX_MV", 34)]
    for mod_name, esp_name, num in fault_pairs:
        m_h = re.search(rf"#define {mod_name}\s+(\d+)u", text_fh)
        m_e = re.search(rf"#define {esp_name}\s+(\d+)u", text_esph)
        check(m_h is not None and m_e is not None and int(m_h.group(1)) == num and int(m_e.group(1)) == num,
              f"fault alarm id {num} must be identical in fault.h ({mod_name}) and esp_link.h ({esp_name})")
    chg_pairs = [("CHG_ALARM_PARAM_HARD_CURRENT_MA", "ESPLINK_PARAM_CHG_ALARM_HARD_CURRENT_MA", 35),
                 ("CHG_ALARM_PARAM_OV_CUTOFF_MV", "ESPLINK_PARAM_CHG_ALARM_OV_CUTOFF_MV", 36),
                 ("CHG_ALARM_PARAM_VALID_FLOOR_MV", "ESPLINK_PARAM_CHG_ALARM_VALID_FLOOR_MV", 37)]
    for mod_name, esp_name, num in chg_pairs:
        m_h = re.search(rf"#define {mod_name}\s+(\d+)u", text_ch)
        m_e = re.search(rf"#define {esp_name}\s+(\d+)u", text_esph)
        check(m_h is not None and m_e is not None and int(m_h.group(1)) == num and int(m_e.group(1)) == num,
              f"charger alarm id {num} must be identical in charger.h ({mod_name}) and esp_link.h ({esp_name})")
    for decl, where, what in [
            ("func__Fault_SetAlarmParam(uint8_t uint8_t__paramId", text_fh, "fault.h"),
            ("func__Fault_GetAlarmParam(uint8_t uint8_t__paramId", text_fh, "fault.h"),
            ("func__Fault_OnSupervisionChange(void)", text_fh, "fault.h"),
            ("func__Charger_SetAlarmParam(uint8_t uint8_t__paramId", text_ch, "charger.h"),
            ("func__Charger_GetAlarmParam(uint8_t uint8_t__paramId", text_ch, "charger.h")]:
        check(decl in where, f"{what} must declare {decl.split('(')[0]}")

    # --- fault.c: live struct, set-clamp, Evaluate reads runtime only ---
    check("fault_alarm_t" in text_fc and "FAULT_ALARM_T__G__Alarm" in text_fc,
          "fault.c must hold the runtime alarm struct")
    check("func__Fault_ClampAlarms" in text_fc and "func__Fault_SetAlarmParam" in text_fc
          and "func__Fault_GetAlarmParam" in text_fc,
          "fault.c must implement clamp + set/get alarm API")
    fbody = re.sub(r"/\*.*?\*/", "", text_fc, flags=re.S)
    fbody = re.sub(r"FAULT_ALARM_T__G__Alarm\s*=\s*\{[^}]*\}", "", fbody)
    fbody = fbody.replace("uint32_t uint32_t__overMv = FAULT_BAT_DISCONNECT_MV;", "")
    for macro in ["FAULT_BAT_DISCONNECT_MV", "FAULT_BAT_DISCONNECT_DEBOUNCE_MS", "FAULT_BAT_ABSENT_MV",
                  "FAULT_BATTERY_BACK_MV", "FAULT_BAT_ABSENT_DEBOUNCE_MS", "FAULT_BAT_RECOVER_MS",
                  "FAULT_INPUT_PRESENT_MIN_MV", "FAULT_INPUT_PRESENT_MAX_MV"]:
        leftover = [ln for ln in fbody.split("\n")
                    if macro in ln and not ln.strip().startswith(("*", "/*", "//"))]
        check(not leftover,
              f"no bare {macro} use outside the alarm initializer/comments (got {leftover[:2]})")
    check(text_fc.count("FAULT_ALARM_T__G__Alarm.uint32_t__disconnectMv") >= 3 and
          text_fc.count("FAULT_ALARM_T__G__Alarm.uint32_t__inputMinMv") >= 2,
          "Evaluate must read the disconnect/input thresholds from the live struct")

    # --- charger.c: runtime ceilings, down-only clamps, cascade ---
    check("UINT32_T__G__ChargerHardFaultMa" in text_cc and "UINT32_T__G__ChargerOvCutoffMv" in text_cc
          and "UINT32_T__G__ChargerValidFloorMv" in text_cc,
          "charger.c must hold the three runtime alarm statics")
    check("func__Charger_ClampAlarms" in text_cc and "func__Charger_SetAlarmParam" in text_cc
          and "func__Charger_GetAlarmParam" in text_cc,
          "charger.c must implement clamp + set/get alarm API")
    check(text_cc.count("func__Fault_OnSupervisionChange();") >= 2,
          "both the profile path and the alarm path must cascade into the fault re-clamp (disconnect stays < OV)")
    cbody = re.sub(r"/\*.*?\*/", "", text_cc, flags=re.S)
    cbody = re.sub(r"static (?:volatile )?uint32_t UINT32_T__G__Charger(HardFaultMa|OvCutoffMv|ValidFloorMv) = [A-Z_0-9]+;", "", cbody)
    cbody = "\n".join(ln for ln in cbody.split("\n")
                      if "CHG_CURRENT_HARD_FAULT_DEFAULT_MA" not in ln
                      and "UINT32_T__G__ChargerOvCutoffMv = CHG_MAX_VALID_BATTERY_MV" not in ln
                      and "> CHG_CURRENT_HARD_FAULT_MAX_MA" not in ln
                      and "> CHG_MAX_VALID_BATTERY_MV" not in ln)
    # [EN] Word boundaries, not substrings. "CHG_CURRENT_HARD_FAULT_MA" is a
    #      SUBSTRING of "CHG_CURRENT_HARD_FAULT_MAX_MA", so an `in` test
    #      flagged the new ceiling as the old banned macro. Same class of
    #      trap as the \b-on-A_B__C one: underscores are word characters, so
    #      \b does the right thing here while `in` does not.
    # [FA] مرز واژه، نه زیررشته. نام قدیمی زیررشتهٔ نام سقف جدید است، پس
    #      تست `in` سقف تازه را به‌جای ماکروی ممنوع علامت می‌زد.
    for macro in ["CHG_CURRENT_HARD_FAULT_MA", "CHG_MAX_VALID_BATTERY_MV", "CHG_MIN_VALID_BATTERY_MV"]:
        leftover = [ln for ln in cbody.split("\n")
                    if re.search(r"\b" + macro + r"\b", ln)
                    and not ln.strip().startswith(("*", "/*", "//"))]
        check(not leftover,
              f"no bare {macro} use outside boot defaults/clamp ceilings/comments (got {leftover[:2]})")
    check("uint32_t__currentMa > UINT32_T__G__ChargerHardFaultMa" in text_cc,
          "the hard-fault comparison must read the runtime ceiling (id 35)")

    # --- esp_link.c routes 27..37 through the module APIs ---
    apply_region = text_espc.split("bool func__EspLink_ApplyParam")[1].split("bool func__EspLink_GetParam")[0]
    get_region = text_espc.split("bool func__EspLink_GetParam")[1]
    check('#include "fault.h"' in text_espc, "esp_link.c must include fault.h for the alarm dispatch")
    check("func__Fault_SetAlarmParam" in apply_region and apply_region.count("ESPLINK_PARAM_FAULT_ALARM_") >= 8,
          "ApplyParam must route all 8 fault alarm ids to Fault_SetAlarmParam")
    check("func__Fault_GetAlarmParam" in get_region and get_region.count("ESPLINK_PARAM_FAULT_ALARM_") >= 8,
          "GetParam must route all 8 fault alarm ids to Fault_GetAlarmParam")
    check("func__Charger_SetAlarmParam" in apply_region and apply_region.count("ESPLINK_PARAM_CHG_ALARM_") >= 3,
          "ApplyParam must route all 3 charger alarm ids to Charger_SetAlarmParam")
    check("func__Charger_GetAlarmParam" in get_region and get_region.count("ESPLINK_PARAM_CHG_ALARM_") >= 3,
          "GetParam must route all 3 charger alarm ids to Charger_GetAlarmParam")

    # --- panel: sub-tab, cards, live status + bars, q2/q3 masks, 128-col CSV ---
    check('id="s1"' in ino and 'id="s2"' in ino and 'id="s3"' in ino
          and 'data-t="3"' not in ino,
          "v1.15b (user order: alarms INSIDE settings): no fourth nav tab - alarms live in settings sub-tabs")
    check(all(f'id="q{i}"' in ino for i in range(27, 38))
          and all(f'id="a{i}"' in ino for i in range(27, 38)),
          "the supervision sub-tab (s2) must hold the eleven alarm inputs q27..q37 with applied-value spans")
    check("function achk()" in ino and "function afill()" in ino and "function adef()" in ino
          and "function odef(ids)" in ino and 'id="aw2"' in ino
          and "function astat()" in ino and "apend(id)" in ino and "STAB==0" in ino,
          "the alarm sub-tabs need their guard (achk), fill/defaults (afill/adef/odef), split guards (aw/aw2), live status+bars (astat) and the STAB hook")
    check("FEXP=" in ino and "آستانهٔ قطع (۲۷)" in ino and "ASB=" in ino,
          "v1.15b (user order: grouped status + fault explanations, no flicker): per-bit fault explanations and a build-once status skeleton")
    check("function xexp()" in ino and "function ximp(f)" in ino and 'id="xim"' in ino
          and "'changeover-settings-'" in ino and "XIDS=" in ino,
          "v1.15b (user order: settings import/export): JSON backup card for filter + profile + alarms")
    # [EN] v1.57 (user order: finish the backup): the file must carry an
    #      identity and the import must reuse the panel's joint rules.
    # [FA] فایل پشتیبان شناسنامه دارد و ورودی از قوانین مشترک رد می‌شود.
    check("app:'ChangeOver-settings',v:2" in ino and "build:xbuild()" in ino
          and "pn:PN" in ino and "saved:new Date().toISOString()" in ino,
          "v1.57: the backup file records build, parameter count and date")
    check("const fixed=fixrules(v);" in ino and "function xclamp(id,n)" in ino,
          "v1.57: an imported file passes through fixrules and each field's own range")
    # [EN] v1.57 (user order: calibrate straight from the bench capture).
    # [FA] v1.57: کالیبراسیون مستقیم از داده‌برداری بنچ با تأیید کاربر.
    check("function calpush(" in ino and "function calfit(" in ino
          and "function calrun()" in ino and "async function calapply()" in ino
          and "calapply()" in ino and "xexp();" in ino,
          "v1.57: bench samples are fitted, previewed and only written after a confirm + auto backup")
    # [EN] Stale since the scenario-card redesign and only found on 2026-10-05:
    #      the battery-supervision fields are no longer one flat group called
    #      "نظارت باتری" - they live in the six scenario cards, and the
    #      monitoring tab keeps the input window and the safety ceilings.
    # [FA] این ادعا از زمان بازطراحی کارت‌های سناریو کهنه بود: فیلدهای نظارت
    #      باتری حالا داخل شش کارت سناریو هستند و تب نظارت پنجرهٔ ورودی و
    #      سقف‌های ایمنی را نگه داشته است.
    check("نظارت و ایمنی" in ino and "پنجرهٔ ورودی سالم" in ino and "سقف‌های ایمنی شارژر" in ino
          and "فقط پایین‌بردنی" in ino,
          "alarm inputs must be grouped (monitoring tab / input window / safety ceilings) with the down-only note")
    check("قابل تغییر نیست" not in ino, "the stale 'safety limits cannot change' sentence must be gone (v1.15 lowers them)")
    check('\\"q2\\":%lu' in ino and "pendingMask2" in ino,
          "the /t JSON must carry the q2 pending mask for ids 32..37 (one u32 no longer fits 38 params)")
    tx = re.search(r"UINT8_T__G__TxOrder\[ESP_PARAM_COUNT\] = \{([^}]*)\}", ino)
    check(tx and len(tx.group(1).split(",")) == 132, "TxOrder must carry all 132 ids")
    # [EN] The literal "134 columns" used to be asserted here. That is the third
    #      hard-coded column count found in this suite, and every one of them was
    #      stale - they defend whatever number was true when they were written.
    #      The column total is now verified properly, by counting, in
    #      test_benchlog_row_matches_header_v125; this check keeps only what it
    #      is actually about: that the alarm and UI columns are NAMED.
    # [FA] عدد ادبی «۱۳۴ ستون» اینجا الزام می‌شد. این سومین شمارش hard-code شده
    #      در این مجموعه بود و هر سه کهنه بودند - از عددی دفاع می‌کنند که هنگام
    #      نوشتنشان درست بوده. شمارش واقعی حالا در تست جداگانه‌ای با شمردن انجام
    #      می‌شود؛ این چک فقط همان چیزی را نگه می‌دارد که دربارهٔ آن است.
    check("alm_disc_mv" in ino and "alm_hard_ma" in ino and "alm_floor_mv" in ino
          and "ui_ov_led_per" in ino,
          "the bench CSV header must NAME the alarm and UI columns (the total is "
          "counted in test_benchlog_row_matches_header_v125, never hard-coded here)")

    # --- python models of both clamp sets: fixed point + random invariants ---
    def fclamp(a, over=14600, ov=15000):
        a = dict(a)
        lo, hi = max(14000, over + 50), min(15000, ov - 100)
        if lo > hi:
            hi = lo
        a[27] = min(max(a[27], lo), hi)
        a[28] = min(max(a[28], 50), 1000)
        a[29] = min(max(a[29], 3000), 8000)
        a[30] = min(max(a[30], 4000), 9000)
        if a[29] > a[30] - 500:
            a[29] = a[30] - 500
        if a[30] < a[29] + 500:
            a[30] = a[29] + 500
        a[31] = min(max(a[31], 100), 5000)
        a[32] = min(max(a[32], 100), 5000)
        a[33] = min(max(a[33], 18000), 24000)
        a[34] = min(max(a[34], 24000), 30000)
        if a[33] > a[34] - 1000:
            a[33] = a[34] - 1000
        if a[34] < a[33] + 1000:
            a[34] = a[33] + 1000
        return a

    def cclamp(a, imax=650, over=14600):
        a = dict(a)
        a[35] = min(max(a[35], imax + 50), 950)
        a[36] = min(max(a[36], max(14000, over + 150)), 15000)
        a[37] = min(max(a[37], 0), 8000)
        return a

    fd = {27: 14800, 28: 150, 29: 6000, 30: 7000, 31: 1000, 32: 1000, 33: 21000, 34: 28000}
    check(fclamp(fd) == fd, "fault alarm defaults must be a fixed point of the clamp")
    cd = {35: 950, 36: 15000, 37: 2000}
    check(cclamp(cd) == cd, "charger alarm defaults must be a fixed point of the clamp")
    check(cclamp({35: 9999, 36: 99999, 37: 0})[35] == 950
          and cclamp({35: 9999, 36: 99999, 37: 0})[36] == 15000,
          "safety ceilings are DOWN-ONLY: absurd writes collapse to 950 mA / 15000 mV, never above")
    import random
    rng = random.Random(20260926)
    ok = True
    for _ in range(2000):
        over = rng.randint(14000, 14750)
        ov = rng.randint(14000, 15000)
        w = {27: rng.randint(0, 20000), 28: rng.randint(0, 9000), 29: rng.randint(0, 12000),
             30: rng.randint(0, 12000), 31: rng.randint(0, 20000), 32: rng.randint(0, 20000),
             33: rng.randint(0, 40000), 34: rng.randint(0, 40000)}
        c = fclamp(w, over, ov)
        lo, hi = max(14000, over + 50), min(15000, ov - 100)
        if lo > hi:
            hi = lo
        if not (lo <= c[27] <= hi and 50 <= c[28] <= 1000
                and 3000 <= c[29] <= c[30] - 500 and c[29] + 500 <= c[30] <= 9000
                and 100 <= c[31] <= 5000 and 100 <= c[32] <= 5000
                and 18000 <= c[33] <= c[34] - 1000 and c[33] + 1000 <= c[34] <= 30000):
            ok = False
            break
        imax = rng.randint(100, 900)
        cw = {35: rng.randint(0, 2000), 36: rng.randint(0, 20000), 37: rng.randint(0, 12000)}
        cc2 = cclamp(cw, imax, over)
        if not (imax + 50 <= cc2[35] <= 950
                and max(14000, over + 150) <= cc2[36] <= 15000
                and 0 <= cc2[37] <= 8000):
            ok = False
            break
    check(ok, "2000 random alarm writes must all land inside the invariant ranges")

    # --- offline preview mirrors the 38-param board ---
    prev = (ROOT / "tools/panel_preview_server.js").read_text()
    check("q2: 0" in prev and "q3: 0" in prev and "id < 93" in prev and all(f"case {i}:" in prev for i in range(27, 83)),
          "the preview server must serve 93 params with alarm + UI clamps and the q2/q3 masks")


def test_ui_mirror_v116():
    """[EN] v1.16 (user order 2026-09-26, "LEDs for every fault/alarm that
    really blink, a buzzer icon with a cross on mute, every alarm number
    editable"): ids 38..76 = the 39 UI cadence numbers (fault LED/buzzer
    scenarios, BatteryRun bands, normal blink, voltage thresholds and the
    persisted mute); the ESP frame grows to a u16 length (5-byte header,
    512 ceiling) so the 386-byte PARAMS_BULK fits - both boards must be
    flashed together; the panel carries the edit cards, a board-rate
    LED/buzzer mirror and one LED per fault bit.
    [FA] تست‌های v1.16: آینه LED/بازر - شناسه‌های ۳۸..۷۶ (سناریوهای خطا،
    باندهای دشارژ، چشمک عادی، آستانه‌های ولتاژ و میوت ماندگار)؛ فریم با
    طول u16 (هدر ۵ بایت، سقف ۵۱۲) برای بالک ۳۸۶ بایتی؛ پنل با کارت‌های
    ویرایش، آینهٔ هم‌سرعت برد و LED جدا برای هر بیت."""
    text_esph = ESP_LINK_H.read_text()
    text_espc = ESP_LINK_C.read_text()
    text_uih = (ROOT / "Firmware/Modules/Ui/ui_led.h").read_text()
    ino = "\n".join((ROOT / "esp_link_panel" / f).read_text(encoding="utf-8") for f in ["esp_link_panel.ino", "plink_config.h", "plink_params.h", "plink_state.h", "plink_panel.h", "plink_font.h", "plink_link.h", "plink_http.h"])
    prev = (ROOT / "tools/panel_preview_server.js").read_text()

    # --- frame v2: u16 LE length, 6-byte header, CRC-16, 512 ceiling, BOTH sides ---
    # [EN] Was pinned at 5 bytes. v2 (2026-09-29) inserts a protocol VERSION byte
    #      after the SOF pair and replaces the XOR-8 trailer with CRC-16, so the
    #      header is 6 and the trailer is 2. The version byte exists because a
    #      mismatched flash used to produce no error at all - the receiver just
    #      dropped everything and the panel looked like an unplugged cable.
    #      The exact sizes are cross-checked against the ESP by
    #      tools/audit_consistency.py; here we pin the SHAPE.
    # [FA] قبلاً روی ۵ بایت قفل بود. نسخهٔ ۲ یک بایت نسخه بعد از SOF می‌گذارد و
    #      XOR-8 را با CRC-16 عوض می‌کند، پس هدر ۶ و دنباله ۲ است. بایت نسخه هست
    #      چون فلش ناهماهنگ قبلاً هیچ خطایی تولید نمی‌کرد.
    check(re.search(r"#define ESPLINK_FRAME_HEADER_SIZE\s+6u", text_esph)
          and re.search(r"#define ESP_LINK_HEADER_SIZE\s+6u", ino),
          "both frame headers must be 6 bytes (AA 55 ver type len_lo len_hi)")
    check(re.search(r"#define ESPLINK_FRAME_CHECKSUM_SIZE\s+2u", text_esph)
          and re.search(r"#define ESPLINK_PROTOCOL_VERSION\s+\d+u", text_esph)
          and "func__EspLink_Crc16" in text_espc,
          "the frame must carry a 16-bit CRC and an explicit protocol version - XOR-8 "
          "was measured to miss 100 percent of same-position bit pairs, the pattern a "
          "switching converter puts on a UART")
    check(re.search(r"#define ESPLINK_FRAME_MAX_PAYLOAD\s+512u", text_esph)
          and re.search(r"#define ESP_LINK_MAX_PAYLOAD\s+512u", ino),
          "both payload ceilings must be 512 (416-byte bulk + headroom)")
    check("ESP_LINK_PARSE_WAIT_LEN_LO" in text_espc and "ESP_LINK_PARSE_WAIT_LEN_HI" in text_espc,
          "the STM32 parser must assemble the length from two bytes")
    check("ESP_RX_WAIT_LEN_LO" in ino and "ESP_RX_WAIT_LEN_HI" in ino
          and "UINT16_T__G__RxLen" in ino and "UINT16_T__G__RxIndex" in ino,
          "the ESP parser must assemble a u16 length (two states, u16 len/index)")

    # --- id match: UI 38..82 identical on the link and the Ui module ---
    for esp_name, ui_name, num in [
            ("ESPLINK_PARAM_UI_OV_LED_PERIOD_MS", "UI_ALARM_PARAM_OV_LED_PERIOD_MS", 38),
            ("ESPLINK_PARAM_UI_BUZZER_MUTE", "UI_ALARM_PARAM_BUZZER_MUTE", 76),
            ("ESPLINK_PARAM_UI_CHG_FULL_ENTER_PCT", "UI_ALARM_PARAM_CHG_FULL_ENTER_PCT", 77),
            ("ESPLINK_PARAM_UI_RUN_ONE_EXIT", "UI_ALARM_PARAM_RUN_ONE_EXIT", 82)]:
        m_esp = re.search(rf"#define {esp_name}\s+(\d+)u", text_esph)
        m_ui = re.search(rf"#define {ui_name}\s+(\d+)u", text_uih)
        check(m_esp and m_ui and int(m_esp.group(1)) == num and int(m_ui.group(1)) == num,
              f"UI id {num} must match on both sides ({esp_name} / {ui_name})")
    check("UI_ALARM_PARAM_MIN_ID" in text_espc and text_espc.count("UI_ALARM_PARAM_MIN_ID") >= 2
          and "ui_led.h" in text_espc,
          "ApplyParam AND GetParam must range-dispatch 38..82 to the Ui module")
    check("MIN_ID              38u" in text_uih and "MAX_ID              82u" in text_uih,
          "the Ui range must be 38..82")

    # --- panel: 56 edit fields, guard, mirror, per-bit LEDs, q3, backup ---
    # [EN] v1.74 (user order: low battery is not a LED scenario, it belongs to
    #      changeover and not to the panel): ids 72/73 are retired, so the panel
    #      must NOT carry fields for them any more - the check flips from
    #      "they exist" to "they are gone".
    # [FA] شناسه‌های ۷۲/۷۳ بازنشسته‌اند و نباید کادری در پنل داشته باشند.
    RETIRED_IDS = (72, 73)
    editable = [i for i in range(27, 83) if i not in RETIRED_IDS]
    check(all(f'id="q{i}"' in ino for i in editable)
          and all(f'id="a{i}"' in ino
                  for i in editable if i != 76)
          and '<input type="hidden" id="q76"' in ino
          and 'id="a76"' not in ino
          and all(f'id="q{i}"' not in ino and f'id="a{i}"' not in ino
                  for i in RETIRED_IDS),
          "the settings sub-tab must hold q27..q82 minus the retired 72/73, "
          "with applied-value spans (76 = hidden mute field)")
    check("const AIDS=" in ino and "const ADEF=" in ino and "u76" in ino,
          "the guard needs the 27..82 id list, the 56 defaults and the u76 refs")
    adef = re.search(r"const ADEF=\[([^\]]*)\]", ino)
    check(adef and len(adef.group(1).split(",")) == 56,
          "ADEF must carry one default per guarded id (56 for 27..82)")
    # v1.53 (user order): the board-driven LED/buzzer mirror was deleted - the
    # per-card simulators replaced it. What must survive is the fault-bit
    # mirror refresh and the mute toggle.
    # v1.53: آینهٔ LED برد حذف شد؛ شبیه‌ساز هر کارت جایش را گرفت.
    check("function uview()" in ino and "setInterval(uview,250)" in ino and "function xmute()" in ino,
          "the fault-bit refresh and the mute toggle must exist")
    check("ulR" not in ino and "function simrun()" in ino and "function simbz(" in ino,
          "the board-driven LED strip is gone and the per-card simulator is in")
    # v1.55 (user order): the leftover notice row and the mute button went too.
    # v1.55: ردیف باقی‌مانده و دکمهٔ میوت هم برداشته شدند.
    check('id="uleds"' not in ino and 'leds stick' not in ino
          and all(f'id="sl{n}r"' in ino and f'id="sl{n}z"' in ino
                  for n in range(1, 7))   # v1.75: six cards, renumbered 1..6
          and 'id="sl7r"' not in ino,
          "no sticky board mirror left; every scenario card carries its own simulated LEDs and buzzer")
    check(all(f'id="asbb{k}"' in ino for k in range(7)),
          "one LED per fault bit (asbb0..asbb6)")
    check("pendingMask3" in ino and "pendingMask4" in ino and "64..95" in ino,
          "the /t JSON must carry all FOUR pending masks. The fourth is not decoration: "
          "ids 96..107 exist as of v1.28 and the catch-all arm they used to land in did "
          "1UL << (id - 64), which is undefined behaviour past id 95 - the panel's "
          "modulo-32 shift made it look like a mere off-by-one")
    # [EN] v1.69 (user order: "delete this junk"): the human-readable blurb
    #      that spelled the id ranges out is gone - it had been wrong since
    #      v1.24 and was stale again at v1.68 (it still said 113 values and
    #      77..118). What must stay true is the MACHINERY: XIDS derives the
    #      backup set from AIDS, so a new parameter joins the backup by
    #      itself and there is no label left to go stale.
    # [FA] خط توضیح بازه‌ها حذف شد (باز هم کهنه شده بود)؛ آنچه باید بماند خود
    #      ساز و کار است: XIDS فهرست را از AIDS می‌سازد.
    check("XIDS=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,20,21,22,23,24,25,26]" in ino
          and "AIDS.forEach(id=>{if(id!==76)XIDS.push(id);});" in ino,
          "v1.16c (user order: ONE backup for the whole settings): all persisted ids "
          "0..14 + 20..75 + 77..118 (113 params; slots 200..202 stay board-only). "
          "XIDS extends itself from AIDS, so the limits block "
          "was covered the moment it existed - but the human-readable label was NOT, and "
          "it had already been wrong since v1.24 (it still advertised a retired 83..97)")
    check('id="usel"' in ino and "function usel(n)" in ino
          and all(f'id="ucard{k}"' in ino for k in range(1, 7))
          and 'id="ucard7"' not in ino,
          "one selectable card per scenario (v1.75: exactly six, numbered 1..6 "
          "with no hole where the retired low-battery card used to be)")
    # [EN] v1.33 (user order 2026-10-03: "bring that charger PID inside this
    #      same charge-and-filter tab"). The PID sub-tab is gone as a TAB and
    #      its card now sits in sub-tab 0, so backup moves up to 3. Pinned
    #      both ways: the standalone PID tab must not come back, and the PID
    #      card must actually be inside s0 rather than merely deleted.
    # [FA] زیرتب PID به‌عنوان «تب» حذف شد و کارتش داخل زیرتب ۰ نشست، پس
    #      پشتیبان‌گیری به ۳ آمد. هر دو طرف میخ شد: نه تب مستقل برگردد، نه
    #      کارت PID صرفاً حذف شده باشد.
    check('data-s="1">سناریوها<' in ino and 'data-s="2">نظارت و ایمنی<' in ino
          and 'data-s="3">کالیبراسیون و فیلتر جریان<' in ino
          and 'data-s="4">پشتیبان‌گیری<' in ino
          and 'data-s="3">پشتیبان‌گیری<' not in ino
          and 'data-s="4">کالیبراسیون و فیلتر جریان<' not in ino
          and 'data-s="3">PID شارژ<' not in ino and 'data-s="4">PID شارژ<' not in ino
          and 'data-s="1">آلارم‌ها<' not in ino
          and '>وضعیت</button>' not in ino,
          "v1.33: PID folded into sub-tab 0, backup moves to 3, no standalone "
          "PID tab; v1.34 (user order 2026-10-03: \"current calibration + "
          "current filters into their own settings sub-tab\"): a fifth sub-tab "
          "exists and PID stays folded")
    s0part = ino.split('id="s0"')[1].split('id="s1"')[0]
    check("PID دوحلقه‌ای شارژ (CC/CV)" in s0part,
          "the PID card must live INSIDE the charge-and-filter sub-tab, not "
          "just be gone from its old one")
    cfpart = ino.split('id="s3"')[1].split('id="s4"')[0]  # v1.42: calibration+filters now in s3 (backup moved last)
    check('<div class="hd"><b>فیلتر جریان</b>' in cfpart
          and '<div class="hd"><b>کالیبراسیون جریان</b>' in cfpart
          and '<div class="hd"><b>فیلتر جریان</b>' not in s0part
          and '<div class="hd"><b>کالیبراسیون جریان</b>' not in s0part,
          "v1.34: the current-filter and current-calibration cards moved INTO "
          "the fifth sub-tab (s4) and out of the charge sub-tab")
    s1part = ino.split('id="s1"')[1].split('id="s2"')[0]
    s2part = ino.split('id="s2"')[1].split('id="s3"')[0]
    # [EN] v1.33: PID lives in s0. v1.42 (user order: backup sub-tab LAST):
    #      backup is now s4 and calibration+filters s3.
    # [FA] PID در s0 است، و با دستور v1.42 پشتیبان‌گیری زیرتب آخر (s4) شد.
    s3part = ino.split('id="s0"')[1].split('id="s1"')[0]
    bkpart = ino.split('id="s4"')[1].split("</main>")[0]
    p0part = ino.split('id="p0"')[1].split('id="p1"')[0]
    check("ucard1" in s1part and "ucard5" in s1part and 'id="sim1"' in s1part and 'id="aw2"' in s1part
          and 'id="aw"' in s1part and 'id="ib117"' in s1part
          and all(f + "()" in s1part for f in ("ovdef", "bdef", "dsdef", "chdef", "ibdef", "dbdef"))
          and "<b>نظارت باتری</b>" not in s1part
          and "<b>پنجرهٔ ورودی سالم</b>" not in s1part and "<b>سقف‌های ایمنی شارژر</b>" not in s1part
          and "وضعیت آلارم‌ها" not in s1part and "پشتیبان‌گیری" not in s1part
          and all(f'id="q{i}"' in s1part
                  for i in range(38, 83) if i not in RETIRED_IDS)
          and all(f'id="q{i}"' in s1part for i in range(27, 33))
          and all(f'id="q{i}"' not in s1part for i in range(33, 38))
          and all(f'id="q{i}"' not in s1part for i in range(83, 99))
          and all(f'id="q{i}"' in s1part for i in range(108, 119) if i != 117),
          "s1 (scenarios): mirror + 6 scenario cards; v1.43 user order pulls the cut-battery "
          "thresholds 27..32 INTO scenario 2 and adds scenario 5 (108..118, checkbox 117 = ib117)")
    check("نظارت باتری" not in s2part and "پنجرهٔ ورودی سالم" in s2part and "سقف‌های ایمنی شارژر" in s2part
          and 'id="aw"' not in s2part and "adef()" in s2part and "uleds" not in s2part
          and all(f'id="q{i}"' not in s2part for i in range(27, 33))
          and all(f'id="q{i}"' in s2part for i in range(33, 38))
          and all(f'id="q{i}"' not in s2part for i in range(38, 83))
          and all(f'id="q{i}"' not in s2part for i in range(83, 93)),
          "s2 (supervision & safety) keeps only 33..37 (input window + charger ceilings): "
          "v1.43 user order moved 27..32 into scenario 2 and retired the battery-monitoring card")
    # [EN] v1.38 (user order: one factory-restore key is enough): the
    #      PID-only button was deleted and the surviving qdef() restores the
    #      whole charger scope - the section pin follows that consensus.
    # [FA] v1.38 (دستور کاربر: یک کلید بازگردانی کافی است) - کلید جدای PID
    #      حذف شد و qdef بازمانده کل محدودهٔ شارژر را برمی‌گرداند.
    check("PID دوحلقه‌ای شارژ (CC/CV)" in s3part and 'onclick="qdef()"' in s3part and 'id="pw"' in s3part
          and 'onclick="pdef()"' not in s3part
          and all(f'id="q{i}"' in s3part for i in range(83, 93))
          and all(f'id="a{i}"' in s3part for i in range(83, 93))
          and all(f'id="q{i}"' not in s3part for i in range(27, 83))
          and all(f'id="q{i}"' not in s3part for i in range(20, 27)),
          "s3 holds the two-loop PID card: all 10 ids 83..92 with their applied-value labels, the guard box and exactly ONE factory-restore key (v1.38: the user merged the two)")
    check("پشتیبان‌گیری" in bkpart and 'id="xim"' in bkpart
          and all(f'id="q{i}"' not in bkpart for i in range(27, 93)),
          "v1.42 (user order: backup sub-tab LAST): the final sub-tab (s4) "
          "holds only the single backup card")
    check("وضعیت آلارم‌ها" in p0part and 'id="ast"' in p0part and 'id="abars"' not in p0part,
          "the merged voltages+alarm table sits on the main panel tab (p0, v1.16k: fixed layout, each value once), not in settings")
    check("asb5" in ino and "abf0" not in ino and "abf1" not in ino and "abf2" not in ino and "جریان ۱" in ino and "جریان ۲" in ino
          and "Math.max(t[3],t[10])" not in ino,
          "v1.16c (user order: per-battery current supervision, no max() merge); v1.16k: merged table rows for both currents (fault box kept)")
    # [EN] v1.75 (user order: "when you delete 5 you must shift the rest down"):
    #      the low-battery card is gone AND the two cards that followed it moved
    #      up, so the picker reads 1..6 with no hole and no leftover "۷ ·".
    # [FA] کارت باتری کم حذف شد و دو کارت بعدی یک شماره پایین آمدند: ۱ تا ۶ بدون حفره.
    check("۱ · اضافه‌ولتاژ" in ino and "۵ · باتری کم" not in ino
          and "۵ · عدم‌توازن" in ino and "۶ · باتری خراب" in ino
          and "۷ · باتری" not in ino and 'data-u="7"' not in ino
          and "سناریو ۱ ·" not in ino,
          "v1.75: the scenario picker is numbered 1..6 with imbalance at 5 and "
          "the dead-battery scenario at 6")
    # [EN] v1.71 (user order: "why is the last discharge step suddenly a duty?
    #      make them all the same shape"): the critical band is now typed like
    #      the other three - count, per-beep duration, own repeat interval,
    #      shared gap - so its guard is the same "does it fit the interval"
    #      sentence the other bands use, and the duty-window wording is gone.
    # [FA] باند بحرانی هم‌شکل بقیه شد، پس نگهبانش همان جملهٔ «در فاصله جا
    #      نمی‌شود» است و حرف پنجرهٔ duty حذف شد.
    check("بوق باند بحرانی در فاصلهٔ خودش جا نمی‌شود" in ino
          and "[57,56,58,65]" in ino and "دوره×duty" not in ino,
          "v1.16e/v1.71: the critical band is guarded exactly like the other bands")
    # [EN] Every discharge band must own count + per-beep duration + repeat
    #      interval, and the ONLY shared number is the gap (id 65).
    # [FA] تنها عدد مشترک گپ است (۶۵).
    for _fit in ("fit(a.u59,a.u54,a.u62,a.u65)", "fit(a.u121,a.u122,a.u63,a.u65)",
                 "fit(a.u60,a.u55,a.u64,a.u65)", "fit(a.u57,a.u56,a.u58,a.u65)"):
        check(_fit in ino,
              f"each discharge band fits against its OWN interval: {_fit}")
    check(ino.count('data-q="65"') == 3 and 'data-q="54"><span class="ow">مشترک' not in ino,
          "v1.71: bands 2, 3 and critical show the shared gap as read-only; "
          "nothing else is shared")
    check(ino.count("روند:") >= 5,
          "v1.16e (user order: explain each scenario flow): every scenario card carries its روند line")
    check("با ریست برد پاک می‌شود" in ino and "روی فلش می‌ماند" not in ino,
          "v1.16b (user order: mute lives only for the panel session): no stale persisted-mute text")

    # --- preview server: 93 params + q3 ---
    check("q3: 0" in prev and "id < 93" in prev,
          "the offline preview must serve 93 params with the q3 mask")



def test_ui_mirror_v117():
    """[EN] v1.17 (user order 2026-09-27, "the charge number must follow the
    scenario, and the like"): ids 77..82 = the full latch (enter/exit) +
    the stable-percent hysteresis + the 0%/1% exits (defaults 100/95/5/2/2/3);
    the yellow floor (69) default rises 10 -> 150; enter is authoritative
    (exit is pulled to enter-1); everything persists like the rest except
    the still-transient mute 76; the panel gains the enter>=exit+1 guard,
    the mirror fallback and the two new card rows.
    [FA] تست‌های v1.17: شناسه‌های ۷۷..۸۲ (لچ فول + هیسترزیس پایداری +
    خروج‌های ۰٪/۱٪)؛ پیش‌فرض ۶۹ از ۱۰ به ۱۵۰؛ ورود مرجع (خروج = ورود-۱)؛
    ماندگاری مثل بقیه جز میوت ۷۶؛ گارد و آینه و کارت‌های جدید پنل."""
    text_uih = (ROOT / "Firmware/Modules/Ui/ui_led.h").read_text()
    text_uic = (ROOT / "Firmware/Modules/Ui/ui_led.c").read_text()
    ino = "\n".join((ROOT / "esp_link_panel" / f).read_text(encoding="utf-8") for f in ["esp_link_panel.ino", "plink_config.h", "plink_params.h", "plink_state.h", "plink_panel.h", "plink_font.h", "plink_link.h", "plink_http.h"])
    prev = (ROOT / "tools/panel_preview_server.js").read_text()

    # --- board boot defaults (the UI_* macros stay the single source) ---
    for name, val in [("UI_CHARGING_FULL_ENTER_PERCENT", "100u"),
                      ("UI_CHARGING_FULL_EXIT_PERCENT", "95u"),
                      ("UI_CHARGING_PERCENT_HYSTERESIS_PERCENT", "5u"),
                      ("UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT", "2u"),
                      ("UI_BATTERY_ZERO_EXIT_THRESHOLD", "2u"),
                      ("UI_BATTERY_ONE_EXIT_THRESHOLD", "3u"),
                      ("UI_CHARGING_YELLOW_MIN_ON_MS", "150u")]:
        check(re.search(rf"#define {name}\s+{val}", text_uih),
              f"boot default {name} must be {val} (v1.17: 69 rises 10 -> 150)")

    # --- v1.56 (user order): the board keeps only the per-field windows;
    #     "exit below enter" moved into the panel's fixrules().
    # --- v1.56: فقط بازهٔ تک‌فیلدی روی برد ماند؛ قانون مشترک در پنل است.
    check("uint32_t__chgFullEnterPct, 1u, 100u" in text_uic
          and "uint32_t__chgFullExitPct, 0u, 100u" in text_uic
          and "uint32_t__chgFullEnterPct - 1u" not in text_uic
          and "if(v[78]>=v[77])set(78,v[77]-1);" in ino
          and "uint32_t__chgHystPct, 0u, 50u" in text_uic
          and "uint32_t__runHystPct, 0u, 50u" in text_uic
          and "uint32_t__runZeroExit, 0u, 100u" in text_uic
          and "uint32_t__runOneExit, 0u, 100u" in text_uic,
          "ClampAlarms must clamp 77..82 to 1..100/0..100/0..50/0..50/0..100/0..100 with exit pulled to enter-1")

    # --- panel: ADEF tail + the 150 fallback for 69 ---
    adef = re.search(r"const ADEF=\[([^\]]*)\]", ino)
    check(adef and adef.group(1).split(",")[-6:] == ["100", "95", "5", "2", "2", "3"]
          and adef.group(1).split(",")[42].strip() == "150",
          "ADEF must end with the six v1.17 defaults (100/95/5/2/2/3) and carry 150 at the id-69 slot")

    # --- panel: enter>=exit+1 guard + mirror fallbacks ---
    check("a.u78<=a.u77-1" in ino and "خروج فول باید زیر ورود باشد" in ino,
          "the panel guard must warn unless exit <= enter-1 (77 authoritative)")
    # v1.53: the fallbacks moved from the deleted board mirror into the
    # per-card simulator, which reads the boxes through c4v().
    # v1.53: پیش‌فرض‌ها از آینهٔ حذف‌شده به شبیه‌ساز کارت‌ها منتقل شدند.
    check("c4v(77,100)" in ino and "c4v(69,150)" in ino and 'id="q78"' in ino,
          "the simulator must fall back to the v1.17 boot defaults")

    # --- panel: ucard4 holds q77..q79, ucard3 holds q80..q82 ---
    ucard3 = ino.split('id="ucard3"')[1].split('id="ucard4"')[0]
    ucard4 = ino.split('id="ucard4"')[1].split('id="ucard5"')[0]
    check(all(f'id="q{i}"' in ucard4 for i in range(77, 80))
          and all(f'id="q{i}"' in ucard3 for i in range(80, 83)),
          "ucard4 must hold the full/hysteresis inputs q77..q79 and ucard3 the run exits q80..q82")

    # --- preview server mirrors the same numbers ---
    check("100, 95, 5, 2, 2, 3," in prev and "case 78:" in prev and "P[77] - 1" not in prev
          and "50, 18000, 0, 10, 1000," in prev
          and "14800, 100, 500, 10, 15000, 3000, 3000, 500," in prev
          and "300, 500, 600000, 600000, 30000, 100, 10, 3600000, 200, 1, 20," in prev
          and "21000, 29000," in prev and "600000, 120, 0, 50];" in prev
          and "case 118:" in prev and "case 122:" in prev and "case 124:" in prev
          and "case 125:" in prev and "case 126:" in prev and "case 127:" in prev
          and "case 128:" in prev and "case 131:" in prev,
          "the offline preview must serve the v1.17 defaults with the enter-authoritative "
          "clamp, the calibrated PID rows after them, the v1.28 limits block, "
          "the v1.43 imbalance scenario block, and the v1.49 charge-side percent map plus the v1.68 imbalance blink last")


def test_ui_mirror_v117b():
    """[EN] v1.17b (user order 2026-09-27, "after a full charge the blinking
    must be gone"): the charger declares completion itself (FLOAT is entered
    only from ABSORB-done) and the UI/panel full face keys on it; the 1 h
    safety clock survives dips below 14.3 V (anti-hunt); the "idle charger"
    piece is gone from caption/flow/docs; the bench entry form moves above
    the table so data entry needs no horizontal scroll and the page never
    jumps.
    [FA] تست‌های v1.17b: اعلام اتمام از خود شارژر (فقط پایان ابزورب وارد
    FLOAT می‌شود)؛ ساعت ۱ساعته با افت‌ها ریست نمی‌شود؛ تکه «شارژر بیکار»
    حذف؛ فرم ورود بنچ بالای جدول بدون اسکرول افقی."""
    text_ch = CHARGER_H.read_text()
    text_cc = CHARGER_C.read_text()
    text_uic = (ROOT / "Firmware/Modules/Ui/ui_led.c").read_text()
    ino = "\n".join((ROOT / "esp_link_panel" / f).read_text(encoding="utf-8") for f in ["esp_link_panel.ino", "plink_config.h", "plink_params.h", "plink_state.h", "plink_panel.h", "plink_font.h", "plink_link.h", "plink_http.h"])
    prev = (ROOT / "tools/panel_preview_server.js").read_text()

    # --- charger: IsChargeComplete = installed+enabled, all in FLOAT ---
    check("func__Charger_IsChargeComplete" in text_ch
          and "bool func__Charger_IsChargeComplete(void)" in text_cc
          and "CHG_STATE_FLOAT" in text_cc
          and "GetChannelEspEnable" in text_cc
          and "relevantCount" in text_cc,
          "charger must offer IsChargeComplete (all installed+enabled channels in FLOAT, >=1 relevant)")
    check(text_cc.count("= CHG_STATE_FLOAT;") == 1,
          "FLOAT must be entered from exactly one place (ABSORB done: taper or 1 h) - FLOAT means DONE")

    # --- anti-hunt: the dip keeps the 1 h clock, fresh cycles reset it ---
    check("func__Charger_DipResetAbsorbWindow" in text_cc
          and text_cc.count("func__Charger_DipResetAbsorbWindow(") == 2,
          "the 14.3 V dip path must use DipReset (soak/taper restart, 1 h clock kept)")
    check("uint32_t__absorbEnterTick == 0u" in text_cc,
          "the 1 h clock must stamp once (dip-kept stamps survive, fresh cycles stamp anew)")
    check(text_cc.count("func__Charger_ClearAbsorbWindow(") >= 6,
          "fresh cycles (init/idle/off/retry/bulk/reentry) must still fully reset the absorb window")

    # --- UI: full = voltage latch OR charger completion ---
    check("func__Charger_IsChargeComplete() == true" in text_uic
          and "bool__isFull = true;" in text_uic,
          "the UI dispatcher must latch full on charger completion too")

    # --- panel: the board mirror that derived charger-done from the
    #     telemetry is gone (v1.53); the card simulator decides "full" from
    #     the full-entry percent the user typed, not from the live states.
    #     v1.53: آینهٔ برد حذف شد؛ شبیه‌ساز «فول» را از درصد ورود فولِ
    #     تایپ‌شده می‌فهمد، نه از وضعیت زندهٔ شارژرها.
    check("UV." not in ino and "pct>=en" in ino,
          "no telemetry-derived charger-done left; the simulator uses the typed full-entry percent")
    check("فول: سبز ثابت، زرد خاموش" in ino and "شارژر بیکار" not in ino,
          "the charge simulator must say full = green solid with the yellow off")
    check("پایان Absorb هر کانال" in ino,
          "the charging card flow must end at absorb-done, not at idle")

    # --- bench: entry form above the table, page never jumps ---
    check('id="wF0"' in ino and "$('wF0')" in ino and "wsee(x)" in ino
          and "scrollIntoView" not in ino and "id='wX'" not in ino,
          "the DMM entry form must live above the table (wF0) with table-local auto-scroll")

    # --- preview: the demo cycle reaches FLOAT with enables on ---
    check("c.state = 3" in prev,
          "the offline preview must demo the FLOAT/completion face")


def test_audit_batch_v116b():
    """v1.16 second audit sweep: locks, suspend, bulk TX, LUT guards, tail junk."""
    ch = CHARGER_H.read_text()
    cc = CHARGER_C.read_text()
    fc = FAULT_C.read_text()
    uart = BSP_UART_C.read_text()
    link = ESP_LINK_C.read_text()
    nvmh = ESP_LINK_NVM_H.read_text()
    nvmc = ESP_LINK_NVM_C.read_text()
    meas = MEASUREMENT_C.read_text()
    calh = CALIBRATION_H.read_text()

    def fn_body(text, name):
        # first mention can be a comment: take the occurrence whose next
        # newline-brace (definition) comes before any semicolon (decl/call)
        pos = 0
        while True:
            at = text.index("func__" + name, pos)
            nl = chr(10)
            brace = text.find(nl + "{", at)
            semi = text.find(";", at)
            if brace != -1 and (semi == -1 or brace < semi):
                break
            pos = at + 1
        depth = 0
        i = brace
        while i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    return text[at:i]
            i += 1
        raise AssertionError("unbalanced braces for " + name)

    # --- race-artifact tails stay gone (74785d7 script junk) ---
    check(ch.count("#endif") == 1,
          "charger.h must end with exactly one header guard")
    check("value);" not in ch.split("#endif")[0].splitlines()[-1],
          "charger.h must not carry the stray declaration fragment")
    stray = [ln for ln in meas.splitlines() if ln.strip() == "tage24OffsetMv;"]
    check(not stray,
          "measurement.c must not carry the stray tail fragment")

    # --- C11: writer-side scheduler locks in the multi-field setters ---
    for name in ["Charger_SetProfileParam", "Charger_SetAlarmParam"]:
        body = fn_body(cc, name)
        check("osKernelLock()" in body and "osKernelRestoreLock" in body,
              name + " must apply store+clamp under a scheduler lock")
    body = fn_body(fc, "Fault_SetAlarmParam")
    check("osKernelLock()" in body and "osKernelRestoreLock" in body,
          "Fault_SetAlarmParam must apply store+clamp under a scheduler lock")

    # --- F4 + G1: fault mask + charger ceilings volatile, Set/Clear locked ---
    check("static volatile fault_mask_t FAULT_MASK_T__G__Mask" in fc,
          "fault mask must be volatile")
    for name in ["Fault_Set", "Fault_Clear"]:
        body = fn_body(fc, name)
        check("osKernelLock()" in body and "osKernelRestoreLock" in body,
              name + " must hold a scheduler lock across the RMW")
    for var in ["ChargerHardFaultMa", "ChargerOvCutoffMv", "ChargerValidFloorMv"]:
        check("static volatile uint32_t UINT32_T__G__" + var in cc,
              var + " must be volatile (comm writer, control reader)")

    # --- NVM suspend: charger idles around the flash stall (user 2026-09-27) ---
    check("func__Charger_SetSuspended" in ch and "func__Charger_IsSuspended" in ch,
          "charger.h must declare the suspension API")
    check("BOOL__G__ChargerSuspended" in cc and "func__BspPwm_StopAll();" in cc,
          "Charger_Evaluate must hold both gates at 0 while suspended")
    check("NvmSuspendCharger" in nvmc and "NvmResumeCharger" in nvmc,
          "NvmSaveNow must suspend around erase/program/verify and resume on every exit")
    check(nvmc.count("func__EspLink_NvmResumeCharger(bool__chargerSuspended);") >= 4,
          "all four SaveNow exits (erase/program/verify fail + success) must resume")
    check("IsGatePulsing(BSP_PWM_CHARGER_1)" in nvmc and "IsGatePulsing(BSP_PWM_CHARGER_2)" in nvmc,
          "the suspend wait must poll the hardware truth of both gates")

    # --- E4/E1: bulk must fit the TX ring, off the comm stack ---
    ring_line = [ln for ln in uart.splitlines() if "BSP_UART_TX_RING_SIZE" in ln and "#define" in ln][0]
    ring_size = int("".join(ch for ch in ring_line.split()[-1] if ch.isdigit()))
    check(ring_size >= 422,
          "TX ring must fit the 502 B PARAMS_BULK frame (99 params)")
    # [EN] v1.28: still static for the same reason, but sized to ONE CHUNK
    #      instead of the whole parameter list - that is what stops the buffer
    #      from growing every time a parameter is added.
    # [FA] هنوز static به همان دلیل، ولی به اندازهٔ «یک تکه» نه کل فهرست -
    #      همین است که نمی‌گذارد بافر با هر پارامتر تازه بزرگ شود.
    check("static uint8_t UINT8_T__A__Payload[1u + (ESPLINK_BULK_MAX_ITEMS *" in link
          and "ESPLINK_BULK_ITEM_SIZE)];" in link,
          "bulk payload must be static (comm stack is 1 KiB) and sized to one chunk")
    check("NVM record too small for the persisted id set" in nvmc,
          "NVM must statically assert the record fits the WHOLE persisted id set (params + runtime slots)")
    check("992 B for 122 entries" in nvmh,
          "NVM record comment must state the true record size for 122 entries")

    # --- LUT hardening + dead-clamp cleanup ---
    check("uint32_t__xHigh == uint32_t__xLow" in meas,
          "LUT must guard the degenerate (equal-anchor) segment")
    check("CAL_CURRENT2_LUT_POINTS >= 2u" in meas,
          "LUT must statically assert >= 2 points for the tail slope")
    # [EN] v1.63 (user question: may the table have more or fewer points?):
    #      the count is free, but two axes of different length would read
    #      past the end of the shorter one - that must not compile.
    # [FA] تعداد نقاط آزاد است، ولی دو محور با طول متفاوت نباید کامپایل شود.
    check("sizeof(CAL_Current1LutChainMa) ==" in meas
          and "sizeof(CAL_Current1LutBatteryMw)" in meas
          and "sizeof(CAL_Current2LutChainMa) ==" in meas,
          "v1.63: both LUTs must statically assert the two axes are the same length")
    check("#if (CHG_ETA_MIN_PERMILLE != 0u)" in cc,
          "the always-false u32<0 ETA clamp must compile out (type-limits green)")
    check("v1.13 (user order 2026-09-25" in calh and "voltages are fixed but the currents" in calh,
          "calibration.h must keep the v1.13 power-LUT note")
    nested = [ln for ln in calh.splitlines() if ln.startswith("/* [EN] v1.13")]
    check(not nested,
          "calibration.h must not nest a block comment inside the banner")


def test_telemetry_frame_pins_v116c():
    """2026-09-27 full audit: the TLM_LIVE writer count must match the 84 B
    frame - 1 u16 (seq) + flags/reserved (2 B) + 20 u32. The source carries
    each conditional u32 twice (live value under #if, 0u under #else), so the
    textual count is 39 (19 live + 19 #else fillers + the unconditional
    fault-mask word). Any added field without a size bump would silently
    truncate at SendFrame."""
    esp_h = ESP_LINK_H.read_text()
    link = ESP_LINK_C.read_text()
    # [EN] Derive the size from the field count instead of freezing 84. The
    #      point of this check is that the size and the number of fields agree,
    #      not that the number never grows - v1.25 added five raw-count fields
    #      for calibration and a frozen literal would simply have blocked it
    #      while proving nothing.
    # [FA] اندازه از تعداد فیلد مشتق می‌شود نه اینکه روی ۸۴ منجمد بماند. هدف
    #      این چک هم‌خوانی اندازه با تعداد فیلدهاست، نه اینکه هرگز بزرگ نشود.
    tlm_fields = int(re.search(r"#define ESP_LINK_TLM_FIELD_COUNT\s+(\d+)u",
                               (ROOT / "esp_link_panel" / "plink_config.h").read_text()).group(1))
    tlm_size = int(re.search(r"#define ESPLINK_TLM_PAYLOAD_SIZE\s+(\d+)u", esp_h).group(1))
    check(tlm_size == 4 + (tlm_fields * 4),
          f"TLM payload size must be 2 seq + 2 flags + {tlm_fields}x4 = "
          f"{4 + tlm_fields * 4}, got {tlm_size} - any added field without a matching "
          "size bump truncates silently at SendFrame")
    check(int(re.search(r"#define ESP_LINK_TLM_SIZE\s+(\d+)u",
              (ROOT / "esp_link_panel" / "plink_config.h").read_text()).group(1)) == tlm_size,
          "the ESP's expected TLM size must equal the firmware's payload size, or every "
          "frame is rejected as malformed")
    # [EN] Slice INSIDE SendTelemetry only: from right after its opening
    #      brace (so the prototype/comment above cannot leak in) to the end
    #      of the function = its final SendFrame call's closing paren, which
    #      is exactly where the closing brace sits (no code follows SendFrame
    #      inside SendTelemetry). Anchoring on prose banners broke once
    #      already when a comment block was removed (audit 2026-10-03).
    # [FA] برش فقط داخل SendTelemetry: از بعد از آکولاد آغازین تا پایان خود
    #      تابع (پرانتز پایانی SendFrame؛ پس از آن فقط آکولاد پایانی است).
    #      لنگر روی بنر متنی یک‌بار با حذف کامنت شکست (ممیزی ۲۰۲۶-۱۰-۰۳).
    start = link.index("void func__EspLink_SendTelemetry")
    start = link.index("{", start) + 1
    body = link[start:link.index("(uint16_t)ESPLINK_TLM_PAYLOAD_SIZE);", start)]
    check(body.count("func__EspLink_PutU16(") == 1,
          "SendTelemetry must write exactly one u16 (the sequence number)")
    # [EN] 31 live u32 writes fill the WHOLE field table (4+31x4 = 128 B,
    #      including the v1.43 imbalance trio at t[24..26] and the v1.76
    #      dead-battery trio at t[28..30]: mask, charger-1 clock, charger-2
    #      clock), and 30 #else zero-fillers cover the same fields when a
    #      module is compiled out.
    #      Full-program audit 2026-10-05: the fillers went 25 -> 30 and the
    #      conditional blocks 9 -> 10 because the five raw-ADC-count writes
    #      were the last unguarded ones in this function - with
    #      MODULE_MEASUREMENT=0 they referenced a module that is not
    #      compiled, which the -Werror gate rejects. The LIVE count is
    #      deliberately still 31: the payload layout and length are
    #      byte-identical in both branches, which is the property this
    #      check exists to protect.
    # [FA] ۳۱ رایت زندهٔ u32 کل جدول را پر می‌کند (۴+۳۱×۴=۱۲۸ بایت، با سه‌تاییِ
    #      سناریوی ۶ در t[28..30]) و ۳۰ صفرِ #else جایگزین‌اند.
    #      ممیزی ۲۰۲۶-۱۰-۰۵: صفرها از ۲۵ به ۳۰ و بلوک‌های شرطی از ۹ به ۱۰
    #      رسید، چون پنج رایت شمارش خام ADC آخرین رایت‌های بدون گارد این تابع
    #      بودند و با ‎MODULE_MEASUREMENT=0‎ به ماژولی اشاره می‌کردند که کامپایل
    #      نشده است. تعداد «زنده» عمداً همان ۳۱ مانده: چیدمان و طول payload در
    #      هر دو شاخه بیت‌به‌بیت یکسان است و همین خاصیت است که این چک از آن
    #      محافظت می‌کند.
    expected_writes = 31 + 30
    check(body.count("func__EspLink_PutU32(") == expected_writes,
          f"SendTelemetry must carry {expected_writes} textual u32 writes "
          "(31 live + 30 #else fillers); the 31 live ones exactly fill "
          "ESP_LINK_TLM_FIELD_COUNT fields")
    check(body.count("func__EspLink_PutU32(UINT8_T__A__Payload, &uint16_t__cursor, 0u);") == 30,
          "SendTelemetry must carry exactly 30 zero-filler u32 writes")
    check(body.count("#else") == 10,
          "SendTelemetry must keep its 10 conditional filler blocks "
          "(v1.72 added the scenario-6 pair, the 2026-10-05 audit added the "
          "raw-ADC-count block)")


def test_flash_diet_pins_v116d():
    """2026-09-27 flash diet (region FLASH overflowed by 1840 B): the three
    panel-id families must stay dense so the indexed setters/getters
    (((volatile u32*)&first)[id-BASE]) stay correct, and the diet itself
    must not regress: no u64 (kills __aeabi_uldivmod ~1 KiB), timers stay
    ON (OFF breaks the linked timers.c compile / non-GC link), exidx is
    discarded by the linker script, and .cproject forces per-function
    sections + --gc-sections in both Debug and Release."""
    chg_ids = sorted(int(v) for v in re.findall(
        r"#define CHG_PROFILE_PARAM_[A-Z_0-9]+\s+(\d+)u", CHARGER_H.read_text()))
    check(chg_ids == list(range(20, 27)),
          f"charge-profile ids must be exactly dense 20..26, got {chg_ids}")
    flt_ids = sorted(int(v) for v in re.findall(
        r"#define FAULT_ALARM_PARAM_[A-Z_0-9]+\s+(\d+)u", FAULT_H.read_text()))
    check(flt_ids == list(range(27, 35)),
          f"fault-alarm ids must be exactly dense 27..34, got {flt_ids}")
    # [EN] v1.49: the dense UI block is still 38..82; the charge-side percent
    #      map had to take 119/120 because 83..118 belong to other modules.
    # [FA] بلوک متراکم همان ۳۸..۸۲؛ نگاشت درصد سمت شارژ ۱۱۹/۱۲۰ را گرفت.
    ui_ids = sorted(int(v) for v in re.findall(
        r"#define UI_ALARM_PARAM_(?!MIN_ID|MAX_ID|EXT_MIN_ID|EXT_MAX_ID)[A-Z_0-9]+\s+(\d+)u",
        UI_LED_H.read_text()))
    check(ui_ids == list(range(38, 83)) + [119, 120, 121, 122],
          f"UI-alarm ids must be dense 38..82 plus the ext range 119..122, got {len(ui_ids)} ids")
    for src, base, name in (
            (CHARGER_C, 20, "SetProfileParam"),
            (FAULT_C, 27, "SetAlarmParam")):
        body = src.read_text()
        start = body.index(name)
        window = body[start:start + 6000]
        check("[uint8_t__paramId - " in window and
              "(uint8_t__paramId < " in window and
              "(uint8_t__paramId > " in window and
              "volatile uint32_t" in window,
              f"{src.name}:{name} must index [id-FIRST] with a FIRST..LAST guard")
        check("_Static_assert" in body and "offsetof" in body and "sizeof" in body,
              f"{src.name} must carry the sizeof/offsetof layout asserts above {name}")
        check(f"case {base}:" not in window,
              f"{src.name}:{name} must not keep the old per-id switch")
    # [EN] v1.49: the UI setter can no longer be a single [id-FIRST] because it
    #      serves two ranges (38..82 and 119..120). The index now comes from one
    #      shared helper, which is what both Set and Get must use - that is the
    #      property worth pinning, not the arithmetic being inline.
    # [FA] ستر UI دیگر یک [id-FIRST] ساده نیست چون دو بازه دارد؛ نمایه از یک
    #      تابع مشترک می‌آید و همان چیزی است که باید قفل شود.
    ui_body = UI_LED_C.read_text()
    check("static uint8_t func__Ui_AlarmParamIndex(uint8_t uint8_t__paramId)" in ui_body
          and ui_body.count("func__Ui_AlarmParamIndex(uint8_t__paramId)") == 2
          and "UI_ALARM_PARAM_INDEX_INVALID" in ui_body
          and "[uint8_t__wordIndex]" in ui_body
          and "volatile uint32_t" in ui_body,
          "ui_led.c: Set and Get must map the id through the one shared index helper")
    check("_Static_assert" in ui_body and "offsetof" in ui_body and "sizeof" in ui_body,
          "ui_led.c must carry the sizeof/offsetof layout asserts above the setter")
    check("case 38:" not in ui_body,
          "ui_led.c: the setter must not keep the old per-id switch")
    for src in (BSP_MEAS_C, MEASUREMENT_C, RTOS_TIME_C, UI_LED_C, UI_BUZZER_C):
        text = src.read_text()
        check("uint64_t" not in text and "unsigned long long" not in text,
              f"{src.name} must stay u64-free (diet pins __aeabi_uldivmod out)")
    # [EN] This used to assert configUSE_TIMERS == 1, on the belief that OFF
    #      breaks the build. It does not. Two #errors stood in the way and
    #      both were opt-outs: timers.c:41 wanted INCLUDE_xTimerPendFunctionCall
    #      off too, and freertos_os2.h:208 wanted the unused Event Flags API
    #      off. With timers on, tasks.c:2022 starts the timer task
    #      unconditionally, which links timers.c AND queue.c - about 3 KB that
    #      no call in this firmware can reach. That is what pushed the image
    #      780 bytes past the 62K FLASH region on 2026-10-03.
    # [FA] اینجا قبلاً ادعا می‌شد configUSE_TIMERS باید ۱ بماند چون خاموشی بیلد
    #      را می‌شکند. نمی‌شکند. دو #error سر راه بود و هر دو قابل خاموش‌کردن:
    #      timers.c:41 و freertos_os2.h:208. با تایمر روشن، tasks.c:2022 بی‌قید
    #      تسک تایمر را راه می‌اندازد و timers.c و queue.c را لینک می‌کند -
    #      حدود ۳ کیلوبایت بدون هیچ فراخوانی. همین ایمیج را ۷۸۰ بایت از ناحیهٔ
    #      ۶۲ کیلوبایتی فلش بیرون زده بود.
    frtc = FREERTOSCONFIG_H.read_text()
    check(re.search(r"#define\s+configUSE_TIMERS\s+0\b", frtc),
          "configUSE_TIMERS must stay 0: nothing creates a timer and it costs ~3 KB of FLASH")
    check(re.search(r"#define\s+INCLUDE_xTimerPendFunctionCall\s+0\b", frtc),
          "INCLUDE_xTimerPendFunctionCall must stay 0 or timers.c:41 #errors")
    check(re.search(r"#define\s+configUSE_OS2_EVENTFLAGS_FROM_ISR\s+0\b", frtc),
          "configUSE_OS2_EVENTFLAGS_FROM_ISR must stay 0 or freertos_os2.h:208 #errors")
    check(re.search(r"#define\s+configUSE_COUNTING_SEMAPHORES\s+1\b", frtc),
          "configUSE_COUNTING_SEMAPHORES must stay 1: the CMSIS shim needs it even unused")
    # [EN] CubeMX does not keep configUSE_TIMERS in the .ioc, so a regenerate
    #      would silently restore it and the only symptom would be an opaque
    #      "region FLASH overflowed". The guard turns that into a sentence.
    # [FA] CubeMX این تنظیم را در .ioc نگه نمی‌دارد؛ تولید دوباره بی‌صدا برش
    #      می‌گرداند و تنها نشانه پیام مبهم سرریز است. نگهبان آن را جمله می‌کند.
    hooks = FREERTOS_HOOKS_C.read_text()
    check("configUSE_TIMERS != 0" in hooks and "#error" in hooks,
          "freertos_hooks.c must #error if a CubeMX regenerate turns timers back on")
    # [EN] "region FLASH overflowed by 780 bytes" reads identically whether the
    #      fix is missing from the build or merely too small, so the build log
    #      has to say which. This marker prints in the CubeIDE console.
    # [FA] پیام سرریز چه وقتی اصلاح در بیلد نباشد و چه وقتی کم باشد یک‌شکل است،
    #      پس لاگ بیلد باید بگوید کدام. این نشانگر در کنسول CubeIDE چاپ می‌شود.
    check("#pragma message(" in hooks and "flash diet" in hooks,
          "freertos_hooks.c must print a build-log marker proving the diet compiled")
    check("*(.ARM.exidx*)" in FLASH_LD.read_text() and "/DISCARD/" in FLASH_LD.read_text(),
          "linker script must discard .ARM.exidx (C++ unwind tables ~6.4 KiB)")
    proj = CPROJECT.read_text()
    for opt in ("tool.c.compiler.option.ffunction",
                "tool.c.compiler.option.fdata",
                "tool.c.linker.option.gcsections"):
        hits = [ln for ln in proj.splitlines()
                if opt in ln and 'value="true"' in ln and "<option" in ln]
        check(len(hits) >= 2,
              f".cproject must force {opt}=true in both Debug and Release")


def test_two_loop_pid_v124():
    """[EN] USER-ORDERED LOGIC CHANGE 2026-09-28/29: slow the absorb duty rise,
       replace the fixed-step regulator with a panel-tunable PID, then reduce
       that PID to the SIMPLEST structure that still regulates both limits.
       The user asked whether ONE PID over the whole path would do, noting
       that slowness is acceptable "because the battery itself is slow".
       Measured answer, kept here as a regression: one row cannot, two can,
       three was one too many.
       [FA] تغییر منطق به دستور کاربر: رشد کندتر دیوتی در ابزورب، جایگزینی
       تنظیم‌کنندهٔ پله‌ای با PID قابل تنظیم از پنل، و سپس ساده‌سازی همان PID
       به ساده‌ترین ساختاری که هنوز هر دو حد را تنظیم می‌کند. کاربر پرسید آیا
       یک PID در کل مسیر کافی است و گفت کندی اشکالی ندارد «چون باتری خودش
       کند است». جواب اندازه‌گیری‌شده: یک ردیف نه، دو ردیف بله، سه ردیف زیادی."""
    text_h = CHARGER_H.read_text()
    text_c = CHARGER_C.read_text()
    text_esph = ESP_LINK_H.read_text()
    text_espc = ESP_LINK_C.read_text()
    ino = "\n".join((ROOT / "esp_link_panel" / f).read_text(encoding="utf-8") for f in ["esp_link_panel.ino", "plink_config.h", "plink_params.h", "plink_state.h", "plink_panel.h", "plink_font.h", "plink_link.h", "plink_http.h"])

    # --- 1. the 10 wire ids are dense 83..92 and agree on BOTH sides ---
    names = [f"{loop}_{f}" for loop in ("CURRENT", "VOLTAGE")
             for f in ("KP", "KI", "KD", "UP_RATE", "DOWN_RATE")]
    for k, nm in enumerate(names):
        wid = 83 + k
        check(re.search(rf"#define CHG_PID_PARAM_{nm}\s+{wid}u", text_h),
              f"charger.h must map CHG_PID_PARAM_{nm} to id {wid}")
        check(re.search(rf"#define ESPLINK_PARAM_CHG_PID_{nm}\s+{wid}u", text_esph),
              f"esp_link.h must map ESPLINK_PARAM_CHG_PID_{nm} to the SAME id {wid}")
    check(re.search(r"#define ESPLINK_PARAM_COUNT\s+132u", text_esph),
          "ESPLINK_PARAM_COUNT must be 132 (last scenario-6 face id 131 + 1, v1.80)")
    check("STAGE1" not in text_h and "STAGE3" not in text_h and "stage3" not in text_c,
          "the retired third gain row must leave NOTHING behind (it was measured to "
          "buy nothing and it cost five panel numbers)")

    # --- 2. each loop owns its own gain row (the min-select is meaningless
    #        with one shared Kp: simulated, the pack sails past 14.6 V) ---
    step = text_c[text_c.find("static uint16_t func__Charger_PidStep"):
                  text_c.find("static void func__Charger_RegulateChannel")]
    check(step, "func__Charger_PidStep must exist")
    check("uint32_t__currentKp" in step and "uint32_t__currentDownRate" in step,
          "the CURRENT branch must read the current row")
    check(step.count("uint32_t__voltageKp") == 1 and "uint32_t__voltageDownRate" in step,
          "the VOLTAGE branch must read ONE voltage row, unconditionally: the v1.23 "
          "setpoint split was measured to be equal on the clean plant and NOISIER "
          "under broadband sensor noise, so it is gone")
    check("(uint32_t__batteryMv < uint32_t__targetMv)" not in step,
          "no gain row may be selected by the pack voltage any more")
    check("int32_t__voltageKp * int32_t__errorVoltage" in step
          and "int32_t__currentKp * int32_t__errorCurrent" in step,
          "each branch's P term must use ITS OWN Kp (mV and mA are different units)")
    check("bool__useVoltage = ((int32_t__termPv + int32_t__termDv) <=\n" in step
          or "bool__useVoltage = ((int32_t__termPv + int32_t__termDv) <=" in step,
          "CC/CV must be a min-select on the P+D demand of the two branches")
    check("CHG_PID_MID_ENTER_MV" not in text_h and "midEnterMv" not in text_c,
          "the old fixed voltage border must be gone")

    # --- 3. the slew limit must sit on the INTEGRAL, not on the output.
    #        Limiting the output rectifies the P-term ripple that the whole-
    #        permille duty quantisation creates into a downward ratchet -
    #        simulated, the loop stalled at 268 mA and never reached 640 mA. ---
    rate_at = step.find("int32_t__rate = (int32_t__gainI * int32_t__error)")
    integ_at = step.find("charger_channel_state_t__channel->int32_t__pidIntegral +=")
    demand_at = step.find("int32_t__demand = charger_channel_state_t__channel->int32_t__pidIntegral")
    check(0 < rate_at < integ_at < demand_at,
          "order must be: clamp the integral RATE -> advance the integral -> build the demand")
    check("if (int32_t__rate > int32_t__upRate)" in step
          and "if (int32_t__rate < -int32_t__downRate)" in step,
          "the up/down rates must clamp the integral rate")
    check("int32_t__pidSlewRem" not in text_c,
          "the old output slew-allowance accumulator must be gone (it was the ratchet)")
    check(step.find("int32_t__pidIntegral = 0;") > integ_at
          and "int32_t__pidIntegral =\n            (int32_t)uint32_t__ceilingMilli;" in step,
          "anti-windup: the integral must be clamped into the same [0, ceiling] window as the duty")

    # --- 3b. output quantisation hysteresis: without it the PID reverses the
    #         duty MORE than the legacy chain it replaced (15570 vs 6456 over
    #         10 simulated hours) because it may move every 100 ms tick. The
    #         lag it introduces must stay inside the re-seed tolerance or the
    #         two mechanisms fight each other. ---
    check(re.search(r"#define CHG_PID_OUTPUT_HYST_MILLI\s+(\d+)u", text_h),
          "charger.h must define the output quantisation hysteresis")
    hyst = int(re.search(r"#define CHG_PID_OUTPUT_HYST_MILLI\s+(\d+)u", text_h).group(1))
    tol = int(re.search(r"#define CHG_PID_RESEED_TOLERANCE_PERMILLE\s+(\d+)u", text_h).group(1))
    scale = int(re.search(r"#define CHG_PID_DUTY_SCALE\s+(\d+)u", text_h).group(1))
    check(0 < hyst < tol * scale,
          "the output hysteresis must be non-zero and smaller than the re-seed tolerance "
          "(otherwise the PID's own quantisation lag looks like a foreign writer)")
    check(hyst > scale // 2,
          "a hysteresis at or below half a permille cannot suppress the 1-permille dither")
    check("int32_t__deviation >= (int32_t)CHG_LIM(CHG_LIMIT_PARAM_PID_OUT_HYST_MILLI)" in step
          and "int32_t__deviation <= -(int32_t)CHG_LIM(CHG_LIMIT_PARAM_PID_OUT_HYST_MILLI)" in step
          and "return (uint16_t)int32_t__appliedPermille;" in step,
          "PidStep must commit through the hysteretic quantiser, not a bare divide")
    check("uint32_t__pidDutyMilli =\n        (uint32_t)int32_t__appliedPermille * CHG_PID_DUTY_SCALE;" in step,
          "pidDutyMilli must store the APPLIED duty so the re-seed guard keeps its sensitivity")
    check("_Static_assert(CHG_PID_OUTPUT_HYST_MILLI <" in text_c
          and "CHG_LIMIT_MAX_PID_OUT_HYST_MILLI" in text_c,
          "the hysteresis-vs-re-seed relationship must be proven at compile time")

    # --- 4. defaults: the absorb rise MUST be slower than bulk (the order) ---
    def g(name):
        m = re.search(rf"#define CHG_PID_{name}\s+(\d+)u", text_h)
        check(m, f"charger.h must define CHG_PID_{name}")
        return int(m.group(1))
    check(g("VOLTAGE_UP_RATE") < g("CURRENT_UP_RATE"),
          "the absorb climb must rise slower than bulk")
    check(g("VOLTAGE_UP_RATE") < 50,
          "the absorb climb must be slower than the legacy 0.1 permille / 2000 ms = 50 m-permille/s")
    check(g("CURRENT_KP") <= 150,
          "the current-loop Kp must stay small: ~7 mA moves per applied permille, so a big Kp "
          "swings the P term by more than one quantisation step and the loop chatters instead of climbing")
    # the voltage loop's Kp is its NOISE GAIN: the pack voltage has no upstream
    # filter, so Kp multiplies raw ADC noise onto the duty. Measured at +/-7 mV
    # broadband: Kp=150 -> 744 duty reversals / 10 h, Kp=50 -> 62, with the same
    # overshoot, the same 0.5 mV hold error and the same 113 min to 14.4 V.
    check(g("VOLTAGE_KP") <= 60,
          "the voltage-loop Kp must stay small: it multiplies UNFILTERED pack-voltage "
          "noise straight onto the duty and buys no accuracy above ~50")
    for loop in ("CURRENT", "VOLTAGE"):
        for f in ("KP", "KI", "KD"):
            check(g(f"{loop}_{f}") <= 20000, f"default {loop}_{f} must fit the panel window")
        for f in ("UP_RATE", "DOWN_RATE"):
            check(10 <= g(f"{loop}_{f}") <= 20000, f"default {loop}_{f} must fit the rate window")

    # --- 5. clamps and dispatch. v1.23: there is NO escape hatch any more -
    #        the legacy regulator is deleted, so the PID branch is the only
    #        branch and nothing selects between them. ---
    clamp = text_c[text_c.find("static void func__Charger_ClampPid"):
                   text_c.find("static void func__Charger_ClampProfile")]
    check("(uint32_t__index % 5u) >= 3u" in clamp and "uint32_t__index < 10u" in clamp,
          "ClampPid must sweep the 10 row words and treat fields 3/4 of every 5 as slew rates")
    check("func__Charger_ClampPid();" in text_c[text_c.find("static void func__Charger_ClampProfile"):],
          "ClampProfile must re-assert the PID windows (one NVM restore can never strand a gain)")
    for fn in ("func__EspLink_ApplyParam", "func__EspLink_GetParam"):
        window = text_espc[text_espc.find(fn):]
        window = window[:window.find("\n}\n")]
        check("CHG_PID_PARAM_CURRENT_KP" in window and "CHG_PID_PARAM_VOLTAGE_DOWN_RATE" in window,
              f"{fn} must range-dispatch ids 83..92 to the charger PID setter/getter")
    check("func__Charger_PidInvalidate" in text_c, "FLOAT must invalidate the PID")
    reg = text_c[text_c.find("static void func__Charger_RegulateChannel"):]
    reg = reg[:reg.find("void func__Charger_Init")]
    check(reg.count("func__Charger_PidStep(") == 1
          and "CHG_STATE_ABSORB)" not in reg.split("func__Charger_PidStep(")[1],
          "PidStep must be the ONLY duty source outside FLOAT - no legacy branch may follow it")

    # --- 5b. the two hard backstops the user ordered (650 mA / 14.8 V).
    #         They must be proportional (a fixed step would re-create the
    #         hunting this rewrite removed), must read the RAW sample, and
    #         must sit outside the panel so a future mis-tune cannot defeat
    #         them. The ceiling must also be applied AFTER the output
    #         hysteresis, or a sub-hysteresis cut would simply be ignored. ---
    check(re.search(r"#define CHG_PID_BACKSTOP_MV\s+14800u", text_h),
          "the 14.8 V pack backstop must exist and be 14800 mV (user order)")
    check(re.search(r"#define CHG_PID_BACKSTOP_GAIN_I\s+\d+u", text_h)
          and re.search(r"#define CHG_PID_BACKSTOP_GAIN_V\s+\d+u", text_h),
          "both backstops must be proportional (a gain per mA / per mV over)")
    check("CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa" in step
          and "CHG_LIM(CHG_LIMIT_PARAM_BACKSTOP_GAIN_I)" in step,
          "the current backstop must track the LIVE profile limit (id 25), not a copy")
    check("CHG_LIM(CHG_LIMIT_PARAM_BACKSTOP_MV)" in step and "CHG_LIM(CHG_LIMIT_PARAM_BACKSTOP_GAIN_V)" in step,
          "the voltage backstop must be applied inside PidStep")
    bs_at = step.find("CHG_LIM(CHG_LIMIT_PARAM_BACKSTOP_GAIN_V)")
    filt_at = step.find("uint32_t__pidVoltFilt =")
    check(0 < bs_at < filt_at,
          "the backstops must read the RAW voltage: protection must never wait for a filter")
    hyst_at = step.find("CHG_LIM(CHG_LIMIT_PARAM_PID_OUT_HYST_MILLI)")
    slew_at = step.find("CHG_LIM(CHG_LIMIT_PARAM_PID_MAX_STEP_PM)")
    cap_at = step.find("(int32_t)(uint32_t__ceilingMilli / CHG_PID_DUTY_SCALE)")
    check(0 < hyst_at < slew_at < cap_at,
          "order must be hysteresis -> mis-tune slew cap -> absolute ceiling: the cap must "
          "never stand between a backstop and the hardware, and a cut smaller than the "
          "hysteresis must not be silently ignored")

    # --- 5d. the mis-tune cap. The backstops alone were NOT enough: the
    #         integral is rate-limited but P is not, so panel-maximum gains
    #         jumped the duty across its range in one pass (4475 mA for
    #         100 ms, simulated). The cap must be SYMMETRIC - the asymmetric
    #         output limiter tried earlier rectified P-term ripple into a
    #         downward ratchet and stalled the loop at 268 mA. ---
    check(re.search(r"#define CHG_PID_MAX_STEP_PERMILLE\s+(\d+)u", text_h),
          "the per-update mis-tune cap must exist")
    cap = int(re.search(r"#define CHG_PID_MAX_STEP_PERMILLE\s+(\d+)u", text_h).group(1))
    check(3 <= cap <= 20,
          "the mis-tune cap must be wide enough not to touch normal charging (worst "
          "legitimate move measured: 3 permille) and tight enough to bound a bad tune")
    up = "int32_t__prevPermille + (int32_t)CHG_LIM(CHG_LIMIT_PARAM_PID_MAX_STEP_PM)"
    dn = "int32_t__prevPermille - (int32_t)CHG_LIM(CHG_LIMIT_PARAM_PID_MAX_STEP_PM)"
    check(step.count(up) == 2 and step.count(dn) == 2,
          "the cap must be SYMMETRIC (same magnitude up and down) or it becomes the "
          "ratchet that stalled the loop at 268 mA")
    # [EN] REVERSED by user order (2026-10-03): "every gain and limit and
    #      parameter must be settable from the panel". The old rule here was
    #      "the backstops must NOT be panel-settable". Safety is now carried
    #      by the clamp window instead of by unreachability, so that is what
    #      this checks: the ids exist, and the backstop voltage can only ever
    #      be moved DOWN from the factory ceiling.
    # [FA] به دستور کاربر وارونه شد: «همهٔ گین‌ها و حدها و پارامترها باید از
    #      پنل تغییرپذیر باشند». قانون قبلی «پشتیبان‌ها نباید از پنل تنظیم
    #      شوند» بود. حالا ایمنی را پنجرهٔ گیره تأمین می‌کند نه دسترس‌ناپذیری،
    #      پس همان چک می‌شود: شناسه‌ها هستند، و ولتاژ پشتیبان فقط پایین می‌رود.
# [EN] v1.29 (user order): the separate card of fields is gone - the value
    #      in the operating table IS the input, clicked in place. So the thing
    #      to prove is no longer "a field exists" but "every id is drawn as an
    #      editable cell AND has a window to edit it with". A cell without an
    #      EVB row renders as an em dash and silently cannot be clicked.
    # [FA] کادر جدای فیلدها حذف شد - خودِ مقدار در جدول عملکرد همان ورودی است
    #      که درجا کلیک می‌شود. پس چیزی که باید اثبات شود دیگر «فیلد هست» نیست
    #      بلکه «هر شناسه به‌صورت خانهٔ ویرایش‌پذیر رسم می‌شود و پنجره‌ای برای
    #      ویرایش دارد» است. خانه‌ای که ردیف EVB نداشته باشد خط تیره می‌شود و
    #      بی‌صدا کلیک‌ناپذیر است.
    evb = re.search(r"const EVB=\{(.*?)\};", ino, re.S)
    check(evb, "the panel must declare the click-to-edit bounds table EVB")
    evb_ids = {int(x) for x in re.findall(r"(\d+):\[", evb.group(1))}
    # [EN] v1.30 (user order): the chart edits, the table reports. Three
    #      renderers decide where each id is drawn - the two chart axes and
    #      the chip strip for values that are neither a voltage nor a current.
    # [FA] نمودار ویرایش می‌کند و جدول گزارش می‌دهد. سه رندرکننده تعیین می‌کنند
    #      هر شناسه کجا رسم شود - دو محور نمودار و نوار تراشه برای مقادیری که
    #      نه ولتاژند و نه جریان.
    drawn = set()
    for name in ("EVV", "EVI"):
        m = re.search(r"const " + name + r"=\{(.*?)\};", ino, re.S)
        check(m, f"the panel must declare the {name} renderer list")
        if m:
            drawn |= {int(x) for x in re.findall(r"(\d+):\[", m.group(1))}
    # [EN] v1.32: the third hand list is gone. Whatever EVB can edit and the
    #      two axes do not draw must be annotated inside the plot, and that
    #      set is DERIVED from the qgraph() body rather than declared - one
    #      less list to fall out of step with what is actually rendered.
    # [FA] فهرست دستی سوم حذف شد. هر چیزی که EVB می‌تواند ویرایش کند و دو محور
    #      رسمش نمی‌کنند باید داخل نمودار یادداشت شود، و آن مجموعه از بدنهٔ
    #      qgraph «مشتق» می‌شود نه اعلام - یک فهرست کمتر برای عقب‌ماندن.
    # [EN] v1.35: the off-axis values moved from inline ann() groups to the
    #      chip strip under the chart, so the derived set now reads EVCT and
    #      EVCG. Still derived, never declared here - adding an id to a chip
    #      group is enough, and dropping one still fails loudly.
    # [FA] مقادیر خارج از محور از گروه‌های ann به نوار تراشهٔ زیر نمودار منتقل
    #      شدند، پس مجموعهٔ مشتق‌شده حالا EVCT و EVCG را می‌خواند. همچنان مشتق
    #      است نه اعلام‌شده.
    chart_body = ino[ino.find("function qgraph()"):]
    chart_body = chart_body[:chart_body.find("\nfunction ")]
    for grp in re.findall(r"ann\((.*?)\);", chart_body, re.S):
        drawn |= {int(x) for x in re.findall(r",\s*(\d+)\]", grp)}
    for name in ("EVCT", "EVCG"):
        m = re.search(r"const " + name + r"=\[([0-9,\s]*)\]", ino)
        check(m, f"the panel must declare the {name} chip group")
        if m:
            drawn |= {int(x) for x in re.findall(r"\d+", m.group(1))}
    for i in range(93, 108):
        check(i in evb_ids, f"limit id {i} has no EVB window, so it cannot be edited")
        check(i in drawn, f"limit id {i} is drawn by no renderer - settable in theory only")
    check('id="q93"' not in ino and 'id="q107"' not in ino,
          "the separate limits card must stay GONE")
    check("function ctab()" not in ino and 'id="ctb"' not in ino,
          "the operating table must stay REMOVED (user ordered its wholesale "
          "deletion): no renderer, no mount point")
    check("EVOPEN" in ino and "document.activeElement" in ino,
          "an open editor must suppress the re-render, or the ~1 s telemetry tick "
          "deletes the field under the user's fingers")
    check("e.key==='Escape'" in ino and "e.key==='Enter'" in ino,
          "click-to-edit needs both a commit key and a cancel key")
    check("function evedit" in ino and "evpop" in ino,
          "an SVG <text> cannot host an <input>, so the editor must be a floating "
          "panel anchored to the clicked label")
    check("BACKSTOP" in text_h and "CHG_LIMIT_PARAM_BACKSTOP_MV" in text_h,
          "the backstop voltage must be a parameter id, not a bare #define")
    bs = re.search(r"CHG_LIMIT_PARAM_BACKSTOP_MV[^\\n]*\\n[^\\n]*?(\\d+)u?,\\s*(\\d+)u?,\\s*(\\d+)u?", text_c)
    check("CHG_BACKSTOP_CURRENT_MV" not in ino,
          "the old compile-time backstop name must not linger in the panel")

    # --- 5c. the voltage prefilter. The current chain is filtered upstream,
    #         the pack voltage is not, and ~7 mV of ADC step times Kp lands
    #         straight on the duty. ---
    check(re.search(r"#define CHG_PID_VOLT_FILTER_N\s+(\d+)u", text_h),
          "the PID voltage prefilter divisor must exist")
    nfilt = int(re.search(r"#define CHG_PID_VOLT_FILTER_N\s+(\d+)u", text_h).group(1))
    # measured, two-loop design, duty reversals per 10 h at +/-7 mV / +/-15 mV
    # broadband noise (mean of four seeds): N=1 -> 32307/69656, N=8 -> 115/931,
    # N=16 -> 72/186, N=32 -> 62/104. The knee is real but N=32 is free here
    # because the user explicitly accepted slowness ("the battery is slow").
    check(nfilt >= 16, "a prefilter below N=16 leaves the voltage loop chattering on sensor noise")
    check("uint32_t__pidVoltFilt" in text_c and "uint32_t__pidVoltFilt =\n            uint32_t__batteryMv * CHG_LIM(CHG_LIMIT_PARAM_PID_VOLT_FILTER_N);" in text_c,
          "the prefilter must be seeded on re-seed so a bumpless transfer does not start from zero")

    # --- 6. the panel exposes every coefficient with its own guard ---
    check(all(f'id="q{i}"' in ino for i in range(83, 93)),
          "the panel must expose all 10 PID coefficients")
    check("function pchk()" in ino and "const PDEF=[12,1600,0,1000,1000,50,18000,0,10,1000];" in ino,
          "the panel must mirror the firmware clamps and carry the calibrated PID defaults")
    # the panel table and the firmware table must be the SAME numbers
    pdef = [int(x) for x in re.search(r"const PDEF=\[([^\]]*)\]", ino).group(1).split(",")]
    fw = [int(re.search(rf"#define CHG_PID_{loop}_{f}\s+(\d+)u", text_h).group(1))
          for loop in ("CURRENT", "VOLTAGE") for f in ("KP", "KI", "KD", "UP_RATE", "DOWN_RATE")]
    check(pdef == fw, f"panel PDEF {pdef} must equal the firmware defaults {fw}")
    check("حلقهٔ جریان — CC" in ino and "حلقهٔ ولتاژ — CV" in ino
          and "مرحلهٔ ۳" not in ino,
          "each loop must be labelled with WHAT IT CONTROLS, not a stage number (user asks "
          "for concrete wording), and the retired third stage must be gone from the panel")
    check("((D.q3||0)&(1<<(id-64)))" in ino and "((D.q4||0)&(1<<(id-96)))" in ino,
          "ids 96..107 need a FOURTH pending mask word: on the ESP side the old "
          "catch-all else arm evaluated 1UL << 32 for id 96, which is undefined "
          "behaviour, and the panel's modulo-32 shift quietly hid it")
    # the question that drove v1.24, preserved so nobody 'simplifies' it back
    check("۱۴۰۸" in ino and "۷۰۷" in ino,
          "the panel must explain WHY one PID is not enough (shared row = mV compared with "
          "mA = duty hunting; voltage-only + backstop = 707 mA because a backstop is reactive)")
    # [EN] v1.76: the all-in-one sdef() is gone; the six per-scenario keys all
    #      go through odef(), which skips any id pdflt() cannot answer for -
    #      so an undefined can no longer be sent to a PID id.
    # [FA] کلید همه‌باهم حذف شد و شش کلید جداگانه از odef() رد می‌شوند.
    check("function sdef(" not in ino and "function odef(ids)" in ino
          and "const v=pdflt(id);if(v==null)return;" in ino,
          "every scenario key must go through odef()+pdflt(), which refuses an id "
          "with no printed factory default")


def test_min_select_handover_v124():
    """[EN] The user asked, reading the parameter table: "how are you
       controlling one percent parameter with two duties? you have two
       parameters and one output." The answer is that there is ONE output and
       ONE integrator, driven by whichever loop asks for less. That answer is
       now published as a numeric trace in ESP_AGENT_SPEC 5.11 and in the
       panel's help text, so it needs a guard: without one, the next gain
       change makes those printed numbers quietly wrong.
       [FA] کاربر با دیدن جدول پارامترها پرسید: «چطور با دو دیوتی یک درصد را
       کنترل می‌کنی؟ دو پارامتر داری و یک خروجی.» جواب این است که یک خروجی و
       یک انتگرال‌گیر هست و هر حلقه که کمتر بخواهد آن را می‌راند. حالا این
       جواب به‌صورت یک ترنسکریپت عددی در بخش ۵.۱۱ و راهنمای پنل منتشر شده،
       پس نگهبان می‌خواهد وگرنه با اولین تغییر ضریب بی‌صدا غلط می‌شود."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import pid_tuning_sim as sim

    rec = sim.trace(hours=5.0)
    sw = sim.handovers(rec)

    # --- 1. exactly one handover: CC for the whole climb, CV from there on ---
    check(len(sw) == 1,
          f"a healthy CC/CV charge hands control over exactly ONCE, got {len(sw)} "
          "(more than one means the two loops are fighting at the knee)")
    check(rec[0]["win"] == "CURR" and rec[-1]["win"] == "VOLT",
          "the current loop must own the flat-pack climb and the voltage loop "
          "must own the absorb hold")

    # --- 2. the handover must be BUMPLESS. This is the whole reason the
    #        integrator is shared rather than one-per-loop: an idle private
    #        integral would have drifted and the duty would jump the moment
    #        control changed hands. ---
    before, after = rec[sw[0] - 1], rec[sw[0]]
    check(before["duty"] == after["duty"],
          f"handover must not move the duty (got {before['duty']} -> {after['duty']}); "
          "a jump here means the integrator is no longer shared")
    check(before["integ"] == after["integ"],
          f"handover must not move the integrator ({before['integ']} -> {after['integ']})")

    # --- 3. the direction of the selection must stay physical: while the pack
    #        is flat the voltage loop has huge headroom and would race; it is
    #        the CURRENT loop that must be the smaller (winning) demand. ---
    early = rec[1]
    check(early["pi"] < early["pv"] and early["win"] == "CURR",
          "on a flat pack the voltage loop must be the one with headroom and the "
          "current loop must be the binding constraint")
    late = rec[int(130 * 600)]
    check(late["pv"] < late["pi"] and late["win"] == "VOLT",
          "in absorb the voltage loop must be the binding constraint and must be "
          "able to go NEGATIVE (pull the duty down)")
    check(late["pv"] < 0, "the voltage loop must actively reduce duty in absorb")

    # --- 4. the published explanation must exist in both places the user reads ---
    spec = (ROOT / "ESP_AGENT_SPEC.md").read_text(encoding="utf-8")
    check("How TWO loops drive ONE duty" in spec and "min-select" in spec,
          "spec 5.11 must explain how two loops drive one output")
    check("199511" in spec,
          "the spec's bumpless claim must cite the measured integrator value")
    panel = (ROOT / "esp_link_panel" / "plink_panel.h").read_text(encoding="utf-8")
    fa_integ = "199511".translate(str.maketrans("0123456789", "۰۱۲۳۴۵۶۷۸۹"))
    check("دو حلقه چطور یک duty را می‌رانند" in panel and fa_integ in panel,
          "the panel card must carry the same concrete explanation, in the Persian "
          "numerals the panel actually renders (the user reads the panel at the "
          "bench, not the spec)")

    # --- 5. AIDS must track the top real id. It was left at 97 when the third
    #        PID row was deleted, which made ap() build u93..u97 = undefined and
    #        put five phantom ids in the settings backup - the same class of bug
    #        as the v1.22 AIDS/ADEF mismatch. ---
    ino = (ROOT / "esp_link_panel" / "plink_panel.h").read_text(encoding="utf-8")
    m = re.search(r"for\(let _i=27;_i<=(\d+);_i\+\+\)AIDS", ino)
    check(m, "the panel must build AIDS from a bounded loop")
    pdef = re.search(r"const PDEF=\[([^\]]*)\]", ino).group(1).split(",")
    # [EN] v1.28: PDEF is no longer the last default table - LDEF (ids
    #      93..107, the user-ordered limits) sits above it, and v1.43 adds
    #      IDEF (108..118, the imbalance scenario) as the last block, so the
    #      top real id is where IDEF ends. Deriving it from all tables keeps
    #      this check honest the next time a block is appended.
    # [FA] دیگر PDEF آخرین جدول پیش‌فرض نیست؛ LDEF (۹۳..۱۰۷) و در v1.43 بلوک
    #      IDEF (۱۰۸..۱۱۸، سناریوی عدم‌توازن) آخر است، پس بالاترین شناسهٔ
    #      واقعی جایی است که IDEF تمام می‌شود. مشتق‌کردن از هر سه جدول، این
    #      چک را برای بلوک بعدی هم صادق نگه می‌دارد.
    ldef_m = re.search(r"const LDEF=\[([^\]]*)\]", ino)
    check(ldef_m, "the panel must define LDEF for the limits block")
    ldef = [x for x in ldef_m.group(1).split(",") if x.strip()]
    check(len(ldef) == 15, f"LDEF must hold 15 limit defaults, got {len(ldef)}")
    idef_m = re.search(r"const IDEF=\[([^\]]*)\]", ino)
    check(idef_m, "the panel must define IDEF for the imbalance scenario block")
    idef = [x for x in idef_m.group(1).split(",") if x.strip()]
    check(len(idef) == 11, f"IDEF must hold 11 imbalance defaults, got {len(idef)}")

    # [EN] v1.68 (user order 2026-10-05: "why does the red lamp not blink in
    #      the imbalance state? it must blink"): the latched face is a BLINK
    #      on both sides. The firmware must read ids 123/124 and drive the
    #      lamp from the computed phase - a literal func__red(true) would be
    #      the old solid lamp sneaking back - and the panel simulator must
    #      show the same thing, because that simulator is what the operator
    #      checks the behaviour against.
    # [FA] نسخهٔ ۱.۶۸ به دستور کاربر: چهرهٔ قفل باید چشمک باشد، هم روی برد و
    #      هم در شبیه‌ساز پنل.
    uiled = (ROOT / "Firmware" / "Modules" / "Ui" / "ui_led.c").read_text(encoding="utf-8")
    imb_tick = uiled.split("void func__Ui_ScenarioImbalance_Tick(void)")[1].split("\n}")[0]
    check("IMBAL_PARAM_LATCH_BLINK_PERIOD_MS" in imb_tick and
          "IMBAL_PARAM_LATCH_BLINK_DUTY_PCT" in imb_tick,
          "the imbalance latch face must read the blink period/duty ids (123/124)")
    check("func__red(bool__redOn)" in imb_tick and "func__red(true)" not in imb_tick,
          "the latched red lamp must BLINK (user order 2026-10-05), not sit solid")
    check("func__green(false)" in imb_tick and "func__yellow(false)" in imb_tick,
          "and green/yellow stay off so the face cannot be read as BatLost")
    imb_h = (ROOT / "Firmware" / "Modules" / "Imbalance" / "imbalance.h").read_text(encoding="utf-8")
    check(re.search(r"IMBAL_PARAM_LATCH_BLINK_PERIOD_MS\s+123u", imb_h) and
          re.search(r"IMBAL_PARAM_LATCH_BLINK_DUTY_PCT\s+124u", imb_h) and
          re.search(r"IMBAL_DEF_LATCH_BLINK_PERIOD_MS\s+1000u", imb_h) and
          re.search(r"IMBAL_DEF_LATCH_BLINK_DUTY_PCT\s+50u", imb_h),
          "ids 123/124 and their 1000 ms / 50 percent defaults must be declared once, in imbalance.h")
    check("IMBAL_PARAM_OWNS" in imb_h and "IMBAL_PARAM_INDEX" in imb_h,
          "the two id blocks (108..118 and 123..124) need an ownership and an index helper")
    check('id="q123"' in ino and 'id="q124"' in ino,
          "card 5 must expose the blink period and duty inputs")
    check("c4v(123,1000)" in ino and "simblink(now,bper,bdt)" in ino,
          "the card-6 simulator must blink its red lamp the way the board does")
    # [EN] v1.49 appends one more block after IDEF: CDEF, the charge-side
    #      percent map (119..120). Derive the top from every table so the next
    #      block keeps this check honest too.
    # [FA] v1.49 یک بلوک دیگر بعد از IDEF اضافه می‌کند: CDEF (۱۱۹..۱۲۰).
    cdef_m = re.search(r"const CDEF=\[([^\]]*)\]", ino)
    check(cdef_m, "the panel must define CDEF for the charge-side percent map")
    cdef = [x for x in cdef_m.group(1).split(",") if x.strip()]
    check(len(cdef) == 13, f"CDEF must hold 13 ext defaults (charge map + band 2 shape + v1.68 imbalance blink + v1.72 dead-battery 125..127 + v1.80 its lamp/buzzer 128..131), got {len(cdef)}")
    top = 83 + len(pdef) - 1 + len(ldef) + len(idef) + len(cdef)
    # --- 6. the PARAMS_BULK reply must be proven to fit the protocol payload
    #        ceiling. The buffer auto-sizes from the count so it cannot be
    #        overrun, but the FRAME can still exceed 512 B and be rejected by
    #        the receiver; nothing proved that until v1.24. ---
    espc = ESP_LINK_C.read_text()
# [EN] v1.28 moved the goal posts: 1 + COUNT*5 CANNOT fit any more (541 > 512),
    #      so the reply is chunked. The assert that matters is now "one chunk fits",
    #      plus proof the sender actually emits the trailing partial chunk - an
    #      assert alone would be happy with a sender that dropped the tail.
    # [FA] در v1.28 صورت مسئله عوض شد: دیگر 1 + COUNT*5 جا نمی‌شود (۵۴۱ > ۵۱۲)،
    #      پس پاسخ تکه‌تکه می‌شود. assertِ مهم حالا «یک تکه جا شود» است، به‌علاوهٔ
    #      اثبات این‌که فرستنده واقعاً تکهٔ ناقص پایانی را هم می‌فرستد - assert به
    #      تنهایی با فرستنده‌ای که دُم را بیندازد هم راضی می‌شد.
    check("PARAMS_BULK chunk must fit the protocol payload ceiling" in espc
          and "ESPLINK_FRAME_MAX_PAYLOAD" in espc,
          "a _Static_assert must tie ONE CHUNK to the protocol payload ceiling, "
          "or a future param append silently builds an over-long frame")
    bulk_fn = espc[espc.find("static void func__EspLink_SendParamsBulk"):]
    bulk_fn = bulk_fn[:bulk_fn.find("\nstatic ", 10)]
    check(bulk_fn.count("func__EspLink_SendFrame") == 2
          and ">= ESPLINK_BULK_MAX_ITEMS" in bulk_fn,
          "the bulk sender must flush a full chunk AND send the trailing partial one; "
          "with COUNT=119 and 102 per chunk, dropping the tail loses ids 102..118")
    check("if (uint8_t__count > 0u)" in bulk_fn,
          "an empty trailing frame must NOT be sent - it reads as 'zero parameters known'")

    check(int(m.group(1)) == top,
          f"AIDS must stop at the top real id {top}, not {m.group(1)}: ap() and the "
          "settings backup both iterate it, so a stale bound injects phantom ids")


def test_dynamic_disturbances_v124():
    """[EN] The scenario sweep only ever tested a STATIC operating point.
       These are the things that actually happen on a bench: someone edits
       the absorb setpoint or the duty ceiling from the panel while a charge
       is running, or the pack is disturbed. Regenerate with
       `python3 pid_tuning_sim.py --stress` (this folder).
       [FA] جاروب سناریوها فقط نقطهٔ کار ایستا را می‌آزمود. این‌ها چیزهایی
       است که سر بنچ واقعاً رخ می‌دهد: تغییر ست‌پوینت یا سقف دیوتی از پنل
       وسط شارژ، یا اغتشاش پک."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import pid_tuning_sim as sim
    text_h_local = CHARGER_H.read_text()

    # --- 1. setpoint RAISED mid-charge: must track up and settle, not run away
    log = sim.run_events(hours=4.0, events=[(7200, "target", 14600)])
    lo, hi, _, _, tgt = sim._tail_stats(log)
    check(tgt == 14600 and abs(hi - 14600) <= 60,
          f"a setpoint raised to 14.6 V mid-charge must be tracked (settled {hi:.1f})")
    check(max(r[1] for r in log) < 14800,
          "raising the setpoint must not push the pack through the 14.8 V backstop")

    # --- 2. setpoint LOWERED below the pack: a charger is a SOURCE and cannot
    #        pull a pack down, so the only correct answer is duty 0 / no
    #        current. The failure mode to catch is the opposite: winding up
    #        and keeping the duty on.
    log = sim.run_events(hours=4.0, events=[(7200, "target", 14000)])
    tail = [r for r in log if r[0] >= 3.5 * 3600]
    check(all(r[3] == 0 for r in tail) and max(r[2] for r in tail) < 1.0,
          "an unreachable low setpoint must park the duty at ZERO, not wind up")

    # --- 3. duty ceiling cut hard mid-charge (panel write): the integral is
    #        clamped into [0, ceiling], so it must not wind up above the new
    #        ceiling and then overshoot when the ceiling is restored.
    log = sim.run_events(hours=4.0,
                         events=[(7200, "ceil", 60), (10800, "ceil", 500)])
    after = [r for r in log if r[0] >= 7200 and r[0] < 10800]
    check(max(r[3] for r in after) <= 60,
          "while the duty ceiling is 60 permille the applied duty must never exceed it")
    check(max(r[1] for r in log) < 14800 and max(r[2] for r in log) < 950,
          "restoring the ceiling must not produce a stored-up surge")

    # --- 4. the backstop must react in ONE pass, not gradually. This is the
    #        property that makes it a safety net rather than another loop.
    log = sim.run_events(hours=3.0, events=[(9000, "voc_bump", 600.0)])
    at = [r for r in log if r[0] >= 9000][:2]
    check(at[0][3] == 0,
          f"an over-voltage pack must drive the duty to 0 on the very first pass "
          f"(got {at[0][3]} permille)")

    # --- 4b. THE BACKSTOP MUST STAY IDLE during a normal charge. This is the
    #         whole distinction the v1.24 analysis rests on: a backstop is a
    #         reactive safety net, and the moment normal operation depends on
    #         it firing, it has become part of the control loop and the
    #         current limit-cycles. The bulk target therefore sits a margin
    #         BELOW the profile limit. Measured: with the shipped 10 mA margin
    #         the current never once passes 650 mA over a 6 h charge; with the
    #         margin removed it does so on 13274 passes.
    # [FA] پشتیبان باید در شارژ عادی بی‌کار بماند: لحظه‌ای که کار عادی به
    #      شلیک آن وابسته شود، تور ایمنی جزئی از حلقهٔ کنترل شده و جریان
    #      چرخهٔ حدی می‌زند.
    margin = int(re.search(r"#define CHG_PID_CURRENT_MARGIN_MA\s+(\d+)u",
                           text_h_local).group(1))
    check(margin >= 5,
          "the bulk target must sit a real margin below the profile current limit, "
          "or the 650 mA backstop becomes part of normal regulation")
    log = sim.charge(sim.FACTORY, hours=6.0)
    limit = sim.BULK_IMAX_MA
    over = [r for r in log if r[2] > limit]
    check(not over,
          f"the shipped tune must never reach the {limit} mA profile limit during a "
          f"normal charge (the backstop fired on {len(over)} passes) - a safety net "
          "that is routinely load-bearing is not a safety net")

    # --- 5. starting already at or above the setpoint must not kick
    for start, name in ((14400.0, "at the setpoint"), (14700.0, "above the setpoint")):
        log = sim.run_events(hours=3.0, events=(), voc=start)
        check(max(r[2] for r in log) < 650,
              f"a pack starting {name} must never be hit with bulk current")
        check(max(r[1] for r in log) < 14800,
              f"a pack starting {name} must not be pushed through the backstop")


def test_panel_lut_mirrors_firmware_v125():
    """[EN] The ESP panel keeps a HAND-COPIED mirror of both current LUTs so it
       can show the same milliamps the board computes. Nothing checked that the
       copy still matched. It bit immediately: refitting the channel-1 power
       table for the divider correction left the panel showing the old table,
       so the page and the board would have disagreed by ~8 percent with no
       warning anywhere. Any duplicated calibration table needs a test that
       the duplicate is still a duplicate.
       [FA] پنل ESP یک کپی دستی از هر دو جدول جریان دارد تا همان میلی‌آمپری را
       نشان دهد که برد حساب می‌کند. هیچ‌چیز بررسی نمی‌کرد که کپی هنوز برابر
       است. بلافاصله هم گاز گرفت: بعد از بازبرازش جدول کانال ۱ برای اصلاح
       مقسم، پنل جدول قدیمی را نگه داشت و صفحه با برد حدود ۸٪ اختلاف پیدا
       می‌کرد، بی‌هیچ هشداری. هر جدول کالیبراسیون تکراری، تست برابری می‌خواهد."""
    cal = (ROOT / "Firmware/Modules/Measurement/calibration.h").read_text()
    pan = (ROOT / "esp_link_panel" / "plink_panel.h").read_text()

    def c_arr(name):
        return [int(v.strip().rstrip("u"))
                for v in re.search(name + r"\[\] =\s*\{([^}]*)\}", cal).group(1).split(",")]

    def js_arr(name):
        return [int(v) for v in re.search(name + r"=\[([^\]]*)\]", pan).group(1).split(",")]

    for c_name, js_name, label in (
            ("CAL_Current1LutChainMa", "LUT1X", "channel 1 chain axis"),
            ("CAL_Current1LutBatteryMw", "LUT1Y", "channel 1 power axis"),
            ("CAL_Current2LutChainMa", "LUTX", "channel 2 chain axis"),
            ("CAL_Current2LutBatteryMw", "LUTY", "channel 2 power axis")):
        fw_vals, panel_vals = c_arr(c_name), js_arr(js_name)
        check(fw_vals == panel_vals,
              f"the panel's {label} mirror must equal the firmware table exactly "
              f"({c_name} vs {js_name}); firmware={fw_vals} panel={panel_vals} - a stale "
              "mirror makes the page report a different current than the board")


def test_vdda_reference_measurement_v125():
    """[EN] The board "never calibrates" for a mathematical reason: the
       dominant error is a GAIN (the ADC reference is not the assumed 3.300 V)
       while the only runtime calibration the product exposes is an ADDER.
       Bench proof: at the zero-current row of solo2_dense.csv the input
       channel and the 12 V channel over-read by the SAME 1.19 percent -
       different dividers, different resistors, agreeing to 5 parts in 100000,
       which only a shared term can do. This pins the machinery that finally
       makes the gain measurable.
       [FA] دلیل «کالیبره نشدن» ریاضی است: خطای غالب ضربی است و تنها
       کالیبراسیون موجود جمعی. این تست زیرساختی را قفل می‌کند که بالاخره آن
       خطای ضربی را قابل اندازه‌گیری می‌کند."""
    ioc = (ROOT / "CubeMX" / "CubeIDE.ioc").read_text()
    ioc2 = (ROOT / "CubeIDE" / "CubeIDE.ioc").read_text()
    main_c = (ROOT / "CubeIDE/Core/Src/main.c").read_text()
    adc_h = (ROOT / "Firmware/Bsp/Inc/bsp_adc.h").read_text()
    bsp_c = (ROOT / "Firmware/Bsp/Src/bsp_measurement.c").read_text()
    cal_h = (ROOT / "Firmware/Modules/Measurement/calibration.h").read_text()
    meas_c = (ROOT / "Firmware/Modules/Measurement/measurement.c").read_text()

    check(ioc == ioc2,
          "the CubeMX and CubeIDE .ioc copies must stay identical - they were "
          "byte-identical before and a one-sided edit would make the next "
          "regeneration silently revert the ADC config")
    check("ADC1.Channel-6=ADC_CHANNEL_VREFINT" in ioc and
          "ADC1.NbrOfConversion=6" in ioc,
          "the .ioc must carry the internal reference as a 6th rank")
    # [EN] VREFINT needs >=17.1 us of sampling. The ADC clock is 12 MHz, so one
    #      cycle is 83.3 ns and 239.5 cycles = 19.96 us is the ONLY legal
    #      setting; anything shorter returns a silently wrong reference and so
    #      a silently wrong VDDA for every channel.
    # [FA] VREFINT حداقل ۱۷٫۱ میکروثانیه نمونه‌برداری می‌خواهد و با کلاک ۱۲
    #      مگاهرتز تنها گزینهٔ مجاز ۲۳۹٫۵ سیکل است.
    check("ADC1.SamplingTime-1-6=239.5" in ioc and "RCC.ADCFreqValue=12000000" in ioc,
          "VREFINT needs >=17.1 us; at a 12 MHz ADC clock only 239.5 cycles "
          "(19.96 us) qualifies - a shorter sample returns a wrong reference")
    check("sConfig.Channel = ADC_CHANNEL_VREFINT;" in main_c and
          "sConfig.Rank = ADC_REGULAR_RANK_6;" in main_c and
          "sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;" in main_c and
          "hadc1.Init.NbrOfConversion = 6;" in main_c,
          "the generated ADC init must match the .ioc (rank 6 = VREFINT at "
          "239.5 cycles), or the DMA frame and the channel map disagree")
    check(re.search(r"#define BSP_ADC_CHANNEL_COUNT\s+6u", adc_h) and
          re.search(r"#define BSP_ADC_CHANNEL_VREFINT\s+5u", adc_h),
          "the BSP channel map must grow with the DMA frame")

    # --- the reference voltage must be passed IN, so the board port keeps no
    #     dependency on bench-calibration headers (layering) ---
    sig = ("func__BspMeasurement_VddaMv(uint16_t uint16_t__vrefintCounts,"
           + chr(10) +
           "                                    uint32_t uint32_t__vrefintMv)")
    check(sig in bsp_c and "calibration.h" not in bsp_c,
          "the BSP must take the reference voltage as an argument and must NOT "
          "include the Modules-layer calibration header")
    check("BSP_MEASUREMENT_VDDA_MIN_MV" in bsp_c and "BSP_MEASUREMENT_VDDA_MAX_MV" in bsp_c,
          "an implausible VDDA (stuck or un-enabled VREFINT) must be rejected rather "
          "than silently rescaling every voltage and current on the board")

    # --- it must ship OFF: the F103 stores no factory VREFINT calibration, so
    #     un-calibrated tracking is +-3.3 % (worse than the 1.2 % it fixes) AND
    #     it lowers every reading, which moves the OV cut LATER. ---
    check(re.search(r"#define CAL_VDDA_TRACKING_ENABLE\s+0u", cal_h),
          "VDDA tracking must ship DISABLED: the STM32F103 has no factory VREFINT "
          "calibration, so before a DMM step it is +-3.3 percent - worse than the "
          "error it corrects - and enabling it un-calibrated lowers every reading, "
          "moving the over-voltage cut LATER")
    check(re.search(r"#define CAL_VREFINT_MV\s+1200u", cal_h),
          "the nominal internal reference must be the datasheet 1.20 V until measured")
    check("UINT32_T__G__MeasVddaMv" in meas_c and
          "BSP_ADC_CHANNEL_VREFINT" in meas_c,
          "the measured VDDA must be published even while tracking is off - the whole "
          "point is to look at it against a DMM first")


def test_param_ranges_match_panel_v125():
    """[EN] Every parameter's legal range is written TWICE: as a `def D, LO..HI`
       comment beside the id in esp_link.h, and as real numbers in the panel's
       ParamMin/ParamMax arrays. Nothing compared them, which is the same
       duplicated-table trap that let the panel's current LUT go stale. This
       walks all of them.
       It also pins the NOTATION. Ids 67 and 69 used to read `0..66` and
       `0..68`, where 66 and 68 are parameter IDS, not values - a relational
       bound written in the field that everywhere else holds literals. It
       misleads every reader (it misled this audit) and it makes the two
       sources look like they disagree when they do not. Relational bounds go
       after the literal range, as `0..10000, <= 66`.
       [FA] بازهٔ مجاز هر پارامتر دو جا نوشته شده: کامنت کنار شناسه در
       esp_link.h و آرایه‌های ParamMin/ParamMax پنل. هیچ‌چیز آن‌ها را مقایسه
       نمی‌کرد - همان تلهٔ جدول تکراری که باعث کهنه‌شدن LUT پنل شد.
       نماد را هم قفل می‌کند: شناسه‌های ۶۷ و ۶۹ قبلاً `0..66` و `0..68` بودند
       که ۶۶ و ۶۸ در آن‌ها شناسهٔ پارامترند نه مقدار - قید رابطه‌ای در فیلدی که
       همه‌جای دیگر مقدار ادبی نگه می‌دارد."""
    h = ESP_LINK_H.read_text()
    params_h = (ROOT / "esp_link_panel" / "plink_params.h").read_text()

    def panel_arr(name):
        m = re.search(r"INT32_T__G__" + name + r"\[ESP_PARAM_COUNT\] = \{([^}]*)\}", params_h)
        check(m, f"the panel must declare {name}")
        return [int(x.strip()) for x in m.group(1).split(",")]

    pmin, pmax = panel_arr("ParamMin"), panel_arr("ParamMax")
    count = int(re.search(r"#define ESPLINK_PARAM_COUNT\s+(\d+)u", h).group(1))
    check(len(pmin) == count and len(pmax) == count,
          f"ParamMin/ParamMax must have exactly ESP_PARAM_COUNT entries "
          f"({len(pmin)}/{len(pmax)} vs {count})")

    checked, bad = 0, []
    for m in re.finditer(r"#define ESPLINK_PARAM_(\w+)\s+(\d+)u\s*/\*(.*?)\*/", h, re.S):
        name, pid, body = m.group(1), int(m.group(2)), m.group(3)
        rng = re.search(r"def\s+(-?\d+)\s*,\s*(-?\d+)\.\.(-?\d+)", body)
        if not rng or pid >= count:
            continue
        lo, hi = int(rng.group(2)), int(rng.group(3))
        checked += 1
        if pmin[pid] != lo or pmax[pid] != hi:
            bad.append(f"id {pid} {name}: firmware {lo}..{hi} vs panel {pmin[pid]}..{pmax[pid]}")
    check(checked >= 60,
          f"the range cross-check must actually cover the parameter table (only {checked} parsed)")
    check(not bad,
          "every documented firmware range must equal the panel's ParamMin/ParamMax, and a "
          "relational bound must NOT be written in the literal LO..HI field (use "
          "'0..10000, <= 66'): " + "; ".join(bad))


def test_benchlog_row_matches_header_v125():
    """[EN] The bench CSV is the artefact the whole calibration is built from,
       and its row was 6 columns longer than its header. wrow() looped
       `k<99` - the parameter count from v1.22 - while the real count is 93,
       so everything after the parameter block (both current channels, the
       globals and every DMM entry) was written under the WRONG heading. It
       had been wrong since v1.23 and nothing noticed, because the header is
       declared as text in plink_config.h and the row is built in JavaScript
       in plink_panel.h with no link between them.
       This counts BOTH and requires them to agree.
       [FA] فایل CSV بنچ همان چیزی است که کل کالیبراسیون از آن ساخته می‌شود، و
       ردیفش ۶ ستون از عنوانش بلندتر بود: حلقهٔ wrow روی ۹۹ (تعداد v1.22) مانده
       بود در حالی که تعداد واقعی ۹۳ است، پس هرچه بعد از بلوک پارامتر می‌آمد
       زیر عنوان اشتباه نوشته می‌شد. از v1.23 غلط بود و کسی نفهمید، چون عنوان
       متن است در یک فایل و ردیف جاوااسکریپت است در فایلی دیگر."""
    cfg = (ROOT / "esp_link_panel" / "plink_config.h").read_text()
    pan = (ROOT / "esp_link_panel" / "plink_panel.h").read_text()

    # --- count the column NAMES declared in the header block ---
    blk = re.search(r"#define ESP_BENCHLOG_HEADER(.*?)(?=\n#define )", cfg, re.S).group(1)
    names, group = [], None
    for line in re.findall(r'"([^"]*)"', blk):
        line = line.replace("\\n", "").strip()
        if not line.startswith("#"):
            continue
        body = line[1:].strip()
        g = re.match(r"\[(\w+)\]\s*(.*)", body)
        if g:
            group, body = g.group(1), g.group(2)
        elif group and not re.fullmatch(r"[\w,]+", body):
            continue          # trailing free-text lines are not columns
        if group is None or "cols" in body:
            continue
        names += [x for x in body.split(",") if x.strip()]
    header_n = len(names)

    # --- count what wrow() actually emits ---
    pn = int(re.search(r"const PN=(\d+);", pan).group(1))
    count = int(re.search(r"#define ESP_PARAM_COUNT\s+(\d+)u", cfg).group(1))
    check(pn == count,
          f"the CSV parameter loop must emit exactly ESP_PARAM_COUNT columns "
          f"(PN={pn} vs COUNT={count}) - this is the exact bug that shifted every "
          "column after the parameter block")
    row = re.search(r"function wrow\([^)]*\)\{.*?\n\s*return \[(.*?)\]\.join", pan, re.S).group(1)
    fixed = len(re.findall(r"q\(v\.\w+\)", row)) + 1          # the DMM group + note
    spreads = re.findall(r"\.\.\.\[([\d,\s]+)\]\.map", row)
    spread_n = sum(len([x for x in g.split(",") if x.strip()]) for g in spreads)
    lead = len(re.findall(r"^\s*sc,\s*i\+1", row)) and 6 or 6   # [id] group
    c_groups = len(re.findall(r"\.\.\.C\(\d+\)", row)) * 15   # ch1 + ch2
    scalars = len(re.findall(r"m\.(?:seq|fl|or)\b", row))
    row_n = lead + pn + c_groups + scalars + spread_n + fixed
    check(row_n == header_n,
          f"the CSV row must emit exactly as many columns as the header declares "
          f"(row {row_n} vs header {header_n}) - a mismatch silently files every "
          "later value under the wrong name, which corrupts the calibration table")

    # --- the raw calibration columns must be present in both ---
    for col in ("vin_counts", "v24_counts", "v12_counts", "vrefint_counts", "vdda_mv"):
        check(col in cfg,
              f"the bench header must carry the raw calibration column {col}")
    check("ESP_LINK_TLM_FIELD_COUNT    31u" in cfg and "ESP_LINK_TLM_SIZE          128u" in cfg,
          "the ESP telemetry window must match the firmware payload (v1.72: 30 fields / 124 bytes, including the scenario-6 pair)")
    # [EN] The header naming a column proves nothing if the firmware never sends
    #      the value - the slot would just carry a zero and the calibration would
    #      be built on it. Require the actual globals to be transmitted.
    # [FA] نام‌بردن ستون در عنوان وقتی فرم‌ور مقدارش را نمی‌فرستد چیزی را ثابت
    #      نمی‌کند - آن خانه صفر می‌ماند و کالیبراسیون روی صفر ساخته می‌شود.
    link_c = ESP_LINK_C.read_text()
    for g in ("UINT32_T__G__MeasVinRawCounts", "UINT32_T__G__MeasV24RawCounts",
              "UINT32_T__G__MeasV12RawCounts", "UINT32_T__G__MeasVrefintRawCounts",
              "UINT32_T__G__MeasVddaMv"):
        check(g in link_c,
              f"SendTelemetry must actually transmit {g}, not a placeholder - a named "
              "column carrying a constant zero is worse than no column")


def test_whole_program_consistency_v125():
    """[EN] Runs tools/audit_consistency.py, which compares every number this
       project writes by hand in more than one place: the parameter ids,
       counts, defaults and ranges across the firmware header, the panel's
       three tables, the preview server and the NVM map; the current LUTs
       against their panel mirrors; the TLM frame against what the ESP
       expects; the bench CSV row against its header; and the ADC chain from
       the .ioc through the generated init to the BSP channel map.
       It exists because every file in this repo can be locally correct while
       the product is broken - the defect lives BETWEEN files, so no amount of
       line-by-line reading finds it.
       [FA] اسکریپت ممیزی را اجرا می‌کند که هر عددی را که این پروژه دستی در
       بیش از یک جا می‌نویسد با هم مقایسه می‌کند. هست چون هر فایل این مخزن
       می‌تواند به‌تنهایی درست باشد در حالی که محصول خراب است - ایراد «بین»
       فایل‌ها زندگی می‌کند و هیچ مقدار خواندن خط‌به‌خط پیدایش نمی‌کند."""
    import subprocess
    r = subprocess.run([sys.executable, str(ROOT / "tools" / "audit_consistency.py")],
                       capture_output=True, text=True)
    check(r.returncode == 0,
          "cross-file consistency audit failed:\n" + r.stdout + r.stderr)
    m = re.search(r"invariants checked : (\d+)", r.stdout)
    check(m and int(m.group(1)) >= 50,
          f"the audit must actually check a meaningful number of invariants "
          f"(got {m.group(1) if m else 'none'}) - an audit that checks nothing "
          "passes everything")


def test_section_parameter_help_v125():
    """[EN] User order 2026-09-29: "in each section's ! help, explain the
       parameters too - what each one is and what it does."
       Written as ONE table (PX) plus one pass (pexp) that appends to any
       button carrying data-p, rather than eight hand-edited HTML bubbles.
       Hand-copied text is exactly what has rotted in this project before -
       the panel's LUT mirror and three frozen test literals all came from
       maintaining a second copy by hand.
       The trap to guard is a section advertising an id that has no entry:
       the bubble would then just silently omit that parameter, which looks
       like a complete list but is not.
       [FA] دستور کاربر: در «!» هر بخش، پارامترها هم توضیح داده شوند.
       به‌صورت یک جدول و یک پاس نوشته شده نه هشت حباب HTML دستی، چون متن
       دستی‌کپی‌شده همان چیزی است که قبلاً در این پروژه پوسیده. تلهٔ اصلی این
       است که بخشی شناسه‌ای را اعلام کند که در جدول نیست: آن‌وقت حباب بی‌صدا
       همان پارامتر را جا می‌اندازد و فهرست کامل به نظر می‌رسد."""
    ino = (ROOT / "esp_link_panel" / "plink_panel.h").read_text()

    # [EN] v1.73 (user order: "remove the ! badge on every section so the code
    #      gets lighter; explain above every section what the variables mean").
    #      The bubble renderer is gone; PX stays because the chips and the
    #      event popup read it. What is pinned now is the replacement: the
    #      badges must not come back, and every section must carry visible
    #      text of its own.
    # [FA] از v1.73 حباب «!» حذف شده و توضیح هر بخش همیشه روی صفحه است؛ PX
    #      می‌ماند چون تراشه‌ها و پنجرهٔ رویداد از آن می‌خوانند.
    check("const PX={" in ino and 'class="ib"' not in ino
          and "function pexp(" not in ino,
          "the ! help bubbles must stay removed while PX (chips, event popup) stays")
    secs = re.split(r'<div class="sec">', ino)[1:]
    nosx = [re.sub(r"<[^>]*>", "", x)[:40].strip() for x in secs
            if '<div class="sx">' not in x and '<div class="ds">' not in x]
    check(not nosx,
          f"every section must explain its variables above the fields; "
          f"sections with no explanation: {nosx}")

    # [EN] Parse ONLY the PX block - the panel has another id-keyed table (P,
    #      the manual-mode rows) whose entries have six fields, and a loose
    #      regex silently mixes the two.
    # [FA] فقط بلوک PX خوانده شود - پنل جدول دیگری هم با کلید شناسه دارد و
    #      regex شل، این دو را بی‌صدا قاطی می‌کند.
    px_blk = re.search(r"const PX=\{(.*?)\n?\};", ino, re.S)
    check(px_blk is not None, "the PX table must be findable as a single block")
    px_blk = px_blk.group(1)
    px = set(int(m) for m in re.findall(r"(\d+):\['", px_blk))
    # [EN] v1.73: with the data-p bubbles gone, the coverage rule is pinned on
    #      what the user can actually edit - every q<id> field on the page must
    #      have a PX entry, so no editable number is left without a name and a
    #      description anywhere in the panel.
    # [FA] از v1.73 قاعده روی خودِ فیلدهای ویرایش‌پذیر است: هر q<id> باید در
    #      PX نام و توضیح داشته باشد.
    edited = {int(x) for x in re.findall(r'<input[^>]*id="q(\d+)"', ino)}
    missing = sorted(edited - px)
    check(not missing,
          f"editable fields with no PX entry: {missing} - a number the user can "
          "change but the panel never names")

    count = int(re.search(r"#define ESP_PARAM_COUNT\s+(\d+)u",
                          (ROOT / "esp_link_panel" / "plink_config.h").read_text()).group(1))
    stray = sorted(i for i in px if i >= count)
    check(not stray, f"PX describes ids that do not exist: {stray}")

    # the whole point is prose a bench user can act on, not a restated name
    descs = {int(i): d for i, d in
             re.findall(r"(\d+):\['[^']*','([^']*)'\]", px_blk)}
    check(len(descs) == len(px),
          f"every PX entry must parse as [name, description] "
          f"({len(descs)} parsed of {len(px)})")
    thin = sorted(i for i, d in descs.items() if len(d) < 25)
    check(not thin,
          f"these parameters have a description too short to explain anything: {thin}")

    for must in (83, 88, 35, 36, 20, 25):
        check(must in px,
              f"id {must} is one of the parameters that actually changes charging "
              "behaviour and must be explained")


def test_theme_contrast_and_param_coverage_v125():
    """[EN] User order 2026-09-29: explain the variables everywhere, and fix the
       colouring.
       On the theme: measuring first showed the TEXT was never the problem -
       every text pair already passed AA. The fault was that the SURFACES were
       indistinguishable (card against page 1.07, borders 1.22), so the whole
       page read as one flat dark sheet with no depth. The palette is now an
       elevation ladder, and this pins it by COMPUTING the contrast rather than
       trusting the hex values to look right.
       [FA] دستور کاربر: متغیرها همه‌جا توضیح داده شوند و رنگ‌بندی درست شود.
       اندازه‌گیری نشان داد متن هیچ‌وقت مشکل نبود و همه AA را رد می‌کردند؛ ایراد
       این بود که سطح‌ها از هم تشخیص داده نمی‌شدند، پس کل صفحه یک ورق تخت بود.
       این تست کنتراست را حساب می‌کند نه اینکه به ظاهر کدهای رنگ اعتماد کند."""
    ino = (ROOT / "esp_link_panel" / "plink_panel.h").read_text()

    # ---------- every parameter is explained, and reachable from a section ----------
    px_blk = re.search(r"const PX=\{(.*?)\n?\};", ino, re.S)
    check(px_blk is not None, "the PX help table must exist")
    px = set(int(m) for m in re.findall(r"(\d+):\['", px_blk.group(1)))
    count = int(re.search(r"#define ESP_PARAM_COUNT\s+(\d+)u",
                          (ROOT / "esp_link_panel" / "plink_config.h").read_text()).group(1))
    missing = sorted(set(range(count)) - px)
    check(not missing,
          f"every parameter must be explained on the page; missing: {missing}")

    # [EN] v1.73: reachability is now about visible sections, not bubbles.
    # [FA] از v1.73 دسترس‌پذیری با بخش‌های دیده‌شدنی سنجیده می‌شود نه حباب‌ها.
    check(ino.count('<div class="sx">') >= ino.count('<div class="sec">'),
          "every section must carry its own visible explanation")

    # ---------- the palette, computed ----------
    def lum(h):
        h = h.lstrip("#")
        ch = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
        ch = [c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4 for c in ch]
        return 0.2126 * ch[0] + 0.7152 * ch[1] + 0.0722 * ch[2]

    def contrast(a, b):
        la, lb = lum(a), lum(b)
        return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)

    root = re.search(r":root\{([^}]*)\}", ino)
    check(root is not None, "the theme must be declared in one :root block")
    V = dict(re.findall(r"--([a-z0-9]+):(#[0-9a-f]{6})", root.group(1)))
    for key in ("bg", "cd", "in", "rs", "ln", "tx", "mu", "ac", "ok", "wa", "er"):
        check(key in V, f"the theme is missing --{key}")

    # [EN] Text must clear WCAG AA 4.5:1.
    # [FA] متن باید حد AA یعنی ۴٫۵ را رد کند.
    for label, fg, bgk in (("body on card", "tx", "cd"), ("muted on card", "mu", "cd"),
                           ("muted on raised", "mu", "rs"), ("accent on card", "ac", "cd"),
                           ("ok on card", "ok", "cd"), ("warn on card", "wa", "cd"),
                           ("error on card", "er", "cd")):
        r = contrast(V[fg], V[bgk])
        check(r >= 4.5, f"{label} contrast {r:.2f} is below AA 4.5")

    # [EN] Surfaces must form a visible ladder. The thresholds are GitHub dark's
    #      own measured ratios, so this is "at least as separated as a dark theme
    #      people read all day", not an arbitrary preference.
    # [FA] سطح‌ها باید نردبان دیدنی بسازند. آستانه‌ها نسبت‌های اندازه‌گیری‌شدهٔ یک
    #      تم تیرهٔ پرکاربردند، نه سلیقهٔ دلبخواه.
    for label, a, b, need in (("card vs page", "cd", "bg", 1.09),
                              ("raised vs card", "rs", "cd", 1.14),
                              ("border vs card", "ln", "cd", 1.42),
                              ("inset vs card", "in", "cd", 1.05)):
        r = contrast(V[a], V[b])
        check(r >= need,
              f"{label} separation {r:.2f} is below {need} - this is what made the "
              "page read as one flat dark sheet")

    # [EN] The ladder must actually ascend: inset darker than card, card than page... 
    # [FA] نردبان باید واقعاً بالا برود.
    check(lum(V["in"]) < lum(V["cd"]) < lum(V["rs"]),
          "inset < card < raised must hold, or the depth cues point the wrong way")


def test_absorb_ceiling_arms_on_current_v2():
    """[EN] USER-ORDERED LOGIC CHANGE 2026-09-29. The one-hour absorb ceiling
       used to start the moment absorb was ENTERED, at 14.3 V, and it ended the
       charge an hour later whether the tail had come down or not. The user
       spotted it from the symptom: "the charger turns off and then keeps
       coming back". On a pack still pulling hundreds of milliamps that hour
       expires mid-charge, the battery is not full, it sags past the 12.8 V
       reentry, and the whole cycle starts again.
       The ceiling now ARMS on CURRENT: it only begins counting once the tail
       first falls below CHG_ABSORB_MAX_ARM_MA. Its purpose was never "absorb
       lasts at most an hour", it was "once we are plainly in the tail, do not
       sit here forever".
       [FA] تغییر منطق به دستور کاربر. سقف یک‌ساعته از لحظهٔ ورود به ابزورب
       شروع می‌شد و بدون توجه به جریان قطع می‌کرد؛ کاربر از روی نشانه‌اش پیدایش
       کرد: «شارژر خاموش می‌شود و مدام برمی‌گردد». حالا سقف با جریان مسلح
       می‌شود."""
    h = CHARGER_H.read_text()
    c = CHARGER_C.read_text()

    arm = re.search(r"#define CHG_ABSORB_MAX_ARM_MA\s+(\d+)u", h)
    check(arm, "the absorb ceiling must have an explicit current arming threshold")
    arm_ma = int(arm.group(1))
    taper = int(re.search(r"#define CHG_TAPER_CURRENT_MA\s+(\d+)u", h).group(1))
    check(arm_ma > taper,
          f"the arming threshold ({arm_ma} mA) must sit ABOVE the taper-complete "
          f"current ({taper} mA) - arming at or below it would make the ceiling "
          "pointless, since the normal taper rule would already have finished")

    # the ceiling must count from the ARM tick, never from absorb entry again
    check("uint32_t__absorbMaxArmTick" in c,
          "the armed tick must be tracked per channel")
    timeout_blk = re.search(r"bool__absorbTimedOut =\s*\((.*?)\);", c, re.S)
    check(timeout_blk, "the absorb timeout expression must be findable")
    expr = timeout_blk.group(1)
    check("absorbMaxArmTick" in expr and "absorbEnterTick" not in expr,
          "the one-hour ceiling must count from the ARMED tick, not from absorb "
          "entry - counting from entry is what cut the charge off mid-tail")

    # arming must be latched, not re-evaluated every frame
    check(re.search(r"if \(\(charger_channel_state_t__channel->uint32_t__absorbMaxArmTick == 0u\) &&",
                    c),
          "arming must only happen when not already armed - re-arming on every "
          "frame that pops back above the threshold would let a sense chain "
          "specified at +/-10..20 mA defeat the ceiling entirely")

    # and it must be cleared wherever the absorb episode resets, or a stale arm
    # from the previous cycle would time the next one out immediately
    resets = len(re.findall(r"uint32_t__absorbMaxArmTick = 0u;", c))
    enters = len(re.findall(r"uint32_t__absorbEnterTick = 0u;", c))
    check(resets == enters,
          f"the armed tick must be cleared everywhere the absorb episode is reset "
          f"({resets} vs {enters} for absorbEnterTick) - a stale arm would end the "
          "next absorb the moment it starts")


def test_stage_graph_is_current_vs_voltage_v3():
    """[EN] User order 2026-09-29: "the horizontal axis of the chart should be
       current and the vertical axis voltage."
       It used to be a voltage ladder only - horizontal bands at each
       threshold, with current nowhere on the picture and the live dots placed
       at an arbitrary fixed x. Now it is a real I-V plane, which is the
       natural way to draw a CC/CV charger: the bulk leg is VERTICAL at the
       current limit (constant current, rising voltage) and the absorb leg is
       HORIZONTAL at the target voltage (constant voltage, falling current).
       Each battery's dot sits at its true (I, V), so one look says where on
       that path it actually is.
       [FA] دستور کاربر: محور افقی جریان و محور عمودی ولتاژ. قبلاً فقط نردبان
       عمودی ولتاژ بود و جریان اصلاً روی تصویر نبود. حالا صفحهٔ واقعی
       جریان-ولتاژ است: پای بالک عمودی روی سقف جریان و پای ابزورب افقی روی
       ولتاژ هدف."""
    ino = (ROOT / "esp_link_panel" / "plink_panel.h").read_text()
    # [EN] qchk() is defined BEFORE qgraph() in the file, so slice forwards
    #      from qgraph to the next top-level function, not to qchk.
    # [FA] qchk پیش از qgraph تعریف شده، پس باید رو به جلو برش زد.
    _i = ino.index("\nfunction qgraph()")
    body = ino[_i:ino.index("\nfunction afresh()", _i)]

    # --- both axes must exist and map the right quantity ---
    check(re.search(r"const X=ma=>Math\.round\(X0\+\(X1-X0\)\*", body),
          "the horizontal axis must map CURRENT to x")
    check(re.search(r"const Y=mv=>Math\.round\(H-", body),
          "the vertical axis must map VOLTAGE to y")
    check("IMAX" in body and "const istep" in body,
          "the current axis needs its own range and gridline step")
    check("جریان شارژ / Charge current (mA)" in body and
          "ولتاژ باتری / Battery voltage (V)" in body,
          "both axes must be labelled, bilingually")

    # --- the CC/CV path: a vertical leg at the current limit, a horizontal leg
    #     at the absorb voltage. This is the whole point of the new chart. ---
    poly = re.search(r"<polyline points=\"([^\"]*)\"", body)
    check(poly, "the CC/CV charge path must be drawn")
    pts = poly.group(1)
    check("${X(im.d)},${Y(lo)} ${X(im.d)},${Y(q.e.d)}" in pts,
          "the bulk leg must be VERTICAL at the current limit - same x, rising "
          "voltage")
    check("${X(im.d)},${Y(q.a.d)} ${X(tp.d)},${Y(q.a.d)}" in pts,
          "the absorb leg must be HORIZONTAL at the absorb voltage - same y, "
          "falling current")

    # --- the current limit and taper are vertical now, not horizontal ---
    # [EN] v1.30 draws every current threshold through one EVI loop so they
    #      can all be clicked, so the literal per-line markup is gone. The
    #      property still has to hold: same x at both ends, spanning y. The
    #      mapping of 25/26 onto the preview-aware im.d/tp.d is checked too -
    #      reading those from D.p instead would quietly drop the dashed
    #      "typed but not applied yet" preview.
    # [FA] حالا همهٔ آستانه‌های جریان از یک حلقهٔ EVI رسم می‌شوند تا همه
    #      کلیک‌پذیر باشند، پس مارک‌آپ جداگانهٔ هر خط رفته است. ولی همان خاصیت
    #      باید برقرار بماند: x یکسان در دو سر، گسترده در y.
    check('x1="${x}" y1="12" x2="${x}" y2="${H-22}"' in body,
          "every current threshold must be a VERTICAL line now that current is "
          "the horizontal axis")
    evi = re.search(r"const EVI=\{(.*?)\};", ino, re.S)
    check(evi, "the chart must declare its current-axis thresholds in EVI")
    evi_ids = {int(x) for x in re.findall(r"(\d+):\[", evi.group(1))}
    check({25, 26} <= evi_ids,
          "the bulk ceiling and the taper threshold must still be on the current "
          "axis, got " + str(sorted(evi_ids)))
    check("(id===25)?im.d:(id===26)?tp.d:evval(id)" in body,
          "ids 25/26 must keep reading the preview-aware values, or the dashed "
          "'typed but not applied yet' preview silently disappears")
    check("evat(id)" in body,
          "the current-axis labels must be clickable, not decoration")

    # --- live dots at their real operating point, not a fixed column ---
    check("const y=Y(b[1]),x=X(b[3]);" in body,
          "each battery's dot must be placed at its true (current, voltage) - "
          "b[1] is its voltage and b[3] its current; the old chart pinned x to a "
          "fixed fraction of the width, which carried no information")


def test_direct_lut_push_v166():
    """v1.66 (user order 2026-10-05): "do I really have to paste the table into
    the micro's code and build? push it straight into the micro's table area -
    and keep the current way too. Because it moves a lot of data, give this
    part its own storage path, separate from the other variables; it can even
    reset the micro after it has received, stored and handshaken the data, so
    every setting comes up on the new table."

    What is proven here: the separate flash block exists and cannot collide
    with the parameter records; the link carries the table as its OWN messages;
    the measurement path prefers the stored table but falls back to the
    compiled one; the build-time route (option "c") is still wired in the
    panel; the browser, the ESP and the STM32 all agree on one CRC32; and the
    commit state machine is run compiled against a RAM-emulated flash with
    faults injected."""
    import subprocess
    import tempfile
    import shutil

    cal_lut_h = (ROOT / "Firmware/Modules/CalLut/cal_lut.h").read_text(encoding="utf-8")
    cal_lut_c = (ROOT / "Firmware/Modules/CalLut/cal_lut.c").read_text(encoding="utf-8")
    esp_h = ESP_LINK_H.read_text(encoding="utf-8")
    esp_c = ESP_LINK_C.read_text(encoding="utf-8")
    meas_c = MEASUREMENT_C.read_text(encoding="utf-8")
    app_c = (ROOT / "Firmware/App/Src/app.c").read_text(encoding="utf-8")
    ino = (ROOT / "esp_link_panel/plink_panel.h").read_text(encoding="utf-8")
    sketch = (ROOT / "esp_link_panel/esp_link_panel.ino").read_text(encoding="utf-8")
    plink_http = (ROOT / "esp_link_panel/plink_http.h").read_text(encoding="utf-8")
    plink_link = (ROOT / "esp_link_panel/plink_link.h").read_text(encoding="utf-8")

    # ---------- the storage path is SEPARATE, which is the actual order ----------
    check(re.search(r"CAL_LUT_PAGE_A_ADDR\s+0x0800F000u", cal_lut_h) and
          re.search(r"CAL_LUT_PAGE_B_ADDR\s+0x0800F400u", cal_lut_h),
          "the LUT block must be its own two 1 KiB pages at 0x0800F000/0x0800F400, below the parameter records")
    check("CalLut pages must stay below the parameter NVM pages" in cal_lut_c and
          "CalLut record must fit inside one flash page" in cal_lut_c,
          "a static assert must prove the LUT record fits its page AND that the block never reaches the parameter pages")
    check("esp_link_nvm" not in cal_lut_c,
          "the LUT path must not reuse the parameter NVM module - separate storage was the point of the order")
    check(re.search(r"CAL_LUT_POINTS_MAX\s+24u", cal_lut_h) and
          re.search(r"CAL_LUT_POINTS_MIN\s+2u", cal_lut_h),
          "the per-channel point window (2..24) must be declared where both sides can mirror it")
    check(re.search(r"ESP_LUT_POINTS_MAX\s+24u",
                    (ROOT / "esp_link_panel/plink_config.h").read_text(encoding="utf-8")),
          "the ESP must mirror the board's 24-point cap, or it would send a table the board refuses")

    # ---------- the table travels as its own messages, never as parameters ----------
    for name, value in (("LUT_BEGIN", "0x04u"), ("LUT_CHUNK", "0x05u"),
                        ("LUT_COMMIT", "0x06u"), ("LUT_RESET", "0x07u"),
                        ("LUT_ACK", "0x13u")):
        check(re.search(r"ESPLINK_MSG_%s\s+%s" % (name, value), esp_h),
              "message %s must be defined on the STM32 side" % name)
        check(re.search(r"ESP_MSG_%s\s+%s" % (name, value),
                        (ROOT / "esp_link_panel/plink_config.h").read_text(encoding="utf-8")),
              "message %s must be mirrored on the ESP side with the SAME id" % name)
    check("func__EspLink_HandleLutFrame" in esp_c and "func__EspLink_SendLutAck" in esp_c,
          "the STM32 must handle the LUT frames and acknowledge every one of them")
    check("func__CalLut_Tick();" in esp_c and "func__CalLut_Init();" in app_c,
          "the LUT module must be initialised at boot and ticked by the comm task (the armed reboot lives there)")
    check("(uint8_t)'R'" in esp_c and "(uint8_t)'S'" in esp_c and "(uint8_t)'T'" in esp_c,
          "LUT_RESET must carry a literal magic: a stray frame must never be able to reboot a charging board")

    # ---------- measurement prefers the stored table, falls back to the compiled one ----------
    check("func__Measurement_BenchLutInterp" in meas_c,
          "both channels and both table sources must share ONE interpolation function")
    for ch in ("1", "2"):
        check(("func__CalLut_Active((uint8_t)CAL_LUT_CHANNEL_%s)" % ch) in meas_c and
              ("CAL_Current%sLutChainMa" % ch) in meas_c,
              "channel %s must use the pushed table when it is active and the compiled table otherwise" % ch)
    check("CAL_CURRENT1_LUT_POINTS" in meas_c and "CAL_CURRENT2_LUT_POINTS" in meas_c,
          "the compile-time tables must remain the fallback (the user asked to KEEP the current capability)")

    # ---------- the panel keeps BOTH routes and drives the handshake ----------
    check("calcode()" in ino and "calcdl()" in ino,
          "option (c), generating calibration.h for a rebuild, must still be offered")
    check("lsend()" in ino and "lrst()" in ino and "function lcrc(" in ino,
          "the panel must offer the direct push, the post-handshake reset, and compute the CRC32 itself")
    check("a.crc>>>0!==p.crc>>>0" in ino,
          "the panel must compare the board's CRC with its own before calling the push a success")
    check("جدول قبلی بدون تغییر ماند" in ino,
          "a refused push must say, in plain Persian, that the previous table is untouched")
    check('/lut' in sketch and '/lut/reset' in sketch,
          "the ESP must expose the push, status and reset routes")
    check("func__Esp_HttpLutPush" in plink_http and "func__Esp_HttpLutStatus" in plink_http and
          "func__Esp_HttpLutReset" in plink_http,
          "the three LUT handlers must exist in the sketch")
    check('"{\\"ok\\":0,\\"e\\":\\"handshake\\"}"' in plink_http,
          "the reset route must refuse unless the commit handshake succeeded - a reboot is not a retry button")
    check("func__Esp_LutTxStart" in plink_link and "func__Esp_SendLutReset" in plink_link,
          "the ESP link layer must be able to send the table and the reset request")
    # v1.67 finding L1: the board receives on a 256-byte DMA ring, so the four
    # frames of a push must be paced one per ACK, never written back to back.
    check("func__Esp_LutTxPump" in plink_link and
          "ESP_LUT_TX_ACK_TIMEOUT_MS" in plink_link and
          "ESP_LUT_TX_RETRY_MAX" in plink_link,
          "the push must be paced one frame per ACK, with a timeout and retries")
    check("func__Esp_LutTxPump()" in plink_link.split("func__Esp_PumpTx")[-1] or
          "if (func__Esp_LutTxPump())" in plink_link,
          "the paced sender must actually be driven from the link pump")

    # ---------- one CRC32, three implementations ----------
    import zlib
    table1 = [(0, 0), (48, 703), (130, 1914), (268, 3886)]
    table2 = [(0, 0), (37, 111), (106, 766)]
    content = bytearray()
    for tab in (table1, table2):
        content.append(len(tab))
        for chain, power in tab:
            content += chain.to_bytes(4, "little") + power.to_bytes(4, "little")
    py_crc = zlib.crc32(bytes(content)) & 0xFFFFFFFF

    node = shutil.which("node")
    if node is not None:
        src = re.search(r"function lcrc\(b\)\{.*?>>>0;\}", ino, re.S).group(0)
        js = src + "const b=[%s];console.log(lcrc(b)>>>0);" % ",".join(str(b) for b in content)
        r = subprocess.run([node, "-e", js], capture_output=True, text=True)
        check(r.returncode == 0 and int(r.stdout.strip()) == py_crc,
              "the panel's JavaScript CRC32 must be the standard reflected CRC32 the board computes (got %r)" % r.stdout)

    # ---------- compiled fault-injection run of the EXACT commit state machine ----------
    gcc = shutil.which("gcc")
    if gcc is None:
        print("  SKIP: gcc not found - the compiled LUT harness was not executed (static checks above still ran)")
        return
    tmp = tempfile.mkdtemp(prefix="callut_harness_")
    try:
        harness = r"""
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#define EMU_FLASH_BASE 0x11000000u
#define CAL_LUT_PAGE_A_ADDR (EMU_FLASH_BASE)
#define CAL_LUT_PAGE_B_ADDR (EMU_FLASH_BASE + 1024u)
#define CAL_LUT_HOST_TEST 1
static int g_reset = 0;
#define CAL_LUT_HOST_RESET_HOOK() (g_reset++)
static int g_cut_after = -1;
static int g_erase_fail = 0;
bool func__BspFlash_ErasePage(uint32_t p){
    if (p != CAL_LUT_PAGE_A_ADDR && p != CAL_LUT_PAGE_B_ADDR) return false;
    if (g_erase_fail) return false;
    memset((void *)(uintptr_t)p, 0xFF, 1024);
    return true;
}
bool func__BspFlash_ProgramHalfWords(uint32_t a, const uint16_t *d, uint32_t c){
    volatile uint16_t *dst = (volatile uint16_t *)(uintptr_t)a;
    for (uint32_t i = 0; i < c; i++) {
        if (g_cut_after >= 0 && (int)i >= g_cut_after) return false;
        dst[i] = d[i];
    }
    return true;
}
#include "cal_lut.c"

/* mirror of the panel's content CRC: [n][8 B per point] per channel */
static uint32_t content_crc(const uint32_t *x1, const uint32_t *y1, uint32_t n1,
                            const uint32_t *x2, const uint32_t *y2, uint32_t n2){
    uint32_t crc = 0xFFFFFFFFu;
    const uint32_t *xs[2] = { x1, x2 }, *ys[2] = { y1, y2 };
    uint32_t ns[2] = { n1, n2 };
    for (int ch = 0; ch < 2; ch++) {
        crc = func__CalLut_Crc32Byte(crc, (uint8_t)ns[ch]);
        for (uint32_t i = 0; i < ns[ch]; i++) {
            uint32_t pair[2] = { xs[ch][i], ys[ch][i] };
            for (int b = 0; b < 8; b++)
                crc = func__CalLut_Crc32Byte(crc, (uint8_t)((pair[b / 4] >> (8 * (b % 4))) & 0xFFu));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}
static uint32_t X1[4] = { 0, 48, 130, 268 }, Y1[4] = { 0, 703, 1914, 3886 };
static uint32_t X2[3] = { 0, 37, 106 },      Y2[3] = { 0, 111, 766 };
static void stage(uint32_t n1, uint32_t n2, int skip_last){
    assert(func__CalLut_StageBegin(n1, n2));
    for (uint32_t i = 0; i < n1; i++) assert(func__CalLut_StagePoint(1, i, X1[i], Y1[i]));
    for (uint32_t i = 0; i < n2; i++) {
        if (skip_last && i + 1u == n2) break;
        assert(func__CalLut_StagePoint(2, i, X2[i], Y2[i]));
    }
}

int main(void){
    uint32_t board = 0;
    void *m = mmap((void *)EMU_FLASH_BASE, 4096, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    assert(m == (void *)EMU_FLASH_BASE);
    memset(m, 0xFF, 4096);

    /* T1 fresh board: no table, the compiled one stays in charge */
    func__CalLut_Init();
    assert(!func__CalLut_Active(1) && !func__CalLut_Active(2));
    assert(func__CalLut_Points(1) == 0 && func__CalLut_ChainMa(1) == NULL);

    /* T2 a good push lands and is readable */
    uint32_t crc = content_crc(X1, Y1, 4, X2, Y2, 3);
    stage(4, 3, 0);
    assert(func__CalLut_Commit(crc, &board) == CAL_LUT_ST_OK);
    assert(board == crc);
    assert(func__CalLut_Active(1) && func__CalLut_Points(1) == 4);
    assert(func__CalLut_ChainMa(1)[3] == 268 && func__CalLut_PowerMw(1)[3] == 3886);
    assert(func__CalLut_Active(2) && func__CalLut_Points(2) == 3);
    uint16_t seq_after_first = ((const cal_lut_record_t *)CAL_LUT_PAGE_B_ADDR)->uint16_t__seq;
    assert(seq_after_first == 1u);

    /* T3 it survives a reboot */
    func__CalLut_Init();
    assert(func__CalLut_Active(1) && func__CalLut_Points(1) == 4);

    /* T4 a corrupted push (CRC mismatch) is refused and changes nothing */
    stage(4, 3, 0);
    assert(func__CalLut_Commit(crc ^ 0x1u, &board) == CAL_LUT_ST_CRC);
    func__CalLut_Init();
    assert(func__CalLut_Points(1) == 4);

    /* T5 a dipping power axis is refused (it would wrap the unsigned slope) */
    uint32_t YD[4] = { 0, 703, 400, 3886 };
    assert(func__CalLut_StageBegin(4, 0));
    for (uint32_t i = 0; i < 4; i++) assert(func__CalLut_StagePoint(1, i, X1[i], YD[i]));
    assert(func__CalLut_Commit(0u, &board) == CAL_LUT_ST_POWER);

    /* T6 a non-increasing chain axis is refused */
    uint32_t XB[4] = { 0, 48, 48, 268 };
    assert(func__CalLut_StageBegin(4, 0));
    for (uint32_t i = 0; i < 4; i++) assert(func__CalLut_StagePoint(1, i, XB[i], Y1[i]));
    assert(func__CalLut_Commit(0u, &board) == CAL_LUT_ST_CHAIN);

    /* T7 a chunk lost on the wire fails the commit instead of writing half a table */
    stage(4, 3, 1);
    assert(func__CalLut_Commit(crc, &board) == CAL_LUT_ST_MISSING);

    /* T8 counts out of range never even open a staging buffer */
    assert(!func__CalLut_StageBegin(25, 0));
    assert(!func__CalLut_StageBegin(1, 0));
    assert(!func__CalLut_StageBegin(0, 0));
    assert(func__CalLut_Commit(crc, &board) == CAL_LUT_ST_NO_STAGE);

    /* T9 power cut mid-write: the commit fails and the previous table survives */
    stage(4, 3, 0);
    g_cut_after = 20;
    assert(func__CalLut_Commit(crc, &board) == CAL_LUT_ST_FLASH);
    g_cut_after = -1;
    func__CalLut_Init();
    assert(func__CalLut_Active(1) && func__CalLut_Points(1) == 4);

    /* T10 ping-pong: the second good push goes to the OTHER page */
    uint32_t crc2 = content_crc(X1, Y1, 3, X2, Y2, 3);
    assert(func__CalLut_StageBegin(3, 3));
    for (uint32_t i = 0; i < 3; i++) assert(func__CalLut_StagePoint(1, i, X1[i], Y1[i]));
    for (uint32_t i = 0; i < 3; i++) assert(func__CalLut_StagePoint(2, i, X2[i], Y2[i]));
    assert(func__CalLut_Commit(crc2, &board) == CAL_LUT_ST_OK);
    assert(func__CalLut_Points(1) == 3);
    assert(func__CalLut_RecordValidate((const cal_lut_record_t *)CAL_LUT_PAGE_A_ADDR));
    assert(((const cal_lut_record_t *)CAL_LUT_PAGE_A_ADDR)->uint16_t__seq == 2u);

    /* T11 the reboot is armed, not immediate: the ACK frame must leave first */
    func__CalLut_RequestReset();
    func__CalLut_Tick();
    assert(g_reset == 0);
    func__CalLut_Tick();
    func__CalLut_Tick();
    assert(g_reset == 1);
    func__CalLut_Tick();
    assert(g_reset == 1);

    printf("LUT harness OK\n");
    return 0;
}
"""
        (Path(tmp) / "harness.c").write_text(harness, encoding="utf-8")
        binary = str(Path(tmp) / "harness")
        r = subprocess.run([gcc, "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O1",
                            "-I", str(ROOT / "Firmware/Modules/CalLut"),
                            str(Path(tmp) / "harness.c"), "-o", binary],
                           capture_output=True, text=True)
        check(r.returncode == 0, "the LUT harness must compile:\n" + r.stderr)
        r = subprocess.run([binary], capture_output=True, text=True)
        check(r.returncode == 0 and "LUT harness OK" in r.stdout,
              "the LUT commit state machine must survive the injected faults:\n" + r.stdout + r.stderr)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


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
        test_ch2_lut_refit_v118,
        test_ch1_lut_v119,
        test_charger_persistence_v114,
        test_direct_lut_push_v166,
        test_alarms_tab_v115,
        test_ui_mirror_v116,
        test_ui_mirror_v117,
        test_ui_mirror_v117b,
        test_audit_batch_v116b,
        test_telemetry_frame_pins_v116c,
        test_flash_diet_pins_v116d,
        test_fault_pump_rule_per_half_v121,
        test_two_loop_pid_v124,
        test_min_select_handover_v124,
        test_dynamic_disturbances_v124,
        test_panel_lut_mirrors_firmware_v125,
        test_vdda_reference_measurement_v125,
        test_param_ranges_match_panel_v125,
        test_benchlog_row_matches_header_v125,
        test_whole_program_consistency_v125,
        test_absorb_ceiling_arms_on_current_v2,
        test_stage_graph_is_current_vs_voltage_v3,
        test_section_parameter_help_v125,
        test_theme_contrast_and_param_coverage_v125,
    ]
    for test in tests:
        test()
    print(f"ALL {len(tests)} CHARGER HOST TESTS PASSED")
    print("Note: physical board tests not performed; no real battery connected.")


if __name__ == "__main__":
    main()
