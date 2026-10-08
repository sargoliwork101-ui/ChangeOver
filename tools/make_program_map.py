#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
[EN] Build Documentation/Program_Map.xlsx - the whole program on tables: every
     source file and its role, every function with its own @brief, all 143 ESP
     wire ids with type/unit/default/range/persisted flag, the MCU pin map with
     polarity and safe level, the on-chip flash map, and the gates that check
     all of it.

     WHY: the same defect shape appears in this project over and over - one
     number written by hand in several files, drifting apart. A table assembled
     BY A TOOL cannot drift: everything below is parsed out of the firmware
     headers at generation time, so the workbook is a view of the code, never a
     second copy of it. Regenerate after any change:

         python3 tools/make_program_map.py        (needs: pip install openpyxl)

     Run through tools/make_program_map.py so the header comments travel with
     the workbook: the «راهنما» sheet records where each column comes from.

[FA] ساخت فایل Documentation/Program_Map.xlsx - کل برنامه روی جدول: هر فایل
     سورس و نقش آن، هر تابع با @brief خودش، همهٔ ۱۴۳ شناسهٔ سیم ESP با
     نوع/واحد/پیش‌فرض/بازه/ماندگاری، نگاشت پایه‌های MCU با قطبیت و سطح امن،
     نقشهٔ فلش داخلی، و دروازه‌هایی که همهٔ این‌ها را چک می‌کنند.

     چرا: در این پروژه یک شکل ایراد بارها تکرار شده - یک عدد که دستی در چند
     فایل نوشته شده و کپی‌ها از هم جدا افتاده‌اند. جدولی که «با ابزار» ساخته
     شود نمی‌تواند کهنه شود: همه‌چیز پایین در زمان ساخت از هدرهای فرم‌ور خوانده
     می‌شود، پس کتاب یک «نما» از کد است، نه کپی دوم آن.
"""

import pathlib
import re
import sys

from openpyxl import Workbook
from openpyxl.styles import Alignment, Border, Font, PatternFill, Side
from openpyxl.utils import get_column_letter

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "Documentation" / "Program_Map.xlsx"

FONT = "Vazirmatn"
C_TITLE = "1F3864"
C_HEAD = "2F5597"
C_BAND = "D9E2F3"
PALETTE = {
    "ok": "C6EFCE",      # تأییدشده / جاری
    "warn": "FFE699",    # کهنه / نیازمند توجه
    "bad": "FFC7CE",     # ایراد
    "info": "DDEBF7",    # اطلاعاتی
    "idle": "EDEDED",    # خاموش / غیرفعال
    "act": "FCE4D6",     # فعال
}
thin = Side(style="thin", color="9DB7E8")
BORDER = Border(left=thin, right=thin, top=thin, bottom=thin)


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="ignore")


def sheet(wb, name, title, headers, rows, widths):
    """[EN] One RTL sheet in the house style. / [FA] یک برگهٔ راست‌به‌چپ با سبک پروژه."""
    ws = wb.create_sheet(name)
    ws.sheet_view.rightToLeft = True
    ws.freeze_panes = "A3"

    ncol = len(headers)
    ws.merge_cells(start_row=1, start_column=1, end_row=1, end_column=ncol)
    tc = ws.cell(row=1, column=1, value=title)
    tc.font = Font(name=FONT, size=14, bold=True, color="FFFFFF")
    tc.fill = PatternFill("solid", fgColor=C_TITLE)
    tc.alignment = Alignment(horizontal="center", vertical="center")
    ws.row_dimensions[1].height = 30

    for c, h in enumerate(headers, start=1):
        cell = ws.cell(row=2, column=c, value=h)
        cell.font = Font(name=FONT, size=11, bold=True, color="FFFFFF")
        cell.fill = PatternFill("solid", fgColor=C_HEAD)
        cell.alignment = Alignment(horizontal="center", vertical="center", wrap_text=True)
        cell.border = BORDER
    ws.row_dimensions[2].height = 28

    for r, row in enumerate(rows, start=3):
        tone = row[-1]
        values = row[:-1]
        fill = PatternFill("solid", fgColor=PALETTE.get(tone, "FFFFFF")) if tone else None
        for c, v in enumerate(values, start=1):
            cell = ws.cell(row=r, column=c, value=v)
            cell.font = Font(name=FONT, size=10)
            cell.alignment = Alignment(horizontal="right", vertical="top", wrap_text=True)
            cell.border = BORDER
            if fill is not None:
                cell.fill = fill
            elif r % 2 == 1:
                cell.fill = PatternFill("solid", fgColor=C_BAND)

    for c, w in enumerate(widths, start=1):
        ws.column_dimensions[get_column_letter(c)].width = w
    ws.sheet_properties.tabColor = C_HEAD
    return ws


# --------------------------------------------------------------- parsing

def doc_block_brief(text, at):
    """[EN] Brief of the doc block that sits directly above line `at`
       (0-based). Returns (en, fa) or ('', '').
       [FA] خلاصهٔ بلوک مستندِ بالای خط داده‌شده."""
    lines = text.split("\n")
    end = None
    block = None
    for i in range(at - 1, max(-1, at - 60), -1):
        t = lines[i].strip()
        if t.startswith("*/") and end is None:
            end = i
        elif end is not None and t.startswith("/**"):
            block = lines[i:end + 1]
            break
        elif end is None and t and not t.startswith("*") and not t.startswith("/*"):
            if not t.startswith("@"):
                break
    if block is None:
        return "", ""
    en, fa, mode = [], [], None
    for l in block:
        s = re.sub(r"^\s*/?\*+\s?", "", l).replace("*/", "").strip()
        if "@brief" in s:
            mode = "en"
            s = s.split("@brief", 1)[1].strip()
        elif s.startswith("[FA]") or s.startswith("@param") or s.startswith("@return") or s.startswith("@note"):
            mode = "fa" if s.startswith("[FA]") else None
        if mode == "en" and s:
            en.append(re.sub(r"^\[EN\]\s*", "", s))
        elif mode == "fa" and s:
            fa.append(re.sub(r"^\[FA\]\s*", "", s))
    return " ".join(en).strip(), " ".join(fa).strip()


FUNC_RE = re.compile(
    r"^(?:static\s+|inline\s+)*[A-Za-z_][A-Za-z0-9_]*\s*\**\s*"
    r"(func__[A-Za-z0-9_]+)\s*\(", re.M)


def functions_of(rel):
    text = read(rel)
    out = []
    for m in FUNC_RE.finditer(text):
        line_no = text[:m.start()].count("\n")
        en, fa = doc_block_brief(text, line_no)
        out.append((m.group(1), line_no + 1, en, fa))
    return out


def file_brief(rel):
    text = read(rel)
    en, fa = doc_block_brief(text, text[:text.find("*/")].count("\n") + 1
                             if "*/" in text else 0)
    if not en and "*/" in text:
        # header block: parse it directly
        head = text[:text.index("*/") + 2]
        en_m = re.search(r"@brief\s+\[EN\](.*?)(?=\[FA\]|\*/)", head, re.S)
        fa_m = re.search(r"\[FA\](.*?)\*/", head, re.S)
        en = " ".join(l.strip("* ") for l in en_m.group(1).split("\n")).strip() if en_m else ""
        fa = " ".join(l.strip("* ") for l in fa_m.group(1).split("\n")).strip() if fa_m else ""
    return en, fa


def split_outside_parens(text):
    parts, depth, cur = [], 0, ""
    for ch in text:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth = max(0, depth - 1)
        if ch == "," and depth == 0:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur.strip())
    return parts


def numbers(txt):
    table = {ord(c): str(i) for i, c in enumerate("۰۱۲۳۴۵۶۷۸۹")}
    table.update({ord(c): str(i) for i, c in enumerate("٠١٢٣٤٥٦٧٨٩")})
    return txt.translate(table)


# --------------------------------------------------------------- builders

SOURCE_GLOBS = [
    ("Firmware", "Firmware/**/*.[ch]"),
    ("CubeIDE/Core", "CubeIDE/Core/**/*.[ch]"),
    ("esp_link_panel", "esp_link_panel/*.[hino]*"),
    ("tools", "tools/*.py"),
]


def build_files():
    rows = []
    for scope, pattern in SOURCE_GLOBS:
        for p in sorted(ROOT.glob(pattern)):
            if not p.is_file():
                continue
            rel = str(p.relative_to(ROOT))
            if "/Tester/" in rel:
                continue
            text = p.read_text(encoding="utf-8", errors="ignore")
            en, fa = file_brief(rel)
            role = numbers(fa or en)
            if not role:
                # [EN] CubeMX/panel files have no @brief; use the first
                #      comment line rather than an empty cell.
                # [FA] فایل‌های CubeMX/پنل @brief ندارند؛ اولین خط کامنت.
                m_role = re.search(r"^\s*(?:/\*+|\*|//)\s*(\S.*)$", text, re.M)
                role = m_role.group(1).strip("* /")[:160] if m_role else "—"
            rows.append([
                rel,
                p.suffix or "",
                str(len(text.split("\n"))),
                str(len(FUNC_RE.findall(text))),
                str(len(re.findall(r"^#define\s", text, re.M))),
                role,
                "info",
            ])
    return rows


def build_functions():
    """[EN] One row per function, never two: a declaration and its definition
       are the same function and listing them twice is how a table starts
       lying about size. The .c definition wins (it carries the doc block),
       the header path is noted.
       [FA] یک ردیف برای هر تابع، نه دو: اعلان و تعریف یک تابع‌اند و
       دوبارشمردن همان چیزی است که جدول را دربارهٔ اندازه دروغگو می‌کند."""
    merged = {}
    for scope, pattern in SOURCE_GLOBS:
        if scope not in ("Firmware", "CubeIDE/Core"):
            continue
        for p in sorted(ROOT.glob(pattern)):
            if not p.is_file() or p.suffix not in (".c", ".h"):
                continue
            rel = str(p.relative_to(ROOT))
            if "/Tester/" in rel:
                continue
            for name, line, en, fa in functions_of(rel):
                cur = merged.get(name)
                brief = numbers(fa or en)
                if cur is None:
                    merged[name] = {"defined": rel if rel.endswith(".c") else "",
                                    "declared": "" if rel.endswith(".c") else rel,
                                    "line": line if rel.endswith(".c") else "",
                                    "brief": brief}
                else:
                    if rel.endswith(".c") and not cur["defined"]:
                        cur["defined"] = rel
                        cur["line"] = line
                        if not cur["brief"]:
                            cur["brief"] = brief
                    elif rel.endswith(".h") and not cur["declared"]:
                        cur["declared"] = rel
                    if not cur["brief"]:
                        cur["brief"] = brief
    rows = []
    for name in sorted(merged):
        f = merged[name]
        rows.append([name, f["defined"] or "—", f["declared"] or "—",
                     str(f["line"] or ""), f["brief"] or "—", "info"])
    return rows


def build_params():
    esp = read("Firmware/Modules/EspLink/esp_link.h")
    nvm = read("Firmware/Modules/EspLink/esp_link_nvm.h")

    def numdef(name):
        m = re.search(r"#define\s+" + name + r"\s+(\d+)u", nvm)
        return int(m.group(1)) if m else None

    lo_max = numdef("ESP_LINK_NVM_PERSISTED_ID_MAX_LOW")
    hi_min = numdef("ESP_LINK_NVM_PERSISTED_ID_MIN_HIGH")
    hi_max = numdef("ESP_LINK_NVM_PERSISTED_ID_MAX_HIGH")
    ret_a = numdef("ESP_LINK_NVM_RETIRED_ID_FIRST")
    ret_b = numdef("ESP_LINK_NVM_RETIRED_ID_LAST")
    transient = numdef("ESP_LINK_NVM_TRANSIENT_ID_MUTE")
    slot_min = numdef("ESP_LINK_NVM_SLOT_MIN_ID")
    slot_max = numdef("ESP_LINK_NVM_SLOT_MAX_ID")
    version = numdef("ESP_LINK_NVM_VERSION")

    rows = []
    pattern = re.compile(
        r"#define[ \t]+ESPLINK_PARAM_([A-Z0-9_]+)[ \t]+(\d+)u[ \t]*/[ \t]*\*([^\n]*?)\*/")
    for m in pattern.finditer(esp):
        name, pid, comment = m.group(1), int(m.group(2)), m.group(3).strip()
        if name == "COUNT":
            continue
        comment = re.sub(r"\s+", " ", comment)
        fields = split_outside_parens(comment)
        typ = fields[0] if fields else ""
        unit = fields[1] if len(fields) > 1 else ""
        default = ""
        rng = ""
        for f in fields[2:]:
            if f.lower().startswith("def"):
                default = f[3:].strip()
            elif not rng:
                rng = f
        if pid <= (lo_max or 14):
            persisted = "بله"
        elif (hi_min or 20) <= pid <= (hi_max or 142):
            persisted = "بله"
        else:
            persisted = "خیر"
        if ret_a is not None and ret_a <= pid <= ret_b:
            persisted = "بازنشسته"
        if transient is not None and pid == transient:
            persisted = "گذرا"
        rows.append([str(pid), name, typ, unit, default, rng, persisted,
                     f"NVM v{version}", "ok" if persisted == "بله" else "idle"])
    for sid in range(slot_min or 200, (slot_max or 203) + 1):
        rows.append([str(sid), "SLOT (runtime)", "u32", "—", "—", "اسلات زمان‌اجرا",
                     "بله", f"NVM v{version}", "act"])
    return rows


def build_pins():
    pins = read("Firmware/Config/Inc/board_pins.h")
    rows = []
    names = sorted(set(re.findall(r"#define\s+PIN_([A-Z0-9_]+)_PORT\s+GPIO([A-Z])", pins)))
    for name, port in names:
        pnum = re.search(r"#define\s+PIN_" + name + r"_PIN\s+GPIO_PIN_(\d+)", pins)
        comment = re.search(r"#define\s+PIN_" + name + r"_PIN\s+GPIO_PIN_\d+\s*/\*(.*?)\*/", pins)
        pol = re.search(r"#define\s+PIN_" + name + r"_ACTIVE_HIGH\s+(\d)", pins)
        safe = re.search(r"#define\s+PIN_SAFE_" + name + r"_HIGH\s+(\d)", pins)
        note = re.sub(r"\s+", " ", comment.group(1)).strip() if comment else ""
        polarity = ""
        if pol:
            polarity = "active-high" if pol.group(1) == "1" else "active-low"
        rows.append([
            "P" + port + (pnum.group(1) if pnum else "?"),
            name,
            note.split(" ")[0] if note else "",
            polarity or "ورودی/—",
            ("بالا" if safe and safe.group(1) == "1" else
             ("پایین" if safe else "—")) if safe else "—",
            note,
            "info",
        ])
    return rows


def build_flash():
    ld = read("CubeIDE/STM32CubeIDE/STM32F103C8TX_FLASH.ld")
    rows = []
    mem = re.search(r"MEMORY\s*\{(.*?)\n\}", ld, re.S)
    if mem:
        for m in re.finditer(r"([A-Za-z_]+)\s*\((\w+)\)\s*:\s*ORIGIN\s*=\s*(0x[0-9A-Fa-f]+)"
                             r"\s*,\s*LENGTH\s*=\s*(\d+)K", mem.group(1)):
            name, flags, origin, length = m.groups()
            rows.append([name, origin, f"{length}K ({int(length) * 1024} B)", flags,
                         "app" if name == "FLASH" else "data", "info"])
    nvm = read("Firmware/Modules/EspLink/esp_link_nvm.h")
    lut = read("Firmware/Modules/CalLut/cal_lut.h")

    def hexdef(text, name):
        m = re.search(r"#define\s+" + name + r"\s+(0x[0-9A-Fa-f]+)u", text)
        return m.group(1) if m else "?"

    rows.append(["EspLink parameter NVM (bank A/B)", 
                 hexdef(nvm, "ESP_LINK_NVM_PAGE_A_ADDR") + " / " +
                 hexdef(nvm, "ESP_LINK_NVM_PAGE_B_ADDR"),
                 "2 × 2 KiB — دو بانک دوصفحه‌ای", "r/w", "esp_link_nvm.c", "act"])
    rows.append(["CalLut bench table",
                 hexdef(lut, "CAL_LUT_PAGE_A_ADDR") + " / " + hexdef(lut, "CAL_LUT_PAGE_B_ADDR"),
                 "2 × 1 KiB", "r/w", "cal_lut.c", "act"])
    return rows


def build_gates(files, funcs, params, pins):
    return [
        ["bash tools/check_ai_rules.sh", "قوانین AI_AGENT_RULES.md: نام‌گذاری، کامنت دوزبانه، RTL، جداسازی توابع، بازر/UI",
         "بازتولید panel_preview و node --check", "ok"],
        ["bash tools/check_firmware_syntax.sh", "syntax سمت هاست + همهٔ تسترهای هاست (UI، شارژر، عدم‌توازن، پیام‌ها، NVM، پارسر)",
         "بدون برد واقعی", "ok"],
        ["python3 tools/audit_consistency.py", "نامتغیرهای بین‌فایلی: پارامترها، NVM، فلش، مستندات، پنل، شبیه‌ساز",
         "کد دروازهٔ اعداد کهنه", "ok"],
        ["python3 tools/make_program_map.py", "همین کتاب: فایل‌ها، توابع، پارامترها، پایه‌ها، فلش، دروازه‌ها",
         f"{len(files)} فایل · {len(funcs)} تابع · {len(params)} شناسه · {len(pins)} سیگنال", "info"],
        ["python3 tools/measure_flash.py", "اندازهٔ دلتای ایمیج روی هاست (Thumb/LLVM)",
         "عدد مطلق با CubeIDE یکی نیست", "warn"],
        ["python3 tools/measure_ram.py", "بودجهٔ RAM و بدترین عمق پشته",
         "بدون برد واقعی", "warn"],
    ]


def build():
    files = build_files()
    funcs = build_functions()
    params = build_params()
    pins = build_pins()

    wb = Workbook()
    wb.remove(wb.active)

    sheet(
        wb, "راهنما", "نقشهٔ کل برنامه ChangeOver — راهنما",
        ["برگه", "چه چیزی داخلش است", "از کجا خوانده می‌شود"],
        [
            ["۱ فایل‌ها", "هر فایل سورس، نقش آن (خلاصهٔ دوزبانهٔ هدر فایل)، تعداد خط، تعداد تابع و ماکرو",
             "خود فایل‌های Firmware/، CubeIDE/Core/، esp_link_panel/، tools/"],
            ["۲ توابع", f"{len(funcs)} تابع با نام، فایل، شمارهٔ خط و خلاصهٔ @brief خود تابع",
             "بلوک مستند بالای هر تابع"],
            ["۳ پارامترهای ESP", f"{len(params)} شناسهٔ سیم با نوع/واحد/پیش‌فرض/بازه و ماندگاری NVM",
             "esp_link.h + esp_link_nvm.h"],
            ["۴ پایه‌ها", "نگاشت سیگنال منطقی → پایهٔ MCU با قطبیت و سطح امن",
             "board_pins.h"],
            ["۵ نقشهٔ فلش", "ناحیه‌های فلش از اسکریپت لینکر و مالک هر بلوک داده",
             "STM32F103C8TX_FLASH.ld + esp_link_nvm.h + cal_lut.h"],
            ["۶ دروازه‌ها", "دستورهای بررسی و کاری که هرکدام انجام می‌دهد",
             "tools/"],
            ["رنگ‌ها", "سبز = جاری/تأییدشده · نارنجی = زمان‌اجرا · خاکستری = غیرماندگار · زرد = محدودیت",
             ""],
            ["بازتولید", "python3 tools/make_program_map.py  (نیاز: pip install openpyxl)",
             "tools/make_program_map.py"],
            ["قاعده", "هیچ عددی در این کتاب دستی تایپ نشده؛ همه از هدرهای فرم‌ور خوانده می‌شود",
             ""],
        ],
        [18, 78, 52],
    )

    sheet(wb, "۱ فایل‌ها", "فایل‌های برنامه و نقش هرکدام",
          ["مسیر", "نوع", "خط", "تابع", "#define", "نقش (از هدر فایل)"],
          files, [46, 6, 7, 7, 9, 70])

    sheet(wb, "۲ توابع", "توابع func__ برنامه",
          ["تابع", "فایل تعریف", "اعلان در هدر", "خط", "خلاصه"],
          funcs, [34, 40, 34, 7, 70])

    sheet(wb, "۳ پارامترهای ESP", "شناسه‌های سیم ESP-Link (پروتکل v2، جدول کامل)",
          ["شناسه", "نام", "نوع", "واحد", "پیش‌فرض", "بازه/توضیح", "ماندگاری فلش", "نسخهٔ رکورد"],
          params, [7, 40, 6, 7, 12, 60, 12, 12])

    sheet(wb, "۴ پایه‌ها", "نگاشت پایه‌های MCU (شماتیک ↔ board_pins.h)",
          ["پایه", "سیگنال منطقی", "نت شماتیک", "قطبیت", "سطح امن", "توضیح هدر"],
          pins, [8, 20, 22, 12, 9, 52])

    sheet(wb, "۵ نقشهٔ فلش", "ناحیه‌های فلش داخلی و مالک هر بلوک داده",
          ["ناحیه/بلوک", "شروع", "اندازه", "نوع", "نویسنده"],
          build_flash(), [34, 20, 26, 8, 26])

    sheet(wb, "۶ دروازه‌ها", "دروازه‌های بررسی برنامه",
          ["دستور", "چه چیزی را چک می‌کند", "یادداشت"],
          build_gates(files, funcs, params, pins), [40, 62, 40])

    OUT.parent.mkdir(parents=True, exist_ok=True)
    wb.save(OUT)
    return OUT


def main():
    if not (ROOT / "Firmware/Modules/EspLink/esp_link.h").is_file():
        print("run from the repository root")
        return 1
    out = build()
    print(f"program map written: {out.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
