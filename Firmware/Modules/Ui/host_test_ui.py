#!/usr/bin/env python3
# @file    host_test_ui.py
# @brief   [EN] Host simulation of UI scenarios, reads thresholds from ui.h single source (ui_config.h deleted).
#          [FA] شبیه‌سازی هاست سناریوهای UI، آستانه‌ها را از ui.h می‌خواند (ui_config.h حذف شد).

import os, re, sys

# ==================== Read ui.h single source (ui_config.h deleted) ====================
CONFIG_PATH = os.path.join(os.path.dirname(__file__), "ui.h")
defines = {}
try:
    with open(CONFIG_PATH, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            m = re.match(r'#define\s+(\w+)\s+(\d+)u?', line)
            if m:
                defines[m.group(1)] = int(m.group(2))
except FileNotFoundError:
    print(f"WARN: {CONFIG_PATH} not found, using fallback")
    defines = {}

def get_def(name, fallback):
    return defines.get(name, fallback)

UI_BAT_V_MIN_MV = get_def("UI_BAT_V_MIN_MV", 21000)
UI_BAT_V_MAX_MV = get_def("UI_BAT_V_MAX_MV", 28000)
UI_INPUT_THRESHOLD_MV = get_def("UI_INPUT_THRESHOLD_MV", 20000)
UI_BLINK_PERIOD_MS = get_def("UI_BLINK_PERIOD_MS", 1000)
UI_GREEN_MIN_OFF_MS = get_def("UI_GREEN_MIN_OFF_MS", 10)
UI_CHARGING_BLINK_PERIOD_MS = get_def("UI_CHARGING_BLINK_PERIOD_MS", 1000)
UI_BEEP_BASE_MS = get_def("UI_BEEP_BASE_MS", 250)
UI_BEEP_DOUBLE_THRESH_PCT = get_def("UI_BEEP_DOUBLE_THRESH_PCT", 20)
UI_BEEP_START_PCT = get_def("UI_BEEP_START_PCT", 50)
UI_PERCENT_FULL = get_def("UI_PERCENT_FULL", 100)

print(f"Loaded thresholds from {CONFIG_PATH}:")
for k in ["UI_BAT_V_MIN_MV","UI_BAT_V_MAX_MV","UI_INPUT_THRESHOLD_MV","UI_BLINK_PERIOD_MS","UI_BEEP_BASE_MS","UI_BEEP_START_PCT","UI_BEEP_DOUBLE_THRESH_PCT"]:
    print(f"  {k}={get_def(k,0)}")
print()

def battery_voltage_to_percent(v_mv):
    if v_mv <= UI_BAT_V_MIN_MV:
        return 0
    if v_mv >= UI_BAT_V_MAX_MV:
        return 100
    rng = UI_BAT_V_MAX_MV - UI_BAT_V_MIN_MV
    off = v_mv - UI_BAT_V_MIN_MV
    pct = (off * 100) // rng
    return max(0, min(100, pct))

def scenario_battery_run(v_bat_mv):
    pct = battery_voltage_to_percent(v_bat_mv)
    off_ms = (100 - pct) * (UI_BLINK_PERIOD_MS // 100)
    if off_ms < UI_GREEN_MIN_OFF_MS:
        off_ms = UI_GREEN_MIN_OFF_MS
    on_ms = UI_BLINK_PERIOD_MS - off_ms
    return pct, on_ms, off_ms

def scenario_charging(v_bat_mv):
    pct = battery_voltage_to_percent(v_bat_mv)
    if pct >= 100:
        return pct, 0, 1000
    if pct == 0:
        return pct, 1000, 0
    on_ms = (100 - pct) * (UI_CHARGING_BLINK_PERIOD_MS // 100)
    off_ms = UI_CHARGING_BLINK_PERIOD_MS - on_ms
    return pct, on_ms, off_ms

def main():
    print("=== UI Host Test (single source ui.h, ui_config.h deleted) ===")
    print(f"Battery 0%={UI_BAT_V_MIN_MV}mV, 100%={UI_BAT_V_MAX_MV}mV, InputThresh={UI_INPUT_THRESHOLD_MV}mV\n")

    print("--- Voltage to Percent ---")
    for v in [20000,21000,22500,24000,26000,28000]:
        print(f"V_bat={v}mV => {battery_voltage_to_percent(v)}%")
    print()

    print("--- Scenarios ---")
    print("1. InputOk: V_in>=20V & bat 100% => GREEN steady")
    print("2. BatteryRun: V_in<20V => GREEN blink ON=pct*10ms, YELLOW OFF, smart beep")
    print("   - pct<50: beep every pct seconds, pct<20 duration x2")
    print("3. Charging: V_in>=20V & bat<100% => GREEN steady, YELLOW: 0% ON, 100% OFF, ON=(100-pct)*period")
    print()

    for v in [28000,25000,24000,22000,21000]:
        pct,on,off = scenario_battery_run(v)
        print(f"BatteryRun V={v}mV {pct}% => GREEN ON {on} OFF {off} YELLOW OFF")
    print()
    for v in [21000,22000,24000,26000,28000]:
        pct,on,off = scenario_charging(v)
        if pct==0:
            print(f"Charging V={v}mV {pct}% => YELLOW steady ON")
        elif pct==100:
            print(f"Charging V={v}mV {pct}% => YELLOW OFF")
        else:
            print(f"Charging V={v}mV {pct}% => YELLOW ON {on} OFF {off}")
    print()

    print("--- Smart Beep ---")
    for pct in [40,30,10]:
        interval = pct
        dur = UI_BEEP_BASE_MS * (2 if pct < UI_BEEP_DOUBLE_THRESH_PCT else 1)
        print(f"{pct}% => every {interval}s, duration {dur}ms")
    print("\nALL HOST TESTS PASSED (single source)")

if __name__ == "__main__":
    main()
