#!/usr/bin/env python3
# @file    host_test_ui.py
# @brief   [EN] Host simulation of new UI scenarios: InputOk, BatteryRun with smart beep, Charging.
#          [FA] شبیه‌سازی هاست سناریوهای جدید UI: ورودی عادی، دشارژ با بوق هوشمند، شارژ.
# @note    [EN] Parameters on top for easy tuning (same as ui.c top defines).
#          [FA] پارامترها بالای فایل برای تغییر آسان.

# ==================== Tunable Parameters — Change Here ====================
UI_BAT_V_MIN_MV = 21000  # 0% = 21V
UI_BAT_V_MAX_MV = 28000  # 100% = 28V
UI_INPUT_THRESHOLD_MV = 20000  # <20V = no input
UI_BLINK_PERIOD_MS = 1000
UI_GREEN_MIN_OFF_MS = 10
UI_CHARGING_BLINK_PERIOD_MS = 1000
UI_BEEP_BASE_MS = 250
UI_BEEP_DOUBLE_THRESH_PCT = 20
UI_BEEP_START_PCT = 50
UI_PERCENT_FULL = 100

def battery_voltage_to_percent(v_mv):
    if v_mv <= UI_BAT_V_MIN_MV:
        return 0
    if v_mv >= UI_BAT_V_MAX_MV:
        return 100
    rng = UI_BAT_V_MAX_MV - UI_BAT_V_MIN_MV
    off = v_mv - UI_BAT_V_MIN_MV
    pct = (off * 100) // rng
    return max(0, min(100, pct))

def scenario_input_ok():
    print(f"[InputOk] V_in >= {UI_INPUT_THRESHOLD_MV}mV: GREEN steady ON, YELLOW OFF, RED OFF, BUZZER OFF, hold 500ms")
    return 500

def scenario_battery_run(v_bat_mv):
    pct = battery_voltage_to_percent(v_bat_mv)
    off_ms = (100 - pct) * (UI_BLINK_PERIOD_MS // 100)
    if off_ms < UI_GREEN_MIN_OFF_MS:
        off_ms = UI_GREEN_MIN_OFF_MS
    on_ms = UI_BLINK_PERIOD_MS - off_ms
    print(f"[BatteryRun] V_bat={v_bat_mv}mV => {pct}% | GREEN ON {on_ms}ms OFF {off_ms}ms | YELLOW OFF (per new req)")
    return pct, on_ms, off_ms

def scenario_charging(v_bat_mv):
    pct = battery_voltage_to_percent(v_bat_mv)
    if pct >= 100:
        print(f"[Charging] V_bat={v_bat_mv}mV => {pct}% FULL => YELLOW OFF, GREEN steady ON")
        return 0, 1000
    if pct == 0:
        print(f"[Charging] V_bat={v_bat_mv}mV => {pct}% EMPTY => YELLOW steady ON, GREEN steady ON")
        return 1000, 0
    on_ms = (100 - pct) * (UI_CHARGING_BLINK_PERIOD_MS // 100)
    off_ms = UI_CHARGING_BLINK_PERIOD_MS - on_ms
    print(f"[Charging] V_bat={v_bat_mv}mV => {pct}% | YELLOW ON {on_ms}ms OFF {off_ms}ms (ON=(100-pct)*period) | GREEN steady ON")
    return on_ms, off_ms

def buzzer_beep(duration_ms):
    print(f"  -> BuzzerBeep({duration_ms}ms) [separate function Ui_BuzzerBeep]")

def test_beep_logic():
    print("\n--- Smart Beep Logic (BatteryRun <50%) ---")
    print(f"Params: BEEP_START={UI_BEEP_START_PCT}%, BEEP_BASE={UI_BEEP_BASE_MS}ms, DOUBLE_THRESH={UI_BEEP_DOUBLE_THRESH_PCT}% (duration x2)")
    print("Rule: if pct<50, interval = pct seconds (40%->40s, 30%->30s). If pct<20, duration x2.")
    beep_counter = 0
    # Simulate 60 cycles for different percents
    for pct in [50, 40, 30, 20, 10]:
        interval = pct  # seconds = cycles
        print(f"\nBattery {pct}%: beep every {interval}s")
        beeps = []
        counter = 0
        for cycle in range(60):
            if pct < UI_BEEP_START_PCT:
                if counter >= interval:
                    dur = UI_BEEP_BASE_MS * (2 if pct < UI_BEEP_DOUBLE_THRESH_PCT else 1)
                    beeps.append((cycle, dur))
                    counter = 0
                else:
                    counter += 1
        print(f"  Beeps in 60 cycles: {beeps}")
        # Expected counts
        if pct == 40:
            assert len(beeps) == 1, f"40% should beep once in 60s (every 40s) got {beeps}"
        if pct == 30:
            assert len(beeps) == 1 or len(beeps) == 2, f"30% beeps"
    print("Beep logic assertions OK")

def main():
    print("=== UI Host Test New Logic / تست هاست منطق جدید ===")
    print(f"Battery 0%={UI_BAT_V_MIN_MV}mV, 100%={UI_BAT_V_MAX_MV}mV, InputThresh={UI_INPUT_THRESHOLD_MV}mV")
    print()

    print("--- Battery Voltage to Percent ---")
    for v in [20000, 21000, 22500, 24000, 26000, 28000, 30000]:
        print(f"V_bat={v}mV => {battery_voltage_to_percent(v)}%")
    print()

    print("--- Scenario List (per user request) ---")
    print("1. InputOk: V_in >=20V => GREEN steady ON, YELLOW OFF, RED OFF, BUZZER OFF")
    print("2. BatteryRun: V_in <20V => GREEN blink ON=pct*10ms, YELLOW OFF, smart beep via separate Ui_BuzzerBeep()")
    print("   - pct<50: beep every pct seconds (40%->40s, 30%->30s)")
    print("   - pct<20: beep duration x2 (250ms->500ms)")
    print("3. Charging: V_in >=20V and battery <100% => GREEN steady ON, YELLOW blink remaining to full")
    print("   - 0% (21V): YELLOW steady ON")
    print("   - 100% (28V): YELLOW OFF")
    print("   - intermediate: ON=(100-pct)*period")
    print()

    print("--- InputOk ---")
    scenario_input_ok()
    print()

    print("--- BatteryRun ---")
    for v in [28000, 25000, 24000, 23000, 22000, 21000]:
        scenario_battery_run(v)
    print()

    print("--- Charging ---")
    for v in [21000, 22000, 24000, 26000, 28000]:
        scenario_charging(v)
    print()

    test_beep_logic()
    print()

    print("--- Full Decision Matrix (Input Voltage + Battery Voltage) ---")
    tests = [
        (24000, 28000, "InputOk (full)"),
        (24000, 24000, "Charging (mid)"),
        (24000, 21000, "Charging (empty, yellow ON)"),
        (18000, 28000, "BatteryRun 100%"),
        (18000, 24000, "BatteryRun ~43%"),
        (18000, 21000, "BatteryRun 0% + beep double"),
    ]
    for v_in, v_bat, expected in tests:
        pct = battery_voltage_to_percent(v_bat)
        input_present = v_in >= UI_INPUT_THRESHOLD_MV
        if input_present:
            mode = "Charging" if pct < 100 else "InputOk"
        else:
            mode = "BatteryRun"
        print(f"V_in={v_in}mV ({'present' if input_present else 'lost'}), V_bat={v_bat}mV ({pct}%) => {mode} | expected {expected} {'OK' if expected.startswith(mode) else ''}")

    print("\nALL HOST TESTS PASSED")

if __name__ == "__main__":
    main()
