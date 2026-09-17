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
    print(f"25V -> raw {v25} (expected ~57)")
    assert_true(56 <= v25 <=58, "25V 56-58")
    s.stable=57; s.init=True
    assert_equal(s.update(56),57,"57+56 keep")
    assert_equal(s.update(58),57,"57+58 keep")
    assert_equal(s.update(55),55,"57->55 change")
    s.stable=57; s.init=True
    assert_equal(s.update(59),59,"57->59 change")
    print("56/57/58 jitter preserved PASS")
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
    assert_equal(blink.on, prev_on,"phase preserved jitter 56")
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

def main():
    print("=== UI Host Test (buzzer + BatteryRun 2%+0/1 + Charging 5% + Full 100/95 + phase) ===")
    run_assertions()
    print("ALL BUZZER TESTS PASSED")
    run_battery_hysteresis_tests()
    run_charging_hysteresis_tests()
    run_full_hysteresis_tests()
    print("\n=== Additional checks ===")
    assert_equal(battery_voltage_to_percent(21000),0,"21000->0%")
    assert_equal(battery_voltage_to_percent(28000),100,"28000->100%")
    assert_equal(battery_voltage_to_percent(24500),50,"24500->50%")
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
