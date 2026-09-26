#!/usr/bin/env python3
# @file    host_test_ui.py
# @brief   [EN] Host tests for UI buzzer, BatteryRun 2%+0/1, Charging 5%, InputOk 100/95 and phase preserve.
#          [FA] تست هاست بوق، هیسترزیس ۲٪ BatteryRun + ۵٪ Charging و ۱۰۰/۹۵ فول.

import os, re

BASE_DIR = os.path.dirname(__file__)
BUZZER_HEADER = os.path.join(BASE_DIR, "ui_buzzer.h")
LED_HEADER = os.path.join(BASE_DIR, "ui_led.h")
defines={}
for hdr in [BUZZER_HEADER, LED_HEADER]:
    try:
        with open(hdr, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                m=re.match(r"#define\s+(\w+)\s+\(?(-?\d+)\)?u?", line)
                if m:
                    defines[m.group(1)] = int(m.group(2))
    except: pass

UI_BUZZER_PERCENT_SCALE = defines.get("UI_BUZZER_PERCENT_SCALE",100)
UI_BUZZER_DUTY_MAX_PERCENT = defines.get("UI_BUZZER_DUTY_MAX_PERCENT",100)
UI_BUZZER_MIN_PERIOD_MS = defines.get("UI_BUZZER_MIN_PERIOD_MS",1000)
UI_BUZZER_MIN_GAP_MS = defines.get("UI_BUZZER_MIN_GAP_MS",100)
UI_BUZZER_CHECK_PERCENT = defines.get("UI_BUZZER_CHECK_PERCENT",10)
UI_BUZZER_MIN_CHECK_MS = defines.get("UI_BUZZER_MIN_CHECK_MS",1)
UI_BUZZER_OFF_RESULT = defines.get("UI_BUZZER_OFF_RESULT",0)
UI_BUZZER_INVALID_RESULT = defines.get("UI_BUZZER_INVALID_RESULT",-1)

UI_BAT_V_MIN_MV = defines.get("UI_BAT_V_MIN_MV",21000)
UI_BAT_V_MAX_MV = defines.get("UI_BAT_V_MAX_MV",28000)
UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT = defines.get("UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT", defines.get("UI_BATTERY_PERCENT_HYSTERESIS_PERCENT",2))
UI_BATTERY_PERCENT_HYSTERESIS_PERCENT = UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT
UI_BATTERY_ZERO_EXIT_THRESHOLD = defines.get("UI_BATTERY_ZERO_EXIT_THRESHOLD",2)
UI_BATTERY_ONE_EXIT_THRESHOLD = defines.get("UI_BATTERY_ONE_EXIT_THRESHOLD",3)
UI_CHARGING_PERCENT_HYSTERESIS_PERCENT = defines.get("UI_CHARGING_PERCENT_HYSTERESIS_PERCENT",5)
UI_CHARGING_FULL_ENTER_PERCENT = defines.get("UI_CHARGING_FULL_ENTER_PERCENT",100)
UI_CHARGING_FULL_EXIT_PERCENT = defines.get("UI_CHARGING_FULL_EXIT_PERCENT",95)
UI_PERCENT_FULL = defines.get("UI_PERCENT_FULL",100)
UI_PERCENT_SCALE = defines.get("UI_PERCENT_SCALE",100)
UI_BLINK_PERIOD_MS = defines.get("UI_BLINK_PERIOD_MS",1000)
UI_GREEN_MIN_OFF_MS = defines.get("UI_GREEN_MIN_OFF_MS",10)
UI_CHARGING_BLINK_PERIOD_MS = defines.get("UI_CHARGING_BLINK_PERIOD_MS",1000)
UI_CHARGING_YELLOW_MIN_OFF_MS = defines.get("UI_CHARGING_YELLOW_MIN_OFF_MS",10)

def calculate_pattern(period_ms, duty_percent, beep_count, gap_ms):
    if period_ms <=0 or duty_percent<=0 or beep_count<=0: return None
    if period_ms < UI_BUZZER_MIN_PERIOD_MS: return None
    if duty_percent > UI_BUZZER_DUTY_MAX_PERCENT: return None
    if beep_count>1 and gap_ms < UI_BUZZER_MIN_GAP_MS: return None
    effective_gap= gap_ms if beep_count>1 else 0
    duty_window = (period_ms * duty_percent)//UI_BUZZER_PERCENT_SCALE
    total_gap = effective_gap*(beep_count-1)
    if duty_window <= total_gap: return None
    available = duty_window - total_gap
    if available < beep_count: return None
    beep_on = available//beep_count
    rem = available % beep_count
    durs=[beep_on]*beep_count
    durs[-1]+=rem
    tail = period_ms - duty_window
    return duty_window, durs, effective_gap, tail

def buzzer_level_at(period_ms,duty_percent,beep_count,gap_ms,elapsed_ms):
    p=calculate_pattern(period_ms,duty_percent,beep_count,gap_ms)
    if p is None: return False
    duty_window, beep_durs, eff_gap, _ = p
    ce= elapsed_ms % period_ms
    if ce >= duty_window: return False
    cursor=0
    for i,d in enumerate(beep_durs):
        if ce < cursor+d: return True
        cursor+=d
        if i< beep_count-1:
            if ce < cursor+eff_gap: return False
            cursor+=eff_gap
    return False

def sample_waveform_segments(period_ms,duty_percent,beep_count,gap_ms):
    segs=[]
    prev=buzzer_level_at(period_ms,duty_percent,beep_count,gap_ms,0)
    l=0
    for e in range(period_ms):
        cur=buzzer_level_at(period_ms,duty_percent,beep_count,gap_ms,e)
        if cur!=prev:
            segs.append((prev,l))
            prev=cur
            l=0
        l+=1
    segs.append((prev,l))
    return segs

def calculate_next_check_ms(period_ms,duty_percent,beep_count,gap_ms):
    if period_ms<=0 or duty_percent<=0 or beep_count<=0: return UI_BUZZER_OFF_RESULT
    p=calculate_pattern(period_ms,duty_percent,beep_count,gap_ms)
    if p is None: return UI_BUZZER_INVALID_RESULT
    _, beep_durs, eff_gap, tail = p
    segs=list(beep_durs)
    if eff_gap>0: segs.append(eff_gap)
    if tail>0: segs.append(tail)
    smallest=min(segs)
    nxt=(smallest*UI_BUZZER_CHECK_PERCENT)//UI_BUZZER_PERCENT_SCALE
    return max(nxt, UI_BUZZER_MIN_CHECK_MS)

def calculate_scenario_one_shot(duration_ms):
    if duration_ms<=0: return None
    period_ms=max(duration_ms, UI_BUZZER_MIN_PERIOD_MS)
    if duration_ms >= period_ms: duty=UI_BUZZER_DUTY_MAX_PERCENT
    else:
        duty=(duration_ms*UI_BUZZER_PERCENT_SCALE + period_ms-1)//period_ms
        duty=max(duty,1)
    return calculate_pattern(period_ms,duty,1,0)

def battery_voltage_to_percent(vbat_mv):
    if vbat_mv <= UI_BAT_V_MIN_MV: return 0
    if vbat_mv >= UI_BAT_V_MAX_MV: return UI_PERCENT_FULL
    rng=UI_BAT_V_MAX_MV-UI_BAT_V_MIN_MV
    off=vbat_mv-UI_BAT_V_MIN_MV
    if rng==0: return 0
    scaled=off*UI_PERCENT_SCALE
    pct=scaled//rng
    if pct>UI_PERCENT_FULL: pct=UI_PERCENT_FULL
    return int(pct)

class BatteryRunStable:
    def __init__(self):
        self.stable=0
        self.init=False
    def reset(self): self.stable=0; self.init=False
    def update(self, raw):
        if not self.init:
            self.stable=raw; self.init=True; return self.stable
        s=self.stable
        if s==0:
            if raw >= UI_BATTERY_ZERO_EXIT_THRESHOLD: s=1
        elif s==1:
            if raw==0: s=0
            elif raw >= UI_BATTERY_ONE_EXIT_THRESHOLD: s=2
        else:
            diff=abs(int(raw)-int(s))
            if diff >= UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT: s=raw
        self.stable=s; return s

class ChargingStable:
    def __init__(self):
        self.stable=0
        self.init=False
    def reset(self): self.stable=0; self.init=False
    def update(self, raw):
        if not self.init:
            self.stable=raw; self.init=True; return self.stable
        diff=abs(int(raw)-int(self.stable))
        if diff >= UI_CHARGING_PERCENT_HYSTERESIS_PERCENT:
            self.stable=raw
        return self.stable

class ChargingFull:
    def __init__(self):
        self.active=False
    def reset(self): self.active=False
    def update(self, raw):
        if self.active:
            if raw < UI_CHARGING_FULL_EXIT_PERCENT: self.active=False
        else:
            if raw >= UI_CHARGING_FULL_ENTER_PERCENT: self.active=True
        return self.active

def green_timing(stable, period=UI_BLINK_PERIOD_MS, min_off=UI_GREEN_MIN_OFF_MS):
    remaining=UI_PERCENT_FULL-stable
    per=period//UI_PERCENT_SCALE
    off=remaining*per
    if off<min_off: off=min_off
    if off>period: off=period
    on=period-off
    return on, off

def yellow_timing(stable, period=UI_CHARGING_BLINK_PERIOD_MS, min_off=UI_CHARGING_YELLOW_MIN_OFF_MS):
    # [EN] REMAINING to full drives the ON time (final user directive 2026-09-19):
    # more charged -> shorter ON; 95% charged (5 remaining) -> 50 ms per 1000 ms.
    # [FA] «مانده تا فول» زمان روشن بودن را می‌دهد: پرتر کوتاه‌تر؛ ۹۵٪ شارژ ⇒ ۵۰ms.
    remaining=UI_PERCENT_FULL-stable
    per=period//UI_PERCENT_SCALE
    on=remaining*per
    if on<min_off: on=min_off
    if on>period: on=period
    off=period-on
    return on, off

class BlinkPhase:
    def __init__(self):
        self.on=False; self.init=False; self.start=0; self.on_ms=0; self.off_ms=0
    def reset(self):
        self.on=False; self.init=False; self.start=0; self.on_ms=0; self.off_ms=0
    def update(self, on_ms, off_ms, now):
        if not self.init:
            self.init=True; self.on=True; self.start=now; self.on_ms=on_ms; self.off_ms=off_ms; return self.on
        if self.on_ms!=on_ms or self.off_ms!=off_ms:
            self.on_ms=on_ms; self.off_ms=off_ms; return self.on
        elapsed=now-self.start
        if self.on and elapsed>=self.on_ms:
            self.on=False; self.start=now
        elif not self.on and elapsed>=self.off_ms:
            self.on=True; self.start=now
        return self.on

def assert_equal(actual, expected, label):
    assert actual==expected, f"{label}: expected {expected}, got {actual}"
def assert_true(cond,label):
    assert cond, f"{label}: expected True"

def run_assertions():
    assert_equal(calculate_pattern(10000,10,2,100),(1000,[450,450],100,9000),"ten-second two-beep")
    assert_equal(calculate_next_check_ms(10000,10,2,100),10,"next check")
    assert_equal(sample_waveform_segments(10000,10,2,100),[(True,450),(False,100),(True,450),(False,9000)],"waveform")
    assert_equal(calculate_pattern(60000,2,1,0),(1200,[1200],0,58800),"one-beep 60s")
    assert_equal(calculate_pattern(60000,4,2,100),(2400,[1150,1150],100,57600),"two-beep 60s")
    assert_equal(calculate_pattern(20000,31,3,100),(6200,[2000,2000,2000],100,13800),"three-beep 20s")
    assert_equal(calculate_pattern(10000,100,1,0),(10000,[10000],0,0),"critical 10s")
    assert_equal(calculate_scenario_one_shot(150),(150,[150],0,850),"one-shot 150")
    assert_equal(calculate_scenario_one_shot(250),(250,[250],0,750),"one-shot 250")
    assert_equal(calculate_scenario_one_shot(500),(500,[500],0,500),"one-shot 500")
    assert_equal(calculate_pattern(1000,50,1,1),(500,[500],0,500),"single-beep ignore gap")
    assert_equal(calculate_next_check_ms(1000,50,1,1),50,"single-beep next")
    assert_equal(calculate_pattern(1000,60,3,100),(600,[133,133,134],100,400),"remainder")
    assert_equal(calculate_next_check_ms(0,10,2,100),UI_BUZZER_OFF_RESULT,"zero period")
    assert_equal(calculate_next_check_ms(1000,0,2,100),UI_BUZZER_OFF_RESULT,"zero duty")
    assert_equal(calculate_next_check_ms(1000,10,0,100),UI_BUZZER_OFF_RESULT,"zero count")
    assert_equal(calculate_next_check_ms(999,10,2,100),UI_BUZZER_INVALID_RESULT,"short period")
    assert_equal(calculate_next_check_ms(1000,50,2,99),UI_BUZZER_INVALID_RESULT,"short gap")
    assert_equal(calculate_next_check_ms(1000,10,3,100),UI_BUZZER_INVALID_RESULT,"gaps without pulse")
    assert_equal(calculate_next_check_ms(1000,101,1,100),UI_BUZZER_INVALID_RESULT,"duty >100")

def run_battery_hysteresis_tests():
    print("\n=== BatteryRun 2% hysteresis ===")
    s=BatteryRunStable()
    v25=battery_voltage_to_percent(25000)
    print(f"25V -> raw {v25} (expected 50 with the 21..29 V scale)")
    assert_true(v25 == 50, "25V 50: (25000-21000)*100/(29000-21000) with the 29 V ceiling")
    s.stable=57; s.init=True
    assert_equal(s.update(56),57,"57+56 keep")
    assert_equal(s.update(58),57,"57+58 keep")
    assert_equal(s.update(55),55,"57->55 change")
    s.stable=57; s.init=True
    assert_equal(s.update(59),59,"57->59 change")
    print("49/50/51 jitter preserved PASS")
    s.reset(); s.update(0)
    assert_equal(s.stable,0,"raw0 ->0")
    assert_equal(s.update(1),0,"stable0 raw1 keep0")
    assert_equal(s.update(2),1,"stable0 raw2->1")
    s.stable=1; s.init=True
    assert_equal(s.update(0),0,"stable1 raw0->0")
    s.stable=1; s.init=True
    assert_equal(s.update(2),1,"stable1 raw2 keep1")
    assert_equal(s.update(3),2,"stable1 raw3->2")
    s.stable=1; s.init=True
    assert_equal(s.update(1),1,"stable1 raw1 keep1")
    print("0/1 special PASS")
    s.reset(); s.update(v25)
    stable=s.stable
    on,off=green_timing(stable)
    print(f"Stable {stable} -> green on {on} off {off}")
    blink=BlinkPhase()
    blink.update(on,off,0)
    assert_true(blink.on==True,"green start ON")
    s.update(56); stable2=s.stable; on2,off2=green_timing(stable2)
    prev_on=blink.on; prev_start=blink.start
    blink.update(on2,off2,100)
    assert_equal(blink.on, prev_on,"phase preserved jitter 49")
    assert_equal(blink.start, prev_start,"start preserved jitter")
    s.update(55); stable3=s.stable; on3,off3=green_timing(stable3)
    prev_on=blink.on; prev_start=blink.start
    blink.update(on3,off3,200)
    assert_equal(blink.on, prev_on,"phase preserved 55")
    print("Green phase preserved PASS")
    s.reset(); s.update(0)
    assert_equal(s.stable,0,"critical 0")
    assert_equal(s.update(1),0,"0/1 noise keep0")
    assert_equal(s.update(0),0,"keep0")
    print("0/1 noise no restart PASS")
    s.stable=1; s.init=True
    assert_equal(s.update(1),1,"1% keep1")
    assert_equal(s.update(2),1,"1% raw2 keep1")
    assert_equal(s.update(3),2,"1% raw3->2")
    print("1% independent PASS")
    # 0% critical: ensure BatteryRun uses stable not raw for buzzer
    # If raw flickers 0->1, stable stays 0, so still critical
    s.reset(); s.update(0)
    assert_true(s.stable<1,"stable critical")
    s.update(1)
    assert_true(s.stable<1,"still critical after 0->1")
    print("Critical stable 0 persists on 1 jitter PASS")

def run_charging_hysteresis_tests():
    print("\n=== Charging 5% hysteresis ===")
    c=ChargingStable()
    c.stable=57; c.init=True
    assert_equal(c.update(53),57,"57+53 keep (diff4)")
    assert_equal(c.update(61),57,"57+61 keep (diff4)")
    assert_equal(c.update(56),57,"57+56 keep")
    assert_equal(c.update(58),57,"57+58 keep")
    assert_equal(c.update(52),52,"57->52 change diff5")
    c.stable=57; c.init=True
    assert_equal(c.update(62),62,"57->62 change")
    print("53..61 keep PASS, outside 5 change PASS")
    # [EN] Yellow ON must follow the REMAINING percent (final user directive
    # 2026-09-19): more charged -> shorter ON; 95% charged (5 remaining) ->
    # 50 ms ON per 1000 ms; 5% charged -> 950 ms ON.
    # [FA] زرد بر اساس «مانده»: ۹۵٪ شارژ ⇒ ۵۰ms روشن؛ ۵٪ شارژ ⇒ ۹۵۰ms روشن.
    assert_equal(yellow_timing(95), (50, 950), "yellow 95% charged -> 50ms on")
    assert_equal(yellow_timing(5), (950, 50), "yellow 5% charged -> 950ms on")
    assert_equal(yellow_timing(99), (10, 990), "yellow 99% charged -> min 10ms on")
    # yellow timing stable
    on57,off57=yellow_timing(57)
    on53,off53=yellow_timing(53) # diff but should not be used if stable 57
    # ensure timing would differ but is not applied due to hysteresis
    assert_true(on57!=on53,"timing diff would exist but hysteresis prevents")
    # verify after hysteresis change timing updates but phase preserved
    blink=BlinkPhase()
    on,off=yellow_timing(57)
    blink.update(on,off,0)
    # jitter 56 keeps stable 57, phase preserved
    c.stable=57; c.init=True
    c.update(56)
    assert_equal(c.stable,57,"charging 57+56 keep")
    on2,off2=yellow_timing(c.stable)
    prev_on=blink.on; prev_start=blink.start
    blink.update(on2,off2,100)
    assert_equal(blink.on, prev_on,"yellow phase preserved jitter")
    assert_equal(blink.start, prev_start,"yellow start preserved")
    # change to 62 -> stable 62, new timing from next boundary, phase still preserved at change moment
    c.update(62)
    on3,off3=yellow_timing(c.stable)
    prev_on=blink.on; prev_start=blink.start
    blink.update(on3,off3,200)
    assert_equal(blink.on, prev_on,"yellow phase preserved on real change")
    print("Charging yellow phase preserved PASS")
    # 25V jitter charging
    v25=battery_voltage_to_percent(25000)
    c.reset(); c.update(v25)
    assert_true(abs(c.stable - v25) <1, "charging init 25V")
    # small jitter should not change
    c.update(v25+1)
    assert_equal(c.stable, v25, "charging 25V jitter +1 keep")
    print("Charging 25V jitter PASS")

def run_full_hysteresis_tests():
    print("\n=== Charging Full hysteresis 100/95 ===")
    f=ChargingFull()
    # enter only at 100
    assert_equal(f.update(99),False,"99 -> Charging")
    assert_equal(f.update(100),True,"100 -> InputOk")
    assert_equal(f.update(99),True,"InputOk stays at 99")
    assert_equal(f.update(96),True,"stays at 96")
    assert_equal(f.update(95),True,"stays at 95")
    assert_equal(f.update(94),False,"94 -> Charging")
    # again
    assert_equal(f.update(100),True,"100 again -> InputOk")
    assert_equal(f.update(94),False,"94 -> Charging via hysteresis")
    print("Full 100/95 hysteresis PASS")
    # ensure charging vs InputOk decision respects input_present
    # Simulate Ui_Tick decision: InputPresent true + raw 99 + fullActive false -> Charging
    f.reset()
    f.update(99)
    assert_true(f.active==False,"full not active at 99 init")
    # After 100, active stays
    f.update(100)
    # now raw 98 while InputPresent should stay InputOk
    assert_equal(f.update(98),True,"stay InputOk at 98")
    print("Full decision PASS")

def run_batlost_tests():
    """[EN] Battery-lost scenario: LED/buzzer pattern math + source contracts.
       [FA] سناریوی قطع باتری: ریاضی الگو + قراردادهای سورس."""
    print("\n=== BatLost scenario (central fault flag) ===")
    ui_led_h = open(LED_HEADER, "r", encoding="utf-8", errors="ignore").read()
    ui_led_c = open(os.path.join(BASE_DIR, "ui_led.c"), "r", encoding="utf-8", errors="ignore").read()

    # [EN] Header must own the BatLost constants and prototype.
    # [FA] هدر باید ثابت‌ها و پروتوتایپ سناریو را داشته باشد.
    assert_true(re.search(r"#define UI_BAT_LOST_LED_PERIOD_MS\s+1000u", ui_led_h), "batlost LED period 1000 ms")
    assert_true(re.search(r"#define UI_BAT_LOST_LED_DUTY_PERCENT\s+50u", ui_led_h), "batlost LED duty 50%")
    assert_true(re.search(r"#define UI_BAT_LOST_BEEP_PERIOD_MS\s+3000u", ui_led_h), "batlost beep period 3000 ms")
    assert_true(re.search(r"#define UI_BAT_LOST_BEEP_DURATION_MS\s+900u", ui_led_h), "batlost beep window 900 ms")
    assert_true(re.search(r"#define UI_BAT_LOST_BEEP_COUNT\s+3u", ui_led_h), "batlost beep count 3")
    assert_true(re.search(r"#define UI_BAT_LOST_BEEP_GAP_MS\s+100u", ui_led_h), "batlost beep gap 100 ms")
    assert_true("void func__Ui_ScenarioBatLost_Tick(void);" in ui_led_h, "batlost prototype in header")

    # [EN] Pattern math must match the shared buzzer engine's expectations:
    #      duty = 900*100/3000 = 30 -> window 900, three 233/233/234 ms beeps,
    #      100 ms gaps, 2100 ms silence. Valid per min-period/min-gap rules.
    # [FA] ریاضی الگو باید با موتور بوق سازگار باشد.
    period = defines.get("UI_BAT_LOST_BEEP_PERIOD_MS", 3000)
    duration = defines.get("UI_BAT_LOST_BEEP_DURATION_MS", 900)
    count = defines.get("UI_BAT_LOST_BEEP_COUNT", 3)
    gap = defines.get("UI_BAT_LOST_BEEP_GAP_MS", 100)
    duty = (duration * UI_PERCENT_SCALE) // period
    assert_equal(duty, 30, "batlost beep duty percent")
    assert_equal(calculate_pattern(period, duty, count, gap), (900, [233, 233, 234], 100, 2100), "batlost 3-beep pattern")
    assert_true(period >= UI_BUZZER_MIN_PERIOD_MS, "batlost period >= min")
    assert_true(gap >= UI_BUZZER_MIN_GAP_MS, "batlost gap >= min")
    assert_true(calculate_next_check_ms(period, duty, count, gap) != UI_BUZZER_INVALID_RESULT, "batlost buzzer pattern valid")

    # [EN] Trigger: only the central fault bit, AFTER overvoltage, with return.
    # [FA] ماشه: فقط پرچم متمرکز، بعد از اضافه‌ولتاژ، با return.
    assert_true("func__Ui_ScenarioBatLost_Tick" in ui_led_c, "batlost scenario implemented")
    assert_true("func__Fault_Get() & FAULT_CHARGER_BAT_LOST" in ui_led_c, "batlost trigger reads the central fault bit")
    assert_true("func__Charger_IsAnyChannelActive()" in ui_led_c, "charging yellow must be gated by the charger being active (user directive)")
    assert_true('#include "charger.h"' in ui_led_c, "ui must include charger.h for the activity query")
    ov_idx = ui_led_c.find("func__Ui_ScenarioInputOverVoltage_Tick();\n        return;")
    bl_idx = ui_led_c.find("func__Ui_ScenarioBatLost_Tick();")
    assert_true(ov_idx != -1 and bl_idx != -1 and ov_idx < bl_idx, "batlost has priority right after overvoltage")
    print("BatLost scenario PASS")

UI_ALARM_DEFAULTS = {
    # id: (struct field, boot default) - must match the positional struct init in ui_led.c
    38: ("ovLedPeriodMs", 1000), 39: ("ovLedDutyPct", 50),
    40: ("ovBeepPeriodMs", 10000), 41: ("ovBeepDurMs", 1000),
    42: ("ovBeepCount", 1), 43: ("ovBeepGapMs", 0),
    44: ("blLedPeriodMs", 1000), 45: ("blLedDutyPct", 50),
    46: ("blBeepPeriodMs", 3000), 47: ("blBeepDurMs", 233),
    48: ("blBeepCount", 3), 49: ("blBeepGapMs", 100),
    50: ("runBeepStartPct", 40), 51: ("runBeepDoublePct", 20),
    52: ("runBeepTriplePct", 10), 53: ("runBeepCritPct", 1),
    54: ("runStdIntervalMs", 60000), 55: ("runTriIntervalMs", 20000),
    56: ("runCritPeriodMs", 10000), 57: ("runCritDutyPct", 100),
    58: ("runCritCount", 1), 59: ("runStdDurMs", 1000),
    60: ("runTriDurMs", 2000), 61: ("runCritDurMs", 10000),
    62: ("runStdCount", 1), 63: ("runDoubleCount", 2),
    64: ("runTriCount", 3), 65: ("runGapMs", 100),
    66: ("greenPeriodMs", 1000), 67: ("greenMinOffMs", 10),
    68: ("yellowPeriodMs", 1000), 69: ("yellowMinOffMs", 10),
    70: ("ovThreshMv", 28000), 71: ("ovHystMv", 1000),
    72: ("lowBatThreshMv", 21000), 73: ("lowBatClearMv", 21200),
    74: ("pctVminMv", 21000), 75: ("pctVmaxMv", 29000),
    76: ("buzzerMute", 0),
}

def _w(v, lo, hi): return min(hi, max(lo, v))
def _period(v): return 0 if v == 0 else _w(v, 1000, 600000)
def _maxdur(period, count, gap):
    if count == 0: return period
    gt = gap * (count - 1)
    if gt >= period: return 0
    return (period - gt) // count

def ui_clamp_mirror(s):
    """[EN] Exact single-pass mirror of func__Ui_ClampAlarms (same order).
       [FA] آینهٔ دقیق گیرهٔ C با همان ترتیب."""
    s = dict(s)
    s["ovLedPeriodMs"] = _w(s["ovLedPeriodMs"], 100, 10000)
    s["ovLedDutyPct"] = _w(s["ovLedDutyPct"], 0, 100)
    s["ovBeepPeriodMs"] = _period(s["ovBeepPeriodMs"])
    s["ovBeepCount"] = _w(s["ovBeepCount"], 0, 10)
    s["ovBeepGapMs"] = _w(s["ovBeepGapMs"], 0, 5000)
    if s["ovBeepCount"] > 1 and s["ovBeepPeriodMs"] != 0 and s["ovBeepGapMs"] < 100:
        s["ovBeepGapMs"] = 100
    s["ovBeepDurMs"] = _w(s["ovBeepDurMs"], 0, 600000)
    if s["ovBeepPeriodMs"] != 0:
        s["ovBeepDurMs"] = min(s["ovBeepDurMs"],
            _maxdur(s["ovBeepPeriodMs"], s["ovBeepCount"], s["ovBeepGapMs"]))
    s["blLedPeriodMs"] = _w(s["blLedPeriodMs"], 100, 10000)
    s["blLedDutyPct"] = _w(s["blLedDutyPct"], 0, 100)
    s["blBeepPeriodMs"] = _period(s["blBeepPeriodMs"])
    s["blBeepCount"] = _w(s["blBeepCount"], 0, 10)
    s["blBeepGapMs"] = _w(s["blBeepGapMs"], 0, 5000)
    if s["blBeepCount"] > 1 and s["blBeepPeriodMs"] != 0 and s["blBeepGapMs"] < 100:
        s["blBeepGapMs"] = 100
    s["blBeepDurMs"] = _w(s["blBeepDurMs"], 0, 600000)
    if s["blBeepPeriodMs"] != 0:
        s["blBeepDurMs"] = min(s["blBeepDurMs"],
            _maxdur(s["blBeepPeriodMs"], s["blBeepCount"], s["blBeepGapMs"]))
    for k in ["runBeepStartPct", "runBeepDoublePct", "runBeepTriplePct", "runBeepCritPct"]:
        s[k] = _w(s[k], 0, 100)
    if s["runBeepDoublePct"] > s["runBeepStartPct"]: s["runBeepDoublePct"] = s["runBeepStartPct"]
    if s["runBeepTriplePct"] > s["runBeepDoublePct"]: s["runBeepTriplePct"] = s["runBeepDoublePct"]
    if s["runBeepCritPct"] > s["runBeepTriplePct"]: s["runBeepCritPct"] = s["runBeepTriplePct"]
    s["runStdIntervalMs"] = _period(s["runStdIntervalMs"])
    s["runTriIntervalMs"] = _period(s["runTriIntervalMs"])
    s["runCritPeriodMs"] = _period(s["runCritPeriodMs"])
    s["runCritDutyPct"] = _w(s["runCritDutyPct"], 0, 100)
    for k in ["runCritCount", "runStdCount", "runDoubleCount", "runTriCount"]:
        s[k] = _w(s[k], 0, 10)
    s["runGapMs"] = _w(s["runGapMs"], 0, 5000)
    if any(s[k] > 1 for k in ["runCritCount", "runStdCount", "runDoubleCount", "runTriCount"]) \
            and s["runGapMs"] < 100:
        s["runGapMs"] = 100
    s["runStdDurMs"] = _w(s["runStdDurMs"], 0, 600000)
    if s["runStdIntervalMs"] != 0:
        s["runStdDurMs"] = min(s["runStdDurMs"], _maxdur(s["runStdIntervalMs"],
            max(s["runStdCount"], s["runDoubleCount"]), s["runGapMs"]))
    s["runTriDurMs"] = _w(s["runTriDurMs"], 0, 600000)
    if s["runTriIntervalMs"] != 0:
        s["runTriDurMs"] = min(s["runTriDurMs"], _maxdur(s["runTriIntervalMs"],
            s["runTriCount"], s["runGapMs"]))
    s["runCritDurMs"] = _w(s["runCritDurMs"], 0, 120000)
    s["greenPeriodMs"] = _w(s["greenPeriodMs"], 100, 10000)
    s["greenMinOffMs"] = _w(s["greenMinOffMs"], 0, 10000)
    if s["greenMinOffMs"] > s["greenPeriodMs"]: s["greenMinOffMs"] = s["greenPeriodMs"]
    s["yellowPeriodMs"] = _w(s["yellowPeriodMs"], 100, 10000)
    s["yellowMinOffMs"] = _w(s["yellowMinOffMs"], 0, 10000)
    if s["yellowMinOffMs"] > s["yellowPeriodMs"]: s["yellowMinOffMs"] = s["yellowPeriodMs"]
    s["ovThreshMv"] = _w(s["ovThreshMv"], 24000, 32000)
    s["ovHystMv"] = _w(s["ovHystMv"], 0, 2000)
    s["lowBatThreshMv"] = _w(s["lowBatThreshMv"], 15000, 24000)
    s["lowBatClearMv"] = _w(s["lowBatClearMv"], 15000, 24000)
    if s["lowBatClearMv"] < s["lowBatThreshMv"]: s["lowBatClearMv"] = s["lowBatThreshMv"]
    if s["lowBatThreshMv"] > s["lowBatClearMv"]: s["lowBatThreshMv"] = s["lowBatClearMv"]
    s["pctVminMv"] = _w(s["pctVminMv"], 15000, 25000)
    s["pctVmaxMv"] = _w(s["pctVmaxMv"], 25000, 32000)
    if s["pctVmaxMv"] < s["pctVminMv"] + 100: s["pctVmaxMv"] = s["pctVminMv"] + 100
    if s["pctVminMv"] > s["pctVmaxMv"] - 100: s["pctVminMv"] = s["pctVmaxMv"] - 100
    s["buzzerMute"] = _w(s["buzzerMute"], 0, 1)
    return s

def _ui_duty(period, dur, count, gap):
    if period == 0 or dur == 0 or count == 0: return 0
    window = dur * count + (gap * (count - 1) if count > 1 else 0)
    if window > period: return 101  # infeasible -> service INVALID (deterministic silence)
    return (window * 100 + period - 1) // period

def run_ui_alarm_tests():
    """[EN] v1.16: 39 runtime UI ids 38..76 - defines, boot defaults, exact
       legacy-sound equivalence, mute gate, read swaps, clamp invariants.
       [FA] تست شناسه‌های ۳۸..۷۶: دیفاین‌ها، دیفالت بوت، تطابق دقیق صدا،
       میوت، تعویض خوانش‌ها، نامتغیرهای گیره."""
    print("\n=== UI alarms v1.16 (ids 38..76) ===")
    ui_led_h = open(LED_HEADER, "r", encoding="utf-8", errors="ignore").read()
    ui_led_c = open(os.path.join(BASE_DIR, "ui_led.c"), "r", encoding="utf-8", errors="ignore").read()

    # --- ID defines: contiguous 38..76 + MIN/MAX ---
    ids = sorted(int(m.group(2)) for m in
                 re.finditer(r"#define\s+(UI_ALARM_PARAM_\w+)\s+\(?(\d+)\)?u?",
                             ui_led_h) if "MIN_ID" not in m.group(1) and "MAX_ID" not in m.group(1))
    assert_equal(ids, list(range(38, 77)), "UI alarm ids contiguous 38..76")
    assert_equal(defines.get("UI_ALARM_PARAM_MIN_ID"), 38, "MIN_ID 38")
    assert_equal(defines.get("UI_ALARM_PARAM_MAX_ID"), 76, "MAX_ID 76")
    assert_true("bool func__Ui_SetAlarmParam(uint8_t uint8_t__paramId," in ui_led_h, "Set prototype")
    assert_true("bool func__Ui_GetAlarmParam(uint8_t uint8_t__paramId," in ui_led_h, "Get prototype")

    # --- struct init order: positional init must list the 39 boot defaults in id order ---
    m = re.search(r"static ui_alarm_t UI_ALARM_T__G__Alarm =\n\{(.*?)\n\};", ui_led_c, re.S)
    assert_true(m, "alarm struct init found")
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    inits = [x.strip().rstrip(",").strip() for x in body.strip().split("\n")]
    inits = [x for x in inits if x]
    assert_equal(len(inits), 39, "39 init entries")
    expected_macros = ["UI_INPUT_OVERVOLTAGE_LED_PERIOD_MS", "UI_INPUT_OVERVOLTAGE_LED_DUTY_PERCENT",
        "UI_INPUT_OVERVOLTAGE_BEEP_PERIOD_MS", "UI_INPUT_OVERVOLTAGE_BEEP_DURATION_MS",
        "UI_INPUT_OVERVOLTAGE_BEEP_COUNT", "UI_INPUT_OVERVOLTAGE_BEEP_GAP_MS",
        "UI_BAT_LOST_LED_PERIOD_MS", "UI_BAT_LOST_LED_DUTY_PERCENT", "UI_BAT_LOST_BEEP_PERIOD_MS",
        "233u", "UI_BAT_LOST_BEEP_COUNT", "UI_BAT_LOST_BEEP_GAP_MS",
        "UI_BATTERY_RUN_BEEP_START_PERCENT", "UI_BATTERY_RUN_BEEP_DOUBLE_PERCENT",
        "UI_BATTERY_RUN_BEEP_TRIPLE_PERCENT", "UI_BATTERY_RUN_BEEP_CRITICAL_PERCENT",
        "UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS", "UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS",
        "UI_BATTERY_RUN_BEEP_CRITICAL_PERIOD_MS", "UI_BATTERY_RUN_BEEP_CRITICAL_DUTY_PERCENT",
        "UI_BATTERY_RUN_BEEP_CRITICAL_COUNT", "UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS",
        "UI_BATTERY_RUN_BEEP_TRIPLE_DURATION_MS", "UI_BATTERY_RUN_BEEP_CRITICAL_DURATION_MS",
        "UI_BATTERY_RUN_BEEP_STANDARD_COUNT", "UI_BATTERY_RUN_BEEP_DOUBLE_COUNT",
        "UI_BATTERY_RUN_BEEP_TRIPLE_COUNT", "UI_BATTERY_RUN_BEEP_GAP_MS",
        "UI_BLINK_PERIOD_MS", "UI_GREEN_MIN_OFF_MS", "UI_CHARGING_BLINK_PERIOD_MS",
        "UI_CHARGING_YELLOW_MIN_OFF_MS", "UI_INPUT_OVERVOLTAGE_THRESHOLD_MV",
        "UI_INPUT_OVERVOLTAGE_HYSTERESIS_MV", "UI_LOW_BATTERY_ALARM_THRESHOLD_MV",
        "UI_LOW_BATTERY_ALARM_CLEAR_MV", "UI_BAT_V_MIN_MV", "UI_BAT_V_MAX_MV", "0u"]
    assert_equal(inits, expected_macros, "init order == id order (positional!)")
    print("IDs + boot defaults PASS")

    # --- legacy-sound equivalence: ceil duty on defaults must equal the old DUTY macros ---
    assert_equal(_ui_duty(10000, 1000, 1, 0), 10, "OV duty 10 (legacy macro 10)")
    assert_equal(_ui_duty(3000, 233, 3, 100), 30, "BatLost duty 30 from 233 ms/beep")
    assert_equal(calculate_pattern(3000, 30, 3, 100), (900, [233, 233, 234], 100, 2100),
                 "BatLost default triple unchanged")
    assert_equal(_ui_duty(60000, 1000, 1, 100), 2, "standard duty 2 (legacy ceil macro)")
    assert_equal(_ui_duty(60000, 1000, 2, 100), 4, "double duty 4 (legacy ceil macro)")
    assert_equal(_ui_duty(20000, 2000, 3, 100), 31, "triple duty 31 (legacy ceil macro)")
    assert_equal(calculate_pattern(60000, 2, 1, 100), (1200, [1200], 0, 58800), "std 1200 ms beep")
    print("Legacy-sound equivalence PASS")

    # --- read swaps: scenarios read the live struct through the mute gate ---
    assert_equal(ui_led_c.count("func__Ui_Buzzer_Gated("), 12,
                 "11 scenario sites + 1 prototype-free def use the mute gate")
    # direct Tick calls left: 2 inside Gated + 1 all_off + 1 OV-clear explicit off
    # + 2 BoardTest (mute bypass, still proves the buzzer works at boot)
    assert_equal(ui_led_c.count("func__Ui_Buzzer_Tick("), 6,
                 "direct Tick only in Gated/all_off/OV-clear/BoardTest")
    assert_true("uint32_t__pctVminMv" in ui_led_c and "uint32_t__pctVmaxMv" in ui_led_c,
                "percent map reads live 74/75")
    assert_true("uint32_t__ovThreshMv" in ui_led_c and "uint32_t__lowBatThreshMv" in ui_led_c,
                "thresholds read live 70/72/73")
    assert_true("APP_CONFIG.ui_blink_period_ms" not in ui_led_c
                and "APP_CONFIG.ui_charging_blink_period_ms" not in ui_led_c,
                "no scenario reads blink periods from APP_CONFIG anymore")
    print("Read swaps + mute gate PASS")

    # --- clamp invariants: defaults no-op + 2000 random states incl. idempotence ---
    import random
    defs = {f: d for _, (f, d) in UI_ALARM_DEFAULTS.items()}
    assert_equal(ui_clamp_mirror(defs), defs, "clamp is a no-op on boot defaults")
    def check_inv(s, label):
        assert_true(100 <= s["ovLedPeriodMs"] <= 10000, label + " 38 window")
        assert_true(0 <= s["ovLedDutyPct"] <= 100, label + " 39 window")
        assert_true(s["ovBeepPeriodMs"] == 0 or 1000 <= s["ovBeepPeriodMs"] <= 600000, label + " 40")
        assert_true(100 <= s["blLedPeriodMs"] <= 10000, label + " 44 window")
        assert_true(s["blBeepPeriodMs"] == 0 or 1000 <= s["blBeepPeriodMs"] <= 600000, label + " 46")
        assert_true(s["runBeepStartPct"] >= s["runBeepDoublePct"] >= s["runBeepTriplePct"]
                    >= s["runBeepCritPct"], label + " bands ordered")
        for k in ["runStdIntervalMs", "runTriIntervalMs", "runCritPeriodMs"]:
            assert_true(s[k] == 0 or 1000 <= s[k] <= 600000, label + " " + k)
        assert_true(0 <= s["runCritDutyPct"] <= 100, label + " 57")
        for k in ["runCritCount", "runStdCount", "runDoubleCount", "runTriCount",
                  "ovBeepCount", "blBeepCount"]:
            assert_true(0 <= s[k] <= 10, label + " " + k)
        for k in ["ovBeepGapMs", "blBeepGapMs", "runGapMs"]:
            assert_true(0 <= s[k] <= 5000, label + " " + k)
        if s["ovBeepCount"] > 1 and s["ovBeepPeriodMs"] != 0:
            assert_true(s["ovBeepGapMs"] >= 100, label + " OV gap rule")
        if s["blBeepCount"] > 1 and s["blBeepPeriodMs"] != 0:
            assert_true(s["blBeepGapMs"] >= 100, label + " BL gap rule")
        if any(s[k] > 1 for k in ["runCritCount", "runStdCount", "runDoubleCount", "runTriCount"]):
            assert_true(s["runGapMs"] >= 100, label + " run gap rule")
        for (dur, per, cnt, gap) in [("ovBeepDurMs", "ovBeepPeriodMs", "ovBeepCount", "ovBeepGapMs"),
                                     ("blBeepDurMs", "blBeepPeriodMs", "blBeepCount", "blBeepGapMs"),
                                     ("runStdDurMs", "runStdIntervalMs", "runStdCount", "runGapMs"),
                                     ("runStdDurMs", "runStdIntervalMs", "runDoubleCount", "runGapMs"),
                                     ("runTriDurMs", "runTriIntervalMs", "runTriCount", "runGapMs")]:
            d = _ui_duty(s[per], s[dur], s[cnt], s[gap])
            window = s[dur] * s[cnt] + (s[gap] * (s[cnt] - 1) if s[cnt] > 1 else 0)
            # sounding (1..100) exactly when period active + dur/count live + window fits
            expect_sound = (s[per] != 0 and s[dur] > 0 and s[cnt] > 0 and window <= s[per])
            assert_true((1 <= d <= 100) == expect_sound, label + " " + dur + " sound iff fits")
            if 1 <= d <= 100:
                assert_true(calculate_pattern(s[per], d, s[cnt], s[gap]) is not None,
                            label + " " + dur + " service-valid")
        assert_true(100 <= s["greenPeriodMs"] <= 10000 and s["greenMinOffMs"] <= s["greenPeriodMs"],
                    label + " green")
        assert_true(100 <= s["yellowPeriodMs"] <= 10000 and s["yellowMinOffMs"] <= s["yellowPeriodMs"],
                    label + " yellow")
        assert_true(24000 <= s["ovThreshMv"] <= 32000 and 0 <= s["ovHystMv"] <= 2000
                    and s["ovHystMv"] < s["ovThreshMv"], label + " OV thresh (no underflow)")
        assert_true(s["lowBatThreshMv"] <= s["lowBatClearMv"], label + " lowbat order")
        assert_true(s["pctVmaxMv"] >= s["pctVminMv"] + 100, label + " pct range strictly positive")
        assert_true(s["buzzerMute"] in (0, 1), label + " mute 0/1")
    random.seed(1616)
    fields = list(defs.keys())
    for trial in range(2000):
        r = {}
        for f in fields:
            if "Pct" in f or "Duty" in f: r[f] = random.randint(0, 150)
            elif "Count" in f or "Mute" in f or "mute" in f: r[f] = random.randint(0, 15)
            elif "Mv" in f: r[f] = random.randint(0, 40000)
            else: r[f] = random.randint(0, 700000)
        c1 = ui_clamp_mirror(r)
        check_inv(c1, f"trial {trial}")
        assert_equal(ui_clamp_mirror(c1), c1, f"clamp idempotent trial {trial}")
    print("Clamp invariants + idempotence (2000 random) PASS")

def main():
    print("=== UI Host Test (buzzer + BatteryRun 2%+0/1 + Charging 5% + Full 100/95 + phase) ===")
    run_assertions()
    print("ALL BUZZER TESTS PASSED")
    run_ui_alarm_tests()
    run_batlost_tests()
    run_battery_hysteresis_tests()
    run_charging_hysteresis_tests()
    run_full_hysteresis_tests()
    print("\n=== Additional checks ===")
    assert_equal(battery_voltage_to_percent(21000),0,"21000->0%")
    assert_equal(battery_voltage_to_percent(29000),100,"29000->100% (charging yellow blink ceiling raised 28->29 V, user order 2026-09-20)")
    assert_equal(battery_voltage_to_percent(28000),87,"28000->87% now: (28000-21000)*100/(29000-21000) since the 100% ceiling moved 28->29 V")
    assert_equal(battery_voltage_to_percent(24500),43,"24500->43% now: (24500-21000)*100/(29000-21000) with the 29 V ceiling")
    print("Voltage mapping PASS")
    on,off=green_timing(57)
    assert_true(on+off==UI_BLINK_PERIOD_MS,"green on+off period")
    assert_true(off>=UI_GREEN_MIN_OFF_MS,"green off min")
    print(f"57% green on {on} off {off} PASS")
    onY,offY=yellow_timing(57)
    assert_true(onY+offY==UI_CHARGING_BLINK_PERIOD_MS,"yellow on+off period")
    print(f"57% yellow on {onY} off {offY} PASS")
    print("\nALL HOST TESTS PASSED (buzzer + BatteryRun 2%+0/1 + Charging 5% + Full 100/95 + phase)")
if __name__=="__main__":
    main()
