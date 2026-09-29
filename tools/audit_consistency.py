#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
audit_consistency.py - whole-program cross-file consistency audit.

[EN] WHY THIS EXISTS
     Almost every defect found in this project has had the same shape: one
     number written by hand in several files, and the copies drifting apart.
     The parameter table alone is duplicated across EIGHT files - the firmware
     header, the panel's ranges, its transmit order, its HTML defaults, its
     config, the preview server, the NVM map and charger.h. Nothing linked
     them, so a change in one place silently rotted the others:

       - the panel's current LUT mirror kept the pre-refit table  (~8 % error)
       - AIDS ran to id 97 after the id block stopped at 92
       - the bench CSV row was 6 columns longer than its header
       - three tests froze literal counts and defended the WRONG answer

     Reading the code line by line does not catch these, because each file is
     locally correct - the defect only exists BETWEEN files. So this script
     compares them instead, and is meant to be run as a gate.

[FA] چرا این فایل هست
     تقریباً هر ایرادی که در این پروژه پیدا شده یک شکل داشته: یک عدد که دستی
     در چند فایل نوشته شده و کپی‌ها از هم جدا افتاده‌اند. فقط جدول پارامترها در
     هشت فایل تکرار شده و هیچ‌چیز به هم وصلشان نمی‌کرد، پس تغییر در یک جا بی‌صدا
     بقیه را می‌پوساند. خواندن خط‌به‌خط این‌ها را نمی‌گیرد، چون هر فایل به‌تنهایی
     درست است و ایراد فقط «بین» فایل‌ها وجود دارد. پس این اسکریپت آن‌ها را با هم
     مقایسه می‌کند و قرار است به‌عنوان دروازه اجرا شود.

Run: python3 tools/audit_consistency.py
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

FINDINGS = []
CHECKS = [0]


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="ignore")


def ok(cond, what, detail=""):
    """[EN] Record one invariant. / [FA] ثبت یک نامتغیر."""
    CHECKS[0] += 1
    if not cond:
        FINDINGS.append((what, detail))
    return bool(cond)


def c_array(text, name):
    """[EN] Values of a C initialiser, comments stripped (they contain commas -
       that detail produced a false alarm in an earlier hand-written audit).
       [FA] مقادیر یک آرایهٔ C با حذف کامنت‌ها (کامنت‌ها کاما دارند و همین یک بار
       هشدار کاذب ساخت)."""
    m = re.search(re.escape(name) + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\}", text, re.S)
    if not m:
        return None
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    body = re.sub(r"//[^\n]*", "", body)
    return [v.strip() for v in body.split(",") if v.strip()]


def js_array(text, name):
    m = re.search(r"\b" + re.escape(name) + r"\s*=\s*\[(.*?)\]", text, re.S)
    if not m:
        return None
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    return [v.strip() for v in body.split(",") if v.strip()]


def define(text, name):
    m = re.search(r"#define\s+" + re.escape(name) + r"\s+(-?\d+)u?", text)
    return int(m.group(1)) if m else None


# ---------------------------------------------------------------- load
ESP_H = read("Firmware/Modules/EspLink/esp_link.h")
ESP_C = read("Firmware/Modules/EspLink/esp_link.c")
NVM_H = read("Firmware/Modules/EspLink/esp_link_nvm.h")
CHG_H = read("Firmware/Modules/Charger/charger.h")
CAL_H = read("Firmware/Modules/Measurement/calibration.h")
BSP_M = read("Firmware/Bsp/Src/bsp_measurement.c")
BSP_A = read("Firmware/Bsp/Inc/bsp_adc.h")
P_CFG = read("esp_link_panel/plink_config.h")
P_PAR = read("esp_link_panel/plink_params.h")
P_STA = read("esp_link_panel/plink_state.h")
P_PAN = read("esp_link_panel/plink_panel.h")
PREV = read("tools/panel_preview_server.js")
IOC_MX = read("CubeMX/CubeIDE.ioc")
IOC_IDE = read("CubeIDE/CubeIDE.ioc")
MAIN_C = read("CubeIDE/Core/Src/main.c")

COUNT = define(ESP_H, "ESPLINK_PARAM_COUNT")


# ================================================================ 1. ids
def sec_ids():
    """[EN] The id space itself: defined once each, contiguous, no gaps.
       [FA] فضای شناسه: هرکدام یک‌بار، پشت‌سرهم، بدون حفره."""
    ids = {}
    dup = []
    for m in re.finditer(r"#define\s+ESPLINK_PARAM_(\w+)\s+(\d+)u", ESP_H):
        name, pid = m.group(1), int(m.group(2))
        if name == "COUNT":
            continue
        if pid in ids:
            dup.append((pid, ids[pid], name))
        ids[pid] = name
    ok(not dup, "two parameters share one id", str(dup))
    missing = [i for i in range(COUNT) if i not in ids]
    ok(not missing, "gap in the parameter id space",
       f"ids {missing} are below COUNT={COUNT} but never defined")
    over = [i for i in ids if i >= COUNT]
    ok(not over, "parameter id at or beyond COUNT",
       f"{[(i, ids[i]) for i in over]} vs COUNT={COUNT}")
    return ids


# ============================================================= 2. counts
def sec_counts():
    """[EN] Everything that must equal the parameter count.
       [FA] هرچه باید برابر تعداد پارامتر باشد."""
    ok(define(P_CFG, "ESP_PARAM_COUNT") == COUNT,
       "panel ESP_PARAM_COUNT != firmware COUNT",
       f"{define(P_CFG, 'ESP_PARAM_COUNT')} vs {COUNT}")

    for nm, arr in (("ParamMin", c_array(P_PAR, "INT32_T__G__ParamMin")),
                    ("ParamMax", c_array(P_PAR, "INT32_T__G__ParamMax")),
                    ("TxOrder", c_array(P_STA, "UINT8_T__G__TxOrder"))):
        ok(arr is not None and len(arr) == COUNT,
           f"{nm} length != COUNT",
           f"{len(arr) if arr else 'missing'} vs {COUNT}")

    tx = c_array(P_STA, "UINT8_T__G__TxOrder")
    if tx:
        vals = sorted(int(v.rstrip("uU")) for v in tx)
        ok(vals == list(range(COUNT)),
           "TxOrder is not a permutation of every id",
           "each id must be transmitted exactly once")

    p = js_array(PREV, "const P")
    ok(p is not None and len(p) == COUNT,
       "preview server P length != COUNT",
       f"{len(p) if p else 'missing'} vs {COUNT}")
    ok(f"id < {COUNT}" in PREV,
       "preview server id guard != COUNT",
       f"expected 'id < {COUNT}'")

    ok(define(NVM_H, "ESP_LINK_NVM_ENTRY_MAX") >= COUNT,
       "NVM ENTRY_MAX smaller than COUNT",
       f"{define(NVM_H, 'ESP_LINK_NVM_ENTRY_MAX')} < {COUNT}")

    bulk = 1 + COUNT * 5
    ok(bulk <= define(ESP_H, "ESPLINK_FRAME_MAX_PAYLOAD"),
       "PARAMS_BULK reply exceeds the protocol payload ceiling",
       f"{bulk} > {define(ESP_H, 'ESPLINK_FRAME_MAX_PAYLOAD')}")


# ====================================================== 3. defaults/ranges
def sec_defaults(ids):
    """[EN] `def D, LO..HI` in the firmware vs the panel's real numbers.
       [FA] مقدار پیش‌فرض و بازه در فرم‌ور در برابر اعداد واقعی پنل."""
    pmin = [int(v) for v in (c_array(P_PAR, "INT32_T__G__ParamMin") or [])]
    pmax = [int(v) for v in (c_array(P_PAR, "INT32_T__G__ParamMax") or [])]
    adef = [int(v) for v in (js_array(P_PAN, "const ADEF") or [])]
    pdef = [int(v) for v in (js_array(P_PAN, "const PDEF") or [])]
    prev = [int(v) for v in (js_array(PREV, "const P") or [])]

    bad_r, bad_d, bad_p = [], [], []
    for m in re.finditer(r"#define\s+ESPLINK_PARAM_(\w+)\s+(\d+)u\s*/\*(.*?)\*/",
                         ESP_H, re.S):
        name, pid, body = m.group(1), int(m.group(2)), m.group(3)
        if pid >= COUNT:
            continue
        d = re.search(r"def\s+(-?\d+)", body)
        r = re.search(r"def\s+-?\d+\s*,\s*(-?\d+)\.\.(-?\d+)", body)
        if r and pid < len(pmin):
            lo, hi = int(r.group(1)), int(r.group(2))
            if pmin[pid] != lo or pmax[pid] != hi:
                bad_r.append(f"id {pid} {name}: fw {lo}..{hi} vs panel "
                             f"{pmin[pid]}..{pmax[pid]}")
        if d:
            dv = int(d.group(1))
            pv = None
            if 27 <= pid <= 82 and pid - 27 < len(adef):
                pv = adef[pid - 27]
            elif 83 <= pid <= 92 and pid - 83 < len(pdef):
                pv = pdef[pid - 83]
            if pv is not None and pv != dv:
                bad_d.append(f"id {pid} {name}: fw {dv} vs panel {pv}")
            if pid < len(prev) and prev[pid] != dv:
                bad_p.append(f"id {pid} {name}: fw {dv} vs preview {prev[pid]}")

    ok(not bad_r, "documented range != panel ParamMin/ParamMax", "; ".join(bad_r))
    ok(not bad_d, "documented default != panel ADEF/PDEF", "; ".join(bad_d))
    ok(not bad_p, "documented default != preview server P", "; ".join(bad_p))

    lo_hi = [f"id {i}" for i in range(min(len(pmin), len(pmax))) if pmin[i] > pmax[i]]
    ok(not lo_hi, "panel range has min > max", ", ".join(lo_hi))


# ================================================== 4. panel id bookkeeping
def sec_panel(ids):
    """[EN] AIDS/XIDS bounds and the HTML inputs behind them.
       [FA] مرزهای AIDS/XIDS و ورودی‌های HTML پشتشان."""
    m = re.search(r"for\(let _i=(\d+);_i<=(\d+);_i\+\+\)AIDS", P_PAN)
    if ok(m is not None, "AIDS is not built from a bounded loop"):
        lo, hi = int(m.group(1)), int(m.group(2))
        pdef = js_array(P_PAN, "const PDEF") or []
        top = 83 + len(pdef) - 1
        ok(hi == top, "AIDS upper bound != the top real id",
           f"AIDS stops at {hi}, the id block ends at {top}")
        adef = js_array(P_PAN, "const ADEF") or []
        ok(len(adef) == (82 - lo + 1),
           "ADEF length does not cover its id span",
           f"ADEF has {len(adef)}, span {lo}..82 needs {82 - lo + 1}")
        # a phantom id would index past the default tables
        for pid in range(lo, hi + 1):
            if pid > 82 and (pid - 83) >= len(pdef):
                ok(False, "AIDS contains an id with no default",
                   f"id {pid} indexes PDEF[{pid - 83}] of {len(pdef)}")
                break
        # every id AIDS iterates must be shiftable in a 32-bit mask
        ok(hi - 64 < 32, "pending-mask shift would overflow 32 bits",
           f"id {hi} needs 1<<{hi - 64}; JS shifts modulo 32")

    sd = re.search(r"function sdef\(\)\{AIDS\.forEach\(\(id,k\)=>\{if\(id<(\d+)\|\|id>(\d+)\)return;", P_PAN)
    ok(sd is not None, "sdef() lost its id guard")


# ================================================== 5. calibration mirrors
def sec_calibration():
    """[EN] Tables copied by hand between the firmware and the panel.
       [FA] جدول‌هایی که دستی بین فرم‌ور و پنل کپی شده‌اند."""
    pairs = (("CAL_Current1LutChainMa", "LUT1X", "ch1 chain axis"),
             ("CAL_Current1LutBatteryMw", "LUT1Y", "ch1 power axis"),
             ("CAL_Current2LutChainMa", "LUTX", "ch2 chain axis"),
             ("CAL_Current2LutBatteryMw", "LUTY", "ch2 power axis"))
    for cname, jname, label in pairs:
        fw = c_array(CAL_H, cname)
        pn = js_array(P_PAN, jname)
        if fw is None or pn is None:
            ok(False, f"{label}: table missing", f"{cname} / {jname}")
            continue
        fwv = [int(v.rstrip("uU")) for v in fw]
        pnv = [int(v) for v in pn]
        ok(fwv == pnv, f"panel {label} mirror is stale",
           f"firmware {fwv} vs panel {pnv}")

    for x, y, lbl in (("CAL_Current1LutChainMa", "CAL_Current1LutBatteryMw", "ch1"),
                      ("CAL_Current2LutChainMa", "CAL_Current2LutBatteryMw", "ch2")):
        ax, ay = c_array(CAL_H, x), c_array(CAL_H, y)
        ok(ax and ay and len(ax) == len(ay),
           f"{lbl} LUT axes have different lengths",
           f"{len(ax) if ax else 0} vs {len(ay) if ay else 0}")
        if ax:
            xs = [int(v.rstrip("uU")) for v in ax]
            ok(xs == sorted(xs) and len(set(xs)) == len(xs),
               f"{lbl} LUT chain axis is not strictly increasing",
               "interpolation divides by the gap, so a repeat or a step "
               "backwards is a divide-by-zero or a sign flip")

    # the two 24 V sense nets are the same physical network
    ok("#define BSP_MEASUREMENT_DIV24BAT_TOP_OHMS   BSP_MEASUREMENT_DIV24_TOP_OHMS" in BSP_M,
       "the pack divider is not defined as the input divider",
       "R46 and R47 are both 68K; giving them separate numbers is what let a "
       "fabricated 66200 survive")
    ok(not re.search(r"#define\s+\w+\s+(66200|62400)u", BSP_M),
       "a fabricated divider constant is back",
       "62400 and 66200 are not resistors on this board")


# ======================================================== 6. protocol/CSV
def sec_protocol():
    """[EN] Frame sizes and the bench CSV, header vs emitted row.
       [FA] اندازهٔ فریم و فایل CSV بنچ: عنوان در برابر ردیف."""
    fields = define(P_CFG, "ESP_LINK_TLM_FIELD_COUNT")
    size = define(ESP_H, "ESPLINK_TLM_PAYLOAD_SIZE")
    ok(size == 4 + fields * 4, "TLM payload size != 4 + fields*4",
       f"{size} vs {4 + fields * 4} for {fields} fields")
    ok(define(P_CFG, "ESP_LINK_TLM_SIZE") == size,
       "the ESP expects a different TLM size than the firmware sends",
       f"{define(P_CFG, 'ESP_LINK_TLM_SIZE')} vs {size}")

    names, group = [], None
    blk = re.search(r"#define ESP_BENCHLOG_HEADER(.*?)(?=\n#define )", P_CFG, re.S)
    if ok(blk is not None, "bench CSV header block not found"):
        for line in re.findall(r'"([^"]*)"', blk.group(1)):
            line = line.replace("\\n", "").strip()
            if not line.startswith("#"):
                continue
            body = line[1:].strip()
            g = re.match(r"\[(\w+)\]\s*(.*)", body)
            if g:
                group, body = g.group(1), g.group(2)
            elif group and not re.fullmatch(r"[\w,]+", body):
                continue
            if group is None or "cols" in body:
                continue
            names += [x for x in body.split(",") if x.strip()]
        dups = {n for n in names if names.count(n) > 1}
        ok(not dups, "duplicate column name in the bench CSV header", str(dups))

        pn = re.search(r"const PN=(\d+);", P_PAN)
        if ok(pn is not None, "the CSV row no longer derives its parameter bound"):
            pn = int(pn.group(1))
            ok(pn == COUNT, "CSV parameter loop != COUNT",
               f"PN={pn} vs COUNT={COUNT}")
            row = re.search(r"function wrow\([^)]*\)\{.*?\n\s*return \[(.*?)\]\.join",
                            P_PAN, re.S)
            if ok(row is not None, "wrow() return list not found"):
                r = row.group(1)
                spread = sum(len([x for x in g.split(",") if x.strip()])
                             for g in re.findall(r"\.\.\.\[([\d,\s]+)\]\.map", r))
                total = (6 + pn
                         + len(re.findall(r"\.\.\.C\(\d+\)", r)) * 15
                         + len(re.findall(r"m\.(?:seq|fl|or)\b", r))
                         + spread
                         + len(re.findall(r"q\(v\.\w+\)", r)) + 1)
                ok(total == len(names),
                   "bench CSV row length != header column count",
                   f"row emits {total}, header declares {len(names)} - every value "
                   "after the mismatch is filed under the wrong name")


# ============================================================== 7. ADC/HW
def sec_hardware():
    """[EN] The ADC chain, .ioc vs generated code vs the BSP map.
       [FA] زنجیرهٔ ADC: ioc در برابر کد تولیدشده و نقشهٔ BSP."""
    ok(IOC_MX == IOC_IDE, "the CubeMX and CubeIDE .ioc copies have diverged",
       "they were byte-identical; a one-sided edit is reverted by the next "
       "regeneration")
    n = re.search(r"ADC1\.NbrOfConversion=(\d+)", IOC_MX)
    if ok(n is not None, "ADC1.NbrOfConversion missing from the .ioc"):
        n = int(n.group(1))
        ok(f"hadc1.Init.NbrOfConversion = {n};" in MAIN_C,
           "generated ADC init disagrees with the .ioc", f"expected {n}")
        ok(define(BSP_A, "BSP_ADC_CHANNEL_COUNT") == n,
           "BSP channel map disagrees with the ADC rank count",
           f"{define(BSP_A, 'BSP_ADC_CHANNEL_COUNT')} vs {n}")
        chans = len(re.findall(r"ADC1\.Channel-\d+=", IOC_MX))
        ok(chans == n, "the .ioc lists a different number of channels than ranks",
           f"{chans} channels vs {n} conversions")
    if "ADC_CHANNEL_VREFINT" in IOC_MX:
        ok("ADC1.SamplingTime-1-6=239.5" in IOC_MX,
           "VREFINT sampled too fast",
           "it needs >=17.1 us; at a 12 MHz ADC clock only 239.5 cycles qualify")
        ok("ADC_SAMPLETIME_239CYCLES_5" in MAIN_C,
           "generated init does not use the long sample for VREFINT")
    ids_used = set()
    for m in re.finditer(r"#define\s+BSP_ADC_CHANNEL_(\w+)\s+(\d+)u", BSP_A):
        if m.group(1) != "COUNT":
            ids_used.add(int(m.group(2)))
    n_ch = define(BSP_A, "BSP_ADC_CHANNEL_COUNT")
    ok(ids_used == set(range(n_ch)),
       "BSP ADC channel indices are not 0..COUNT-1",
       f"{sorted(ids_used)} vs 0..{n_ch - 1}")


# ============================================================= 8. charger
def sec_charger():
    """[EN] The PID id block and its defaults against the panel.
       [FA] بلوک شناسهٔ PID و پیش‌فرض‌هایش در برابر پنل."""
    base = define(CHG_H, "CHG_PID_PARAM_CURRENT_KP")
    top = define(CHG_H, "CHG_PID_PARAM_VOLTAGE_DOWN_RATE")
    pdef = js_array(P_PAN, "const PDEF") or []
    ok(base is not None and top is not None and (top - base + 1) == len(pdef),
       "the PID id block and PDEF have different sizes",
       f"ids {base}..{top} vs PDEF {len(pdef)}")
    ok(top == COUNT - 1, "the PID block does not end at the last id",
       f"top id {top}, COUNT {COUNT}")
    fw = []
    for loop in ("CURRENT", "VOLTAGE"):
        for f in ("KP", "KI", "KD", "UP_RATE", "DOWN_RATE"):
            fw.append(define(CHG_H, f"CHG_PID_{loop}_{f}"))
    ok(fw == [int(v) for v in pdef],
       "charger.h PID defaults != panel PDEF", f"{fw} vs {pdef}")
    ok(define(CHG_H, "CHG_OV_DECIDE_EARLY_MV") >= 50,
       "the over-voltage decision margin is not a real margin",
       "this is the honest replacement for bending the measurement scale")


# ====================================================== 9. single source
def sec_single_source():
    """[EN] THE STRUCTURAL PROBLEM, checked rather than merely hoped.
       The parameter table is written out by hand in eight places. Renumbering
       it from scratch was considered and rejected: the ids are baked into the
       NVM record, the wire protocol and every saved settings file, so a
       renumber silently loads the wrong value into the wrong setting on the
       first boot after flashing - the worst possible failure for a charger.
       The fix that actually removes the risk is not new numbers, it is
       removing the freedom for the copies to disagree. Every duplicate is
       therefore pinned to the firmware header here, and this section proves
       the pinning itself is complete: if a new copy of the table appears
       anywhere, it must be added to this audit or the count below fails.
       [FA] مشکل ساختاری، سنجیده‌شده نه امیدوارانه.
       جدول پارامترها دستی در هشت جا نوشته شده. شماره‌گذاری دوباره از صفر بررسی
       و رد شد: شناسه‌ها در رکورد NVM، پروتکل سیم و هر فایل تنظیمات ذخیره‌شده
       پخته شده‌اند، پس شماره‌گذاری مجدد در اولین بوت بعد از فلش، بی‌صدا مقدار
       غلط را در تنظیم غلط بار می‌کند - بدترین خرابی ممکن برای یک شارژر. راه‌حلی
       که واقعاً ریسک را برمی‌دارد شمارهٔ جدید نیست، گرفتن آزادیِ اختلاف از
       کپی‌هاست."""
    sources = {
        "firmware header (ESPLINK_PARAM_*)": ESP_H.count("#define ESPLINK_PARAM_"),
        "panel ParamMin/ParamMax": len(c_array(P_PAR, "INT32_T__G__ParamMin") or []),
        "panel TxOrder": len(c_array(P_STA, "UINT8_T__G__TxOrder") or []),
        "panel ADEF+PDEF": len(js_array(P_PAN, "const ADEF") or []) +
                           len(js_array(P_PAN, "const PDEF") or []),
        "panel ESP_PARAM_COUNT": 1 if define(P_CFG, "ESP_PARAM_COUNT") else 0,
        "preview server P": len(js_array(PREV, "const P") or []),
        "NVM entry map": 1 if define(NVM_H, "ESP_LINK_NVM_ENTRY_MAX") else 0,
        "charger PID defaults": 10,
    }
    for name, n in sources.items():
        ok(n > 0, f"a known copy of the parameter table vanished: {name}",
           "if it moved, this audit must follow it or the copies stop being "
           "compared at all")
    ok(len(sources) == 8,
       "the number of known parameter-table copies changed",
       "every copy must be compared here; add the new one or remove the stale one")



# ================================================================= report
def main():
    ids = sec_ids()
    sec_counts()
    sec_defaults(ids)
    sec_panel(ids)
    sec_calibration()
    sec_protocol()
    sec_hardware()
    sec_charger()
    sec_single_source()

    print("whole-program consistency audit")
    print("=" * 72)
    print(f"invariants checked : {CHECKS[0]}")
    print(f"findings           : {len(FINDINGS)}")
    if FINDINGS:
        print()
        for i, (what, detail) in enumerate(FINDINGS, 1):
            print(f"{i:2d}. {what}")
            if detail:
                for line in str(detail).split("; "):
                    print(f"      {line}")
        print()
        print("AUDIT FAILED")
        return 1
    print()
    print("AUDIT PASSED - no cross-file inconsistency found")
    return 0


if __name__ == "__main__":
    sys.exit(main())
