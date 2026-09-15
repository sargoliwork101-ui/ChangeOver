#!/usr/bin/env python3
# @file    host_test_ui.py
# @brief   [EN] Host tests for the single periodic buzzer API: period, duty, count, and gap.
#          [FA] تست هاست API یگانه بوق دوره‌ای: دوره، دیوتی، تعداد و گپ.

import os
import re


# ==================== Read buzzer constants ====================

BASE_DIR = os.path.dirname(__file__)
HEADER_PATH = os.path.join(BASE_DIR, "ui_buzzer.h")

defines = {}
with open(HEADER_PATH, "r", encoding="utf-8", errors="ignore") as header_file:
    for line in header_file:
        match = re.match(r"#define\s+(\w+)\s+(\d+)u?", line)
        if match:
            defines[match.group(1)] = int(match.group(2))

UI_BUZZER_PERCENT_SCALE = defines.get("UI_BUZZER_PERCENT_SCALE", 100)
UI_BUZZER_DUTY_MAX_PERCENT = defines.get("UI_BUZZER_DUTY_MAX_PERCENT", 100)


# ==================== Buzzer timing model ====================


def calculate_pattern(period_ms, duty_percent, beep_count, gap_ms):
    """[EN] Calculate pulse durations and the low tail for one period.
    [FA] زمان پالس‌ها و خاموشی انتهای یک دوره را محاسبه می‌کند.

    The duty window includes the gaps between pulses. This is the rule used by
    func__Ui_Buzzer_Tick().
    """
    if period_ms <= 0 or duty_percent <= 0 or beep_count <= 0:
        return None
    if duty_percent > UI_BUZZER_DUTY_MAX_PERCENT:
        return None

    duty_window_ms = (period_ms * duty_percent) // UI_BUZZER_PERCENT_SCALE
    gap_count = beep_count - 1
    total_gap_ms = gap_ms * gap_count

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

    return duty_window_ms, beep_durations, gap_ms, period_tail_ms


# ==================== Assertions ====================


def assert_equal(actual, expected, label):
    assert actual == expected, f"{label}: expected {expected}, got {actual}"


def run_assertions():
    """[EN] Run deterministic buzzer timing checks; [FA] تست‌های قطعی زمان‌بندی بوق."""
    # User example: 10s period, 10% duty, 2 pulses, 100ms gap.
    assert_equal(
        calculate_pattern(10000, 10, 2, 100),
        (1000, [450, 450], 100, 9000),
        "ten-second two-beep pattern",
    )

    # One pulse consumes the complete duty window and ignores the gap.
    assert_equal(
        calculate_pattern(1000, 50, 1, 200),
        (500, [500], 200, 500),
        "single-beep pattern",
    )

    # Remainder milliseconds are placed on the last pulse so the duty window
    # remains exact instead of losing integer-division time.
    assert_equal(
        calculate_pattern(1000, 50, 3, 9),
        (500, [160, 160, 162], 9, 500),
        "three-beep integer timing",
    )

    assert_equal(calculate_pattern(0, 10, 2, 100), None, "zero period disables")
    assert_equal(calculate_pattern(10000, 0, 2, 100), None, "zero duty disables")
    assert_equal(calculate_pattern(10000, 10, 0, 100), None, "zero count disables")
    assert_equal(calculate_pattern(1000, 10, 3, 100), None, "gaps larger than duty reject")


def main():
    print("=== UI Buzzer Host Test (period + duty + count + gap) ===")
    print("Example: period=10000ms, duty=10%, count=2, gap=100ms")
    print("Output: 450ms ON, 100ms OFF, 450ms ON, 9000ms OFF")
    print()

    run_assertions()

    print("ALL HOST TESTS PASSED (single periodic buzzer API)")


if __name__ == "__main__":
    main()
