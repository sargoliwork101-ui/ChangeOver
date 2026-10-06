#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
[EN] Build ONE state-machine workbook per module, next to that module's own
     validation workbook, and link the two together.

     Why: Documentation/System_State_Machine.xlsx holds every state of the
     whole program in one book. That is the right shape for a system review
     and the wrong shape for bench work - an installer testing the charger
     should not scroll past the ESP parser. Each module now gets a small
     book that covers only its own states, in the same colour language, and
     every module validation workbook gains a «ماشین حالت» sheet that links
     straight to it (and back to the system book).

     Regenerate with:  python3 tools/make_module_state_machines.py
     Needs openpyxl:   pip install --break-system-packages openpyxl

[FA] ساخت یک فایل اکسل ماشین حالت برای هر ماژول، کنار برگهٔ اعتبارسنجی همان
     ماژول، و لینک‌کردن این دو به هم.

     چرا: فایل ماشین حالت کل برنامه همهٔ حالت‌ها را یک‌جا دارد؛ این شکل برای
     مرور سیستمی درست است و برای کار روی میز تست شلوغ. حالا هر ماژول کتاب
     کوچک خودش را دارد - فقط حالت‌های خودش، با همان زبان رنگی - و در فایل
     اعتبارسنجی هر ماژول برگهٔ «ماشین حالت» اضافه می‌شود که مستقیم به آن
     لینک می‌دهد (و به کتاب سیستم برمی‌گردد).
"""

import pathlib

from openpyxl import Workbook, load_workbook
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.utils import get_column_letter

ROOT = pathlib.Path(__file__).resolve().parents[1]
MODULES_DIR = ROOT / "Firmware" / "Modules"
SYSTEM_BOOK = "../../../Documentation/System_State_Machine.xlsx"

FONT = "Vazirmatn"

C_TITLE = "1F3864"
C_HEAD = "2F5597"
C_BAND = "D9E2F3"
C_LINK = "0563C1"
PALETTE = {
    "ok":   "C6EFCE",   # سالم / عادی
    "warn": "FFE699",   # هشدار / انتظار
    "bad":  "FFC7CE",   # خطا / قفل
    "info": "DDEBF7",   # اطلاعاتی
    "idle": "EDEDED",   # بی‌کار
    "act":  "FCE4D6",   # در حال کار
}

thin = Side(style="thin", color="9DB7E8")
BORDER = Border(left=thin, right=thin, top=thin, bottom=thin)

COLOUR_LEGEND = ("سبز = عادی/سالم · زرد = هشدار یا انتظار · قرمز = خطا یا قفل · "
                 "نارنجی = در حال کار · خاکستری = بی‌کار · آبی = اطلاعاتی")


def sheet(wb, name, title, headers, rows, widths):
    """[EN] One right-to-left, coloured sheet. / [FA] یک برگهٔ رنگی راست‌به‌چپ."""
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


def guide_sheet(wb, module, persian_name, sources, sheets, validation_name):
    """
    [EN] The first sheet of every module book: what is inside, which firmware
         file each sheet was written from, the colour legend, and the two
         links (module validation workbook, whole-system workbook).
    [FA] نخستین برگهٔ هر کتاب ماژول: فهرست برگه‌ها، فایل مرجع هر برگه،
         راهنمای رنگ و دو لینک (اعتبارسنجی همان ماژول و کتاب کل سیستم).
    """
    rows = [[name, what, src, "info"] for name, what, src in sheets]
    rows += [
        ["", "", "", ""],
        ["رنگ‌ها", COLOUR_LEGEND, "", "idle"],
        ["فایل‌های مرجع", "، ".join(sources), "", "idle"],
        ["بازتولید", "python3 tools/make_module_state_machines.py",
         "tools/make_module_state_machines.py", "ok"],
    ]
    ws = sheet(
        wb, "راهنما",
        f"ماشین حالت ماژول {persian_name} — راهنما",
        ["برگه", "چه چیزی داخلش است", "فایل مرجع در فرمور"],
        rows,
        [26, 72, 52],
    )

    link_row = ws.max_row + 2
    links = [
        ("برگهٔ اعتبارسنجی همین ماژول", validation_name),
        ("ماشین حالت کل برنامه (نمای سیستمی)", SYSTEM_BOOK),
    ]
    for offset, (label, target) in enumerate(links):
        r = link_row + offset
        cell = ws.cell(row=r, column=1, value=label)
        cell.font = Font(name=FONT, size=10, bold=True)
        cell.alignment = Alignment(horizontal="right", vertical="center")
        link = ws.cell(row=r, column=2, value=target)
        link.hyperlink = target
        link.font = Font(name=FONT, size=10, color=C_LINK, underline="single")
        link.alignment = Alignment(horizontal="right", vertical="center")
    return ws


# ==================== Module content / محتوای ماژول‌ها ====================
# [EN] Written from the firmware sources named in each entry; the audit
#      invariant in tools/audit_consistency.py keeps the file list honest.
# [FA] از روی همان فایل‌های فرمور نوشته شده که در هر ورودی نام برده شده‌اند.

MODULES = {}

MODULES["Changeover"] = {
    "fa": "تغییر مسیر (Changeover)",
    "validation": "Changeover_Board_Validation.xlsx",
    "sources": ["Firmware/Modules/Changeover/changeover.c",
                "Firmware/Config/Inc/app_types.h"],
    "sheets": [
        ("۱ حالت‌ها", "شش حالت اصلی دستگاه و معنی هرکدام", "app_types.h، changeover.c"),
        ("۲ گذارها", "هر گذار با شرط و زمان لازم", "changeover.c"),
    ],
    "tabs": [
        ("۱ حالت‌ها", "حالت‌های اصلی سیستم (app_state_t)",
         ["حالت", "معنی", "شرط ورود", "خروجی سخت‌افزاری (PB11 محافظ باتری)", "اثر روی شارژر"],
         [
             ["APP_STATE_BOOT", "حالت اولیه پس از ریست، هنوز تصمیمی گرفته نشده", "مقدار اولیه در func__Changeover_Init", "بدون تغییر (محافظ غیرفعال)", "شارژ تا رسیدن نمونهٔ معتبر شروع نمی‌شود", "idle"],
             ["APP_STATE_IDLE", "مقدار پیش‌فرض متغیر محلی تسک کنترل؛ خروجی ماشین حالت نیست", "فقط مقداردهی اولیه در task_control.c", "—", "—", "idle"],
             ["APP_STATE_INPUT", "برق ورودی هست و بار از ورودی تغذیه می‌شود", "نمونه معتبر، بدون خطا، input_present = true", "غیرفعال (باتری وصل)", "شارژ مجاز است", "ok"],
             ["APP_STATE_BATTERY", "برق ورودی نیست و بار روی باتری است", "نمونه معتبر، بدون خطا، input_present = false", "غیرفعال (باتری وصل)", "شارژ انجام نمی‌شود (ورودی معتبر نیست)", "act"],
             ["APP_STATE_FAULT", "دست‌کم یک بیت خطا فعال است", "fault_mask ≠ 0 با نمونهٔ معتبر", "بدون تغییر (پین دست نمی‌خورد)", "شارژر به حالت امن می‌رود", "bad"],
             ["APP_STATE_SAFE", "باتری از خروجی جدا شده است", "قطع ولتاژ پایین، یا وتوی عدم‌توازن، یا وتوی باتری خراب", "فعال = باتری قطع", "شارژر به حالت امن می‌رود", "bad"],
         ],
         [22, 34, 40, 34, 34]),
        ("۲ گذارها", "گذارهای حالت سیستم — شرط و زمان",
         ["از", "به", "شرط", "زمان لازم", "توضیح"],
         [
             ["BOOT", "INPUT", "نمونه معتبر و input_present = true", "آنی", "اولین نگاشت پس از بوت", "ok"],
             ["BOOT", "BATTERY", "نمونه معتبر و input_present = false", "آنی", "اولین نگاشت پس از بوت", "act"],
             ["هر حالت", "همان حالت", "نمونه NULL یا نامعتبر", "—", "تصمیمی گرفته نمی‌شود و تایمرها صفر می‌شوند", "idle"],
             ["هر حالت", "FAULT", "fault_mask ≠ 0", "آنی", "پین محافظ دست نمی‌خورد", "bad"],
             ["FAULT", "INPUT / BATTERY", "همهٔ بیت‌های خطا پاک شدند", "آنی", "نگاشت دوباره از input_present", "ok"],
             ["INPUT / BATTERY", "SAFE", "ولتاژ باتری < ۲۰۸۰۰ میلی‌ولت (قطع بحرانی)", "۳۰۰۰ میلی‌ثانیهٔ پیوسته", "محافظ فعال می‌شود", "bad"],
             ["INPUT / BATTERY", "SAFE", "قفل داخلی باتری‌کم بسته است (زیر ۲۱۰۰۰ بسته، فقط در ۲۱۲۰۰ باز)", "۳۰۰۰ میلی‌ثانیهٔ پیوسته", "از v1.81 شمارش تا بازشدن قفل ادامه دارد", "warn"],
             ["SAFE", "INPUT", "input_present = true و ولتاژ ≥ ۲۱۲۰۰ میلی‌ولت", "۳۰۰۰ میلی‌ثانیهٔ پیوسته", "محافظ غیرفعال و باتری دوباره وصل می‌شود", "ok"],
             ["هر حالت", "SAFE", "قفل عدم‌توازن + تیک پارامتر ۱۱۷", "آنی", "وتوی خروجی سناریوی ۵", "bad"],
             ["هر حالت", "SAFE", "حکم باتری خراب + تیک پارامتر ۱۲۷", "آنی", "وتوی خروجی سناریوی ۶", "bad"],
             ["SAFE", "SAFE", "ورودی قطع است", "—", "بدون ورودی، وصل مجدد انجام نمی‌شود (تصمیم تأییدشدهٔ کاربر)", "warn"],
         ],
         [20, 20, 46, 22, 40]),
    ],
}

MODULES["Charger"] = {
    "fa": "شارژر (Charger)",
    "validation": "Charger_Validation.xlsx",
    "sources": ["Firmware/Modules/Charger/charger.c"],
    "sheets": [
        ("۱ حالت‌ها", "ده حالت هر کانال شارژ", "charger.c (charger_state_t)"),
        ("۲ گذارها", "گذارهای کانال با شرط", "charger.c"),
        ("۳ باتری خراب", "ماشین حالت سناریوی ۶ (مهلت شارژ پیوسته)", "charger.c (DeadBatteryTick)"),
        ("۴ خطای فنی", "ماشین حالت سناریوی ۷ و قفل طبقهٔ قدرت", "charger.c (EvaluateTechnicalFault/Run)"),
    ],
    "tabs": [
        ("۱ حالت‌ها", "حالت‌های هر کانال شارژ (charger_state_t)",
         ["حالت", "معنی", "خروجی PWM", "شرط خروج"],
         [
             ["CHG_STATE_OFF", "کانال خاموش و آمادهٔ شروع", "صفر", "گذشتن زمان ثبات ۱۵ ثانیه و معتبربودن ورودی ← BULK", "idle"],
             ["CHG_STATE_BULK", "جریان ثابت؛ حلقهٔ جریان فرمان می‌دهد", "از PID، با سقف و حاشیهٔ جریان", "رسیدن ولتاژ به پنجرهٔ ابزورب ← ABSORB", "act"],
             ["CHG_STATE_ABSORB", "ولتاژ ثابت؛ حلقهٔ ولتاژ فرمان می‌دهد", "از PID، زیر سقف ولتاژ", "افت جریان زیر جریان پایانی، یا سقف زمان ← FLOAT", "act"],
             ["CHG_STATE_FLOAT", "شارژ تمام شده و کانال پارک است", "صفر یا نگه‌دارنده", "افت ولتاژ باتری ← بازگشت به BULK", "ok"],
             ["CHG_STATE_BRINGUP", "حالت آزمایش ترانس ناشناخته", "پله‌های محدود", "شناخته‌شدن ترانس یا خاموش‌کردن آزمون", "warn"],
             ["CHG_STATE_JIT_RETRY_WAIT", "پارک پس از تریپ کمپریتور (جیتر)", "صفر", "پایان زمان قفل و نوبت احیا ← OFF/BULK", "warn"],
             ["CHG_STATE_INPUT_WAIT", "انتظار برای برگشت برق ورودی", "صفر", "معتبرشدن ولتاژ ورودی ← OFF", "warn"],
             ["CHG_STATE_FINAL_FAULT", "سه تریپ پیاپی: قفل نهایی کانال", "صفر و رله باز", "فقط با ریست برد", "bad"],
             ["CHG_STATE_BAT_LOST", "سیم باتری حین شارژ قطع شده", "صفر", "پاک‌شدن بیت خطای قطع باتری ← OFF", "bad"],
             ["CHG_STATE_MANUAL", "مود تست دستی؛ duty از پنل می‌آید", "مقدار فرمان‌شده از پنل", "خروج از مود یا سکوت ۳ ثانیه‌ای لینک", "warn"],
         ],
         [26, 38, 30, 46]),
        ("۲ گذارها", "گذارهای کانال شارژ",
         ["از", "به", "شرط", "توضیح"],
         [
             ["OFF", "BULK", "ولتاژ باتری در محدوده به مدت ۱۵ ثانیه و ورودی معتبر", "شروع نرم از ۱٪ duty", "ok"],
             ["BULK", "ABSORB", "ولتاژ به پنجرهٔ ابزورب رسید", "فرمان به حلقهٔ ولتاژ منتقل می‌شود", "act"],
             ["ABSORB", "FLOAT", "جریان زیر «جریان پایانی» به مدت پارامتر ۹۶، یا سقف زمان ۹۳", "اعلام شارژ کامل", "ok"],
             ["ABSORB", "ABSORB", "افت لحظه‌ای ولتاژ از پنجره", "پنجره ریست می‌شود (DipResetAbsorbWindow)", "warn"],
             ["FLOAT", "BULK", "افت ولتاژ باتری زیر آستانهٔ شروع دوباره", "سیکل بعدی شارژ", "act"],
             ["هر حالت", "JIT_RETRY_WAIT", "تریپ کمپریتور جیتر همان کانال", "کانال بلافاصله با duty صفر پارک می‌شود", "warn"],
             ["JIT_RETRY_WAIT", "OFF", "پایان زمان قفل و نوبت احیا (تک‌به‌تک)", "شمارش تریپ حفظ می‌شود", "warn"],
             ["JIT_RETRY_WAIT", "FINAL_FAULT", "سومین تریپ", "قفل دائمی کانال + باز شدن رله", "bad"],
             ["هر حالت", "BAT_LOST", "ست‌شدن بیت FAULT_CHARGER_BAT_LOST", "جز FINAL_FAULT که بازنویسی نمی‌شود", "bad"],
             ["BAT_LOST", "OFF", "پاک‌شدن بیت خطا پس از زمان بازیابی", "شروع نرم دوباره", "ok"],
             ["هر حالت", "INPUT_WAIT", "ولتاژ ورودی زیر حد اعتبار", "به‌جز FINAL_FAULT", "warn"],
             ["هر حالت", "MANUAL", "فعال‌شدن مود تست دستی از پنل", "گیت‌های باتری دور زده می‌شوند", "warn"],
             ["MANUAL", "OFF", "خروج از مود یا سکوت ۳ ثانیه‌ای لینک", "duty صفر و بازگشت به خودکار", "ok"],
         ],
         [22, 22, 48, 44]),
        ("۳ باتری خراب", "ماشین حالت سناریوی ۶ (مهلت شارژ پیوسته) — برای هر کانال جدا",
         ["حالت", "شرط ورود", "شرط خروج", "پارامترها"],
         [
             ["شمارش متوقف", "کانال شارژ نمی‌کند", "شروع BULK یا ABSORB", "—", "idle"],
             ["در حال شمارش", "کانال در BULK یا ABSORB است", "رسیدن به FLOAT ← صفر شدن ساعت", "۱۲۵ (۰ = سناریو خاموش، تا ۴۸ ساعت)", "act"],
             ["وقفهٔ کوتاه", "شارژ قطع شد ولی کمتر از مهلت وقفه", "برگشت شارژ ← ادامهٔ شمارش", "۱۲۶ (تا ۱ ساعت)", "warn"],
             ["وقفهٔ بلند", "قطع شارژ بیش از مهلت وقفه", "صفر شدن ساعت", "۱۲۶", "idle"],
             ["حکم خرابی (قفل)", "رسیدن شمارش به مهلت ۱۲۵", "فقط تعویض باتری (۳ ثانیه بدون باتری)", "۱۲۵، ۱۲۷، ۱۲۸ تا ۱۳۱", "bad"],
             ["قفل + قطع خروجی", "حکم خرابی و تیک پارامتر ۱۲۷", "همانند بالا", "۱۲۷", "bad"],
             ["چهرهٔ قفل", "در حالت قفل: بوق دوره‌ای و چراغ قرمز", "—", "۱۲۸ دوره بوق (۰ = بی‌صدا)، ۱۲۹ طول بوق، ۱۳۰ دورهٔ چشمک (۰ = ثابت)، ۱۳۱ سهم روشنی", "bad"],
         ],
         [24, 44, 42, 46]),
        ("۴ خطای فنی", "ماشین حالت سناریوی ۷ — تشخیص سوختن ترانزیستور و قفل RAM",
         ["حالت", "شرط ورود", "خروجی اجباری", "شرط خروج", "چهره/پارامتر"],
         [
             ["عادی", "لچ فنی خاموش و snapshot معتبر", "شارژ طبق کنترل عادی", "امضای فنی اتصال‌کوتاه یا قطع‌شدن", "—", "ok"],
             ["اتصال‌کوتاه/سوختن", "رله باز + PWM واقعی صفر + JIT", "FAULT_CHARGER_TECHNICAL و FinalDisconnect", "فقط reset/power-cycle", "q137..q142", "bad"],
             ["قطع‌شدن/سوختن", "PWM واقعی >۲۰٪ + جریان همان کانال = ۰", "FAULT_CHARGER_TECHNICAL و FinalDisconnect", "فقط reset/power-cycle", "q137..q142", "bad"],
             ["قفل نشست برق", "TechnicalFaultLockout فعال", "هر دو PWM صفر، رله باز؛ هیچ retry یا پنل/NVM آزاد نمی‌کند", "func__Charger_Init پس از reset", "RAM-only", "bad"],
         ],
         [26, 58, 58, 40, 34]),
    ],
}

MODULES["Fault"] = {
    "fa": "خطا (Fault)",
    "validation": "Fault_Validation.xlsx",
    "sources": ["Firmware/Modules/Fault/fault.c", "Firmware/Config/Inc/app_types.h"],
    "sheets": [
        ("۱ بیت‌ها", "بیت‌های خطا، شرط ست و پاک‌شدن", "app_types.h، fault.c"),
        ("۲ آشکارسازها", "ماشین حالت دبانس هر آشکارساز قطع باتری", "fault.c (Fault_Evaluate)"),
        ("۳ خطای فنی", "Fault فقط بیت مرکزی سناریوی ۷ را منتشر می‌کند؛ تشخیص و قفل با Charger است", "app_types.h، charger.c، task_control.c"),
    ],
    "tabs": [
        ("۱ بیت‌ها", "بیت‌های خطا (fault_mask_t)",
         ["بیت", "نام", "شرط ست‌شدن", "شرط پاک‌شدن", "اثر"],
         [
             ["۰", "FAULT_ADC", "نمونهٔ اندازه‌گیری نامعتبر", "رسیدن اولین نمونهٔ معتبر (قفل نمی‌شود)", "سیستم به FAULT و شارژر به حالت امن", "bad"],
             ["۱", "FAULT_OVERCURRENT_1", "رزرو — در نسخهٔ فعلی ست نمی‌شود", "—", "—", "idle"],
             ["۲", "FAULT_OVERCURRENT_2", "رزرو — در نسخهٔ فعلی ست نمی‌شود", "—", "—", "idle"],
             ["۳", "FAULT_LOW_BATTERY", "رزرو — از v1.74 تصمیم داخل Changeover است", "—", "—", "idle"],
             ["۴", "FAULT_JITTER_1", "رزرو — تریپ جیتر در خود شارژر مدیریت می‌شود", "—", "—", "idle"],
             ["۵", "FAULT_JITTER_2", "رزرو — همانند بالا", "—", "—", "idle"],
             ["۶", "FAULT_CHARGER_BAT_LOST", "حالت ۱: نیمِ در حال پمپ بالاتر از آستانهٔ قطع؛ حالت ۲: نیمی زیر ۶ ولت با ورودی سالم — هر دو با دبانس", "هر دو نیم ≥ ۷ ولت و بدون پمپ، به مدت زمان بازیابی", "چهرهٔ «قطع باتری» در UI و توقف شارژ", "bad"],
             ["۷", "FAULT_CHARGER_TECHNICAL", "Charger امضای رله باز + PWM صفر + JIT یا PWM >۲۰٪ + جریان صفر را ست می‌کند", "فقط reset/power-cycle؛ مالک RAM latch، نه تنظیمات پنل", "UI سناریوی ۷", "bad"],
         ],
         [8, 32, 56, 46, 40]),
        ("۲ آشکارسازها", "سه تایمر دبانس که بیت قطع باتری را می‌سازند و پاک می‌کنند",
         ["آشکارساز", "شرط شمارش", "مدت", "نتیجه", "چه چیزی شمارش را صفر می‌کند"],
         [
             ["پمپ بالای حد (حالت ۱)", "نیمی که کانال خودش پمپ می‌کند بالاتر از حد قطع (۲۷) باشد", "دبانس ۲۸ (پیش‌فرض ۱۵۰ms)", "ست‌شدن FAULT_CHARGER_BAT_LOST", "پایین‌آمدن آن نیم، یا پارک‌شدن کانالِ خودش", "bad"],
             ["افت زیر حد (حالت ۲)", "نیمی زیر حد نبودِ باتری (۲۹) با ورودی در بازهٔ ۳۳..۳۴", "دبانس ۳۱ (پیش‌فرض ۱۰۰۰ms)", "ست‌شدن FAULT_CHARGER_BAT_LOST", "بالا آمدن آن نیم، یا خارج‌شدن ورودی از بازه", "bad"],
             ["بازیابی", "هر دو نیم ≥ حد بازگشت (۳۰) و هیچ نیمی پمپ نمی‌شود", "زمان بازیابی ۳۲", "پاک‌شدن بیت", "افت هر نیم زیر حد بازگشت یا شروع پمپ", "ok"],
             ["مود تست دستی", "—", "—", "هر سه تایمر فریز می‌شوند: نه بیتی قفل می‌شود نه پاک", "خروج از مود تست", "warn"],
             ["نمونهٔ نامعتبر", "—", "—", "هیچ تصمیمی گرفته نمی‌شود", "هر سه تایمر صفر می‌شوند", "idle"],
         ],
         [26, 48, 26, 40, 40]),
        ("۳ خطای فنی", "نقش Fault در سناریوی ۷ — انتشار بیت مرکزی و پاک‌شدن فقط با reset",
         ["بخش", "شرط", "رفتار", "مرجع"],
         [
             ["بیت مرکزی", "Charger امضای فنی را دید", "FAULT_CHARGER_TECHNICAL (bit 7) ست می‌شود", "app_types.h" , "bad"],
             ["مالک تشخیص", "snapshot معتبر", "Fault تشخیص نمی‌دهد؛ task_control پس از Charger بیت را refresh می‌کند", "task_control.c", "info"],
             ["پاک‌سازی", "reset/power-cycle", "Fault_Init بیت RAM را پاک می‌کند؛ تنظیم پنل یا NVM کافی نیست", "fault.c / charger.c", "ok"],
         ],
         [24, 52, 62, 38]),
    ],
}

MODULES["Ui"] = {
    "fa": "نمایش و بوق (UI)",
    "validation": "UI_Board_Validation.xlsx",
    "sources": ["Firmware/Modules/Ui/ui_led.c", "Firmware/Modules/Ui/ui_buzzer.c"],
    "sheets": [
        ("۱ سناریوها", "اولویت سناریوها و چهرهٔ چراغ و بوق", "ui_led.c (func__Ui_Tick)"),
        ("۲ باندهای دشارژ", "چهار باند بوق سناریوی ۳", "ui_led.c، ui_buzzer.c"),
    ],
    "tabs": [
        ("۱ سناریوها", "اولویت سناریوهای چراغ و بوق (func__Ui_Tick)",
         ["اولویت", "سناریو", "شرط", "چهرهٔ چراغ", "صدا"],
         [
             ["—", "نمونهٔ نامعتبر", "snapshot برابر NULL یا valid = false", "همهٔ چراغ‌ها خاموش", "بی‌صدا", "idle"],
             ["۰ (بالاترین)", "خطای فنی برد (سناریو ۷)", "FAULT_CHARGER_TECHNICAL قفل است", "هر سه LED با فاز مشترک هم‌زمان", "بوق مستقل ۱۳۷ تا ۱۴۰", "bad"],
             ["۱", "اضافه‌ولتاژ ورودی", "ولتاژ ورودی بالای آستانهٔ ۷۰ با هیسترزیس ۷۱", "قرمز چشمک‌زن + سبز روشن", "بوق دوره‌ای (۴۰ تا ۴۳)", "bad"],
             ["۲", "قطع باتری", "بیت FAULT_CHARGER_BAT_LOST قفل است", "چهرهٔ قطع باتری", "سه بوق کوتاه (۴۷ تا ۴۹)", "bad"],
             ["۳", "باتری خراب (سناریو ۶)", "حکم مهلت شارژ قفل شده", "قرمز ثابت (پیش‌فرض ۱۳۰ = ۰)", "بوق هر ۱۲۸ به طول ۱۲۹", "bad"],
             ["۴", "عدم‌توازن (سناریو ۵)", "قفل عدم‌توازن", "قرمز چشمک‌زن (۱۲۳ و ۱۲۴)", "بوق هر ۱۱۵ به طول ۱۱۶", "bad"],
             ["۵", "شارژ کامل", "اعلام اتمام شارژ یا قفل ولتاژی ۱۰۰/۹۵", "سبز ثابت", "بی‌صدا", "ok"],
             ["۶", "ورودی سالم بدون شارژ", "ورودی هست ولی هیچ کانالی پمپ نمی‌کند", "سبز ثابت", "بی‌صدا", "ok"],
             ["۷", "در حال شارژ", "ورودی هست و کانالی فعال است", "سبز ثابت + زرد چشمک‌زن متناسب با درصد", "بی‌صدا", "act"],
             ["۸", "کار روی باتری (دشارژ)", "ورودی قطع است", "سبز چشمک‌زن متناسب با درصد", "باندهای بوق ۱ تا ۳ و باند بحرانی", "warn"],
         ],
         [10, 30, 44, 40, 36]),
        ("۲ باندهای دشارژ", "سناریوی ۳ — چهار باند صدا بر اساس درصد باتری",
         ["باند", "شرط درصد", "چهره", "صدا", "پارامترها"],
         [
             ["بی‌صدا", "بالاتر از «درصد شروع بوق»", "فقط سبز چشمک‌زن", "بی‌صدا", "۵۰", "ok"],
             ["باند ۱", "زیر «شروع بوق» و بالای باند ۲", "سبز چشمک‌زن", "یک الگوی بوق با مدت، تعداد و فاصلهٔ خودش", "۵۴، ۵۸، ۶۶", "warn"],
             ["باند ۲", "زیر درصد باند ۲ و بالای باند ۳", "سبز چشمک‌زن", "الگوی خودش", "۵۱، ۱۲۱، ۶۷، ۱۲۲", "warn"],
             ["باند ۳", "زیر درصد باند ۳ و بالای باند بحرانی", "سبز چشمک‌زن", "الگوی خودش", "۵۲، ۵۹، ۶۸، ۵۵", "warn"],
             ["باند بحرانی", "زیر «درصد بحرانی»", "همهٔ چراغ‌ها خاموش", "الگوی بحرانی فقط یک‌بار پخش می‌شود، بعد سکوت تا برگشت باتری", "۵۳، ۵۶، ۶۰، ۶۱، ۶۹", "bad"],
             ["قانون مشترک", "—", "—", "گپ بین بوق‌ها تنها عدد مشترک چهار باند است و با «تعداد بوق = ۱» بی‌معنی می‌شود", "۶۵", "info"],
         ],
         [16, 34, 28, 52, 30]),
    ],
}

MODULES["Imbalance"] = {
    "fa": "عدم‌توازن (سناریوی ۵)",
    "validation": "Imbalance_Validation.xlsx",
    "sources": ["Firmware/Modules/Imbalance/imbalance.c"],
    "sheets": [
        ("۱ حالت‌ها", "حالت‌های سنجش، رویداد و قفل", "imbalance.c"),
        ("۲ پارامترها", "شناسه‌های تحت مالکیت ماژول و بازهٔ هرکدام", "imbalance.h"),
    ],
    "tabs": [
        ("۱ حالت‌ها", "ماشین حالت سناریوی ۵ (عدم‌توازن دو نیمِ باتری)",
         ["حالت", "شرط ورود", "شرط خروج", "پارامترها"],
         [
             ["سنجش‌نشده", "درون مهلت پس از شروع/پایان شارژ، یا نمونهٔ نامعتبر، یا نبود باتری", "گذشتن مهلت ۱۱۰/۱۱۱", "۱۱۰، ۱۱۱", "idle"],
             ["سالم", "اختلاف دو نیم زیر حد", "عبور اختلاف از حد", "۱۰۸ (استراحت/شارژ)، ۱۰۹ (دشارژ)", "ok"],
             ["در حال سنجش", "اختلاف بالای حد ولی هنوز پایدار نشده", "پایداری ۱۱۲ ← رویداد، یا افت زیر حد ← سالم", "۱۱۲", "warn"],
             ["رویداد باز", "اختلاف به مدت ۱۱۲ بالای حد مانده", "افت اختلاف به «حد منهای ۱۱۳» ← بسته‌شدن رویداد", "۱۱۳", "warn"],
             ["قفل", "رسیدن شمارش رویدادها به ۱۱۴", "فقط تعویض باتری (۳ ثانیه بدون باتری)", "۱۱۴، ۱۱۵، ۱۱۶، ۱۲۳، ۱۲۴", "bad"],
             ["قفل + قطع خروجی", "قفل و تیک پارامتر ۱۱۷", "همانند بالا", "۱۱۷", "bad"],
             ["قفل + توقف شارژ", "گذشتن ۱۱۸ سیکل شارژ پس از قفل", "همانند بالا", "۱۱۸", "bad"],
         ],
         [24, 48, 46, 34]),
        ("۲ پارامترها", "شناسه‌های ماژول، پیش‌فرض و بازهٔ مجاز",
         ["شناسه", "معنی", "پیش‌فرض", "بازهٔ مجاز"],
         [
             ["۱۰۸", "حد اختلاف در استراحت و حین شارژ (mV)", "۳۰۰", "۰ تا ۲۰۰۰", "info"],
             ["۱۰۹", "حد اختلاف در دشارژ (mV)", "۵۰۰", "۰ تا ۲۰۰۰", "info"],
             ["۱۱۰", "مهلت سنجش پس از پایان شارژ (ms)", "۶۰۰۰۰۰", "۰ تا ۳۶۰۰۰۰۰", "info"],
             ["۱۱۱", "مهلت سنجش پس از شروع شارژ (ms)", "۶۰۰۰۰۰", "۰ تا ۳۶۰۰۰۰۰", "info"],
             ["۱۱۲", "پایداری لازم برای ثبت رویداد (ms)", "۳۰۰۰۰", "۱۰۰۰ تا ۶۰۰۰۰۰", "info"],
             ["۱۱۳", "هیسترزیس بسته‌شدن رویداد (mV)", "۱۰۰", "۰ تا ۱۰۰۰", "info"],
             ["۱۱۴", "تعداد رویداد تا قفل", "۱۰", "۱ تا ۲۵۵", "info"],
             ["۱۱۵", "دورهٔ بوق در قفل (ms)", "۳۶۰۰۰۰۰", "۰ تا ۸۶۴۰۰۰۰۰", "info"],
             ["۱۱۶", "طول هر بوق در قفل (ms)", "۲۰۰", "۲۰ تا ۲۰۰۰", "info"],
             ["۱۱۷", "در قفل، باتری از خروجی هم جدا شود", "۱", "۰ یا ۱", "info"],
             ["۱۱۸", "تعداد سیکل شارژ مجاز پس از قفل", "۲۰", "۱ تا ۲۵۵", "info"],
             ["۱۲۳", "دورهٔ چشمک چراغ قرمز در قفل (ms)", "۱۰۰۰", "۰ = ثابت، وگرنه ۱۰۰ تا ۱۰۰۰۰", "info"],
             ["۱۲۴", "سهم روشنی چشمک (٪)", "۵۰", "۵ تا ۹۵", "info"],
             ["۲۰۰ تا ۲۰۲", "اسلات‌های NVM: شمارش رویداد، سیکل و پرچم قفل", "—", "فقط بازپخش از فلش", "idle"],
         ],
         [14, 48, 16, 40]),
    ],
}

MODULES["McuPowerPath"] = {
    "fa": "مسیر تغذیهٔ میکرو (McuPowerPath)",
    "validation": "McuPowerPath_Validation.xlsx",
    "sources": ["Firmware/Modules/McuPowerPath/mcu_power_path.c"],
    "sheets": [
        ("۱ حالت‌ها", "چهار حالت مسیر تغذیهٔ خود میکرو", "mcu_power_path.c"),
    ],
    "tabs": [
        ("۱ حالت‌ها", "مسیر تغذیهٔ خود میکرو (Q1 روی PB5)",
         ["حالت", "شرط", "وضعیت کلید باتری", "زمان"],
         [
             ["تغذیه از باتری", "حالت اولیهٔ بوت، یا ورودی ضعیف", "وصل (سطح پایین = روشن)", "آنی", "act"],
             ["در حال احراز ورودی", "ولتاژ ورودی ≥ ۲۲۰۰۰ میلی‌ولت", "هنوز وصل", "باید ۵۰۰۰ میلی‌ثانیه پایدار بماند", "warn"],
             ["نوار هیسترزیس", "ولتاژ بین ۲۱۵۰۰ و ۲۱۹۹۹ میلی‌ولت", "بدون تغییر", "تایمر ۵ ثانیه هم نمی‌دود", "idle"],
             ["تغذیه از ورودی", "ورودی ۵ ثانیه پایدار بالای ۲۲ ولت و تأیید پایهٔ حضور", "قطع (سطح بالا = خاموش)", "—", "ok"],
             ["بازگشت به باتری", "ولتاژ ورودی < ۲۱۵۰۰ میلی‌ولت، یا وقفهٔ سخت‌افزاری قطع ورودی", "وصل", "آنی (مسیر وقفه)", "bad"],
             ["نمونهٔ نامعتبر", "اندازه‌گیری در دسترس نیست", "بدون تغییر", "تصمیم فریز می‌شود", "idle"],
         ],
         [26, 48, 36, 36]),
    ],
}

MODULES["EspLink"] = {
    "fa": "لینک ESP (EspLink)",
    "validation": "EspLink_Validation.xlsx",
    "sources": ["Firmware/Modules/EspLink/esp_link.c", "Firmware/Modules/EspLink/esp_link_nvm.h"],
    "sheets": [
        ("۱ پارسر", "حالت‌های پارسر فریم روی UART", "esp_link.c"),
        ("۲ ماندگاری", "قاعدهٔ ماندگارشدن پارامترها روی فلش", "esp_link_nvm.h"),
    ],
    "tabs": [
        ("۱ پارسر", "ماشین حالت پارسر فریم (هر بایت دریافتی)",
         ["حالت", "منتظر چیست", "خطا چه می‌شود"],
         [
             ["WAIT_SOF0", "بایت اول شروع فریم", "هر بایت دیگری نادیده گرفته می‌شود", "info"],
             ["WAIT_SOF1", "بایت دوم شروع فریم", "ناسازگاری ← بازگشت به WAIT_SOF0", "info"],
             ["WAIT_VERSION", "بایت نسخهٔ پروتکل", "نسخهٔ ناشناخته ← فریم دور ریخته و شمارنده بالا می‌رود", "warn"],
             ["WAIT_TYPE", "نوع پیام", "نوع ناشناخته در پایان دور ریخته می‌شود", "info"],
             ["WAIT_LEN_LO / WAIT_LEN_HI", "طول payload", "طول بیشتر از ۵۱۲ بایت ← فریم دور ریخته می‌شود", "warn"],
             ["WAIT_PAYLOAD", "بدنهٔ پیام به اندازهٔ همان طول", "—", "act"],
             ["WAIT_CRC_LO / WAIT_CRC_HI", "دو بایت CRC-16", "ناسازگاری ← فریم دور ریخته و شمارندهٔ خطا بالا می‌رود", "bad"],
         ],
         [30, 44, 56]),
        ("۲ ماندگاری", "کدام پارامتر روی فلش می‌ماند (func__EspLink_NvmParamPersisted)",
         ["گروه شناسه", "ماندگار؟", "توضیح"],
         [
             ["۰ تا ۱۴", "بله", "تنظیم‌های پایه", "ok"],
             ["۱۵ تا ۱۹", "خیر", "مودهای گذرای تست؛ با هر بوت خاموش", "idle"],
             ["۲۰ تا ۱۳۱", "بله", "به‌جز شناسهٔ ۷۶ (سکوت موقت) و ۷۲/۷۳ (بازنشسته)", "ok"],
             ["۷۶", "خیر", "سکوت فقط برای همان نشست پنل است", "idle"],
             ["۲۰۰ تا ۲۰۳", "بله", "اسلات‌های حالت اجرا: شمارش‌های عدم‌توازن و پرچم باتری خراب", "ok"],
             ["نکتهٔ ترتیب بوت", "—", "بازپخش NVM پیش از scheduler اجرا می‌شود، پس هیچ Init ماژولی نباید جدول پارامتر را دوباره بنویسد (ایراد IMB-1، اصلاح ۲۰۲۶-۱۰-۰۶)", "warn"],
         ],
         [20, 14, 76]),
    ],
}

MODULES["CalLut"] = {
    "fa": "جدول کالیبراسیون (CalLut)",
    "validation": "CalLut_Validation.xlsx",
    "sources": ["Firmware/Modules/CalLut/cal_lut.c"],
    "sheets": [
        ("۱ مراحل", "مراحل دست‌دادن انتقال جدول", "cal_lut.c"),
        ("۲ کد وضعیت", "پاسخ برد به هر مرحله", "cal_lut.h"),
    ],
    "tabs": [
        ("۱ مراحل", "انتقال جدول از پنل به فلش برد",
         ["مرحله", "چه اتفاقی می‌افتد", "اگر نیمه‌کاره بماند"],
         [
             ["۰ — بی‌کار", "جدول فعلی (فلش یا کامپایل‌شده) سر کار است", "—", "idle"],
             ["۱ — BEGIN", "پنل تعداد نقاط هر کانال را اعلام می‌کند؛ ناحیهٔ چیدن پاک می‌شود", "جدول فعال دست‌نخورده می‌ماند", "act"],
             ["۲ و ۳ — CHUNK", "نقاط کانال ۱ و ۲ (حداکثر ۲۴ نقطه هر کدام) چیده می‌شوند", "جدول فعال دست‌نخورده می‌ماند", "act"],
             ["۴ — COMMIT", "اعتبارسنجی، تعلیق شارژر، پاک و نوشتن صفحه، بازخوانی از خود فلش", "در هر شکستی، جدول فعال عوض نمی‌شود", "act"],
             ["تناوب دو صفحه", "هر کامیت روی صفحهٔ دیگر می‌نشیند؛ در بوت، بزرگ‌ترین شمارهٔ ترتیب برنده است", "رکورد سالم قدیمی‌تر سر کار می‌آید", "ok"],
             ["ریست پس از دست‌دادن", "درخواست ریست داخل هندلر لینک انجام نمی‌شود؛ چند اجرا بعد در Tick", "—", "info"],
         ],
         [22, 62, 44]),
        ("۲ کد وضعیت", "کدهایی که در LUT_ACK به پنل برمی‌گردند",
         ["کد", "نام", "معنی"],
         [
             ["۰", "OK", "موفق؛ جدول تازه روی فلش است", "ok"],
             ["۱", "NO_STAGE", "کامیت بدون شروع", "bad"],
             ["۲", "COUNT", "تعداد نقاط نامعتبر (خارج از ۲ تا ۲۴)", "bad"],
             ["۳", "MISSING", "تکه‌ای از نقاط نرسیده است", "bad"],
             ["۴", "CHAIN", "محور زنجیره اکیداً صعودی نیست", "bad"],
             ["۵", "POWER", "محور توان افت دارد", "bad"],
             ["۶", "CRC", "‏CRC پنل با چیده‌شده نمی‌خواند", "bad"],
             ["۷", "FLASH", "پاک‌کردن، نوشتن یا بازبینی فلش شکست خورد", "bad"],
         ],
         [10, 20, 70]),
    ],
}

MODULES["Jitter"] = {
    "fa": "جیتر (Jitter)",
    "validation": "Jitter_Validation.xlsx",
    "sources": ["Firmware/Modules/Jitter/jitter.c", "Firmware/Bsp/Src/bsp_exti.c"],
    "sheets": [
        ("۱ حالت‌ها", "قفل تریپ هر کانال", "jitter.c"),
    ],
    "tabs": [
        ("۱ حالت‌ها", "آشکارساز تریپ کمپریتور — برای هر کانال جدا",
         ["حالت", "شرط ورود", "شرط خروج", "اثر"],
         [
             ["بدون تریپ", "پس از Init یا پاک‌کردن توسط شارژر", "رسیدن لبهٔ پایین‌رونده از کمپریتور همان کانال", "کانال آزاد است", "ok"],
             ["تریپ قفل‌شده", "لبهٔ معتبر (سطح پایین هم دوباره بررسی می‌شود)", "فقط با func__Jitter_ClearChannel از سوی شارژر", "کانال فوراً با duty صفر پارک می‌شود", "bad"],
             ["رویداد مصرف‌شده", "رویداد BSP با یک‌بار خواندن پاک می‌شود", "—", "تریپِ پاک‌شده خودبه‌خود برنمی‌گردد", "info"],
             ["کانال نامعتبر", "شمارهٔ کانال خارج از ۱ و ۲", "—", "همیشه «بدون تریپ» گزارش می‌شود و پاک‌کردن بی‌اثر است", "idle"],
         ],
         [22, 46, 46, 40]),
    ],
}

MODULES["Protection"] = {
    "fa": "نگهبان اعتبار (Protection)",
    "validation": "Protection_Validation.xlsx",
    "sources": ["Firmware/Modules/Protection/protection.c"],
    "sheets": [
        ("۱ حالت‌ها", "دو حالت زندهٔ بیت ADC", "protection.c"),
    ],
    "tabs": [
        ("۱ حالت‌ها", "نگهبان اعتبار اندازه‌گیری — بیت FAULT_ADC «زنده» است نه قفل‌شونده",
         ["حالت", "شرط", "کار ماژول", "چرا"],
         [
             ["اندازه‌گیری سالم", "snapshot معتبر", "بیت FAULT_ADC پاک می‌شود", "بازگشت خودکار به کار عادی بدون دخالت کاربر", "ok"],
             ["اندازه‌گیری ناسالم", "snapshot برابر NULL یا valid = false", "بیت FAULT_ADC ست می‌شود", "سیستم به FAULT و شارژر به حالت امن می‌رود", "bad"],
             ["بیت‌های دیگر", "—", "هیچ‌وقت دست‌کاری نمی‌شوند", "هر بیت صاحب خودش را دارد", "info"],
         ],
         [24, 40, 40, 50]),
    ],
}

MODULES["Measurement"] = {
    "fa": "اندازه‌گیری (Measurement)",
    "validation": "Measurement_Board_Validation.xlsx",
    "sources": ["Firmware/Modules/Measurement/measurement.c", "Firmware/Bsp/Src/bsp_adc.c"],
    "sheets": [
        ("۱ حالت‌ها", "چرخهٔ گرم‌شدن، انتشار و نامعتبرشدن", "measurement.c"),
    ],
    "tabs": [
        ("۱ حالت‌ها", "چرخهٔ عمر یک snapshot اندازه‌گیری",
         ["حالت", "شرط ورود", "شرط خروج", "آنچه مصرف‌کننده می‌بیند"],
         [
             ["گرم‌شدن", "پس از Init یا پس از هر قطعی فریم", "جمع‌شدن فریم‌های گرم‌شدن (MEASUREMENT_WARMUP_FRAME_COUNT)", "‏valid = false؛ هیچ ماژولی تصمیم نمی‌گیرد", "idle"],
             ["معتبر", "فیلترها پر شده‌اند و فریم تازه می‌رسد", "نرسیدن فریم از BSP", "اعداد منتشرشده (ورودی، پک، دو نیم، دو جریان) + input_present", "ok"],
             ["نامعتبر", "‏BSP فریم تازه نمی‌دهد (DMA مرده)", "بازگشت فریم‌ها و گرم‌شدن دوباره", "‏valid = false؛ دادهٔ کهنه منجمد نمی‌ماند", "bad"],
             ["فیلتر", "در حالت معتبر، روی هر کانال", "—", "میانه سپس میانگین؛ یک نمونهٔ پرتِ نویز کلیدزنی رد نمی‌شود", "info"],
             ["آفست کاربر", "نوشتن آفست از پنل", "—", "آفست پس از تبدیل برد روی خوانش اعمال می‌شود (گیرهٔ ±۵۰۰۰mV)", "info"],
         ],
         [20, 42, 42, 54]),
    ],
}


def add_link_sheet(validation_path, module, state_book_name):
    """
    [EN] Give the module's validation workbook a «ماشین حالت» sheet whose
         only job is to point at the state-machine book next to it. A new
         sheet is used on purpose: no existing row, formula or bench record
         is touched.
    [FA] افزودن برگهٔ «ماشین حالت» به فایل اعتبارسنجی ماژول که تنها کارش
         اشاره به کتاب ماشین حالت کنار آن است. عمداً برگهٔ تازه است تا هیچ
         ردیف و فرمول و ثبت تست موجودی دست نخورد.
    """
    wb = load_workbook(validation_path)
    name = "ماشین حالت"
    if name in wb.sheetnames:
        del wb[name]

    ws = wb.create_sheet(name)
    ws.sheet_view.rightToLeft = True

    ws.merge_cells(start_row=1, start_column=1, end_row=1, end_column=3)
    tc = ws.cell(row=1, column=1, value=f"ماشین حالت ماژول {module}")
    tc.font = Font(name=FONT, size=14, bold=True, color="FFFFFF")
    tc.fill = PatternFill("solid", fgColor=C_TITLE)
    tc.alignment = Alignment(horizontal="center", vertical="center")
    ws.row_dimensions[1].height = 30

    rows = [
        ("حالت‌ها و گذارهای همین ماژول", state_book_name, state_book_name),
        ("ماشین حالت کل برنامه (نمای سیستمی)", SYSTEM_BOOK, SYSTEM_BOOK),
        ("راهنمای رنگ‌ها", COLOUR_LEGEND, None),
        ("بازتولید", "python3 tools/make_module_state_machines.py", None),
    ]
    for r, (label, value, target) in enumerate(rows, start=3):
        lc = ws.cell(row=r, column=1, value=label)
        lc.font = Font(name=FONT, size=10, bold=True)
        lc.alignment = Alignment(horizontal="right", vertical="center", wrap_text=True)
        lc.border = BORDER
        vc = ws.cell(row=r, column=2, value=value)
        vc.font = (Font(name=FONT, size=10, color=C_LINK, underline="single")
                   if target else Font(name=FONT, size=10))
        if target:
            vc.hyperlink = target
        vc.alignment = Alignment(horizontal="right", vertical="center", wrap_text=True)
        vc.border = BORDER
        ws.row_dimensions[r].height = 24

    ws.column_dimensions["A"].width = 34
    ws.column_dimensions["B"].width = 78
    ws.sheet_properties.tabColor = C_HEAD
    wb.save(validation_path)


def build():
    written = []
    for module, spec in MODULES.items():
        out = MODULES_DIR / module / f"{module}_State_Machine.xlsx"
        wb = Workbook()
        wb.remove(wb.active)

        guide_sheet(wb, module, spec["fa"], spec["sources"], spec["sheets"],
                    spec["validation"])
        for name, title, headers, rows, widths in spec["tabs"]:
            sheet(wb, name, title, headers, rows, widths)

        out.parent.mkdir(parents=True, exist_ok=True)
        wb.save(out)
        written.append(out.relative_to(ROOT))

        validation = MODULES_DIR / module / spec["validation"]
        if validation.exists():
            add_link_sheet(validation, spec["fa"], out.name)
        else:
            print("WARNING: no validation workbook for", module)

    for path in written:
        print("wrote", path)
    print(f"{len(written)} module state-machine workbooks")


if __name__ == "__main__":
    build()
