#!/usr/bin/env python3
# @file    host_test_ui.py
# @brief   [EN] Host tests for UI buzzer API, battery percent hysteresis, BatteryRun 0/1 and green blink phase.
#          [FA] تست هاست API بوق، هیسترزیس درصد باتری و فاز چشمک BatteryRun.

import os
import re


# ==================== Read defines ====================

BASE_DIR = os.path.dirname(__file__)
BUZZER_HEADER = os.path.join(BASE_DIR, "ui_buzzer.h")
LED_HEADER = os.path.join(BASE_DIR, "ui_led.h")
defines = {}
for hdr in [BUZZER_HEADER, LED_HEADER]:
    try:
        with open(hdr, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                m = re.match(r"#define\s+(\w+)\s+\(?(-?\d+)\)?u?", line)
                if m:
                    defines[m.group(1)] = int(m.group(2))
    except FileNotFoundError:
        pass

UI_BUZZER_PERCENT_SCALE = defines.get("UI_BUZZER_PERCENT_SCALE", 100)
UI_BUZZER_DUTY_MAX_PERCENT = defines.get("UI_BUZZER_DUTY_MAX_PERCENT", 100)
UI_BUZZER_MIN_PERIOD_MS = defines.get("UI_BUZZER_MIN_PERIOD_MS", 1000)
UI_BUZZER_MIN_GAP_MS = defines.get("UI_BUZZER_MIN_GAP_MS", 100)
UI_BUZZER_CHECK_PERCENT = defines.get("UI_BUZZER_CHECK_PERCENT", 10)
UI_BUZZER_MIN_CHECK_MS = defines.get("UI_BUZZER_MIN_CHECK_MS", 1)
UI_BUZZER_OFF_RESULT = defines.get("UI_BUZZER_OFF_RESULT", 0)
UI_BUZZER_INVALID_RESULT = defines.get("UI_BUZZER_INVALID_RESULT", -1)

UI_BAT_V_MIN_MV = defines.get("UI_BAT_V_MIN_MV", 21000)
UI_BAT_V_MAX_MV = defines.get("UI_BAT_V_MAX_MV", 28000)
UI_BATTERY_PERCENT_HYSTERESIS_PERCENT = defines.get("UI_BATTERY_PERCENT_HYSTERESIS_PERCENT", 2)
UI_BATTERY_ZERO_EXIT_THRESHOLD = defines.get("UI_BATTERY_ZERO_EXIT_THRESHOLD", 2)
UI_BATTERY_ONE_EXIT_THRESHOLD = defines.get("UI_BATTERY_ONE_EXIT_THRESHOLD", 3)
UI_PERCENT_FULL = defines.get("UI_PERCENT_FULL", 100)
UI_PERCENT_SCALE = defines.get("UI_PERCENT_SCALE", 100)
UI_BLINK_PERIOD_MS = defines.get("UI_BLINK_PERIOD_MS", 1000)
UI_GREEN_MIN_OFF_MS = defines.get("UI_GREEN_MIN_OFF_MS", 10)

# ==================== Buzzer timing model ====================

def calculate_pattern(period_ms, duty_percent, beep_count, gap_ms):
    if period_ms <= 0 or duty_percent <= 0 or beep_count <= 0:
        return None
    if period_ms < UI_BUZZER_MIN_PERIOD_MS:
        return None
    if duty_percent > UI_BUZZER_DUTY_MAX_PERCENT:
        return None
    if beep_count > 1 and gap_ms < UI_BUZZER_MIN_GAP_MS:
        return None
    effective_gap_ms = gap_ms if beep_count > 1 else 0
    duty_window_ms = (period_ms * duty_percent) // UI_BUZZER_PERCENT_SCALE
    gap_count = beep_count - 1
    total_gap_ms = effective_gap_ms * gap_count
    if duty_window_ms <= total_gap_ms:
        return None
    available_on_ms = duty_window_ms - total_gap_ms
    if available_on_ms < beep_count:
        return None
    beep_on_ms = available_on_ms // beep_count
    on_remainder_ms = available_on_ms % beep_count
    beep_durations = [beep_on_ms] * beep_count
    beep_durations[-1] += on_remainder_ms
    period_tail_ms = period_ms - duty_window_ms
    return duty_window_ms, beep_durations, effective_gap_ms, period_tail_ms

def buzzer_level_at(period_ms, duty_percent, beep_count, gap_ms, elapsed_ms):
    pattern = calculate_pattern(period_ms, duty_percent, beep_count, gap_ms)
    if pattern is None:
        return False
    duty_window_ms, beep_durations, effective_gap_ms, _ = pattern
    cycle_elapsed_ms = elapsed_ms % period_ms
    if cycle_elapsed_ms >= duty_window_ms:
        return False
    cursor_ms = 0
    for beep_index, beep_duration_ms in enumerate(beep_durations):
        if cycle_elapsed_ms < cursor_ms + beep_duration_ms:
            return True
        cursor_ms += beep_duration_ms
        if beep_index < beep_count - 1:
            if cycle_elapsed_ms < cursor_ms + effective_gap_ms:
                return False
            cursor_ms += effective_gap_ms
    return False

def sample_waveform_segments(period_ms, duty_percent, beep_count, gap_ms):
    segments = []
    previous_level = buzzer_level_at(period_ms, duty_percent, beep_count, gap_ms, 0)
    segment_length_ms = 0
    for elapsed_ms in range(period_ms):
        current_level = buzzer_level_at(period_ms, duty_percent, beep_count, gap_ms, elapsed_ms)
        if current_level != previous_level:
            segments.append((previous_level, segment_length_ms))
            previous_level = current_level
            segment_length_ms = 0
        segment_length_ms += 1
    segments.append((previous_level, segment_length_ms))
    return segments

def calculate_next_check_ms(period_ms, duty_percent, beep_count, gap_ms):
    if period_ms <= 0 or duty_percent <= 0 or beep_count <= 0:
        return UI_BUZZER_OFF_RESULT
    pattern = calculate_pattern(period_ms, duty_percent, beep_count, gap_ms)
    if pattern is None:
        return UI_BUZZER_INVALID_RESULT
    duty_window_ms, beep_durations, effective_gap_ms, period_tail_ms = pattern
    del duty_window_ms
    positive_segments = list(beep_durations)
    if effective_gap_ms > 0:
        positive_segments.append(effective_gap_ms)
    if period_tail_ms > 0:
        positive_segments.append(period_tail_ms)
    smallest_timing_ms = min(positive_segments)
    next_check_ms = (smallest_timing_ms * UI_BUZZER_CHECK_PERCENT) // UI_BUZZER_PERCENT_SCALE
    return max(next_check_ms, UI_BUZZER_MIN_CHECK_MS)

def calculate_scenario_one_shot(duration_ms):
    if duration_ms <= 0:
        return None
    period_ms = max(duration_ms, UI_BUZZER_MIN_PERIOD_MS)
    if duration_ms >= period_ms:
        duty_percent = UI_BUZZER_DUTY_MAX_PERCENT
    else:
        duty_product = duration_ms * UI_BUZZER_PERCENT_SCALE
        duty_percent = (duty_product + period_ms - 1) // period_ms
        duty_percent = max(duty_percent, 1)
    return calculate_pattern(period_ms, duty_percent, 1, 0)

# ==================== Battery percent helpers ====================

def battery_voltage_to_percent(vbat_mv):
    if vbat_mv <= UI_BAT_V_MIN_MV:
        return 0
    if vbat_mv >= UI_BAT_V_MAX_MV:
        return UI_PERCENT_FULL
    voltage_range = UI_BAT_V_MAX_MV - UI_BAT_V_MIN_MV
    offset = vbat_mv - UI_BAT_V_MIN_MV
    if voltage_range == 0:
        return 0
    scaled = offset * UI_PERCENT_SCALE
    pct = scaled // voltage_range
    if pct > UI_PERCENT_FULL:
        pct = UI_PERCENT_FULL
    return int(pct)

class StablePercent:
    def __init__(self):
        self.stable = 0
        self.initialized = False
    def reset(self):
        self.stable = 0
        self.initialized = False
    def update(self, raw):
        if not self.initialized:
            self.stable = raw
            self.initialized = True
            return self.stable
        stable = self.stable
        if stable == 0:
            if raw >= UI_BATTERY_ZERO_EXIT_THRESHOLD:
                stable = 1
        elif stable == 1:
            if raw == 0:
                stable = 0
            elif raw >= UI_BATTERY_ONE_EXIT_THRESHOLD:
                stable = 2
        else:
            diff = abs(int(raw) - int(stable))
            if diff >= UI_BATTERY_PERCENT_HYSTERESIS_PERCENT:
                stable = raw
        self.stable = stable
        return self.stable

def green_timing(stable_percent, period=UI_BLINK_PERIOD_MS, min_off=UI_GREEN_MIN_OFF_MS):
    remaining = UI_PERCENT_FULL - stable_percent
    period_per = period // UI_PERCENT_SCALE
    off = remaining * period_per
    if off < min_off:
        off = min_off
    if off > period:
        off = period
    on = period - off
    return on, off

# ==================== Green blink phase model (non-blocking, preserve) ====================

class GreenBlink:
    def __init__(self):
        self.on = False
        self.initialized = False
        self.start_tick = 0
        self.on_ms = 0
        self.off_ms = 0
    def reset(self):
        self.on = False
        self.initialized = False
        self.start_tick = 0
        self.on_ms = 0
        self.off_ms = 0
    def update(self, on_ms, off_ms, now_tick):
        if not self.initialized:
            self.initialized = True
            self.on = True
            self.start_tick = now_tick
            self.on_ms = on_ms
            self.off_ms = off_ms
            return self.on
        if self.on_ms != on_ms or self.off_ms != off_ms:
            # preserve phase, only update durations
            self.on_ms = on_ms
            self.off_ms = off_ms
            return self.on
        elapsed = now_tick - self.start_tick
        # assume tick 1ms
        if self.on and elapsed >= self.on_ms:
            self.on = False
            self.start_tick = now_tick
        elif not self.on and elapsed >= self.off_ms:
            self.on = True
            self.start_tick = now_tick
        return self.on

# ==================== Assertions ====================

def assert_equal(actual, expected, label):
    assert actual == expected, f"{label}: expected {expected}, got {actual}"

def assert_true(cond, label):
    assert cond, f"{label}: expected True, got False"

def run_assertions():
    # buzzer
    assert_equal(calculate_pattern(10000, 10, 2, 100), (1000, [450, 450], 100, 9000), "ten-second two-beep pattern")
    assert_equal(calculate_next_check_ms(10000, 10, 2, 100), 10, "example next RTOS check")
    assert_equal(sample_waveform_segments(10000, 10, 2, 100), [(True, 450), (False, 100), (True, 450), (False, 9000)], "example GPIO waveform")
    assert_equal(calculate_pattern(60000, 2, 1, 0), (1200, [1200], 0, 58800), "BatteryRun one-beep 60s")
    assert_equal(calculate_pattern(60000, 4, 2, 100), (2400, [1150, 1150], 100, 57600), "two-beep 60s")
    assert_equal(calculate_pattern(20000, 31, 3, 100), (6200, [2000, 2000, 2000], 100, 13800), "three-beep 20s")
    assert_equal(calculate_pattern(10000, 100, 1, 0), (10000, [10000], 0, 0), "critical 10s")
    assert_equal(calculate_scenario_one_shot(150), (150, [150], 0, 850), "one-shot 150")
    assert_equal(calculate_scenario_one_shot(250), (250, [250], 0, 750), "one-shot 250")
    assert_equal(calculate_scenario_one_shot(500), (500, [500], 0, 500), "one-shot 500")
    assert_equal(calculate_pattern(1000, 50, 1, 1), (500, [500], 0, 500), "single-beep ignores gap")
    assert_equal(calculate_next_check_ms(1000, 50, 1, 1), 50, "single-beep next check")
    assert_equal(calculate_pattern(1000, 60, 3, 100), (600, [133, 133, 134], 100, 400), "remainder")
    assert_equal(calculate_next_check_ms(0, 10, 2, 100), UI_BUZZER_OFF_RESULT, "zero period off")
    assert_equal(calculate_next_check_ms(1000, 0, 2, 100), UI_BUZZER_OFF_RESULT, "zero duty off")
    assert_equal(calculate_next_check_ms(1000, 10, 0, 100), UI_BUZZER_OFF_RESULT, "zero count off")
    assert_equal(calculate_next_check_ms(999, 10, 2, 100), UI_BUZZER_INVALID_RESULT, "short period reject")
    assert_equal(calculate_next_check_ms(1000, 50, 2, 99), UI_BUZZER_INVALID_RESULT, "short gap reject")
    assert_equal(calculate_next_check_ms(1000, 10, 3, 100), UI_BUZZER_INVALID_RESULT, "gaps without pulse")
    assert_equal(calculate_next_check_ms(1000, 101, 1, 100), UI_BUZZER_INVALID_RESULT, "duty >100")

def run_battery_hysteresis_tests():
    print("\n=== BatteryRun Hysteresis Tests ===")
    s = StablePercent()
    # 25V approx: battery_voltage_to_percent(25000) -> let's compute
    v25 = battery_voltage_to_percent(25000)
    print(f"25V -> raw {v25} (expected ~57)")
    assert_true(56 <= v25 <= 58, "25V raw 56-58")
    # stable 57, raw 56.keep
    s.stable = 57
    s.initialized = True
    assert_equal(s.update(56), 57, "57+56 keep 57")
    assert_equal(s.update(58), 57, "57+58 keep 57")
    assert_equal(s.update(55), 55, "57->55 change")
    s.stable = 57
    assert_equal(s.update(59), 59, "57->59 change")
    print("Hysteresis 56/57/58 jitter preserved PASS")
    # raw 57 stable 57 to 55/59 diff 2
    # 0% special
    s.reset()
    s.update(0)
    assert_equal(s.stable, 0, "raw 0 -> stable 0")
    assert_equal(s.update(1), 0, "stable0 raw1 keep 0")
    assert_equal(s.update(2), 1, "stable0 raw2 ->1")
    s.stable = 1
    s.initialized = True
    assert_equal(s.update(0), 0, "stable1 raw0 ->0")
    s.stable = 1
    assert_equal(s.update(2), 1, "stable1 raw2 keep1")
    assert_equal(s.update(3), 2, "stable1 raw3 ->2")
    s.stable = 1
    s.initialized = True
    assert_equal(s.update(1), 1, "stable1 raw1 keep1")
    print("0/1 special hysteresis PASS")
    # BatteryRun at 25V green timing stability
    s.reset()
    s.update(v25)  # stable v25
    stable = s.stable
    on, off = green_timing(stable)
    print(f"Stable {stable} -> green on {on} off {off}")
    blink = GreenBlink()
    # start at tick 0
    blink.update(on, off, 0)
    assert_true(blink.on == True, "green start ON")
    # simulate 100ms later with jitter 56
    s.update(56)
    stable2 = s.stable
    on2, off2 = green_timing(stable2)
    # update with new timing but preserve phase
    prev_on = blink.on
    prev_start = blink.start_tick
    blink.update(on2, off2, 100)
    assert_equal(blink.on, prev_on, "phase preserved on jitter 56")
    assert_equal(blink.start_tick, prev_start, "start tick preserved on jitter")
    # after 56->55 change, timing changes but phase still preserved
    s.update(55)
    stable3 = s.stable
    on3, off3 = green_timing(stable3)
    prev_on = blink.on
    prev_start = blink.start_tick
    blink.update(on3, off3, 200)
    assert_equal(blink.on, prev_on, "phase preserved on 55 change")
    print("Green phase preserved PASS")
    # 0% critical behavior
    s.reset()
    s.update(0)
    assert_equal(s.stable, 0, "critical 0 stable 0")
    # noise 0<->1 should not restart critical beep: stable stays 0
    assert_equal(s.update(1), 0, "0/1 noise keep 0, no restart")
    assert_equal(s.update(0), 0, "keep 0")
    print("0/1 noise no restart PASS")
    # 1% independent
    s.stable = 1
    s.initialized = True
    assert_equal(s.update(1), 1, "stable1 raw1 keep1")
    # raw 2 keep 1
    assert_equal(s.update(2), 1, "1% raw2 keep1")
    # raw 3 ->2
    assert_equal(s.update(3), 2, "1% raw3 ->2")
    print("1% independent behavior PASS")

def main():
    print("=== UI Host Test (buzzer + BatteryRun hysteresis + phase + RTOS) ===")
    run_assertions()
    print("ALL BUZZER TESTS PASSED")
    run_battery_hysteresis_tests()
    print("\n=== Additional UI scenario quick checks ===")
    # mapping check: PA3 -> v_bat, PA2 -> v_in per board_pins
    # Just sanity: BatteryVoltageToPercent uses APP_CONFIG 21000-28000
    assert_equal(battery_voltage_to_percent(21000), 0, "21000 ->0%")
    assert_equal(battery_voltage_to_percent(28000), 100, "28000 ->100%")
    assert_equal(battery_voltage_to_percent(24500), 50, "24500 ->50%")
    print("Voltage mapping 21V=0, 28V=100, 24.5V=50 PASS")
    # Input thresholds sanity
    # green timing formula check
    on, off = green_timing(57)
    assert_true(on + off == UI_BLINK_PERIOD_MS, "green on+off = period")
    assert_true(off >= UI_GREEN_MIN_OFF_MS, "green off min")
    print(f"57% green timing on {on} off {off} PASS")
    print("\nALL HOST TESTS PASSED (buzzer + hysteresis + mapping + timing)")

if __name__ == "__main__":
    main()
