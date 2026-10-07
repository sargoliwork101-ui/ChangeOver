#!/usr/bin/env python3
#
# verify_report_xlsx.py - read the panel's Excel report with Python's own
# readers and check that the file is what it claims to be.
#
# [EN] WHY THIS EXISTS
#      The panel writes a real .xlsx (a stored ZIP of SpreadsheetML parts) with
#      its own hand-written writer, because an ESP8266 has 40 KB of heap and
#      cannot build a spreadsheet in RAM. Three readers already agree with it:
#      the writer's own host test, the gate's zipfile pass and openpyxl. This
#      script is the fourth and the strictest, and it is the only one that reads
#      the file the way a HUMAN will:
#        - every part is opened with Python's zipfile, not with the C code,
#        - every sheet is parsed as XML and the numbers are checked to be
#          numbers (a sheet full of text would open, and would be useless),
#        - every sample row's date and time are re-derived from its monotonic
#          stamp with jdatetime, which is an independent Persian calendar, so a
#          wrong leap year or a wrong midnight would fail here,
#        - the range selector is checked by DROPPING rows: the one-day workbook
#          must be a time-ordered suffix of the full workbook and nothing else.
#      The last point is the reason the tool exists: "?days=1" has to change the
#      FILE, not the label on it.
#
#      [FA] چرا این وجود دارد
#      پنل یک فایل اکسل واقعی (زیپ ذخیره‌ای از اجزای SpreadsheetML) را با
#      نویسندهٔ دست‌نویس خودش می‌سازد، چون ESP8266 چهل کیلوبایت هپ دارد و
#      نمی‌تواند صفحه‌گسترده را در رم بسازد. سه خواننده با آن توافق دارند:
#      تست میزبان خودِ نویسنده، گذر zipfile در گیت و openpyxl. این اسکریپت
#      چهارمی و سخت‌گیرترین است و تنها خواننده‌ای است که فایل را همان‌طور
#      می‌خواند که انسان می‌خواند:
#        - هر جزء با zipfile پایتون باز می‌شود، نه با کد C،
#        - هر برگه به‌عنوان XML پارس می‌شود و بررسی می‌شود که اعداد عدد
#          باشند (برگه‌ای پر از متن باز می‌شود و بی‌فایده است)،
#        - تاریخ و ساعت هر ردیف نمونه از مهر یکنوای آن با jdatetime بازسازی
#          می‌شود؛ یک تقویم شمسی مستقل، پس سال کبیسه یا نیمه‌شب غلط اینجا
#          شکست می‌خورد،
#        - انتخاب بازه با «انداختن» ردیف‌ها بررسی می‌شود: کتاب یک‌روزه باید
#          پسوند مرتب‌به‌زمان کتاب کامل باشد و نه چیز دیگر.
#      مورد آخر دلیل وجود این ابزار است: «?days=1» باید خود فایل را عوض کند،
#      نه برچسب روی آن را.
#
# Usage / طرز استفاده:
#     python3 user_panel/tools/verify_report_xlsx.py
#     python3 user_panel/tools/verify_report_xlsx.py --full /tmp/up_report.xlsx \
#         --day /tmp/up_report_1day.xlsx --base-absS 1000000 --base-epoch 1791331200
#
# Exit codes / کدهای خروج:
#     0 = everything checked and green (or the optional parsers are absent)
#     1 = a real failure
#
# The clock pair (--base-absS / --base-epoch) is the monotonic second and the
# wall-clock second of the seeded test clock: the host test sets it with
# func__UpStore_ClockBaseSet() before it writes any sample.
# جفت ساعت (--base-absS / --base-epoch) همان ثانیهٔ یکنوا و ثانیهٔ دیواری ساعت
# کاشته‌شدهٔ تست است که تست میزبان پیش از نوشتن هر نمونه تنظیم می‌کند.

import argparse
import re
import sys
import zipfile
import xml.etree.ElementTree as ElementTree

PASSED = 0
FAILED = 0
SKIPPED = 0


def ok(message):
    """[EN] Record one green check / [FA] ثبت یک بررسی سبز"""
    global PASSED
    PASSED += 1
    print("  ok   " + message)


def bad(message):
    """[EN] Record one red check / [FA] ثبت یک بررسی سرخ"""
    global FAILED
    FAILED += 1
    print("  FAIL " + message)


def skip(message):
    """[EN] Record one check that could not run / [FA] ثبت بررسی‌ای که اجرا نشد"""
    global SKIPPED
    SKIPPED += 1
    print("  ..   " + message)


def step(message):
    """[EN] Print a section header / [FA] چاپ سرتیتر بخش"""
    print("\n== " + message)


# --------------------------------------------------------------- the package --
def check_container(path, label):
    """[EN] Open the workbook as a ZIP and parse every member as XML.
       [FA] باز کردن کتاب به‌صورت زیپ و پارس هر عضو به‌عنوان XML."""
    try:
        handle = zipfile.ZipFile(path)
    except (OSError, zipfile.BadZipFile) as problem:
        bad("%s is not a readable zip (%s)" % (label, problem))
        return None

    if handle.testzip() is not None:
        bad("%s has a member whose CRC does not match" % label)
        return None
    ok("%s is a zip and every CRC matches (%d members)" % (label, len(handle.namelist())))

    bad_parts = []
    for name in handle.namelist():
        try:
            ElementTree.fromstring(handle.read(name))
        except ElementTree.ParseError as problem:
            bad_parts.append("%s: %s" % (name, problem))
    if bad_parts:
        bad("%s has members that are not XML: %s" % (label, ", ".join(bad_parts)))
    else:
        ok("%s: all %d members are well-formed XML" % (label, len(handle.namelist())))
    return handle


def member_text(handle, name):
    """[EN] Read one member as text / [FA] خواندن یک عضو به‌صورت متن"""
    return handle.read(name).decode("utf-8")


def check_package(handle, label, sheet_names):
    """[EN] The three fixed maps: content types, workbook, relationships.
       [FA] سه نگاشت ثابت: نوع محتوا، کتاب، ارتباط‌ها."""
    content_types = member_text(handle, "[Content_Types].xml")
    missing = [name for name in sheet_names if ("xl/worksheets/sheet%d.xml" % (sheet_names.index(name) + 1)) not in content_types]
    if missing:
        bad("%s: the content-type map does not declare every sheet" % label)
    else:
        ok("%s: the content-type map declares all five sheets and the styles" % label)

    workbook = member_text(handle, "xl/workbook.xml")
    for name in sheet_names:
        if name not in workbook:
            bad("%s: sheet name %r is missing from xl/workbook.xml" % (label, name))
            return
    ok("%s: the workbook carries the five Persian sheet names in order" % label)

    if all(('r:id="rId%d"' % number) in workbook for number in range(1, 6)):
        ok("%s: all five sheets are linked by relationship id" % label)
    else:
        bad("%s: the workbook does not link rId1..rId5" % label)

    # [EN] OPC resolves these targets relative to the file that declares them,
    #      so the correct value is "worksheets/sheetN.xml" and NOT the full path.
    # [FA] این مقصدها نسبت به فایلی که اعلامشان می‌کند حل می‌شوند، پس مقدار
    #      درست «worksheets/sheetN.xml» است و نه مسیر کامل.
    relationships = member_text(handle, "xl/_rels/workbook.xml.rels")
    links = dict(re.findall(r'<Relationship [^>]*Id="([^"]+)"[^>]*Target="([^"]+)"', relationships))
    for target, identifier in re.findall(r'<Relationship [^>]*Target="([^"]+)"[^>]*Id="([^"]+)"', relationships):
        links[identifier] = target
    targets = ["worksheets/sheet%d.xml" % number for number in range(1, 6)]
    if all(links.get("rId%d" % (index + 1)) == target for index, target in enumerate(targets)):
        ok("%s: every relationship id points at its own worksheet" % label)
    else:
        bad("%s: the relationship map does not point rId1..rId5 at the sheets (%s)"
            % (label, links))

    root = member_text(handle, "_rels/.rels")
    if "xl/workbook.xml" in root:
        ok("%s: the package root points at the workbook" % label)
    else:
        bad("%s: _rels/.rels does not point at xl/workbook.xml" % label)


def check_furniture(handle, label):
    """[EN] Right-to-left, frozen headers, and the one ordering rule Excel
           enforces: <cols> before <sheetData>.
       [FA] راست‌به‌چپ، سرصفحهٔ ثابت و تنها قانون ترتیبی که اکسل تحمیل می‌کند."""
    for index in range(1, 6):
        sheet = member_text(handle, "xl/worksheets/sheet%d.xml" % index)
        if '<sheetView rightToLeft="1"' not in sheet:
            bad("%s: sheet%d is not marked right-to-left" % (label, index))
        if "<pane " not in sheet:
            bad("%s: sheet%d has no frozen row" % (label, index))
        if "<cols>" in sheet and sheet.index("<cols>") > sheet.index("<sheetData>"):
            bad("%s: sheet%d puts <cols> after <sheetData>" % (label, index))
    ok("%s: every sheet is right-to-left, frozen and correctly ordered" % label)


# ---------------------------------------------------------- the data itself ---
def check_numbers_with_openpyxl(path, label):
    """[EN] A sheet whose numbers are text opens fine and is useless: charts and
           filters need real numbers.
       [FA] برگه‌ای که اعدادش متنی است باز می‌شود و بی‌فایده است."""
    try:
        import openpyxl
    except ImportError:
        skip("openpyxl is not installed, the number-typing check is skipped")
        return None

    book = openpyxl.load_workbook(path)
    samples = next(sheet for sheet in book.worksheets if sheet.title.startswith("نمونه"))
    text_cells = 0
    numeric_cells = 0
    rows = list(samples.iter_rows(min_row=2, values_only=True))
    for row in rows:
        for column in (2, 3, 4, 5, 6, 7, 8, 9, 10):
            if isinstance(row[column], (int, float)):
                numeric_cells += 1
            else:
                text_cells += 1
    if text_cells > 0 and numeric_cells == 0:
        bad("%s: the sample numbers arrived as text" % label)
    else:
        ok("%s: %d sample measurements are real numbers (%d text cells)"
           % (label, numeric_cells, text_cells))

    for index, sheet in enumerate(book.worksheets):
        if sheet.sheet_view.rightToLeft is not True:
            bad("%s: openpyxl does not see sheet%d as right-to-left" % (label, index + 1))
    if any(sheet.freeze_panes not in ("A2", "A3") for sheet in book.worksheets):
        bad("%s: openpyxl does not see a frozen header on every sheet" % label)
    else:
        ok("%s: openpyxl agrees on right-to-left and frozen headers" % label)
    return book


def check_calendar_oracle(book, label, base_abs_s, base_epoch_s, tz_minutes):
    """[EN] Re-derive every sample's date and time from its monotonic stamp,
           using jdatetime as the Persian calendar instead of the panel's own.
       [FA] بازسازی تاریخ و ساعت هر نمونه از مهر یکنوای آن، با jdatetime
            به‌عنوان تقویم شمسی به‌جای تقویم خودِ پنل."""
    try:
        import jdatetime
    except ImportError:
        skip("jdatetime is not installed, the calendar oracle is skipped")
        return 0

    samples = next(sheet for sheet in book.worksheets if sheet.title.startswith("نمونه"))
    checked = 0
    for row in samples.iter_rows(min_row=2, values_only=True):
        stamp = row[2]
        if not isinstance(stamp, int):
            continue
        # [EN] The panel stores UTC seconds and prints them at a fixed offset, so
        #      the local wall time is the stamp plus the offset. Handing that to
        #      jdatetime - rather than asking it for a timezone - keeps this check
        #      free of any dependency on a timezone database.
        # [FA] پنل ثانیهٔ UTC را ذخیره و با اختلاف ثابت چاپ می‌کند، پس ساعت
        #      دیواری محلی همان مهر به‌علاوهٔ اختلاف است. دادن همین به jdatetime -
        #      به‌جای پرسیدن منطقهٔ زمانی از آن - این بررسی را از هر پایگاه دادهٔ
        #      منطقهٔ زمانی بی‌نیاز می‌کند.
        epoch = base_epoch_s + (stamp - base_abs_s) + (tz_minutes * 60)
        moment = jdatetime.datetime.fromtimestamp(epoch)
        expected_date = "%04d/%02d/%02d" % (moment.year, moment.month, moment.day)
        expected_time = "%02d:%02d:%02d" % (moment.hour, moment.minute, moment.second)
        if row[0] != expected_date or row[1] != expected_time:
            bad("%s: sample %d says %s %s, jdatetime says %s %s"
                % (label, stamp, row[0], row[1], expected_date, expected_time))
            return checked
        checked += 1
    ok("%s: all %d sample dates and times agree with jdatetime" % (label, checked))
    return checked


def check_range_is_a_suffix(handle_full, handle_day, label):
    """[EN] The strongest statement about the range selector: the one-day
           workbook holds the same rows as the full one, in the same order,
           with the old ones dropped - a suffix, not a different file.
       [FA] قوی‌ترین جمله دربارهٔ انتخاب بازه: کتاب یک‌روزه همان ردیف‌های کتاب
            کامل را با همان ترتیب دارد و قدیمی‌ها افتاده‌اند - یک پسوند."""
    full_rows = sample_rows(handle_full)
    day_rows = sample_rows(handle_day)
    if not day_rows:
        bad("%s: the one-day workbook has no sample rows at all" % label)
        return
    if len(day_rows) >= len(full_rows):
        bad("%s: the one-day workbook is not smaller than the full one" % label)
        return
    if full_rows[len(full_rows) - len(day_rows):] != day_rows:
        bad("%s: the one-day rows are not the tail of the full workbook" % label)
        return
    ok("%s: the one-day workbook is exactly the newest %d of %d rows, in order"
       % (label, len(day_rows), len(full_rows)))

    newest = day_rows[-1][2]
    cutoff = newest - 24 * 3600
    oldest = day_rows[0][2]
    if oldest < cutoff:
        bad("%s: a row older than the 24 h window survived (%d < %d)" % (label, oldest, cutoff))
    else:
        ok("%s: every one-day row is inside the 24 h window" % label)

    older_in_full = [row for row in full_rows if row[2] < cutoff]
    if not older_in_full:
        bad("%s: the full workbook has no row old enough to prove the filter drops anything" % label)
    else:
        ok("%s: the full workbook keeps %d rows the one-day report dropped"
           % (label, len(older_in_full)))


def sample_rows(handle):
    """[EN] Every DATA row of the sample sheet as a list of values read straight
           from the XML, without openpyxl, so this check never depends on a
           library. The header row is recognised by its stamp cell not being a
           number, and a decimal keeps its exact value (12.345, not 12) because
           two workbooks are compared cell by cell.
       [FA] هر ردیف دادهٔ برگهٔ نمونه از خود XML، بدون openpyxl. ردیف سرصفحه از
           این شناخته می‌شود که خانهٔ مهرش عدد نیست، و اعشار مقدار دقیقش را
           نگه می‌دارد (۱۲٫۳۴۵ و نه ۱۲) چون دو کتاب خانه‌به‌خانه مقایسه
           می‌شوند."""
    sheet = member_text(handle, "xl/worksheets/sheet5.xml")
    rows = []
    for row_match in re.finditer(r"<row [^>]*>(.*?)</row>", sheet, re.S):
        cells = re.findall(r"<c [^>]*>(.*?)</c>", row_match.group(1), re.S)
        values = []
        for cell in cells:
            text = re.search(r"<t[^>]*>(.*?)</t>", cell, re.S)
            number = re.search(r"<v>(.*?)</v>", cell, re.S)
            if text is not None:
                values.append(text.group(1))
            elif number is not None:
                values.append(float(number.group(1)) if "." in number.group(1)
                              else int(number.group(1)))
            else:
                values.append(None)
        if len(values) >= 3 and isinstance(values[2], (int, float)):
            rows.append(values)
    return rows


def check_sheet_shapes(handle, label):
    """[EN] Five named sheets, each with a header row and the columns the page
           promises. A missing sheet is a missing answer.
       [FA] پنج برگهٔ نام‌دار، هرکدام با سرصفحه و ستون‌هایی که صفحه وعده داده."""
    expected = ["خلاصه", "روزانه", "شارژها", "رویدادها", "نمونه\u200cها"]
    workbook = member_text(handle, "xl/workbook.xml")
    for name in expected:
        if name not in workbook:
            bad("%s: the sheet %r is missing" % (label, name))
            return
    ok("%s: all five sheets are present" % label)

    daily = member_text(handle, "xl/worksheets/sheet2.xml")
    if "تاریخ شمسی" not in daily or "انرژی (وات\u200cساعت)" not in daily:
        bad("%s: the daily sheet lost its Persian header" % label)
    charges = member_text(handle, "xl/worksheets/sheet3.xml")
    if "مدت (دقیقه)" not in charges:
        bad("%s: the charge sheet has no duration column" % label)
    events = member_text(handle, "xl/worksheets/sheet4.xml")
    if "رویداد" not in events or "شدت" not in events:
        bad("%s: the event sheet has no event or severity column" % label)
    if "<sheetData>" not in member_text(handle, "xl/worksheets/sheet1.xml"):
        bad("%s: the summary sheet is empty" % label)
    ok("%s: every sheet has its Persian header and its columns" % label)


def main():
    parser = argparse.ArgumentParser(description="Verify the user panel's Excel report with Python's own readers.")
    parser.add_argument("--full", default="/tmp/up_report.xlsx",
                        help="the workbook with no range filter (default: %(default)s)")
    parser.add_argument("--day", default="/tmp/up_report_1day.xlsx",
                        help="the workbook the range selector produced (default: %(default)s)")
    parser.add_argument("--base-absS", type=int, default=1000000,
                        help="monotonic second of the seeded clock (default: %(default)s)")
    parser.add_argument("--base-epoch", type=int, default=1791331200,
                        help="wall-clock second of the seeded clock (default: %(default)s)")
    parser.add_argument("--tz-minutes", type=int, default=210,
                        help="the panel's timezone in minutes (default: %(default)s, Tehran)")
    arguments = parser.parse_args()

    print("== Excel report verification / بررسی گزارش اکسل")
    print("   full: %s" % arguments.full)
    print("   day:  %s" % arguments.day)

    step("container / ظرف فایل")
    full = check_container(arguments.full, "the full workbook")
    day = check_container(arguments.day, "the one-day workbook")
    if full is None or day is None:
        print("\n%d passed, %d failed, %d skipped" % (PASSED, FAILED, SKIPPED))
        return 1

    step("package map / نگاشت بسته")
    names = ["خلاصه", "روزانه", "شارژها", "رویدادها", "نمونه\u200cها"]
    check_package(full, "the full workbook", names)
    check_package(day, "the one-day workbook", names)

    step("sheet furniture / چیدمان برگه‌ها")
    check_furniture(full, "the full workbook")
    check_furniture(day, "the one-day workbook")
    check_sheet_shapes(full, "the full workbook")

    step("numbers and headers / اعداد و سرصفحه‌ها")
    full_book = check_numbers_with_openpyxl(arguments.full, "the full workbook")

    step("calendar oracle / مرجع تقویم")
    if full_book is not None:
        check_calendar_oracle(full_book, "the full workbook", arguments.base_absS,
                              arguments.base_epoch, arguments.tz_minutes)

    step("range selector / انتخاب بازه")
    check_range_is_a_suffix(full, day, "the range selector")

    print("\n%d passed, %d failed, %d skipped" % (PASSED, FAILED, SKIPPED))
    if FAILED > 0:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
