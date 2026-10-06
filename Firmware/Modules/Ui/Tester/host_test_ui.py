#!/usr/bin/env python3
# @file    host_test_ui.py
# @brief   [EN] Host tests for UI buzzer, BatteryRun 2%+0/1, Charging 5%, InputOk 100/95 and phase preserve.
#          [FA] تست هاست بوق، هیسترزیس ۲٪ BatteryRun + ۵٪ Charging و ۱۰۰/۹۵ فول.

import os, re

# [EN] v1.25: this script now lives in the module's Tester/ folder, so the
#      headers it reads are one level UP, in the module root.
# [FA] نسخهٔ ۱.۲۵: این اسکریپت حالا در پوشهٔ Tester ماژول است، پس هدرهایی
#      که می‌خواند یک سطح بالاتر، در ریشهٔ ماژول‌اند.
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
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
UI_CHARGING_YELLOW_MIN_ON_MS = defines.get("UI_CHARGING_YELLOW_MIN_ON_MS",150)
UI_CHARGING_YELLOW_MIN_REMAINING_PERCENT = defines.get("UI_CHARGING_YELLOW_MIN_REMAINING_PERCENT",2)

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

def yellow_timing(stable, period=UI_CHARGING_BLINK_PERIOD_MS, min_on=UI_CHARGING_YELLOW_MIN_ON_MS):
    # [EN] REMAINING to full drives the ON time (final user directive 2026-09-19):
    # more charged -> shorter ON; 95% charged (5 remaining) -> 50 ms per 1000 ms.
    # v1.20 (user order 2026-09-28): remaining is floored at
    # UI_CHARGING_YELLOW_MIN_REMAINING_PERCENT and stable>=100 no longer darks
    # the yellow (this scenario only runs while a charger channel pumps).
    # [FA] «مانده تا فول» زمان روشن بودن را می‌دهد؛ کف ۲٪ و بدون خاموشی در ۱۰۰٪
    # از v1.20 (دستور کاربر ۲۰۲۶-۰۹-۲۸).
    if stable >= UI_PERCENT_FULL:
        remaining = 0
    else:
        remaining = UI_PERCENT_FULL - stable
    if remaining < UI_CHARGING_YELLOW_MIN_REMAINING_PERCENT:
        remaining = UI_CHARGING_YELLOW_MIN_REMAINING_PERCENT
    per=period//UI_PERCENT_SCALE
    on=remaining*per
    if on<min_on: on=min_on
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
    # 2026-09-19): more charged -> shorter ON; 5% charged -> 950 ms ON.
    # v1.17: the 150 ms floor (id 69, was 10) keeps the nearly-full blink
    # visible - 95% charged (5 remaining) -> 150 ms, not 50 ms.
    # [FA] زرد بر اساس «مانده»: ۵٪ شارژ ⇒ ۹۵۰ms روشن؛ کف ۱۵۰ms از v1.17.
    assert_equal(yellow_timing(95), (150, 850), "yellow 95% charged -> 150ms floor (v1.17)")
    assert_equal(yellow_timing(5), (950, 50), "yellow 5% charged -> 950ms on")
    assert_equal(yellow_timing(99), (150, 850), "yellow 99% charged -> min 150ms on")
    # [EN] v1.20 (user order 2026-09-28: "even at 1% one blink; say it never
    # goes below 2%; fully dark only when the charger is cut"):
    # - the remaining floor is 2% -> raw on-time 20 ms per 1000 ms period
    # - the min-on clamp (id 69, default 150) then keeps the blip visible
    # - a 100% reading mid-charge keeps blinking (no early dark) because
    #   this scenario only runs while a charger channel is pumping.
    # [FA] نسخهٔ ۱٫۲۰: کف مانده ۲٪ ⇒ ۲۰ms خام در دوره ۱۰۰۰ms، کف ۱۵۰ms مرئی
    # می‌کند و درصد ۱۰۰ وسط شارژ دیگر زرد را خاموش نمی‌کند.
    assert_equal(yellow_timing(100, min_on=0), (20, 980), "v1.20 100% mid-pump keeps the 2% blink, no dark")
    assert_equal(yellow_timing(99, min_on=0), (20, 980), "v1.20 99% -> 2% remaining floor")
    assert_equal(yellow_timing(98, min_on=0), (20, 980), "v1.20 98% -> 2% remaining floor")
    assert_equal(yellow_timing(97, min_on=0), (30, 970), "v1.20 97% -> 3% remaining, no floor")
    assert_equal(yellow_timing(100), (150, 850), "v1.20 100% mid-pump -> visible 150ms blip")
    # selection mirror (v1.20): the full face yields while any channel pumps
    def v120_face(full_active, complete, any_active):
        if complete: return "InputOk"
        if full_active and not any_active: return "InputOk"
        if any_active: return "Charging"
        return "InputOk"
    assert_equal(v120_face(True, False, True), "Charging", "v1.20 full-latch yields while pumping")
    assert_equal(v120_face(True, False, False), "InputOk", "v1.20 full-latch decides when nothing pumps")
    assert_equal(v120_face(False, True, True), "InputOk", "v1.20 charge complete stays the primary full path")
    ui_led_c_v120 = open(os.path.join(BASE_DIR, "ui_led.c"), "r", encoding="utf-8", errors="ignore").read()
    assert_true("UI_CHARGING_YELLOW_MIN_REMAINING_PERCENT" in ui_led_c_v120,
                "v1.20 remaining floor constant used in ui_led.c")
    assert_true(re.search(r"else if \(func__Charger_IsAnyChannelActive\(\) == true\)", ui_led_c_v120),
                "v1.20 full face yields while a channel pumps (selection)")
    print("Charging v1.20 yellow floor + no-mid-pump-dark PASS")
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
    ui_tick_idx = ui_led_c.find("void func__Ui_Tick")
    bl_idx = ui_led_c.find("func__Ui_ScenarioBatLost_Tick();", ui_tick_idx)
    assert_true(ov_idx != -1 and bl_idx != -1 and ov_idx < bl_idx, "batlost has priority right after overvoltage")
    run_idx = ui_led_c.find("void func__Ui_ScenarioBatteryRun_Tick")
    run_end = ui_led_c.find("/* ==================== Ui Tick", run_idx)
    run_body = ui_led_c[run_idx:run_end]
    assert_true("func__Fault_Get() & FAULT_CHARGER_BAT_LOST" in run_body and
                "func__Ui_ScenarioBatLost_Tick();" in run_body,
                "BatteryRun cannot start its critical beep while BatLost is latched")
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
    56: ("runCritPeriodMs", 10000), 57: ("runCritBeepDurMs", 10000),
    58: ("runCritCount", 1), 59: ("runStdDurMs", 1000),
    60: ("runTriDurMs", 2000), 61: ("runCritDurMs", 10000),
    62: ("runStdCount", 1), 63: ("runDoubleCount", 2),
    64: ("runTriCount", 3), 65: ("runGapMs", 100),
    66: ("greenPeriodMs", 1000), 67: ("greenMinOffMs", 10),
    68: ("yellowPeriodMs", 1000), 69: ("yellowMinOnMs", 150),
    70: ("ovThreshMv", 28000), 71: ("ovHystMv", 1000),
    # v1.74: 72/73 retired - reserved words, no owner, default 0.
    72: ("retiredLowBatThreshMv", 0), 73: ("retiredLowBatClearMv", 0),
    74: ("pctVminMv", 21000), 75: ("pctVmaxMv", 29000),
    76: ("buzzerMute", 0),
    # v1.17: full latch + stable hysteresis + 0%/1% exits
    77: ("chgFullEnterPct", 100), 78: ("chgFullExitPct", 95),
    79: ("chgHystPct", 5), 80: ("runHystPct", 2),
    81: ("runZeroExit", 2), 82: ("runOneExit", 3),
}

def _w(v, lo, hi): return min(hi, max(lo, v))
def _period(v): return 0 if v == 0 else _w(v, 1000, 600000)
def _maxdur(period, count, gap):
    if count == 0: return period
    gt = gap * (count - 1)
    if gt >= period: return 0
    return (period - gt) // count

def ui_clamp_mirror(s):
    """[EN] v1.56: the firmware clamp is per-field only now - every
       cross-field rule moved into the panel (user order), so this mirror
       carries exactly one independent window per field and nothing else.
       [FA] از v1.56 گیرهٔ فرم‌ور فقط تک‌فیلدی است؛ قوانین مشترک به پنل رفتند."""
    s = dict(s)
    W = {"ovLedPeriodMs": (100, 10000), "ovLedDutyPct": (0, 100),
         "ovBeepDurMs": (0, 600000), "ovBeepCount": (0, 10), "ovBeepGapMs": (0, 5000),
         "blLedPeriodMs": (100, 10000), "blLedDutyPct": (0, 100),
         "blBeepDurMs": (0, 600000), "blBeepCount": (0, 10), "blBeepGapMs": (0, 5000),
         "runBeepStartPct": (0, 100), "runBeepDoublePct": (0, 100),
         "runBeepTriplePct": (0, 100), "runBeepCritPct": (0, 100),
         "runCritBeepDurMs": (0, 600000), "runCritCount": (0, 10),
         "runStdDurMs": (0, 600000), "runTriDurMs": (0, 600000),
         "runCritDurMs": (0, 120000), "runStdCount": (0, 10),
         "runDoubleCount": (0, 10), "runTriCount": (0, 10), "runGapMs": (0, 5000),
         "greenPeriodMs": (100, 10000), "greenMinOffMs": (0, 10000),
         "yellowPeriodMs": (100, 10000), "yellowMinOnMs": (0, 10000),
         "ovThreshMv": (24000, 32000), "ovHystMv": (0, 2000),
         "retiredLowBatThreshMv": (0, 0), "retiredLowBatClearMv": (0, 0),
         "pctVminMv": (15000, 25000), "pctVmaxMv": (25000, 32000),
         "buzzerMute": (0, 1), "chgFullEnterPct": (1, 100), "chgFullExitPct": (0, 100),
         "chgHystPct": (0, 50), "runHystPct": (0, 50),
         "runZeroExit": (0, 100), "runOneExit": (0, 100),
         "chgPctVminMv": (15000, 25000), "chgPctVmaxMv": (25000, 32000),
         "runDoubleDurMs": (0, 600000)}
    for k, (lo, hi) in W.items():
        if k in s:
            s[k] = _w(s[k], lo, hi)
    for k in ["ovBeepPeriodMs", "blBeepPeriodMs", "runStdIntervalMs",
              "runTriIntervalMs", "runCritPeriodMs", "runDoubleIntervalMs"]:
        if k in s:
            s[k] = _period(s[k])
    return s

def _ui_duty(period, dur, count, gap):
    if period == 0 or dur == 0 or count == 0: return 0
    window = dur * count + (gap * (count - 1) if count > 1 else 0)
    if window > period: return 101  # infeasible -> service INVALID (deterministic silence)
    return (window * 100 + period - 1) // period

def run_ui_alarm_tests():
    """[EN] v1.16: 39 runtime UI ids 38..76 - defines, boot defaults, exact
       legacy-sound equivalence, mute gate, read swaps, clamp invariants.
       v1.17 appends 77..82 (full latch + hysteresis) and raises the 69
       default 10 -> 150: 45 ids 38..82.
       [FA] تست شناسه‌های ۳۸..۸۲: دیفاین‌ها، دیفالت بوت، تطابق دقیق صدا،
       میوت، تعویض خوانش‌ها، نامتغیرهای گیره."""
    print("\n=== UI alarms v1.16/v1.17 (ids 38..82) ===")
    ui_led_h = open(LED_HEADER, "r", encoding="utf-8", errors="ignore").read()
    ui_led_c = open(os.path.join(BASE_DIR, "ui_led.c"), "r", encoding="utf-8", errors="ignore").read()

    # --- ID defines: contiguous 38..82 + MIN/MAX ---
    ids = sorted(int(m.group(2)) for m in
                 re.finditer(r"#define\s+(UI_ALARM_PARAM_\w+)\s+\(?(\d+)\)?u?",
                             ui_led_h) if "MIN_ID" not in m.group(1) and "MAX_ID" not in m.group(1))
    # [EN] v1.49: the dense block is still 38..82; the charge-side percent map
    #      lives in its own little range 119..120 (83..118 are owned by other
    #      modules, so it could not simply be appended).
    # [FA] بلوک متراکم همان ۳۸..۸۲ است؛ نگاشت درصد سمت شارژ بازهٔ کوچک خودش
    #      (۱۱۹..۱۲۰) را دارد چون ۸۳..۱۱۸ مال ماژول‌های دیگر است.
    assert_equal(ids, list(range(38, 83)) + [119, 120, 121, 122, 137, 138, 139, 140, 141, 142],
                 "UI alarm ids 38..82 plus ext ranges 119..122 and 137..142")
    assert_equal(defines.get("UI_ALARM_PARAM_MIN_ID"), 38, "MIN_ID 38")
    assert_equal(defines.get("UI_ALARM_PARAM_MAX_ID"), 82, "MAX_ID 82")
    assert_equal(defines.get("UI_ALARM_PARAM_EXT_MIN_ID"), 119, "EXT_MIN_ID 119")
    assert_equal(defines.get("UI_ALARM_PARAM_EXT_MAX_ID"), 122, "EXT_MAX_ID 122")
    assert_equal(defines.get("UI_ALARM_PARAM_TECH_EXT_MIN_ID"), 137, "TECH_EXT_MIN_ID 137")
    assert_equal(defines.get("UI_ALARM_PARAM_TECH_EXT_MAX_ID"), 142, "TECH_EXT_MAX_ID 142")
    assert_equal(defines.get("UI_ALARM_PARAM_CHG_PCT_VMIN_MV"), 119, "charge Vmin id 119")
    assert_equal(defines.get("UI_ALARM_PARAM_CHG_PCT_VMAX_MV"), 120, "charge Vmax id 120")
    assert_equal(defines.get("UI_ALARM_PARAM_RUN_DOUBLE_DUR_MS"), 121, "band 2 duration id 121")
    # [EN] v1.71 (user order: "in discharge only the GAP is shared"): id 122
    #      stopped being band 2's gap and became band 2's own repeat
    #      interval; the gap is the single shared id 65 again.
    # [FA] شناسهٔ ۱۲۲ از «گپ باند ۲» به «فاصلهٔ تکرار باند ۲» تبدیل شد و گپ
    #      دوباره فقط یکی است (۶۵).
    assert_equal(defines.get("UI_ALARM_PARAM_RUN_DOUBLE_INTERVAL_MS"), 122,
                 "band 2 repeat interval id 122")
    assert_true("UI_ALARM_PARAM_RUN_DOUBLE_GAP_MS" not in ui_led_h,
                "the per-band gap of band 2 is gone")
    assert_true("uint32_t uint32_t__runDoubleDurMs;" in ui_led_h
                and "uint32_t uint32_t__runDoubleIntervalMs;" in ui_led_h,
                "band 2 owns two words of its own")
    # [EN] v1.50: band 2 must play ITS duration and gap, and the common gap
    #      floor must no longer answer to band 2's count.
    # [FA] باند ۲ با مدت و گپ خودش پخش می‌شود و کف گپ مشترک دیگر به تعداد
    #      باند ۲ پاسخ نمی‌دهد.
    # [EN] v1.56: the clamp is one line per field now, so band 2's duration
    #      appears exactly three times: init, clamp, tick.
    # [FA] از v1.56 گیره تک‌خطی است، پس مدت باند ۲ سه بار می‌آید.
    assert_equal(ui_led_c.count("uint32_t__runDoubleDurMs"), 3,
                 "band 2 duration: init + clamp + tick")
    # [EN] clamp writes it twice (target + argument) and the tick reads it
    #      twice (period + duty derivation); the init uses the macro.
    # [FA] گیره دو بار و تیک دو بار آن را می‌آورد.
    assert_equal(ui_led_c.count("uint32_t__runDoubleIntervalMs"), 4,
                 "band 2 interval: clamp x2 + tick x2")
    # [EN] v1.71: every discharge band is shaped identically - count,
    #      per-beep duration, own repeat interval - and the duty handed to
    #      the buzzer is always DERIVED. No band may pass a typed duty.
    # [FA] هر چهار باند یک شکل‌اند و دیوتی همیشه محاسبه می‌شود.
    assert_true("uint32_t__runCritBeepDurMs" in ui_led_c
                and "runCritDutyPct" not in ui_led_c,
                "the critical band takes a per-beep duration, not a duty")
    _crit = ui_led_c.rsplit("uint32_t__runCritBeepDurMs,", 1)
    assert_true("func__Ui_BeepDutyPercent" in _crit[0].rsplit("(void)", 1)[-1]
                and "runCritCount" in _crit[1].split(";", 1)[0],
                "the critical band derives its duty like every other band")
    assert_true("uint32_t uint32_t__chgPctVminMv;" in ui_led_h
                and "uint32_t uint32_t__chgPctVmaxMv;" in ui_led_h,
                "the two charge-map words are appended to ui_alarm_t")
    assert_true("uint8_t func__Ui_ChargeVoltageToPercent(uint32_t uint32_t__batteryMv);" in ui_led_h,
                "charge-side converter prototype")
    assert_true("bool func__Ui_SetAlarmParam(uint8_t uint8_t__paramId," in ui_led_h, "Set prototype")
    assert_true("bool func__Ui_GetAlarmParam(uint8_t uint8_t__paramId," in ui_led_h, "Get prototype")

    # --- struct init order: positional init must list the 45 boot defaults in id order ---
    m = re.search(r"static (?:volatile )?ui_alarm_t UI_ALARM_T__G__Alarm =\n\{(.*?)\n\};", ui_led_c, re.S)
    assert_true(m, "alarm struct init found")
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    inits = [x.strip().rstrip(",").strip() for x in body.strip().split("\n")]
    inits = [x for x in inits if x]
    assert_equal(len(inits), 55, "45 dense init entries + charge map + band 2 shape + scenario 7")
    expected_macros = ["UI_INPUT_OVERVOLTAGE_LED_PERIOD_MS", "UI_INPUT_OVERVOLTAGE_LED_DUTY_PERCENT",
        "UI_INPUT_OVERVOLTAGE_BEEP_PERIOD_MS", "UI_INPUT_OVERVOLTAGE_BEEP_DURATION_MS",
        "UI_INPUT_OVERVOLTAGE_BEEP_COUNT", "UI_INPUT_OVERVOLTAGE_BEEP_GAP_MS",
        "UI_BAT_LOST_LED_PERIOD_MS", "UI_BAT_LOST_LED_DUTY_PERCENT", "UI_BAT_LOST_BEEP_PERIOD_MS",
        "233u", "UI_BAT_LOST_BEEP_COUNT", "UI_BAT_LOST_BEEP_GAP_MS",
        "UI_BATTERY_RUN_BEEP_START_PERCENT", "UI_BATTERY_RUN_BEEP_DOUBLE_PERCENT",
        "UI_BATTERY_RUN_BEEP_TRIPLE_PERCENT", "UI_BATTERY_RUN_BEEP_CRITICAL_PERCENT",
        "UI_BATTERY_RUN_BEEP_STANDARD_INTERVAL_MS", "UI_BATTERY_RUN_BEEP_TRIPLE_INTERVAL_MS",
        "UI_BATTERY_RUN_BEEP_CRITICAL_PERIOD_MS", "UI_BATTERY_RUN_BEEP_CRITICAL_BEEP_DURATION_MS",
        "UI_BATTERY_RUN_BEEP_CRITICAL_COUNT", "UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS",
        "UI_BATTERY_RUN_BEEP_TRIPLE_DURATION_MS", "UI_BATTERY_RUN_BEEP_CRITICAL_DURATION_MS",
        "UI_BATTERY_RUN_BEEP_STANDARD_COUNT", "UI_BATTERY_RUN_BEEP_DOUBLE_COUNT",
        "UI_BATTERY_RUN_BEEP_TRIPLE_COUNT", "UI_BATTERY_RUN_BEEP_GAP_MS",
        "UI_BLINK_PERIOD_MS", "UI_GREEN_MIN_OFF_MS", "UI_CHARGING_BLINK_PERIOD_MS",
        "UI_CHARGING_YELLOW_MIN_ON_MS", "UI_INPUT_OVERVOLTAGE_THRESHOLD_MV",
        "UI_INPUT_OVERVOLTAGE_HYSTERESIS_MV", "0u",
        "0u", "UI_BAT_V_MIN_MV", "UI_BAT_V_MAX_MV", "0u",
        "UI_CHARGING_FULL_ENTER_PERCENT", "UI_CHARGING_FULL_EXIT_PERCENT",
        "UI_CHARGING_PERCENT_HYSTERESIS_PERCENT", "UI_BATTERY_RUN_PERCENT_HYSTERESIS_PERCENT",
        "UI_BATTERY_ZERO_EXIT_THRESHOLD", "UI_BATTERY_ONE_EXIT_THRESHOLD",
        # [EN] v1.49: the charge-side map boots with the same factory numbers
        #      as the discharge map, so an untouched board does not change.
        "UI_BAT_V_MIN_MV", "UI_BAT_V_MAX_MV",
        # [EN] v1.50: band 2 boots with what it used to borrow from band 1.
        "UI_BATTERY_RUN_BEEP_STANDARD_DURATION_MS", "UI_BATTERY_RUN_BEEP_DOUBLE_INTERVAL_MS",
        "UI_TECH_FAULT_BEEP_PERIOD_MS", "UI_TECH_FAULT_BEEP_DURATION_MS",
        "UI_TECH_FAULT_BEEP_COUNT", "UI_TECH_FAULT_BEEP_GAP_MS",
        "UI_TECH_FAULT_LED_PERIOD_MS", "UI_TECH_FAULT_LED_DUTY_PERCENT"]
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
    # [EN] Pre-existing staleness found 2026-10-05: the count said 12 while
    #      the file had 13 (the imbalance scenario added a site and nobody
    #      re-ran this). Corrected, not weakened.
    # [FA] این عدد از قبل کهنه بود (سناریوی عدم‌توازن یک محل اضافه کرد).
    # [EN] v1.75 audit: 13 call sites + the definition = 14. The pin was one
    #      behind again (the dead-battery scenario added a site in v1.72).
    # [FA] ۱۳ محل فراخوانی + خود تعریف = ۱۴.
    assert_equal(ui_led_c.count("func__Ui_Buzzer_Gated("), 15,
                 "14 scenario sites + 1 def use the mute gate")
    assert_true("IMBAL_PARAM_LATCH_BEEP_COUNT" in ui_led_c and
                "IMBAL_PARAM_LATCH_BEEP_GAP_MS" in ui_led_c,
                "imbalance latch reads the independent count and gap parameters")
    assert_true("uint32_t__beepGapMs" in ui_led_c and
                "uint32_t__beepCount" in ui_led_c,
                "imbalance latch passes live count/gap into the beep pattern")
    # direct Tick calls left: 2 inside Gated + 1 all_off + 1 OV-clear explicit off
    # + 2 BoardTest (mute bypass, still proves the buzzer works at boot)
    # [EN] v1.75 audit: 8 now - the dead-battery lock turns the buzzer off
    #      explicitly like the OV-clear path does.
    # [FA] حالا ۸ مورد است.
    assert_equal(ui_led_c.count("func__Ui_Buzzer_Tick("), 8,
                 "direct Tick only in Gated/all_off/OV-clear/BoardTest/lock")
    assert_true("uint32_t__pctVminMv" in ui_led_c and "uint32_t__pctVmaxMv" in ui_led_c,
                "discharge percent map reads live 74/75")
    # [EN] v1.49 separation: the discharge map feeds BatteryRun only, and the
    #      charging face + the full-charge latch read the charge map.
    # [FA] جداسازی: نگاشت دشارژ فقط برای BatteryRun، و چهرهٔ شارژ و قفل
    #      فول‌شارژ از نگاشت سمت شارژ می‌خوانند.
    assert_equal(ui_led_c.count("func__Ui_BatteryVoltageToPercent("), 2,
                 "the discharge map has exactly one caller (plus its own definition)")
    assert_equal(ui_led_c.count("func__Ui_ChargeVoltageToPercent("), 3,
                 "the charge map is used by the charging face and the full latch")
    assert_true("func__Ui_ChargeVoltageToPercent(uint32_t__batteryClampedMv)" in ui_led_c,
                "the full-charge latch converts with the charge map")
    assert_true("uint32_t__batteryClampedMv > UI_ALARM_T__G__Alarm.uint32_t__chgPctVmaxMv" in ui_led_c,
                "and clamps against the charge ceiling, not the discharge one")
    assert_true("func__Ui_VoltageToPercentMap(" in ui_led_c,
                "both maps share one formula")
    assert_true("uint32_t__ovThreshMv" in ui_led_c
                and "uint32_t__lowBatThreshMv" not in ui_led_c,
                "the OV threshold is live (70) and the retired 72/73 window is gone")
    assert_true("APP_CONFIG.ui_blink_period_ms" not in ui_led_c
                and "APP_CONFIG.ui_charging_blink_period_ms" not in ui_led_c,
                "no scenario reads blink periods from APP_CONFIG anymore")
    print("Read swaps + mute gate PASS")

    # --- clamp invariants: defaults no-op + 2000 random states incl. idempotence ---
    import random
    defs = {f: d for _, (f, d) in UI_ALARM_DEFAULTS.items()}
    assert_equal(ui_clamp_mirror(defs), defs, "clamp is a no-op on boot defaults")
    def check_inv(s, label):
        """[EN] v1.56: only the per-field windows are the firmware's promise
           now; the joint rules are asserted in the panel test suite.
           [FA] تنها بازهٔ تک‌فیلدی وعدهٔ فرم‌ور است؛ قوانین مشترک در تست پنل."""
        assert_true(100 <= s["ovLedPeriodMs"] <= 10000, label + " 38 window")
        assert_true(0 <= s["ovLedDutyPct"] <= 100, label + " 39 window")
        assert_true(s["ovBeepPeriodMs"] == 0 or 1000 <= s["ovBeepPeriodMs"] <= 600000, label + " 40")
        assert_true(100 <= s["blLedPeriodMs"] <= 10000, label + " 44 window")
        assert_true(s["blBeepPeriodMs"] == 0 or 1000 <= s["blBeepPeriodMs"] <= 600000, label + " 46")
        for k in ["runBeepStartPct", "runBeepDoublePct", "runBeepTriplePct", "runBeepCritPct"]:
            assert_true(0 <= s[k] <= 100, label + " " + k)
        for k in ["runStdIntervalMs", "runTriIntervalMs", "runCritPeriodMs"]:
            assert_true(s[k] == 0 or 1000 <= s[k] <= 600000, label + " " + k)
        assert_true(0 <= s["runCritBeepDurMs"] <= 600000, label + " 57")
        for k in ["runCritCount", "runStdCount", "runDoubleCount", "runTriCount",
                  "ovBeepCount", "blBeepCount"]:
            assert_true(0 <= s[k] <= 10, label + " " + k)
        for k in ["ovBeepGapMs", "blBeepGapMs", "runGapMs"]:
            assert_true(0 <= s[k] <= 5000, label + " " + k)
        for k in ["ovBeepDurMs", "blBeepDurMs", "runStdDurMs", "runTriDurMs"]:
            assert_true(0 <= s[k] <= 600000, label + " " + k)
        assert_true(0 <= s["runCritDurMs"] <= 120000, label + " 61 window")
        assert_true(100 <= s["greenPeriodMs"] <= 10000 and 0 <= s["greenMinOffMs"] <= 10000,
                    label + " green windows")
        assert_true(100 <= s["yellowPeriodMs"] <= 10000 and 0 <= s["yellowMinOnMs"] <= 10000,
                    label + " yellow windows")
        assert_true(24000 <= s["ovThreshMv"] <= 32000 and 0 <= s["ovHystMv"] <= 2000
                    and s["ovHystMv"] < s["ovThreshMv"], label + " OV thresh (no underflow)")
        assert_true(s["retiredLowBatThreshMv"] == 0
                    and s["retiredLowBatClearMv"] == 0,
                    label + " retired low-battery words stay zero")
        assert_true(15000 <= s["pctVminMv"] <= 25000
                    and 25000 <= s["pctVmaxMv"] <= 32000, label + " pct map windows")
        assert_true(s["buzzerMute"] in (0, 1), label + " mute 0/1")
        assert_true(1 <= s["chgFullEnterPct"] <= 100, label + " 77 window")
        assert_true(0 <= s["chgFullExitPct"] <= 100, label + " 78 window")
        assert_true(0 <= s["chgHystPct"] <= 50, label + " 79 window")
        assert_true(0 <= s["runHystPct"] <= 50, label + " 80 window")
        assert_true(0 <= s["runZeroExit"] <= 100, label + " 81 window")
        assert_true(0 <= s["runOneExit"] <= 100, label + " 82 window")
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

    # --- v1.56 (user order): every cross-field rule left the MCU ---
    for gone in ["uint32_t__critWindowMs", "runCritCount--", "func__Ui_MaxBeepDurMs",
                 "UI_BUZZER_MIN_GAP_MS"]:
        assert_true(gone not in ui_led_c,
                    f"cross-field rule '{gone}' must no longer live in ui_led.c")
    assert_true("uint32_t__runBeepDoublePct =\n            UI_ALARM_T__G__Alarm.uint32_t__runBeepStartPct"
                not in ui_led_c, "band ordering must no longer be forced by the MCU")
    assert_true(ui_led_c.count("func__Ui_ClampWindow(") >= 40
                and "func__Ui_ClampPeriod(" in ui_led_c,
                "the per-field guard stays: a bad NVM record must not reach the ticks")
    print("Cross-field rules moved to the panel (v1.56) PASS")

    # --- v1.17: enter authoritative, yellow floor 150, scenario reads live 77..82 ---
    e = dict(defs)
    e.update({"chgFullEnterPct": 0, "chgFullExitPct": 0})
    c = ui_clamp_mirror(e)
    assert_equal((c["chgFullEnterPct"], c["chgFullExitPct"]), (1, 0),
                 "enter floored at 1, exit 0 stays (no underflow)")
    assert_equal(defines.get("UI_CHARGING_YELLOW_MIN_ON_MS"), 150,
                 "yellow floor macro is 150 (v1.17)")
    assert_true("uint32_t__chgFullEnterPct" in ui_led_c and "uint32_t__chgFullExitPct" in ui_led_c
                and "uint32_t__chgHystPct" in ui_led_c and "uint32_t__runHystPct" in ui_led_c
                and "uint32_t__runZeroExit" in ui_led_c and "uint32_t__runOneExit" in ui_led_c,
                "scenarios read live 77..82")
    print("Full latch + yellow 150 (v1.17) PASS")

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
    ui_led_c = open(os.path.join(BASE_DIR, "ui_led.c"), "r", encoding="utf-8", errors="ignore").read()
    m = re.search(r"func__Ui_SetAlarmParam.*?\n\}", ui_led_c, flags=re.S)
    assert_true(m and "osKernelLock()" in m.group(0) and "osKernelRestoreLock" in m.group(0),
                "Ui_SetAlarmParam store+clamp under scheduler lock (v1.16 C11)")
    print("\nALL HOST TESTS PASSED (buzzer + BatteryRun 2%+0/1 + Charging 5% + Full 100/95 + phase)")
if __name__=="__main__":
    main()
