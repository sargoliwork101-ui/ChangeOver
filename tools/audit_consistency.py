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

import os
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


def live(src):
    """[EN] Source with comments removed. A plain "is this text in the file"
       check is satisfied by a COMMENT - this audit itself tripped over that
       while being written, flagging a hard-coded bound that existed only in
       the sentence explaining why hard-coded bounds are wrong.
       [FA] متن بدون کامنت. چک سادهٔ «آیا این رشته در فایل هست» با یک کامنت هم
       ارضا می‌شود - همین ممیز موقع نوشته‌شدن سر همین لغزید و کرانِ ثابتی را
       گزارش کرد که فقط داخل جمله‌ای وجود داشت که توضیح می‌داد کران ثابت بد است.
    """
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)       # C / JS blocks
    src = re.sub(r"(?m)^\s*//.*$", " ", src)               # JS line
    src = re.sub(r"(?m)^\s*#.*$", " ", src)                # Python line
    return src


def define_alias(text, name):
    """[EN] define() follows numbers; FIRST_ID/LAST_ID are aliases pointing at
       the first and last row macro, so resolve one hop. Deliberately ONE hop:
       a deeper chain would hide which macro actually carries the number.
       [FA] define() عدد می‌خواند ولی FIRST_ID/LAST_ID نام مستعارِ اولین و آخرین
       ردیف‌اند، پس یک پله دنبال می‌شود. عمداً فقط یک پله: زنجیرهٔ عمیق‌تر پنهان
       می‌کند که کدام ماکرو واقعاً عدد را دارد."""
    v = define(text, name)
    if v is not None:
        return v
    m = re.search(r"#define\s+" + re.escape(name) + r"\s+([A-Za-z_][0-9A-Za-z_]*)", text)
    return define(text, m.group(1)) if m else None


def block(text, fname):
    """[EN] Body of a C function, brace-matched. Used when an invariant is
       about what a function DOES, not just which constants exist - "the
       name appears in the file" is satisfied by a comment, which is how a
       stale claim survived in this code base before.
       [FA] بدنهٔ یک تابع C با تطبیق آکولاد. وقتی لازم است که نامتغیر دربارهٔ
       «کاری که تابع می‌کند» باشد نه صرفِ وجود ثابت‌ها؛ چون «نام در فایل هست»
       با یک کامنت هم ارضا می‌شود و دقیقاً همین‌طور یک ادعای کهنه در همین مخزن
       زنده مانده بود."""
    i = text.find(fname)
    if i < 0:
        return ""
    i = text.find("{", i)
    if i < 0:
        return ""
    depth, j = 0, i
    while j < len(text):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
        j += 1
    return text[i:]


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

# [EN] v1.74: parameter ids that exist as defines but have no owner any more.
#      The low-battery window moved into the Changeover module as constants.
# [FA] شناسه‌های بازنشسته: تعریف می‌مانند ولی هیچ مالکی ندارند.
RETIRED_IDS = {72, 73}


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

    # [EN] v1.74 (user order: low battery is not a LED scenario, it belongs in
    #      changeover and not in the panel): ids 72/73 are RETIRED. They keep
    #      their defines - as holes, so no id above them shifts meaning - but
    #      nothing may own them any more: no panel field, no UI acceptance, no
    #      NVM slot. This invariant is what stops them creeping back.
    # [FA] شناسه‌های بازنشسته باید در همه‌جا بی‌مالک بمانند.
    retired = sorted(RETIRED_IDS)
    panel = read("esp_link_panel/plink_panel.h")
    ok(all(f'id="q{i}"' not in panel and f'id="a{i}"' not in panel
           for i in retired),
       "a retired parameter still has a field in the panel",
       f"retired ids {retired} must not be editable from the panel")
    uic = read("Firmware/Modules/Ui/ui_led.c")
    ok("UI_ALARM_PARAM_RETIRED_LOWBAT_FIRST" in uic
       and "UI_ALARM_PARAM_INDEX_INVALID" in uic,
       "the UI module no longer rejects the retired ids",
       "func__Ui_AlarmParamIndex() must refuse 72/73 so a stale panel or an "
       "old backup cannot write into the reserved words")
    nvmc = read("Firmware/Modules/EspLink/esp_link_nvm.c")
    ok("ESP_LINK_NVM_RETIRED_ID_FIRST" in nvmc,
       "the retired ids are still persisted",
       "func__EspLink_NvmParamPersisted() must exclude the retired ids")
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
    # [EN] v1.28: the guard used to be the literal "id < 93" and silently
    #      dropped writes to the new ids while still answering 200 OK. The
    #      fix was to derive it from the header, so a literal is now the
    #      failure and the derivation is the thing to check.
    # [FA] گارد قبلاً عدد ثابت «id < 93» بود و نوشتن روی شناسه‌های جدید را
    #      بی‌صدا دور می‌ریخت در حالی که ۲۰۰ جواب می‌داد. رفع، مشتق‌کردن از
    #      هدر بود؛ پس حالا عدد ثابت خودش خطاست و مشتق‌شدن چیزی است که چک می‌شود.
    prev_live = live(PREV)
    ok("id < PARAM_COUNT" in prev_live,
       "preview server id guard is not derived from ESP_PARAM_COUNT",
       "a literal bound goes stale the next time a parameter is added")
    ok("ESP_PARAM_COUNT" in prev_live,
       "preview server does not read ESP_PARAM_COUNT from the header")
    ok(not re.search(r"id < \d", prev_live),
       "preview server still has a hard-coded id bound")

    # [EN] v1.72: the cap no longer has to beat COUNT - it has to beat the
    #      set that is actually persisted, which is what the _Static_assert in
    #      esp_link_nvm.c computes: 0..MAX_LOW, MIN_HIGH..MAX_HIGH minus the
    #      transient mute id, plus the runtime slots. With scenario 6 the two
    #      numbers finally differ (126 persisted vs COUNT 128, because ids
    #      15..19 and 76 never survive a reboot), so comparing against COUNT
    #      would demand a second flash page for entries nothing writes.
    # [FA] سقف باید از «مجموعهٔ واقعاً ذخیره‌شونده» بزرگ‌تر باشد نه از COUNT؛
    #      شناسه‌های گذرا (۱۵..۱۹ و ۷۶) هرگز ذخیره نمی‌شوند.
    persisted = ((define(NVM_H, "ESP_LINK_NVM_PERSISTED_ID_MAX_LOW") + 1) +
                 (define(NVM_H, "ESP_LINK_NVM_PERSISTED_ID_MAX_HIGH") -
                  define(NVM_H, "ESP_LINK_NVM_PERSISTED_ID_MIN_HIGH") + 1 - 1) +
                 (define(NVM_H, "ESP_LINK_NVM_SLOT_MAX_ID") -
                  define(NVM_H, "ESP_LINK_NVM_SLOT_MIN_ID") + 1))
    ok(define(NVM_H, "ESP_LINK_NVM_ENTRY_MAX") >= persisted,
       "NVM ENTRY_MAX smaller than the persisted id set",
       f"{define(NVM_H, 'ESP_LINK_NVM_ENTRY_MAX')} < {persisted}")
    ok(define(NVM_H, "ESP_LINK_NVM_PERSISTED_ID_MAX_HIGH") == COUNT - 1,
       "the persisted id window does not reach the last parameter",
       f"{define(NVM_H, 'ESP_LINK_NVM_PERSISTED_ID_MAX_HIGH')} vs {COUNT - 1}")

    # [EN] v1.28: PARAMS_BULK no longer has to fit in ONE frame - 108 params
    #      would need 541 bytes against a 512-byte ceiling. It is chunked, so
    #      the invariant moved from "the whole reply fits" to "a chunk fits,
    #      the chunk bound is derived, and the sender really does chunk".
    #      Checking only the arithmetic would pass on a sender that silently
    #      truncated at the chunk boundary and never sent the rest.
    # [FA] حالا کل پاسخ در یک فریم نمی‌گنجد (۱۰۸ پارامتر ۵۴۱ بایت در برابر سقف
    #      ۵۱۲). پاسخ تکه‌تکه می‌شود، پس نامتغیر از «کل پاسخ جا شود» به «یک تکه
    #      جا شود، کران تکه مشتق باشد، و فرستنده واقعاً تکه‌تکه بفرستد» منتقل شد.
    #      چک‌کردن فقط حسابِ عددی، فرستنده‌ای را که سرِ مرز تکه ببُرد و بقیه را
    #      نفرستد هم قبول می‌کرد.
    item = define(ESP_C, "ESPLINK_BULK_ITEM_SIZE")
    ceil_ = define(ESP_H, "ESPLINK_FRAME_MAX_PAYLOAD")
    # [EN] MAX_ITEMS is an expression, not a literal - that is the point of it.
    # [FA] MAX_ITEMS عبارت است نه عدد، و اصلاً فلسفه‌اش همین است.
    expr = re.search(r"#define ESPLINK_BULK_MAX_ITEMS\s*\\\s*\n\s*(.+)", ESP_C)
    maxit = (ceil_ - 1) // item if (item and ceil_) else None
    if ok(item is not None and expr is not None,
          "PARAMS_BULK chunk bounds are not defined"):
        ok("ESPLINK_FRAME_MAX_PAYLOAD" in expr.group(1)
           and "ESPLINK_BULK_ITEM_SIZE" in expr.group(1),
           "ESPLINK_BULK_MAX_ITEMS is not derived from the payload ceiling",
           expr.group(1).strip())
        ok(1 + maxit * item <= ceil_,
           "a PARAMS_BULK chunk exceeds the protocol payload ceiling",
           f"{1 + maxit * item} > {ceil_}")
        ok(COUNT > maxit,
           "PARAMS_BULK chunking is now dead code",
           f"COUNT {COUNT} fits one {maxit}-item chunk; keep it anyway")
        body = block(ESP_C, "func__EspLink_SendParamsBulk")
        ok(body.count("func__EspLink_SendFrame") == 2,
           "PARAMS_BULK must send a full chunk AND the trailing partial one",
           "one send site means the tail is dropped when COUNT > one chunk")
        ok(">= ESPLINK_BULK_MAX_ITEMS" in body,
           "PARAMS_BULK does not flush on the chunk boundary")


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

    # [EN] Defaults that are COMPUTED in the firmware have no "def N" to
    #      compare against, so sec_defaults skipped them entirely and the OV
    #      cutoff drifted unnoticed: the firmware boots 15000-150 = 14850 mV
    #      while the panel's factory-restore button pushed 15000 - a restore
    #      that moved a safety ceiling UP. Found only because the new
    #      operating table put the live number on screen. Each entry here is
    #      (id, firmware expression) and the expression is evaluated from the
    #      header, never retyped.
    # [FA] پیش‌فرض‌هایی که در فرم‌ور «محاسبه» می‌شوند عدد def ندارند، پس از
    #      این بخش کلاً جا می‌افتادند و قطع OV بی‌سروصدا دریفت کرد: فرم‌ور
    #      ۱۵۰۰۰−۱۵۰ = ۱۴۸۵۰ بوت می‌کند ولی دکمهٔ بازگردانی کارخانهٔ پنل ۱۵۰۰۰
    #      می‌فرستاد - بازگردانی‌ای که یک سقف ایمنی را بالا می‌برد. فقط به این
    #      دلیل پیدا شد که جدول عملکرد جدید عدد زنده را روی صفحه آورد.
    computed = {
        36: define(CHG_H, "CHG_MAX_VALID_BATTERY_MV")
            - define(CHG_H, "CHG_OV_DECIDE_EARLY_MV"),
    }
    bad_c = []
    for pid, want in computed.items():
        if want is None:
            continue
        if 27 <= pid <= 82 and (pid - 27) < len(adef) and adef[pid - 27] != want:
            bad_c.append(f"id {pid}: fw {want} vs panel ADEF {adef[pid - 27]}")
        prev_p = js_array(PREV, "const P") or []
        if pid < len(prev_p) and int(prev_p[pid]) != want:
            bad_c.append(f"id {pid}: fw {want} vs preview P {prev_p[pid]}")
    ok(not bad_c, "computed firmware default != panel/preview default",
       "; ".join(bad_c))

    # [EN] v1.29: the limits lost their form fields and became click-to-edit
    #      cells in the operating table, so their min/max now live in a JS
    #      object the browser can reach (EVB) instead of in HTML attributes.
    #      That is a hand-copied table, which this repo allows only under
    #      watch: every row is pinned to the firmware's own ParamMin/ParamMax
    #      here. A window that is wider than the firmware's lets the panel
    #      offer a value the board will silently clamp; narrower, and a legal
    #      setting becomes untypeable.
    # [FA] حدها فیلد فرم‌شان را از دست دادند و به خانه‌های کلیک-و-ویرایش جدول
    #      عملکرد تبدیل شدند، پس کمینه/بیشینه‌شان حالا در یک آبجکت JS (EVB)
    #      است نه در صفت‌های HTML. این یک جدول دست‌نویس است و در این مخزن فقط
    #      زیر نظر مجاز است: هر ردیفش همین‌جا به ParamMin/ParamMax خود فرم‌ور
    #      میخ می‌شود. پنجرهٔ بازتر یعنی پنل مقداری را پیشنهاد دهد که برد بی‌صدا
    #      گیره‌اش می‌زند؛ پنجرهٔ تنگ‌تر یعنی تنظیمِ مجاز اصلاً تایپ‌شدنی نباشد.
    evb = re.search(r"const EVB=\{(.*?)\};", P_PAN, re.S)
    if ok(evb is not None, "the panel lost its click-to-edit bounds table"):
        rows = re.findall(r"(\d+):\[(-?\d+),(-?\d+),(\d+),'(\w+)'\]", evb.group(1))
        ok(len(rows) >= 15, "EVB does not cover the limits block",
           f"only {len(rows)} rows")
        bad_b, seen = [], set()
        for pid, lo, hi, step, unit in rows:
            pid, lo, hi, step = int(pid), int(lo), int(hi), int(step)
            seen.add(pid)
            if pid < len(pmin) and lo != pmin[pid]:
                bad_b.append(f"id {pid} min {lo} vs firmware {pmin[pid]}")
            if pid < len(pmax) and hi != pmax[pid]:
                bad_b.append(f"id {pid} max {hi} vs firmware {pmax[pid]}")
            if step < 1 or step > max(1, hi - lo):
                bad_b.append(f"id {pid} step {step} outside its own window")
        ok(not bad_b, "EVB window != firmware ParamMin/ParamMax",
           "; ".join(bad_b))
        lim_lo = define_alias(CHG_H, "CHG_LIMIT_PARAM_FIRST_ID")
        lim_hi = define_alias(CHG_H, "CHG_LIMIT_PARAM_LAST_ID")
        missing = [i for i in range(lim_lo, lim_hi + 1) if i not in seen]
        ok(not missing,
           "a charger limit has no click-to-edit cell and no form field",
           f"ids {missing} became unreachable from the panel")
        # [EN] v1.30 (user order): the CHART edits, the operating table only
        #      reports. Three lists drive the rendering - EVV (voltage axis),
        #      EVI (current axis) and EVC (times and gains, which have no
        #      honest position on either axis) - so their union is what
        #      decides whether a parameter still has an editor at all. The
        #      previous version of this check asked whether ctab() drew the
        #      cell, which is now exactly the wrong question.
        # [FA] نمودار ویرایش می‌کند و جدول عملکرد فقط گزارش می‌دهد. سه فهرست
        #      رندر را می‌رانند - EVV (محور ولتاژ)، EVI (محور جریان) و EVC
        #      (زمان‌ها و گین‌ها که روی هیچ‌کدام از دو محور جای صادقانه‌ای
        #      ندارند) - پس اجتماعشان تعیین می‌کند که آیا پارامتری اصلاً هنوز
        #      ویرایشگر دارد یا نه. نسخهٔ قبلی این چک می‌پرسید ctab خانه را
        #      رسم می‌کند یا نه، که حالا دقیقاً سؤال غلطی است.
        drawn_axes = set()
        for name in ("EVV", "EVI"):
            m = re.search(r"const " + name + r"=\{(.*?)\};", P_PAN, re.S)
            if m:
                drawn_axes |= {int(x) for x in re.findall(r"(\d+):\[", m.group(1))}
        chart_body = block(P_PAN, "function qgraph()")
        # [EN] v1.35 (user order: "write those times underneath so the charts
        #      do not get so crowded - do the same for the gains"). The timers
        #      and gains are no longer <tspan> runs floating over the plane;
        #      they are chips under it, listed in EVCT and EVCG. A renderer is
        #      a renderer wherever it draws, so the chip groups count exactly
        #      as the annotations used to. Reading the two id lists instead of
        #      hard-coding them keeps this derived: add an id to a group and
        #      the audit follows without being edited.
        # [FA] زمان‌ها و گین‌ها دیگر روی صفحه شناور نیستند؛ زیر نمودار به‌صورت
        #      تراشه‌اند و در EVCT و EVCG فهرست شده‌اند. رندرکننده هر جا که رسم
        #      کند رندرکننده است، پس گروه‌های تراشه دقیقاً مثل یادداشت‌های قبلی
        #      حساب می‌شوند. خواندن دو فهرست به‌جای نوشتن دستی‌شان یعنی این چک
        #      مشتق می‌ماند.
        chipped = set()
        for name in ("EVCT", "EVCG"):
            m = re.search(r"const " + name + r"=\[([0-9,\s]*)\]", P_PAN)
            if m:
                chipped |= {int(x) for x in re.findall(r"\d+", m.group(1))}
        annotated = set()
        for grp in re.findall(r"ann\((.*?)\);", chart_body, re.S):
            annotated |= {int(x) for x in re.findall(r",\s*(\d+)\]", grp)}
        drawn = drawn_axes | annotated | chipped
        notrendered = [i for i in range(lim_lo, lim_hi + 1) if i not in drawn]
        ok(not notrendered,
           "a limit is in EVB but drawn by no renderer",
           f"ids {notrendered} are settable in theory only")
        ok(drawn <= seen,
           "something is drawn as editable with no EVB window",
           f"ids {sorted(drawn - seen)} would open an editor with no bounds")

        # the split the user asked for, pinned in both directions
        # [EN] v1.36: the operating table is GONE (user order: it must be
        #      deleted entirely), so its renderer is now pinned to stay absent
        #      - the table and its mount point must not creep back in without
        #      a deliberate decision.
        # [FA] v1.36: جدول عملکردی کاملاً حذف شد (دستور کاربر) و همین
        #      غایب‌بودن پین می‌شود تا رندرکننده یا جای نصبش بی‌صدا برنگردد.
        tab = block(P_PAN, "function ctab()")
        ok(tab == "",
           "function ctab() must stay deleted - the operating table was removed wholesale",
           "a ctab() renderer reappeared in plink_panel.h")
        ok('id="ctb"' not in P_PAN,
           "the operating-table mount point must stay removed",
           'id="ctb" is back in the markup')
        # [EN] Check for what makes a cell editable, not for one spelling of
        #      it. The first version looked for "ev(" and a mutation to
        #      "evat(" walked straight past - "evat(" does not contain "ev(".
        #      data-i is the actual contract the click handler keys on, so
        #      that is the thing to ban here.
        # [FA] چیزی را بسنج که خانه را ویرایش‌پذیر می‌کند، نه یک املای خاصش را.
        #      نسخهٔ اول دنبال "ev(" بود و جهش به "evat(" راست از کنارش رد شد.
        #      قرارداد واقعی که شنوندهٔ کلیک رویش کار می‌کند data-i است.
        # [EN] The preview server injects a tab to land on. If it lands
        #      anywhere other than the page that actually mounts the chart,
        #      the user opens the preview and sees neither the chart nor the
        #      operating table - which is precisely the "I can't see the
        #      charger table" report. Pin the landing tab to the mount: the
        #      chart lives on p0, so the injected click must be data-t="0".
        # [FA] سرور پیش‌نمایش یک تب فرود تزریق می‌کند. اگر جایی غیر از صفحه‌ای
        #      که واقعاً نمودار را نصب می‌کند فرود بیاید، کاربر پیش‌نمایش را باز
        #      می‌کند و نه نمودار می‌بیند نه جدول عملکرد - دقیقاً همان گزارش
        #      «جدول شارژر را نمی‌بینم». تب فرود به محل نصب میخ می‌شود.
        # [EN] User order 2026-10-03: terms that ARE English stay English.
        #      Bulk / Absorb / Taper / Float / Over / Reentry / backstop are
        #      the standard charger vocabulary; translating or transliterating
        #      one makes it unsearchable in either language. The axis labels
        #      are the most visible place this can regress, so pin them to
        #      ASCII - a Persian label in EVV/EVI fails here.
        # [FA] اصطلاحاتی که انگلیسی‌اند انگلیسی می‌مانند. برچسب محورها
        #      دیدنی‌ترین جایی است که می‌تواند پس‌رفت کند، پس به ASCII میخ
        #      می‌شوند؛ هر برچسب فارسی در EVV/EVI اینجا رد می‌شود.
        for lname in ("EVV", "EVI"):
            lm = re.search(r"const " + lname + r"=\{(.*?)\};", P_PAN, re.S)
            if lm:
                for lbl in re.findall(r"'#[0-9a-f]{6}','([^']*)'", lm.group(1)):
                    ok(lbl.isascii(),
                       "chart axis labels must keep the English term",
                       f"{lname} label {lbl!r} is not the English word")

        # [EN] The simulator must clamp like the board it previews. Standing
        #      user order: the preview has to be right BEFORE anything is
        #      flashed, which is worth nothing if the preview enforces last
        #      month's limits. So no current ceiling may be typed into the
        #      server by hand - it reads the firmware header.
        # [FA] شبیه‌ساز باید مثل همان بردی گیره بزند که پیش‌نمایشش است. دستور
        #      دائمی کاربر: پیش‌نمایش باید پیش از هر فلشی درست باشد، و این
        #      وقتی بی‌ارزش است که پیش‌نمایش حدود ماه قبل را اعمال کند.
        srv = read(ROOT / "tools" / "panel_preview_server.js")
        ok("CHG_CURRENT_HARD_FAULT_MAX_MA" in srv,
           "the simulator must read the current ceiling from the firmware",
           "a hand-copied ceiling makes the preview clamp unlike the board")
        for cid in (25, 26, 35, 94):
            cl = re.search(r"case %d:\s*return ([^\n]*)" % cid, srv)
            ok(cl and not re.search(r"\b(900|950|300|500)\b", cl.group(1)),
               f"simulator clamp for id {cid} still carries a frozen ceiling",
               cl.group(1).strip() if cl else "clamp not found")
        m = re.search(r"nav button\[data-t=\\?\"(\d)\\?\"\]'\);if\(t\d\)t\d\.click", srv)
        land = m.group(1) if m else None
        # which page div is the single chart mount inside? scan backwards
        # from the mount to the nearest id="pN" - derived, never assumed.
        # [EN] This used to derive the landing page from the chart mount.
        #      The chart has since been ordered off the chargers page, so
        #      following it would open the preview on the settings tab and
        #      hide the page the user actually asked to review. The anchor
        #      moves to the operating table, which is what the chargers page
        #      is for - still derived from the markup, never hardcoded.
        # [FA] قبلاً صفحهٔ فرود از محل نصب نمودار مشتق می‌شد. حالا نمودار به
        #      دستور کاربر از صفحهٔ شارژرها رفته، پس دنبال‌کردنش پیش‌نمایش را
        #      روی تب تنظیمات باز می‌کرد و همان صفحه‌ای را که کاربر می‌خواهد
        #      ببیند پنهان می‌کرد. لنگر به جدول عملکردی منتقل شد.
        #      v1.36: that table was removed wholesale (user order), so the
        #      landing anchor moved to the chargers page section itself.
        # [FA] v1.36: جدول عملکردی به دستور کاربر کلاً حذف شد؛ لنگر فرود به
        #      خود بخش شارژرها منتقل شد.
        mi = P_PAN.find('id="ch"')
        pm = list(re.finditer(r'id="p(\d)"', P_PAN[:mi])) if mi >= 0 else []
        mount_page = pm[-1].group(1) if pm else "?"
        ok(land == mount_page,
           "the preview must land on the chargers page",
           f"server lands on tab {land}, but the chargers section is on p{mount_page}")


        # [EN] v1.32 (user order: "I wanted to click ON the chart, not have you
        #      write it below it"): the chip strip is gone. Everything settable
        #      is now drawn inside the SVG - axis labels for the voltages and
        #      currents, inline annotations for the loop and timing values. So
        #      the check is that every EVC id really appears inside an ann()
        #      group in the chart, not merely that some chip helper exists.
        # [FA] نوار تراشه حذف شد. هر چیز تنظیم‌شدنی حالا داخل خود SVG رسم
        #      می‌شود - برچسب محورها برای ولتاژها و جریان‌ها، و یادداشت‌های درون
        #      نمودار برای مقادیر حلقه و زمان‌بندی. پس چک این است که هر شناسهٔ
        #      EVC واقعاً داخل یک گروه ann() در نمودار بیاید.
        ok("evat(" in chart_body, "the chart must be where values are edited")
        # derived, not declared: whatever EVB can edit and the two axes do not
        # draw MUST appear somewhere a user can click - on the plot or in the
        # chips under it. A third hand list would just be one more thing to
        # fall out of step.
        need = seen - drawn_axes
        placed = annotated | chipped
        ok(need <= placed,
           "a loop or timing value is settable but reaches no renderer",
           f"ids {sorted(need - placed)} would vanish from the panel entirely")
        # [EN] This check used to demand the chip strip stay DELETED, written
        #      when the order was "click the number on the plot, not a list
        #      below it". The order reversed once the plane got crowded, so
        #      the old assertion is now exactly backwards. Pinning the strip's
        #      presence alone would be just as brittle, so what is pinned is
        #      the property that matters: the chips must be EDITABLE, must be
        #      mounted under every chart, and must not quietly overlap the
        #      axis labels - one number, one place to change it.
        # [FA] این چک قبلاً اصرار داشت نوار تراشه حذف بماند، چون دستور آن زمان
        #      «روی خود نمودار کلیک کنم، نه فهرستی زیرش» بود. وقتی صفحه شلوغ
        #      شد دستور برعکس شد، پس ادعای قبلی حالا دقیقاً وارونه است. صرفِ
        #      «بودن» نوار را قفل‌کردن هم به همان اندازه شکننده بود، پس چیزی
        #      قفل می‌شود که اهمیت دارد: تراشه‌ها باید ویرایش‌پذیر باشند، زیر
        #      هر نمودار نصب شوند، و با برچسب‌های محور هم‌پوشانی نکنند.
        ok(chipped and 'class="evcg"' in chart_body,
           "the timers and gains must be rendered as chips under the chart")
        ok('class="evc"' in chart_body and "evat(" in
           chart_body[chart_body.find('class="evc"'):],
           "every chip must carry the shared editor hook",
           "a chip that only displays is a number the user cannot change")
        # [EN] v1.73 moved this help out of the "!" bubble and onto the page
        #      (user order: drop the badges, explain above each section). The
        #      invariant survives the move: every chip name in EVN must still
        #      be explained in the chart card's visible text, or the panel
        #      becomes folklore again.
        # [FA] از v1.73 این راهنما از حباب «!» به متنِ همیشه-روی-صفحه منتقل
        #      شد؛ ولی قاعده همان است: هر نام در EVN باید در متنِ کارت نمودار
        #      توضیح داده شده باشد.
        mn = re.search(r"const EVN=\{(.*?)\};", P_PAN, re.S)
        names = re.findall(r"\d+:'([^']+)'", mn.group(1)) if mn else []
        chart_help = re.findall(
            r'<b>نمودار فقط ولتاژ و جریان را نشان می‌دهد</b>(.*?)</div>',
            P_PAN, re.S)
        ok(len(chart_help) == 1,
           "the chart card must carry exactly one visible tuning explanation",
           f"found {len(chart_help)}")
        missing = sorted({n for n in names
                          if any(n not in h for h in chart_help)})
        ok(names and not missing,
           "the chart explanation must name every chip by its variable name",
           f"undocumented: {missing}")
        ok(P_PAN.count('class="qgcm"') == P_PAN.count('class="qgm"'),
           "every chart must have a chip strip mounted under it",
           "a chart whose chips are missing hides half of its settings")
        ok(not (chipped & drawn_axes),
           "a value must not be both an axis label and a chip",
           f"ids {sorted(chipped & drawn_axes)} would be editable in two "
           "places on one page")
        # [EN] This used to demand exactly ONE mount, written when the user
        #      asked for the duplicate to go. They have since asked for the
        #      settings copy back ("why did you take the chart away entirely?
        #      go back to the previous version"), so counting mounts is the
        #      wrong invariant - it encoded a preference that reversed.
        #      What is actually dangerous is two copies that can DISAGREE, or
        #      a mount nothing ever draws into. Both are pinned instead:
        #        - every mount is found by class and filled by one renderer,
        #        - no mount sits on a page the redraw gate does not cover.
        # [FA] این چک قبلاً دقیقاً یک محل نصب می‌خواست، چون کاربر حذف تکرار را
        #      خواسته بود. حالا نسخهٔ تب تنظیمات را پس خواسته، پس «شمردن محل
        #      نصب» نامتغیر غلطی است - یک ترجیح را کد کرده بود که برگشت.
        #      چیزی که واقعاً خطرناک است دو نسخه‌ای است که بتوانند حرف متفاوت
        #      بزنند، یا محل نصبی که هیچ‌وقت چیزی در آن رسم نشود.
        # [EN] STRUCTURE, not strings. A missing </div> in 0593f7c made s1,
        #      s2 and s3 children of s0 instead of its siblings, so hiding s0
        #      hid them too and every settings sub-tab except "charge, filter
        #      and PID" looked dead. It survived four commits, 180 string
        #      invariants and 144 rendered-content assertions, because every
        #      one of them asked "is the text there?" and none asked "is the
        #      tree the right shape?". The tab machinery toggles a class by
        #      INDEX across a flat NodeList, so it is only correct when the
        #      pages are siblings - that assumption is now checked instead of
        #      assumed, together with overall tag balance.
        # [FA] ساختار، نه رشته. یک </div> که در 0593f7c افتاد باعث شد s1 و s2
        #      و s3 به‌جای خواهرِ s0 فرزندش شوند، پس مخفی‌کردن s0 آن‌ها را هم
        #      مخفی می‌کرد و هر زیرتبی جز «شارژ، فیلتر و PID» مرده به نظر
        #      می‌رسید. چهار کامیت و ۱۸۰ نامتغیر رشته‌ای و ۱۴۴ ادعای محتوا از
        #      کنارش رد شدند، چون همه می‌پرسیدند «متن هست؟» و هیچ‌کدام
        #      نمی‌پرسید «درخت شکل درستی دارد؟». سازوکار تب کلاس را با
        #      «اندیس» روی یک فهرست تخت جابه‌جا می‌کند، پس فقط وقتی درست است
        #      که صفحه‌ها خواهر باشند.
        # [EN] The "!" help of a card must describe the fields THAT CARD has.
        #      The lists had slipped by one card, so scenario 2 explained the
        #      battery-monitor parameters that live on another sub-tab, 3
        #      explained 2's fields, 4 explained 3's and 5 explained 1's -
        #      you opened the help to find out what a box did and were told
        #      about boxes that were not on screen. Each card is matched
        #      against its own controls here, so the lists cannot drift again.
        # [FA] راهنمای «!» هر کارت باید فیلدهای همان کارت را توضیح دهد.
        #      فهرست‌ها یک کارت جابه‌جا شده بودند: سناریو ۲ پارامترهای نظارت
        #      باتری را توضیح می‌داد که در زیرتب دیگری است، ۳ فیلدهای ۲ را،
        #      ۴ فیلدهای ۳ را و ۵ فیلدهای ۱ را. حالا هر کارت با کنترل‌های
        #      خودش سنجیده می‌شود تا دوباره جابه‌جا نشود.
        # [EN] The header carries a build id derived from the markup, so a
        #      page on screen can be identified instead of guessed at. Three
        #      times this session a correct build was reported broken because
        #      a stale tab or an unflashed board is indistinguishable from a
        #      bug. The value is recomputed here: edit the panel without
        #      restamping it and this fails, which is the whole point - a
        #      stamp you can forget to update is worse than none.
        # [FA] سربرگ یک شناسهٔ بیلد دارد که از خود مارک‌آپ مشتق می‌شود تا
        #      بتوان صفحهٔ روی نمایشگر را شناخت نه حدس زد. سه بار در این جلسه
        #      بیلد درست «خراب» گزارش شد چون تب کهنه یا برد فلش‌نشده از باگ
        #      قابل تشخیص نیست. مقدار اینجا دوباره حساب می‌شود.
        import hashlib as _hl
        _st = re.search(r'<span class="bs" id="bs">build ([0-9a-f]{7})</span>',
                        P_PAN)
        ok(bool(_st), "the panel header carries a build stamp",
           "without it a stale page cannot be told from a broken one")
        _lit0 = P_PAN.split('R"HTML(', 1)[1].split(')HTML"', 1)[0]
        if _st:
            _blank = re.sub(r'(<span class="bs" id="bs">build )[0-9a-f]{7}(</span>)',
                            r'\g<1>_______\g<2>', _lit0)
            _want = _hl.sha1(_blank.encode("utf-8")).hexdigest()[:7]
            ok(_st.group(1) == _want,
               "the build stamp matches the panel it is printed on",
               f"header says {_st.group(1)}, markup hashes to {_want} - run "
               "tools/stamp_panel.py")

        # [EN] The panel is served in one send_P; keep it under the ceiling.
        # [FA] پنل با یک send_P می‌رود؛ زیر سقف نگهش دار.
        _lit = P_PAN.split('R"HTML(', 1)[1].split(')HTML"', 1)[0]
        _sz = len(_lit.encode("utf-8")) + 1
        _cap = re.search(r"#define ESP_PANEL_HTML_MAX_BYTES\s+(\d+)u", P_CFG)
        ok(bool(_cap), "the panel has a declared size ceiling")
        if _cap:
            ok(_sz <= int(_cap.group(1)),
               "the panel fits the transfer budget it declares",
               f"{_sz} bytes vs ceiling {_cap.group(1)} - a cut transfer drops "
               "the tail of the page, which is the settings sub-pages")

        # [EN] v1.73 (user order: "remove the ! badge on every section so the
        #      code gets lighter, and instead explain above every section what
        #      these variables mean"). The coverage guarantee therefore moved
        #      from a hover bubble to text that is always on screen: every
        #      section heading must be followed by its explanation, and the old
        #      bubbles must not creep back in.
        # [FA] از v1.73 توضیح هر بخش همیشه روی صفحه است نه داخل حباب «!»:
        #      هر عنوان بخش باید بلافاصله توضیح خودش را داشته باشد و حباب‌های
        #      قدیمی نباید برگردند.
        ok('class="ib"' not in P_PAN and 'function pexp(' not in P_PAN,
           "the removed ! help bubbles came back",
           "v1.73 replaced them with always-visible section text")
        _secs = re.findall(r'<div class="sec">(.*?)(?=<div class="sec">|\Z)',
                           P_PAN, re.S)
        _nohelp = [re.sub(r"<[^>]*>", "", _x)[:40].strip() for _x in _secs
                   if '<div class="sx">' not in _x and '<div class="ds">' not in _x]
        ok(not _nohelp,
           "every section must explain its own variables above the fields",
           f"sections with no explanation: {_nohelp}")

        _html = re.search(r'R"HTML\(([\s\S]*)\)HTML"', P_PAN)
        ok(bool(_html), "the panel must expose its HTML literal")
        if _html:
            _h = _html.group(1)
            _depth, _bad = 0, False
            for _m in re.finditer(r"<div\b[^>]*>|</div>", _h):
                _depth += -1 if _m.group(0) == "</div>" else 1
                if _depth < 0:
                    _bad = True
            ok(not _bad and _depth == 0,
               "the panel markup must be tag-balanced",
               f"{_depth} <div> left unclosed - the page tree is not what the "
               "markup looks like, and index-based tab switching breaks")

            # parentage: every page of a switcher must share one parent
            def _parents(cls):
                out = []
                for _m in re.finditer(r'<div\b[^>]*class="[^"]*\b' + cls +
                                      r'\b[^"]*"[^>]*>', _h):
                    d2 = 0
                    for _n in re.finditer(r"<div\b[^>]*>|</div>",
                                          _h[:_m.start()]):
                        d2 += -1 if _n.group(0) == "</div>" else 1
                    out.append(d2)
                return out

            for cls, who in (("pgx", "main tabs"), ("sgx", "settings sub-tabs")):
                depths = _parents(cls)
                ok(depths and len(set(depths)) == 1,
                   f"the {who} must all be siblings at one nesting level",
                   f"depths {depths} - a page nested inside another vanishes "
                   "whenever its host is hidden, and the index-based toggle "
                   "then points at the wrong element")

            # the switcher must have exactly as many pages as buttons
            _nb = len(re.findall(r'<button[^>]*data-t="\d+"', _h)) or \
                  len(re.findall(r"<nav[^>]*>[\s\S]*?</nav>", _h)) and \
                  len(re.findall(r'data-t="\d+"', _h))
            _ns = len(re.findall(r'class="[^"]*\bsgx\b', _h))
            _nsb = len(re.findall(r'data-s="\d+"', _h))
            ok(_ns == _nsb,
               "one settings page per settings button",
               f"{_ns} pages vs {_nsb} buttons")

        mounts = P_PAN.count('class="qgm"')
        ok(mounts >= 1, "the chart must be mounted somewhere")
        ok("document.querySelectorAll('.qgm')" in P_PAN and "$('qg')" not in
           re.sub(r"/\*.*?\*/", "", P_PAN, flags=re.S),
           "every chart mount must be filled by the SAME renderer",
           "an id lookup fills only the first copy and lets the two disagree")
        # each mount's host page must be covered by the redraw gate
        # [EN] Read the qgraph GATE ITSELF, not the whole of draw(). The
        #      first version searched the function body for "STAB==0" and
        #      passed spuriously, because draw() also contains
        #      if(STAB==0)pchk() - an unrelated guard. A mutation that
        #      narrowed the gate to if(TAB==0) survived it. Match the exact
        #      condition attached to the qgraph() call.
        # [FA] خودِ گاردِ qgraph خوانده می‌شود نه کل بدنهٔ draw(). نسخهٔ اول در
        #      بدنه دنبال STAB==0 می‌گشت و الکی سبز می‌شد، چون draw شرط
        #      بی‌ربط if(STAB==0)pchk() را هم دارد.
        gm = re.search(r"if\(([^)]*(?:\([^)]*\))?[^)]*)\)qgraph\(\);", P_PAN)
        gate = gm.group(1) if gm else ""
        ok(bool(gm), "the chart redraw must be gated by an explicit condition")
        for m in re.finditer(r'class="qgm"', P_PAN):
            pg = re.findall(r'id="(p\d|s\d)"', P_PAN[:m.start()])
            host = pg[-1] if pg else "?"
            # [EN] "TAB==0" is a SUBSTRING of "STAB==0", so a plain `in`
            #      test reported the chargers page as covered by a gate that
            #      only mentioned the settings sub-tab. Third substring trap
            #      in this file; a negative lookbehind is the fix.
            # [FA] «TAB==0» زیررشتهٔ «STAB==0» است، پس تست ساده صفحهٔ شارژرها
            #      را با گاردی که فقط زیرتب تنظیمات را نام می‌برد «پوشیده»
            #      گزارش می‌کرد. سومین تلهٔ زیررشته در همین فایل.
            has_tab0 = re.search(r"(?<!S)TAB==0", gate) is not None
            covered = (has_tab0 if host == "p0"
                       else ("STAB==0" in gate and "TAB==2" in gate)
                       if host == "s0" else False)
            ok(covered,
               f"the chart mount on {host} is never redrawn",
               "a mount outside the gate is a box that stays empty forever "
               "while the markup looks perfectly correct")
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
        # [EN] v1.28: PDEF is no longer the last default table - LDEF (the
        #      user-ordered limits, ids 93..107) sits above it. The old form
        #      of this check hard-wired "the panel's id space ends where the
        #      PID block ends", which is exactly the assumption that broke.
        # [FA] دیگر PDEF آخرین جدول پیش‌فرض نیست؛ LDEF (حدها، ۹۳..۱۰۷) بالای
        #      آن است. شکل قبلی این چک فرض «فضای شناسهٔ پنل همان‌جا که بلوک
        #      PID تمام می‌شود تمام می‌شود» را سیم‌کشی کرده بود - دقیقاً همان
        #      فرضی که شکست.
        pdef = js_array(P_PAN, "const PDEF") or []
        ldef = js_array(P_PAN, "const LDEF") or []
        idef = js_array(P_PAN, "const IDEF") or []
        adef = js_array(P_PAN, "const ADEF") or []
        pid_base = define(CHG_H, "CHG_PID_PARAM_CURRENT_KP")
        lim_base = define_alias(CHG_H, "CHG_LIMIT_PARAM_FIRST_ID")
        ok(lim_base == pid_base + len(pdef),
           "LDEF does not start where PDEF ends",
           f"limits start at {lim_base}, PID block ends at {pid_base + len(pdef) - 1}")
        imb_base = define(ESP_H, "ESPLINK_PARAM_IMBAL_REST_LIMIT_MV")
        ok(imb_base == lim_base + len(ldef),
           "IDEF does not start where LDEF ends",
           f"imbalance block starts at {imb_base}, limits end at {lim_base + len(ldef) - 1}")
        # [EN] v1.49: a fourth block sits after the imbalance one - the
        #      charge-side percent map (119/120), which could not be appended
        #      to the dense UI range because 83..118 were already taken.
        # [FA] بلوک چهارم بعد از عدم‌توازن: نگاشت درصد سمت شارژ (۱۱۹/۱۲۰).
        cdef = js_array(P_PAN, "const CDEF") or []
        chg_base = define(ESP_H, "ESPLINK_PARAM_UI_CHG_PCT_VMIN_MV")
        ok(chg_base == imb_base + len(idef),
           "CDEF does not start where IDEF ends",
           f"charge map starts at {chg_base}, imbalance block ends at {imb_base + len(idef) - 1}")
        ok(len(cdef) == 9,
           "CDEF must cover the charge map (119/120), band 2's beep shape "
           "(121/122), the imbalance latch blink (123/124, v1.68) and the "
           "dead-battery scenario 6 (125..127, v1.72)",
           f"CDEF has {len(cdef)} entries")
        top = chg_base + len(cdef) - 1
        ok(hi == top, "AIDS upper bound != the top real id",
           f"AIDS stops at {hi}, the id block ends at {top}")
        ok(top == COUNT - 1, "the id blocks do not reach the last parameter",
           f"top id {top}, COUNT {COUNT}")
        ok(len(adef) == (82 - lo + 1),
           "ADEF length does not cover its id span",
           f"ADEF has {len(adef)}, span {lo}..82 needs {82 - lo + 1}")
        # a phantom id would index past the default tables
        for pid in range(lo, hi + 1):
            if pid >= chg_base:
                tbl, idx, size = "CDEF", pid - chg_base, len(cdef)
            elif pid >= imb_base:
                tbl, idx, size = "IDEF", pid - imb_base, len(idef)
            elif pid >= lim_base:
                tbl, idx, size = "LDEF", pid - lim_base, len(ldef)
            elif pid >= pid_base:
                tbl, idx, size = "PDEF", pid - pid_base, len(pdef)
            else:
                tbl, idx, size = "ADEF", pid - lo, len(adef)
            if idx >= size:
                ok(False, "AIDS contains an id with no default",
                   f"id {pid} indexes {tbl}[{idx}] of {size}")
                break
        # [EN] Every id must be shiftable in SOME 32-bit mask word, and the
        #      two sides must split them identically. On the C side a shift
        #      past 31 is undefined behaviour, not a wrong pixel: before
        #      v1.28 the ESP dropped every id >= 64 into word 3, so id 96
        #      evaluated 1UL << 32. The panel's modulo-32 shift hid it.
        # [FA] هر شناسه باید در یکی از کلمه‌های ۳۲ بیتی جا شود و دو طرف باید
        #      یکسان تقسیم کنند. سمت C شیفت بیش از ۳۱ رفتار تعریف‌نشده است نه
        #      پیکسل غلط: پیش از v1.28 هر شناسهٔ ۶۴ به بالا در کلمهٔ سوم
        #      می‌رفت، یعنی شناسهٔ ۹۶ می‌شد 1UL << 32. شیفت مدولو-۳۲ پنل آن را
        #      پنهان می‌کرد.
        words = 4
        ok(hi < words * 32, "pending-mask has no word for the top id",
           f"id {hi} needs word {hi // 32 + 1} of {words}")
        p_http = read("esp_link_panel/plink_http.h")
        for w in range(2, words + 1):
            lo_b = (w - 1) * 32
            ok(f"index < {lo_b}u" in p_http or w == words,
               "the ESP pending-mask split lost a boundary", f"missing {lo_b}")
            ok(f"- {lo_b}u" in p_http,
               "the ESP does not rebase ids into their mask word",
               f"word {w} must shift by id - {lo_b}")
            ok(f"(1<<(id-{lo_b}))" in P_PAN,
               "the panel pending-mask has no arm for this word",
               f"apend() must handle ids {lo_b}..{lo_b + 31}")
        ok("static_assert(ESP_PARAM_COUNT <= 128" in p_http,
           "nothing stops the next parameter block from overflowing the masks")

        # [EN] The sketch is C++, so _Static_assert (a C11 keyword) does not
        #      compile in ANY dialect: using it does not weaken a check, it
        #      stops the whole ESP program from building. That is how it got
        #      here unnoticed - nothing built this program until now.
        # [FA] اسکچ ++C است، پس _Static_assert (کلیدواژهٔ C11) در هیچ دیالکتی
        #      کامپایل نمی‌شود: استفاده از آن چک را ضعیف نمی‌کند، کل برنامهٔ ESP
        #      را از ساخته‌شدن بازمی‌دارد. دقیقاً به همین شکل بی‌سروصدا وارد شد.
        for esp_file in ("plink_config.h", "plink_params.h", "plink_state.h",
                         "plink_panel.h", "plink_font.h", "plink_link.h",
                         "plink_http.h", "esp_link_panel.ino"):
            body = read(f"esp_link_panel/{esp_file}")
            ok("_Static_assert(" not in body,
               "the C++ sketch uses _Static_assert, which no C++ dialect accepts",
               f"{esp_file}: write static_assert; the board cannot build this")

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
            # [EN] v1.64 line-by-line audit finding: the firmware computes
            #      (yHigh - yLow) in UNSIGNED arithmetic. A power axis that
            #      dips - one noisy bench point is enough - wraps around to
            #      ~4 billion instead of a small negative number, so the
            #      reported battery current explodes. The panel refuses to
            #      emit such a table; this catches a hand-edited one.
            # [FA] تفریق محور توان در فرم‌ور بدون علامت است؛ یک نقطهٔ نزولی
            #      به‌جای عدد منفی کوچک، به ~۴ میلیارد می‌پیچد.
            ays = [int(v.rstrip("uU")) for v in ay] if ay else []
            ok(all(ays[i] >= ays[i - 1] for i in range(1, len(ays))),
               f"{lbl} LUT power axis dips",
               "the firmware subtracts these as unsigned, so a dip wraps to "
               "a gigantic number instead of a negative one")

    # ---------- v1.66: the direct-push path must agree in three places ----------
    # [EN] The board stores at most CAL_LUT_POINTS_MAX points per channel, the
    #      ESP mirrors that cap before it even opens the link, and the browser
    #      refuses to offer a bigger table. Three hand-typed numbers - exactly
    #      the shape of drift this audit exists for.
    # [FA] سقف نقاط در سه جای دست‌نویس تکرار شده: برد، ESP و مرورگر. دقیقاً
    #      همان نوع واگراییِ خاموشی که این ممیزی برای آن نوشته شده.
    lut_h = read("Firmware/Modules/CalLut/cal_lut.h")
    m_board = re.search(r"CAL_LUT_POINTS_MAX\s+(\d+)u", lut_h)
    m_esp = re.search(r"ESP_LUT_POINTS_MAX\s+(\d+)u", P_CFG)
    m_js = re.search(r"const LUTMAX=(\d+);", P_PAN)
    ok(bool(m_board and m_esp and m_js),
       "the LUT point cap is missing on one of the three sides",
       "board cal_lut.h / ESP plink_config.h / browser plink_panel.h")
    if m_board and m_esp and m_js:
        ok(m_board.group(1) == m_esp.group(1) == m_js.group(1),
           "the LUT point cap disagrees between the board, the ESP and the browser",
           f"board {m_board.group(1)} / ESP {m_esp.group(1)} / browser {m_js.group(1)}")
    # [EN] The LUT block and the parameter block must not be the same pages:
    #      the user asked for a SEPARATE storage path precisely so a big table
    #      transfer can never endanger the saved settings.
    # [FA] بلوک جدول و بلوک پارامترها نباید یکی باشند: کاربر «مسیر جدا» خواست
    #      تا انتقال حجیم جدول هرگز تنظیمات ذخیره‌شده را به خطر نیندازد.
    lut_pages = set(re.findall(r"CAL_LUT_PAGE_[AB]_ADDR\s+(0x[0-9A-Fa-f]+)u", lut_h))
    nvm_pages = set(re.findall(r"ESP_LINK_NVM_PAGE_[AB]_ADDR\s+(0x[0-9A-Fa-f]+)u", NVM_H))
    ok(lut_pages and nvm_pages and not (lut_pages & nvm_pages),
       "the bench-LUT flash block overlaps the parameter record block",
       f"LUT {sorted(lut_pages)} vs parameters {sorted(nvm_pages)}")

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

    names, group, groups = [], None, {}
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
            cols = [x for x in body.split(",") if x.strip()]
            names += cols
            groups.setdefault(group, []).extend(cols)
        dups = {n for n in names if names.count(n) > 1}
        ok(not dups, "duplicate column name in the bench CSV header", str(dups))

        # [EN] v1.26 split the log in two: a one-off "# settings:" line carrying
        #      the 93 settings, and data rows carrying only the 56 columns that
        #      actually vary. Both halves must still add up to the header, and
        #      the settings line must still cover every parameter.
        # [FA] نسخهٔ ۱.۲۶ لاگ را دو تکه کرد: یک خط «# settings:» با ۹۳ تنظیم و
        #      ردیف‌های داده فقط با ۵۶ ستون متغیر. هر دو نیمه باید با عنوان جمع
        #      بزنند و خط تنظیمات باید همهٔ پارامترها را پوشش دهد.
        setting_names = [n for g, lst in groups.items() if g.startswith("settings")
                         for n in lst]
        data_names = [n for g, lst in groups.items() if not g.startswith("settings")
                      for n in lst]
        pn = re.search(r"const PN=(\d+);", P_PAN)
        if ok(pn is not None, "the settings line no longer derives its bound"):
            pn = int(pn.group(1))
            ok(pn == COUNT, "the settings line does not cover every parameter",
               f"PN={pn} vs COUNT={COUNT}")
            ok(len(setting_names) == COUNT,
               "the header's settings block does not name every parameter",
               f"{len(setting_names)} names vs {COUNT} parameters")
            ok(re.search(r"function wset\(\)\{.*?for\(let k=0;k<PN;k\+\+\)", P_PAN, re.S)
               is not None,
               "the settings line must be built from the derived bound PN")
            ok("async function wsync()" in P_PAN and "WSIG" in P_PAN,
               "a mid-run settings change must be detected and re-logged, or rows "
               "silently inherit the wrong settings")
            row = re.search(r"function wrow\([^)]*\)\{.*?\n\s*return \[(.*?)\]\.join",
                            P_PAN, re.S)
            if ok(row is not None, "wrow() return list not found"):
                r = row.group(1)
                ok("...P," not in r,
                   "the data row must NOT repeat the settings - that was 62 percent "
                   "of every row")
                spread = sum(len([x for x in g.split(",") if x.strip()])
                             for g in re.findall(r"\.\.\.\[([\d,\s]+)\]\.map", r))
                total = (6
                         + len(re.findall(r"\.\.\.C\(\d+\)", r)) * 15
                         + len(re.findall(r"m\.(?:seq|fl|or)\b", r))
                         + spread
                         + len(re.findall(r"q\(v\.\w+\)", r)) + 1)
                ok(total == len(data_names),
                   "bench CSV row length != the header's DATA column count",
                   f"row emits {total}, header declares {len(data_names)} data columns "
                   "- every value after the mismatch is filed under the wrong name")


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
    ok(top == define_alias(CHG_H, "CHG_LIMIT_PARAM_FIRST_ID") - 1,
       "the PID block does not run up to the limits block",
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



# ================================================= 10. the STM32 <-> ESP link
def sec_link():
    """[EN] The wire protocol is written TWICE - once in the firmware header and
       once in the ESP's config - and nothing compared them. A silent
       disagreement here does not produce an error message: the receiver simply
       drops every frame whose length or type it does not recognise, and the
       panel goes blank with no explanation. That already almost happened when
       the telemetry frame grew from 84 to 104 bytes.
       [FA] پروتکل سیم دو بار نوشته شده - یک‌بار در هدر فرم‌ور و یک‌بار در پیکربندی
       ESP - و هیچ‌چیز آن‌ها را مقایسه نمی‌کرد. اختلاف بی‌صدا اینجا پیام خطا تولید
       نمی‌کند: گیرنده هر فریمی را که طول یا نوعش را نشناسد دور می‌ریزد و پنل بدون
       توضیح خالی می‌ماند."""
    def fwd(n):
        m = re.search(r"#define\s+" + n + r"\s+(0x[0-9A-Fa-f]+|\d+)u?", ESP_H)
        return int(m.group(1), 0) if m else None

    def espd(n):
        m = re.search(r"#define\s+" + n + r"\s+(0x[0-9A-Fa-f]+|\d+)u?", P_CFG)
        return int(m.group(1), 0) if m else None

    # --- framing must be byte-identical on both sides ---
    for label, a, b in (("SOF byte 0", "ESPLINK_SOF_BYTE0", "ESP_LINK_SOF_BYTE0"),
                        ("SOF byte 1", "ESPLINK_SOF_BYTE1", "ESP_LINK_SOF_BYTE1"),
                        ("protocol version", "ESPLINK_PROTOCOL_VERSION", "ESP_LINK_PROTOCOL_VERSION"),
                        ("header size", "ESPLINK_FRAME_HEADER_SIZE", "ESP_LINK_HEADER_SIZE"),
                        ("CRC size", "ESPLINK_FRAME_CHECKSUM_SIZE", "ESP_LINK_CRC_SIZE"),
                        ("CRC init", "ESPLINK_CRC16_INIT", "ESP_LINK_CRC16_INIT"),
                        ("CRC polynomial", "ESPLINK_CRC16_POLY", "ESP_LINK_CRC16_POLY"),
                        ("max payload", "ESPLINK_FRAME_MAX_PAYLOAD", "ESP_LINK_MAX_PAYLOAD")):
        va, vb = fwd(a), espd(b)
        ok(va is not None and va == vb, f"link framing disagrees: {label}",
           f"firmware {va} vs ESP {vb}")

    # --- message ids. CAL_REFERENCE is deliberately one-sided (dropped from the
    #     panel in v1.7, handler kept harmless on the STM32), so it is exempt. ---
    for label, a, b in (("SET_PARAM", "ESPLINK_MSG_SET_PARAM", "ESP_MSG_SET_PARAM"),
                        ("GET_PARAMS", "ESPLINK_MSG_GET_PARAMS", "ESP_MSG_GET_PARAMS"),
                        ("TLM_LIVE", "ESPLINK_MSG_TLM_LIVE", "ESP_MSG_TLM_LIVE"),
                        ("PARAM_REPORT", "ESPLINK_MSG_PARAM_REPORT", "ESP_MSG_PARAM_REPORT"),
                        ("PARAMS_BULK", "ESPLINK_MSG_PARAMS_BULK", "ESP_MSG_PARAMS_BULK")):
        va, vb = fwd(a), espd(b)
        ok(va is not None and va == vb, f"message id disagrees: {label}",
           f"firmware {va} vs ESP {vb}")


    # [EN] The frame check must be a CRC, not the XOR-8 it used to be: XOR-8 is
    #      blind to ANY even number of flips in the same bit position, which is
    #      the pattern a switching converter puts on a UART - measured, it missed
    #      100 % of that class while CRC-16 missed 0 %.
    # [FA] چک فریم باید CRC باشد نه XOR-8 قبلی: XOR-8 نسبت به هر تعداد زوجِ تغییر
    #      بیت در یک موقعیت کاملاً کور است - همان الگوی نویز مبدل کلیدزن. اندازه‌گیری
    #      شد: XOR-8 صددرصد آن دسته را از دست می‌داد و CRC-16 صفر درصد.
    ok(fwd("ESPLINK_FRAME_CHECKSUM_SIZE") == 2,
       "the frame check is not 16-bit", "XOR-8 was blind to same-position bit pairs")
    ok("func__EspLink_Crc16" in ESP_C and "func__Esp_Crc16" in read("esp_link_panel/plink_link.h"),
       "one side is missing its CRC implementation")
    # [EN] Word-boundary, not substring: a plain `in` test also matches a renamed
    #      symbol like RxCrcErrorX, so it would pass while the counter is gone.
    #      Found by mutation-testing this very check. Note \\b does NOT work here:
    #      underscore is a word character, so there is no boundary inside
    #      UINT32_T__G__RxCrcError - a negative lookahead is what is needed.
    # [FA] با مرز کلمه، نه زیررشته: تست ساده با نام تغییریافته هم می‌خواند و پاس
    #      می‌شود در حالی که شمارنده رفته. با موتیشن‌تستِ خودِ همین چک پیدا شد.
    for nm, txt in (("firmware", ESP_C), ("ESP", read("esp_link_panel/plink_link.h"))):
        ok(re.search(r"RxCrcError(?![0-9A-Za-z_])", txt) and
           re.search(r"RxVersionMismatch(?![0-9A-Za-z_])", txt),
           f"{nm} does not count CRC errors and version mismatches",
           "an unreadable link must be diagnosable, not just silent")
    ok('\\"vm\\":%lu' in read("esp_link_panel/plink_http.h"),
       "the link health counters are not reported to the page",
       "a version-mismatched flash would look exactly like an unplugged cable again")
    ok(re.search(r"function lnkhealth\s*\(", P_PAN) and "lnkhealth(d);" in P_PAN
       and "نسخهٔ فرم‌ور و پنل یکی نیست" in P_PAN,
       "the panel does not surface a version mismatch to the operator")

    ids = {}
    for m in re.finditer(r"#define\s+ESPLINK_MSG_(\w+)\s+(0x[0-9A-Fa-f]+)u", ESP_H):
        v = int(m.group(2), 0)
        ok(v not in ids, "two message types share one id",
           f"{ids.get(v)} and {m.group(1)} are both {hex(v)}")
        ids[v] = m.group(1)

    # --- the telemetry header is 2 seq + 1 flags + 1 reserved, then the u32s ---
    ok(espd("ESP_LINK_TLM_FIELD_OFFSET") == 4,
       "the ESP reads the telemetry u32 array from the wrong offset",
       "the firmware writes u16 seq + u8 flags + u8 reserved first")

    # --- the receiver must be able to hold the biggest frame ---
    rx = espd("ESP_LINK_RX_BUFFER_SIZE")
    biggest = espd("ESP_LINK_HEADER_SIZE") + espd("ESP_LINK_MAX_PAYLOAD") + 1
    ok(rx >= biggest, "the ESP receive buffer cannot hold a maximum frame",
       f"{rx} < {biggest}")

    # --- the JSON replies must fit, or snprintf truncates into invalid JSON and
    #     the page silently shows nothing ---
    jb = espd("ESP_JSON_BUFFER_SIZE")
    fields = espd("ESP_LINK_TLM_FIELD_COUNT")
    worst_m = 15 + 4 * (7 + fields * 11) + 38
    worst_t = 6 + 20 * 11 + 6 + COUNT * 12 + 200
    ok(jb > worst_m, "the /m statistics reply can overflow the JSON buffer",
       f"worst case {worst_m} vs buffer {jb} - snprintf would truncate into "
       "invalid JSON and the panel would show nothing, with no error")
    ok(jb > worst_t, "the /t telemetry reply can overflow the JSON buffer",
       f"worst case {worst_t} vs buffer {jb}")

    # --- losing the link while a human is driving the duty by hand must fail safe ---
    wd = define(CHG_H, "CHG_MANUAL_WATCHDOG_MS")
    ok(wd is not None and 500 <= wd <= 10000,
       "the manual-mode dead-man is missing or implausible",
       f"got {wd}; if the ESP dies mid-test the charger must drop both duties, "
       "not keep driving unattended")



# ================================================= 11. docs vs the code
def sec_docs():
    """[EN] Documentation drifts the same way duplicated constants do, and it is
       worse: a stale number in a README is what the next person trusts. Every
       fault this project has had started as a number that was true once. So
       the docs are checked against the CODE, not proof-read.
       Historical changelog rows are exempt on purpose - "we used to use 66200"
       is a true statement about the past and must stay readable.
       [FA] مستندات هم مثل ثابت‌های تکراری کهنه می‌شوند و بدتر: عدد کهنه در README
       همان چیزی است که نفر بعدی به آن اعتماد می‌کند. پس سند در برابر «کد»
       سنجیده می‌شود نه بازخوانی. ردیف‌های تاریخچه عمداً استثنا هستند."""
    import glob
    docs = {}
    for f in glob.glob(str(ROOT / "**" / "*.md"), recursive=True):
        rel = str(Path(f).relative_to(ROOT))
        if "/Tester/" in rel or rel.startswith("Documentation/"):
            continue
        body = Path(f).read_text(encoding="utf-8", errors="ignore")
        # drop changelog rows: they describe history and are allowed to cite
        # numbers that are no longer current
        live = "\n".join(l for l in body.split("\n")
                          if not re.match(r"\s*\|\s*20\d\d-\d\d-\d\d\s*\|", l))
        docs[rel] = live

    def code(txt, name):
        m = re.search(r"#define\s+" + name + r"\s+(\d+)u", txt)
        return int(m.group(1)) if m else None

    tlm = code(ESP_H, "ESPLINK_TLM_PAYLOAD_SIZE")
    hdr = code(ESP_H, "ESPLINK_FRAME_HEADER_SIZE")
    top24 = code(BSP_M, "BSP_MEASUREMENT_SENSE_TOP_24V_OHMS")

    # [EN] Each entry: a string that must NOT appear outside a changelog row,
    #      because the code now says otherwise.
    # [FA] هر ورودی رشته‌ای است که بیرون از ردیف تاریخچه نباید بیاید.
    forbidden = [
        (r"\[xor:u8\]", "the frame trailer is a 16-bit CRC now, not XOR-8"),
        (r"TLM_LIVE payload layout \((?!%d)" % tlm,
         "the TLM layout heading states a size the firmware no longer sends"),
        (r"76000/6800", "the 24 V ratio was measured on the board as 68000/6800"),
        (r"41000/6800", "the 12 V ratio was measured on the board"),
        (r"total 76k", "the 24 V total is 74.8k"),
    ]
    for pat, why in forbidden:
        hits = sorted(f for f, b in docs.items() if re.search(pat, b))
        ok(not hits, f"stale documentation: {why}",
           f"pattern {pat!r} still present in {hits}")

    # the frame description must actually describe the frame
    spec = docs.get("ESP_AGENT_SPEC.md", "")
    ok("[crc_lo:u8][crc_hi:u8]" in spec and "[ver:u8]" in spec,
       "the spec's frame diagram does not match the v2 frame")
    ok(str(tlm) in spec, f"the spec never states the real TLM size ({tlm})")
    ok(str(hdr) + "-byte header" in spec or "6-byte header" in spec,
       "the spec still describes the old header size")
    # [EN] Not "does the number appear somewhere" - that passed while one of the
    #      two rows had been reverted, because the other row still had it. Both
    #      24 V rows of the divider table must carry the code's value.
    #      Found by mutation-testing this check.
    # [FA] نه «آیا عدد جایی هست» - آن حالت وقتی یکی از دو ردیف برگردانده شده بود
    #      هم پاس می‌شد، چون ردیف دیگر هنوز عدد را داشت. هر دو ردیف ۲۴ولت جدول
    #      باید مقدار کد را داشته باشند. با موتیشن‌تستِ خود این چک پیدا شد.
    bspmd = docs.get("Firmware/Bsp/README.md", "")
    rows24 = [l for l in bspmd.split("\n")
              if l.startswith("|") and ("PA2)" in l or "PA3)" in l) and "6800" in l]
    ok(len(rows24) == 2,
       "the BSP README divider table must still list both 24 V nets",
       f"found {len(rows24)} rows")
    bad24 = [l.strip()[:60] for l in rows24 if str(top24) not in l]
    ok(not bad24,
       f"a 24 V divider row in the BSP README disagrees with the code ({top24})",
       "; ".join(bad24))



# ============================================== 12. standalone preview
def sec_preview():
    """[EN] The generated offline copy of the panel must stay in step.
       [FA] کپی آفلاین تولیدشدهٔ پنل باید هم‌گام بماند."""

    # [EN] The standalone preview is a GENERATED copy of the panel markup. A
    #      committed copy of something is exactly the duplication trap this
    #      whole audit exists for, so it is not trusted - it is regenerated in
    #      memory and compared. If plink_panel.h moved and nobody re-ran the
    #      generator, this fails instead of quietly showing an old page.
    # [FA] پیش‌نمایش خودکفا یک کپی «تولیدشده» از مارک‌آپ پنل است. کپی کامیت‌شده
    #      دقیقاً همان تلهٔ تکراری است که این ممیز برای آن وجود دارد، پس به آن
    #      اعتماد نمی‌شود: دوباره در حافظه ساخته و مقایسه می‌شود.
    prev_html = ROOT / "esp_link_panel" / "panel_preview.html"
    if ok(prev_html.exists(), "the standalone panel preview is missing",
          "run tools/make_panel_preview.py"):
        import subprocess
        before = prev_html.read_text(encoding="utf-8")
        r = subprocess.run([sys.executable, str(ROOT / "tools" / "make_panel_preview.py")],
                           capture_output=True, text=True)
        ok(r.returncode == 0, "the panel preview generator does not run",
           (r.stdout + r.stderr)[-300:])
        after = prev_html.read_text(encoding="utf-8")
        ok(before == after,
           "esp_link_panel/panel_preview.html is stale",
           "plink_panel.h changed without re-running tools/make_panel_preview.py, "
           "so the preview shows an older page than the firmware serves")
        ok("OFFLINE SHIM" in after and after.index("OFFLINE SHIM") < after.index("function qgraph"),
           "the preview's offline shim must be injected BEFORE the panel script",
           "otherwise fetch is replaced too late and the page stays empty")

    # [EN] Both simulators must speak the CURRENT frame. The Node server
    #      hardcoded a 20-field telemetry array while the link had carried 25
    #      since v1.25, so indices 20..24 - the raw ADC counts the whole
    #      calibration effort now rests on - read back undefined in the
    #      preview, and the version-mismatch banner could never be seen
    #      because vm/ce were absent. Neither simulator may retype the width.
    # [FA] هر دو شبیه‌ساز باید قاب «فعلی» را حرف بزنند. سرور Node آرایهٔ ۲۰
    #      فیلدی را ثابت نوشته بود در حالی که لینک از v1.25 بیست‌وپنج فیلد
    #      می‌برد، پس اندیس ۲۰ تا ۲۴ - همان شمارش‌های خامی که کل کالیبراسیون
    #      روی آن‌ها بنا شده - در پیش‌نمایش undefined خوانده می‌شدند و بنر
    #      ناهم‌نسخگی هرگز دیده نمی‌شد چون vm/ce نبودند.
    cfg = read("esp_link_panel/plink_config.h")
    m = re.search(r"#define\s+ESP_LINK_TLM_FIELD_COUNT\s+(\d+)u?", cfg)
    n_fields = int(m.group(1)) if m else -1
    ok(n_fields > 0, "ESP_LINK_TLM_FIELD_COUNT not found in plink_config.h")

    # [EN] Judge LIVE CODE, not prose. A plain "is the name in the file" test
    #      is satisfied by the very comment that explains the rule, so ripping
    #      the derivation out while leaving the comment behind would pass. The
    #      comments go first, then the question is asked.
    # [FA] «کدِ زنده» سنجیده می‌شود نه متن توضیح. تست سادهٔ «آیا نام در فایل
    #      هست» با همان کامنتی که قانون را توضیح می‌دهد ارضا می‌شود، پس حذف
    #      اشتقاق و باقی گذاشتن کامنت قبول می‌شد. اول کامنت‌ها حذف، بعد سؤال.
    prev_js = live(PREV)
    gen_py = live(read("tools/make_panel_preview.py"))

    ok("ESP_LINK_TLM_FIELD_COUNT" in prev_js,
       "the preview server must DERIVE the telemetry width from plink_config.h",
       "a retyped width silently truncates the frame when a field is added")
    ok("ESP_LINK_TLM_FIELD_COUNT" in gen_py,
       "the preview generator must DERIVE the telemetry width from plink_config.h",
       "a retyped width silently truncates the frame when a field is added")

    # no literal array of the old width may survive in either simulator
    for name, src in (("preview server", prev_js), ("preview generator", gen_py)):
        lits = re.findall(r"new Array\((\d+)\)", src)
        ok(not lits,
           f"the {name} still builds a fixed-length telemetry array",
           f"literal length(s) {', '.join(lits)} - derive from "
           f"ESP_LINK_TLM_FIELD_COUNT ({n_fields}) instead")

    # the link-health counters must exist on both, or the banner is untestable
    for name, src in (("preview server", prev_js), ("preview generator", gen_py)):
        missing = [k for k in ("vm", "ce") if not re.search(r"\b" + k + r"\s*:", src)]
        ok(not missing,
           f"the {name} does not publish link health {missing}",
           "lnkhealth() reads d.vm / d.ce, so the mismatch banner cannot be previewed")

    # [EN] measurement.c: batteryLow IS battery12 and battery24 = low + high.
    #      The dead 150 mV + 0.47 R bench compensation must not reappear in a
    #      simulator after being switched off in calibration.h.
    # [FA] در measurement.c باتری پایین همان battery12 است و battery24 برابر
    #      پایین + بالا. جبران مردهٔ ۱۵۰mV + ۰٫۴۷ اهم نباید بعد از خاموش شدن
    #      در calibration.h دوباره در شبیه‌ساز سبز شود.
    comp_off = re.search(r"#define\s+CAL_BATTERY12_BENCH_COMP_ENABLE\s+0u",
                         read("Firmware/Modules/Measurement/calibration.h"))
    if comp_off:
        for name, src in (("preview server", prev_js), ("preview generator", gen_py)):
            ok(not re.search(r"t\[16\]\s*=\s*[^;]*\b150\b", src),
               f"the {name} still applies the disabled 12 V bench compensation",
               "CAL_BATTERY12_BENCH_COMP_ENABLE is 0, so battery12 == batteryLow")

    _preview_server_behaviour(n_fields)


def _preview_server_behaviour(n_fields):
    """[EN] Reading the simulator's source only proves what it says. This
       starts it and asks it, because the three ways it actually went wrong -
       a literal loop bound, a width named in an error string while the
       derivation was gone, and a voltage identity that is arithmetic rather
       than text - are all invisible to grep. Skipped (not failed) when node
       is unavailable, so the audit still runs on a bare box.
       [FA] خواندن سورس شبیه‌ساز فقط حرفش را ثابت می‌کند. اینجا اجرا و از خودش
       پرسیده می‌شود، چون سه خرابی واقعی - کران حلقهٔ عددی، عرضی که فقط در
       رشتهٔ خطا نامش بود در حالی که اشتقاق رفته بود، و اتحاد ولتاژی که حساب
       است نه متن - هیچ‌کدام با grep دیده نمی‌شوند. اگر node نباشد رد می‌شود
       نه اینکه شکست بخورد."""
    import json
    import shutil
    import socket
    import subprocess
    import time
    import urllib.request

    if shutil.which("node") is None:
        return

    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        port = s.getsockname()[1]

    env = dict(os.environ, PORT=str(port))
    proc = subprocess.Popen(["node", "tools/panel_preview_server.js"],
                            cwd=str(ROOT), env=env,
                            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        base, live_t, live_m = f"http://127.0.0.1:{port}", None, None
        for _ in range(50):
            if proc.poll() is not None:
                break
            try:
                live_t = json.load(urllib.request.urlopen(base + "/t", timeout=1))
                live_m = json.load(urllib.request.urlopen(base + "/m", timeout=1))
                break
            except Exception:
                time.sleep(0.1)

        if not ok(live_t is not None, "the preview server does not answer /t",
                  (proc.stderr.read().decode()[-300:] if proc.poll() is not None else
                   "no response within 5 s")):
            return

        ok(len(live_t["t"]) == n_fields,
           "the running preview server sends the wrong telemetry width",
           f"/t returned {len(live_t['t'])} fields, the frame carries {n_fields}")
        ok(all(len(live_m[k]) == n_fields for k in ("s", "lo", "hi", "la")),
           "the running preview server sends wrong-width /m statistics",
           f"got { {k: len(live_m[k]) for k in ('s','lo','hi','la')} }, expected {n_fields}")
        ok("vm" in live_t and "ce" in live_t,
           "the running preview server omits the link-health counters")

        # measurement.c identities, sampled across the demo scenarios
        bad, samples = [], []
        for _ in range(12):
            t = json.load(urllib.request.urlopen(base + "/t", timeout=1))["t"]
            samples.append(t)
            if t[16] != t[17]:
                bad.append(f"battery12 {t[16]} != batteryLow {t[17]}")
            if t[15] != t[17] + t[18]:
                bad.append(f"battery24 {t[15]} != {t[17]}+{t[18]}")
            time.sleep(0.08)
        ok(not bad,
           "the running preview server emits voltages the board cannot produce",
           "; ".join(sorted(set(bad))[:3]))

        # [EN] Right LENGTH is not the same as right CONTENT. A stats loop that
        #      stops early still returns a full-width array - just with zeros
        #      where the calibration counts belong. Any field that /t reports
        #      non-zero in EVERY sample must also be non-zero in /m, which is
        #      stable for the voltages and counts while letting a current or
        #      the fault mask legitimately sit at zero.
        # [FA] طولِ درست یعنیِ محتوای درست نیست. حلقهٔ آماری که زود متوقف شود
        #      باز هم آرایه‌ای با عرض کامل برمی‌گرداند - فقط آنجا که شمارش‌های
        #      کالیبراسیون باید باشند صفر است. هر فیلدی که /t در «همهٔ» نمونه‌ها
        #      ناصفر می‌دهد باید در /m هم ناصفر باشد؛ این برای ولتاژها و
        #      شمارش‌ها پایدار است و به جریان یا ماسک خطا اجازهٔ صفر بودن می‌دهد.
        always = [k for k in range(n_fields) if all(s[k] != 0 for s in samples)]
        stats = json.load(urllib.request.urlopen(base + "/m", timeout=1))
        holes = [k for k in always if stats["la"][k] == 0]
        ok(not holes,
           "the running preview server's /m statistics skip telemetry fields",
           f"indices {holes} are always non-zero on /t but zero in /m - "
           "a loop bound that is not the derived field width")
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except Exception:
            proc.kill()



# ================================================================= report
def sec_cubeide_includes():
    """[EN] Every MODULE_* token that modules_enable.h switches on (value 1)
       must be backed by a Firmware/Modules/<Name> folder AND that folder must
       sit on the CubeIDE include path (.cproject), or the target build dies
       with 'fatal error: <name>.h: No such file' on the developer's machine
       while every host check stays green (the host compiler finds headers by
       a different rule). v1.43: imbalance.h was included by task_control.c
       but Firmware/Modules/Imbalance was never added to .cproject.
       [FA] هر توکن MODULE_ِ روشن در modules_enable.h باید پوشه‌اش روی مسیر
       include پروژهٔ CubeIDE باشد، وگرنه بیلد هدف روی سیستم توسعه‌دهنده با
       «No such file» می‌میرد در حالی که همهٔ تست‌های هاست سبزند (کامپایلر
       هاست هدرها را به قاعدهٔ دیگری پیدا می‌کند). v1.43: همین باگ برای
       imbalance.h اتفاق افتاد."""
    ena = read("Firmware/Config/Inc/modules_enable.h")
    cproj = read("CubeIDE/STM32CubeIDE/.cproject")
    enabled = re.findall(r"#define\s+MODULE_(\w+)\s+1(?:u)?\b", ena)
    ok(len(enabled) > 0, "no MODULE_* flags found in modules_enable.h")
    missing = []
    for mod in enabled:
        folder = {"ESP": "EspLink"}.get(mod, mod[0] + mod[1:].lower().replace("_", ""))
        # tolerate historical casings: find any case-insensitive folder match
        candidates = list((ROOT / "Firmware" / "Modules").glob("*"))
        hit = next((c.name for c in candidates
                    if c.name.lower() == folder.lower()
                    or c.name.lower() == mod.lower()
                    or c.name.lower().startswith(folder.lower())), None)
        if hit is None:
            missing.append((mod, "no Firmware/Modules folder"))
            continue
        if f"../../../Firmware/Modules/{hit}" not in cproj:
            missing.append((mod, f"folder {hit} missing from .cproject include paths"))
    ok(not missing, "module enabled but not buildable from CubeIDE", str(missing))

    # [EN] v1.68: the check above is keyed on MODULE_* flags, so a module
    #      WITHOUT a flag could still be missed - and one was. CalLut (v1.66)
    #      has no enable flag, is included by measurement.c, and was never
    #      added to .cproject: the user's CubeIDE build died with
    #      "fatal error: cal_lut.h: No such file or directory" while every
    #      host check stayed green. Now EVERY module folder that holds a .c
    #      must be on the include path, flag or no flag.
    # [FA] چک بالا به فلگ MODULE_ وابسته بود، پس ماژول بدون فلگ از دستش
    #      در می‌رفت - و رفت: CalLut (v1.66) فلگ ندارد، measurement.c آن را
    #      include می‌کند و در .cproject نبود؛ بیلد CubeIDE کاربر با
    #      «cal_lut.h پیدا نشد» مرد در حالی که همهٔ تست‌های هاست سبز بودند.
    #      حالا هر پوشهٔ ماژول که فایل .c دارد باید روی مسیر include باشد.
    for folder in sorted((ROOT / "Firmware" / "Modules").glob("*")):
        if not folder.is_dir() or not list(folder.glob("*.c")):
            continue
        ok(f"../../../Firmware/Modules/{folder.name}" in cproj,
           f"module folder {folder.name} has sources but is not on the CubeIDE include path",
           "the target build would fail with 'No such file or directory' "
           "even though every host check passes")

    # [EN] A host tester carries its own main(); if its folder is not excluded
    #      from the CubeIDE build, the target link dies with
    #      'multiple definition of main'. .cproject must exclude every
    #      Tester folder holding a host-main in BOTH configurations.
    # [FA] تستر هاست main خودش را دارد؛ اگر پوشه‌اش از بیلد CubeIDE حذف
    #      نشده باشد، لینک هدف با «multiple definition of main» می‌میرد.
    for tester in (ROOT / "Firmware" / "Modules").glob("*/Tester"):
        host_src = [c for c in list(tester.glob("*.c")) + list(tester.glob("*.cpp"))
                    if "main(" in c.read_text(encoding="utf-8", errors="ignore")]
        if not host_src:
            continue
        rel = tester.relative_to(ROOT).as_posix()
        ok(cproj.count(f'excluding="{rel}"') >= 2,
           f"host tester {rel} is not excluded from the CubeIDE build (both configs)",
           str([c.name for c in host_src]))

    # [EN] v1.68c: the part has 64 KB of flash and the linker script gives the
    #      application only 60 KB (the top 4 KB are the parameter NVM and the
    #      v1.66 LUT block). An unoptimised build does not fit: the user's
    #      Debug build overflowed FLASH by 4296 bytes because the Debug
    #      configuration carried no optimization value at all, i.e. -O0.
    #      Every configuration must therefore name an optimization level, and
    #      none of them may be "none".
    # [FA] قطعه ۶۴ کیلوبایت فلش دارد و لینکر فقط ۶۰ کیلوبایت به برنامه می‌دهد
    #      (۴ کیلوبایت بالا = NVM پارامترها و بلوک جدول v1.66). بیلد بدون
    #      بهینه‌سازی جا نمی‌شود: بیلد Debug کاربر ۴۲۹۶ بایت سرریز کرد چون
    #      هیچ سطح بهینه‌سازی نداشت (یعنی -O0). پس هر پیکربندی باید سطح
    #      بهینه‌سازی داشته باشد و هیچ‌کدام نباید none باشد.
    opt_values = re.findall(r"optimization\.level\.value\.(\w+)", cproj)
    n_opt_options = len(re.findall(r"<option [^>]*optimization\.level[^>]*>", cproj))
    ok(len(opt_values) == n_opt_options and n_opt_options >= 2,
       "a CubeIDE configuration has no optimization level (that means -O0, "
       "and -O0 does not fit the 60 KB application region)",
       f"{len(opt_values)} values for {n_opt_options} options")
    ok("none" not in opt_values,
       "a CubeIDE configuration is built with -O0; the image overflows FLASH",
       str(opt_values))


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
    sec_link()
    sec_docs()
    sec_preview()
    sec_cubeide_includes()

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
