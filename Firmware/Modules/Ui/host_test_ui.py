#!/usr/bin/env python3
# @file    host_test_ui.py
# @brief   [EN] Host tests for the single periodic buzzer API and its safety limits.
#          [FA] تست هاست API یگانه بوق دوره‌ای و محدودیت‌های ایمنی آن.

import os
import re


# ==================== Read buzzer constants / خواندن ثابت‌های بازر ====================

BASE_DIR = os.path.dirname(__file__)
HEADER_PATH = os.path.join(BASE_DIR, "ui_buzzer.h")
defines = {}
with open(HEADER_PATH, "r", encoding="utf-8", errors="ignore") as header_file:
    for line in header_file:
        match = re.match(r"#define\s+(\w+)\s+\(?(-?\d+)\)?u?", line)
        if match:
            defines[match.group(1)] = int(match.group(2))

UI_BUZZER_PERCENT_SCALE = defines.get("UI_BUZZER_PERCENT_SCALE", 100)
UI_BUZZER_DUTY_MAX_PERCENT = defines.get("UI_BUZZER_DUTY_MAX_PERCENT", 100)
UI_BUZZER_MIN_PERIOD_MS = defines.get("UI_BUZZER_MIN_PERIOD_MS", 1000)
UI_BUZZER_MIN_GAP_MS = defines.get("UI_BUZZER_MIN_GAP_MS", 100)
UI_BUZZER_CHECK_PERCENT = defines.get("UI_BUZZER_CHECK_PERCENT", 10)
UI_BUZZER_MIN_CHECK_MS = defines.get("UI_BUZZER_MIN_CHECK_MS", 1)
UI_BUZZER_OFF_RESULT = defines.get("UI_BUZZER_OFF_RESULT", 0)
UI_BUZZER_INVALID_RESULT = defines.get("UI_BUZZER_INVALID_RESULT", -1)


# ==================== Buzzer timing model / مدل زمان‌بندی بازر ====================


def calculate_pattern(period_ms, duty_percent, beep_count, gap_ms):
    """[EN] Calculate pulse durations and the low tail for one period.
    [FA] زمان پالس‌ها و خاموشی انتهای یک دوره را محاسبه می‌کند.

    The duty window includes the gaps between pulses. This is the rule used by
    func__Ui_Buzzer_Tick(). A non-zero period below the safety minimum and a
    too-small gap for multiple pulses are rejected.
    """
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


# ==================== Buzzer waveform sampling / نمونه‌برداری شکل موج بازر ====================


def buzzer_level_at(period_ms, duty_percent, beep_count, gap_ms, elapsed_ms):
    """[EN] Return GPIO level at one millisecond in the periodic pattern.
    [FA] سطح GPIO را در یک میلی‌ثانیه از الگوی دوره‌ای برمی‌گرداند.
    """
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
    """[EN] Compress one millisecond waveform into level/duration segments.
    [FA] موج یک میلی‌ثانیه‌ای را به بخش‌های سطح/مدت فشرده می‌کند.
    """
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


# ==================== RTOS check interval / فاصله بررسی RTOS ====================


def calculate_next_check_ms(period_ms, duty_percent, beep_count, gap_ms):
    """[EN] Return 0 for valid off, -1 for invalid, or the next check delay.
    [FA] برای خاموشی معتبر صفر، برای نامعتبر منفی یک، وگرنه تأخیر مراجعه بعدی را برمی‌گرداند.
    """
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


# ==================== Scenario one-shot adapter / تبدیل بوق تک‌باره سناریو ====================


def calculate_scenario_one_shot(duration_ms):
    """[EN] Convert a legacy one-shot duration to a valid periodic API pattern.
    [FA] مدت تک‌باره قدیمی را به الگوی معتبر API دوره‌ای تبدیل می‌کند.
    """
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


# ==================== Assertions / بررسی‌های قطعی ====================


def assert_equal(actual, expected, label):
    assert actual == expected, f"{label}: expected {expected}, got {actual}"


def run_assertions():
    """[EN] Run deterministic buzzer timing checks; [FA] تست‌های قطعی زمان‌بندی بوق."""
    # User example: 10s period, 10% duty, 2 pulses, 100ms gap. / مثال کاربر: دوره ۱۰ ثانیه، دیوتی ۱۰ درصد، دو پالس و گپ ۱۰۰ میلی‌ثانیه.
    assert_equal(
        calculate_pattern(10000, 10, 2, 100),
        (1000, [450, 450], 100, 9000),
        "ten-second two-beep pattern",
    )
    assert_equal(
        calculate_next_check_ms(10000, 10, 2, 100),
        10,
        "example next RTOS check",
    )
    assert_equal(
        sample_waveform_segments(10000, 10, 2, 100),
        [(True, 450), (False, 100), (True, 450), (False, 9000)],
        "example GPIO waveform",
    )

    # BatteryRun warning bands use approximate integer duties accepted by the user. / بازه‌های هشدار BatteryRun از دیوتی صحیح تقریبی مورد تأیید کاربر استفاده می‌کنند.
    assert_equal(
        calculate_pattern(60000, 2, 1, 0),
        (1200, [1200], 0, 58800),
        "BatteryRun one-beep 60-second pattern",
    )
    assert_equal(
        calculate_pattern(60000, 4, 2, 100),
        (2400, [1150, 1150], 100, 57600),
        "BatteryRun two-beep 60-second pattern",
    )
    assert_equal(
        calculate_pattern(20000, 31, 3, 100),
        (6200, [2000, 2000, 2000], 100, 13800),
        "BatteryRun three-beep 20-second pattern",
    )
    assert_equal(
        calculate_pattern(10000, 100, 1, 0),
        (10000, [10000], 0, 0),
        "BatteryRun critical ten-second pattern",
    )

    # Legacy one-shot durations are represented with a safe period and stopped / مدت‌های تک‌باره قدیمی با دوره امن نمایش داده و متوقف می‌شوند
    # explicitly by the scenario after the requested duration. / سناریو پس از مدت درخواستی آن‌ها را صریحاً متوقف می‌کند.
    assert_equal(
        calculate_scenario_one_shot(150),
        (150, [150], 0, 850),
        "board-test one-shot adapter",
    )
    assert_equal(
        calculate_scenario_one_shot(250),
        (250, [250], 0, 750),
        "battery-run base one-shot adapter",
    )
    assert_equal(
        calculate_scenario_one_shot(500),
        (500, [500], 0, 500),
        "battery-run doubled one-shot adapter",
    )

    # One pulse has no adjacent gap, so a gap below 100ms is ignored. / یک پالس گپ مجاور ندارد و گپ کمتر از ۱۰۰ میلی‌ثانیه نادیده گرفته می‌شود.
    assert_equal(
        calculate_pattern(1000, 50, 1, 1),
        (500, [500], 0, 500),
        "single-beep pattern ignores gap",
    )
    assert_equal(
        calculate_next_check_ms(1000, 50, 1, 1),
        50,
        "single-beep next RTOS check",
    )

    # Remainder milliseconds are placed on the last pulse so the duty window / میلی‌ثانیه‌های باقی‌مانده روی پالس آخر قرار می‌گیرند تا پنجره دیوتی
    # remains exact instead of losing integer-division time. / دقیق بماند و زمان تقسیم صحیح از بین نرود.
    assert_equal(
        calculate_pattern(1000, 60, 3, 100),
        (600, [133, 133, 134], 100, 400),
        "three-beep safe-gap remainder timing",
    )

    # Safe-off commands return zero and must not be reported as errors. / فرمان‌های خاموشی امن صفر برمی‌گردانند و نباید خطا گزارش شوند.
    assert_equal(calculate_next_check_ms(0, 10, 2, 100), UI_BUZZER_OFF_RESULT, "zero period turns off")
    assert_equal(calculate_next_check_ms(1000, 0, 2, 100), UI_BUZZER_OFF_RESULT, "zero duty turns off")
    assert_equal(calculate_next_check_ms(1000, 10, 0, 100), UI_BUZZER_OFF_RESULT, "zero count turns off")

    # Non-zero unsafe configurations return -1. / تنظیمات غیرصفر و ناامن مقدار منفی یک برمی‌گردانند.
    assert_equal(calculate_next_check_ms(999, 10, 2, 100), UI_BUZZER_INVALID_RESULT, "short period rejects")
    assert_equal(calculate_next_check_ms(1000, 50, 2, 99), UI_BUZZER_INVALID_RESULT, "short gap rejects")
    assert_equal(calculate_next_check_ms(1000, 10, 3, 100), UI_BUZZER_INVALID_RESULT, "gaps without pulse time reject")
    assert_equal(calculate_next_check_ms(1000, 101, 1, 100), UI_BUZZER_INVALID_RESULT, "duty above 100 rejects")


# ==================== Main / اجرای اصلی ====================


def main():
    print("=== UI Buzzer Host Test (limits + adaptive RTOS check) ===")
    print("Example: period=10000ms, duty=10%, count=2, gap=100ms")
    print("Output: 450ms ON, 100ms OFF, 450ms ON, 9000ms OFF")
    print()

    run_assertions()

    print("ALL HOST TESTS PASSED (single periodic buzzer API)")


if __name__ == "__main__":
    main()
