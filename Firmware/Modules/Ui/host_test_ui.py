#!/usr/bin/env python3
# @file    host_test_ui.py
# @brief   [EN] Host checks for UI scenarios, APP_CONFIG defaults, and exact smart-beep cycle timing.
#          [FA] تست هاست سناریوهای UI، پیش‌فرض‌های APP_CONFIG و زمان‌بندی دقیق بوق هوشمند.

import os
import re


# ==================== Read UI default headers ====================

BASE_DIR = os.path.dirname(__file__)
CONFIG_PATHS = [
    os.path.join(BASE_DIR, "ui_led.h"),
    os.path.join(BASE_DIR, "ui_buzzer.h"),
]

defines = {}
for config_path in CONFIG_PATHS:
    with open(config_path, "r", encoding="utf-8", errors="ignore") as config_file:
        for line in config_file:
            match = re.match(r"#define\s+(\w+)\s+(\d+)u?", line)
            if match:
                defines[match.group(1)] = int(match.group(2))


def get_def(name, fallback):
    """[EN] Read a numeric UI default; [FA] یک مقدار پیش‌فرض عددی UI را بخوان."""
    return defines.get(name, fallback)


UI_PERCENT_FULL = get_def("UI_PERCENT_FULL", 100)
UI_PERCENT_SCALE = get_def("UI_PERCENT_SCALE", 100)
UI_BEEP_MIN_INTERVAL_CYCLES = get_def("UI_BEEP_MIN_INTERVAL_CYCLES", 1)

# ==================== APP_CONFIG default model ====================
# [EN] C code reads APP_CONFIG. These values model its const initialisation,
#      which is fed by the UI header defaults in app_config.c.
# [FA] کد C از APP_CONFIG می‌خواند. این مقادیر مقداردهی const آن را مدل می‌کنند
#      که در app_config.c از پیش‌فرض‌های هدرهای UI ساخته می‌شود.
APP_CONFIG = {
    "ui_input_ok_poll_ms": get_def("UI_INPUT_OK_POLL_MS", 500),
    "ui_selftest_led_ms": get_def("UI_SELFTEST_LED_MS", 500),
    "ui_boot_beep_ms": get_def("UI_BOOT_BEEP_MS", 150),
    "ui_blink_period_ms": get_def("UI_BLINK_PERIOD_MS", 1000),
    "ui_green_min_off_ms": get_def("UI_GREEN_MIN_OFF_MS", 10),
    "ui_input_threshold_mv": get_def("UI_INPUT_THRESHOLD_MV", 20000),
    "ui_bat_v_min_mv": get_def("UI_BAT_V_MIN_MV", 21000),
    "ui_bat_v_max_mv": get_def("UI_BAT_V_MAX_MV", 28000),
    "ui_charging_blink_period_ms": get_def("UI_CHARGING_BLINK_PERIOD_MS", 1000),
    "ui_charging_yellow_min_off_ms": get_def("UI_CHARGING_YELLOW_MIN_OFF_MS", 10),
    "ui_beep_base_ms": get_def("UI_BEEP_BASE_MS", 250),
    "ui_beep_double_thresh_pct": get_def("UI_BEEP_DOUBLE_THRESH_PCT", 20),
    "ui_beep_start_pct": get_def("UI_BEEP_START_PCT", 50),
}

APP_CONFIG_DEFAULT_FIELDS = {
    "ui_input_ok_poll_ms": "UI_INPUT_OK_POLL_MS",
    "ui_selftest_led_ms": "UI_SELFTEST_LED_MS",
    "ui_boot_beep_ms": "UI_BOOT_BEEP_MS",
    "ui_blink_period_ms": "UI_BLINK_PERIOD_MS",
    "ui_green_min_off_ms": "UI_GREEN_MIN_OFF_MS",
    "ui_input_threshold_mv": "UI_INPUT_THRESHOLD_MV",
    "ui_bat_v_min_mv": "UI_BAT_V_MIN_MV",
    "ui_bat_v_max_mv": "UI_BAT_V_MAX_MV",
    "ui_charging_blink_period_ms": "UI_CHARGING_BLINK_PERIOD_MS",
    "ui_charging_yellow_min_off_ms": "UI_CHARGING_YELLOW_MIN_OFF_MS",
    "ui_beep_base_ms": "UI_BEEP_BASE_MS",
    "ui_beep_double_thresh_pct": "UI_BEEP_DOUBLE_THRESH_PCT",
    "ui_beep_start_pct": "UI_BEEP_START_PCT",
}


def assert_app_config_initialisation():
    """[EN] Verify app_config.c wires every UI field to its header default.
    [FA] بررسی می‌کند app_config.c هر فیلد UI را به پیش‌فرض هدر وصل کرده باشد.
    """
    app_config_path = os.path.join(BASE_DIR, "../../Config/Src/app_config.c")
    with open(app_config_path, "r", encoding="utf-8", errors="ignore") as config_file:
        app_config_source = config_file.read()

    for field_name, macro_name in APP_CONFIG_DEFAULT_FIELDS.items():
        pattern = rf"\.{field_name}\s*=\s*{macro_name}\b"
        assert re.search(pattern, app_config_source), (
            f"APP_CONFIG field {field_name} is not wired to {macro_name}"
        )


def assert_ui_reads_app_config():
    """[EN] Verify ui_led.c consumes APP_CONFIG instead of tunable macros.
    [FA] بررسی می‌کند ui_led.c به‌جای ماکروهای قابل تنظیم از APP_CONFIG بخواند.
    """
    ui_led_path = os.path.join(BASE_DIR, "ui_led.c")
    with open(ui_led_path, "r", encoding="utf-8", errors="ignore") as ui_file:
        ui_source = ui_file.read()

    for field_name in APP_CONFIG_DEFAULT_FIELDS:
        assert f"APP_CONFIG.{field_name}" in ui_source, (
            f"ui_led.c does not consume APP_CONFIG.{field_name}"
        )


# ==================== Voltage and LED scenarios ====================


def battery_voltage_to_percent(voltage_mv):
    """[EN] Convert battery mV to 0..100%; [FA] تبدیل میلی‌ولت باتری به درصد."""
    minimum_mv = APP_CONFIG["ui_bat_v_min_mv"]
    maximum_mv = APP_CONFIG["ui_bat_v_max_mv"]

    if voltage_mv <= minimum_mv:
        return 0
    if voltage_mv >= maximum_mv:
        return UI_PERCENT_FULL

    voltage_range_mv = maximum_mv - minimum_mv
    voltage_offset_mv = voltage_mv - minimum_mv
    scaled_offset = voltage_offset_mv * UI_PERCENT_SCALE
    battery_percent = scaled_offset // voltage_range_mv
    return max(0, min(UI_PERCENT_FULL, battery_percent))


def scenario_battery_run(battery_mv):
    """[EN] Model BatteryRun green timing; [FA] مدل زمان‌بندی سبز در دشارژ."""
    battery_percent = battery_voltage_to_percent(battery_mv)
    remaining_percent = UI_PERCENT_FULL - battery_percent
    period_per_percent = APP_CONFIG["ui_blink_period_ms"] // UI_PERCENT_SCALE
    green_off_ms = remaining_percent * period_per_percent

    if green_off_ms < APP_CONFIG["ui_green_min_off_ms"]:
        green_off_ms = APP_CONFIG["ui_green_min_off_ms"]

    green_on_ms = APP_CONFIG["ui_blink_period_ms"] - green_off_ms
    return battery_percent, green_on_ms, green_off_ms


def scenario_charging(battery_mv):
    """[EN] Model Charging yellow timing; [FA] مدل زمان‌بندی زرد در شارژ."""
    battery_percent = battery_voltage_to_percent(battery_mv)
    charging_period_ms = APP_CONFIG["ui_charging_blink_period_ms"]

    if battery_percent >= UI_PERCENT_FULL:
        return battery_percent, 0, charging_period_ms
    if battery_percent == 0:
        return battery_percent, charging_period_ms, 0

    remaining_percent = UI_PERCENT_FULL - battery_percent
    period_per_percent = charging_period_ms // UI_PERCENT_SCALE
    yellow_on_ms = remaining_percent * period_per_percent

    if yellow_on_ms < APP_CONFIG["ui_charging_yellow_min_off_ms"]:
        yellow_on_ms = APP_CONFIG["ui_charging_yellow_min_off_ms"]
    if yellow_on_ms > charging_period_ms:
        yellow_on_ms = charging_period_ms

    yellow_off_ms = charging_period_ms - yellow_on_ms
    return battery_percent, yellow_on_ms, yellow_off_ms


# ==================== Smart beep timing ====================


def smart_beep_trigger_cycles(battery_percent, trigger_count=2):
    """[EN] Return the first requested smart-beep cycles.
    [FA] سیکل‌های اولین بوق‌های هوشمند را برمی‌گرداند.

    The model follows ui_led.c: a completed blink cycle is counted first,
    so 40 percent triggers on cycles 40, 80 rather than 41, 81.
    """
    if battery_percent >= APP_CONFIG["ui_beep_start_pct"]:
        return []

    interval_cycles = battery_percent
    if interval_cycles == 0:
        interval_cycles = UI_BEEP_MIN_INTERVAL_CYCLES

    cycle_counter = 0
    trigger_cycles = []
    total_cycles = interval_cycles * trigger_count

    for cycle_number in range(1, total_cycles + 1):
        if cycle_counter < interval_cycles:
            cycle_counter += 1

        if cycle_counter >= interval_cycles:
            trigger_cycles.append(cycle_number)
            cycle_counter = 0

    return trigger_cycles


def smart_beep_duration_ms(battery_percent):
    """[EN] Calculate beep duration; [FA] محاسبه طول بوق."""
    duration_ms = APP_CONFIG["ui_beep_base_ms"]
    if battery_percent < APP_CONFIG["ui_beep_double_thresh_pct"]:
        duration_ms *= 2
    return duration_ms


# ==================== Assertions ====================


def assert_equal(actual, expected, label):
    assert actual == expected, f"{label}: expected {expected}, got {actual}"


def run_assertions():
    """[EN] Run deterministic UI checks; [FA] تست‌های قطعی UI را اجرا کن."""
    assert_app_config_initialisation()
    assert_ui_reads_app_config()

    assert_equal(battery_voltage_to_percent(20000), 0, "below battery minimum")
    assert_equal(battery_voltage_to_percent(21000), 0, "battery minimum")
    assert_equal(battery_voltage_to_percent(24000), 42, "mid battery voltage")
    assert_equal(battery_voltage_to_percent(28000), 100, "battery maximum")

    assert_equal(scenario_battery_run(28000), (100, 990, 10), "full BatteryRun timing")
    assert_equal(scenario_battery_run(21000), (0, 0, 1000), "empty BatteryRun timing")
    assert_equal(scenario_charging(21000), (0, 1000, 0), "empty Charging timing")
    assert_equal(scenario_charging(28000), (100, 0, 1000), "full Charging timing")

    # Exact smart-beep timing: this is the regression test for the old +1 cycle bug.
    assert_equal(smart_beep_trigger_cycles(40), [40, 80], "40 percent beep cycles")
    assert_equal(smart_beep_trigger_cycles(20), [20, 40], "20 percent beep cycles")
    assert_equal(smart_beep_trigger_cycles(1), [1, 2], "1 percent beep cycles")
    assert_equal(smart_beep_trigger_cycles(0), [1, 2], "0 percent beep floor")
    assert_equal(smart_beep_trigger_cycles(50), [], "50 percent no periodic beep")
    assert_equal(smart_beep_duration_ms(40), 250, "normal beep duration")
    assert_equal(smart_beep_duration_ms(10), 500, "critical beep duration")


def main():
    print("Loaded APP_CONFIG defaults from ui_led.h and ui_buzzer.h:")
    for field_name in APP_CONFIG_DEFAULT_FIELDS:
        print(f"  {field_name}={APP_CONFIG[field_name]}")
    print()

    print("=== UI Host Test (APP_CONFIG + exact smart-beep timing) ===")
    print(
        f"Battery 0%={APP_CONFIG['ui_bat_v_min_mv']}mV, "
        f"100%={APP_CONFIG['ui_bat_v_max_mv']}mV, "
        f"InputThresh={APP_CONFIG['ui_input_threshold_mv']}mV\n"
    )

    run_assertions()

    print("Smart beep regression:")
    print("  40% => first beeps on cycles 40 and 80")
    print("  20% => first beeps on cycles 20 and 40")
    print("  10% => duration 500ms")
    print("\nALL HOST TESTS PASSED (APP_CONFIG + exact smart-beep timing)")


if __name__ == "__main__":
    main()
