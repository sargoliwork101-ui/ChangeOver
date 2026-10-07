#!/usr/bin/env python3
#
# verify_page_calendar.py - check the PAGE's own calendar against an independent
# Persian-calendar oracle.
#
# [EN] WHY THIS EXISTS
#      The date a viewer reads on this panel is NOT computed by the board: the
#      page turns the panel's epoch into "۱۵ مهر ۱۴۰۵" in the browser, in its own
#      JavaScript. There is therefore a second calendar in this module, and until
#      this script existed only the C one (`up_calendar.h`) was checked against an
#      independent source - the host test's day-by-day sweep and the workbook
#      check that re-derives every row's date with jdatetime.
#      A page whose dates are one day off looks exactly like a page whose dates
#      are right: nobody reading a dashboard knows what today's Jalali date should
#      be, which is why this cannot be left to the eye. This script:
#        - lifts `jalaliOf`, `gregorianOfJalali` and `panelParts` OUT OF web/app.js
#          by name, so what is tested is the code that ships and not a copy of it,
#        - runs them in node over a dense sweep of days,
#        - re-derives every answer with jdatetime, the same independent oracle the
#          workbook check trusts,
#        - round-trips Gregorian → Jalali → Gregorian inside the page's own code,
#        - and finally puts the panel's own anchor through the whole chain: the
#          epoch 1791331200 at +210 minutes must print 1405/07/15, which is the
#          date the board's own clock route answers for that number (proved by the
#          simulator self test). The two calendars must agree on a real stamp, not
#          only in the abstract.
#
#      [FA] چرا این وجود دارد
#      تاریخی که بینندهٔ این پنل می‌خواند را برد حساب نمی‌کند: صفحه ثانیهٔ مطلق پنل
#      را در مرورگر و با جاوااسکریپت خودش به «۱۵ مهر ۱۴۰۵» تبدیل می‌کند. پس در این
#      ماژول دو تقویم وجود دارد، و تا وقتی این اسکریپت نبود فقط تقویم C
#      (`up_calendar.h`) با یک منبع مستقل سنجیده می‌شد - پویش روز‌به‌روز تست میزبان و
#      بازسازی تاریخ هر ردیف با jdatetime در بررسی کتاب اکسل.
#      صفحه‌ای که تاریخش یک روز جابه‌جاست، دقیقاً مثل صفحه‌ای است که تاریخش درست
#      است: هیچ‌کس با نگاه‌کردن به داشبورد نمی‌داند تاریخ شمسی امروز چه باید باشد،
#      و همین است که این کار را به چشم نمی‌توان سپرد. این اسکریپت:
#        - توابع `jalaliOf`، `gregorianOfJalali` و `panelParts` را با نام از
#          `web/app.js` بیرون می‌کشد، پس آنچه آزمایش می‌شود همان کد تحویلی است و نه
#          رونویسی‌اش،
#        - آن‌ها را در node روی پویشی متراکم از روزها می‌دواند،
#        - هر پاسخ را با jdatetime بازمی‌سازد، همان مرجع مستقلی که بررسی کتاب اکسل
#          هم به آن اعتماد می‌کند،
#        - رفت‌وبرگشت میلادی ← شمسی ← میلادی را در کد خودِ صفحه می‌سنجد،
#        - و آخر سر لنگر خود پنل را از کل زنجیر می‌گذراند: ثانیهٔ مطلق ۱۷۹۱۳۳۱۲۰۰ با
#          اختلاف ۲۱۰ دقیقه باید ۱۴۰۵/۰۷/۱۵ چاپ کند - همان تاریخی که مسیر ساعت خود
#          برد برای آن عدد می‌دهد (اثبات‌شده با خودآزمای شبیه‌ساز). دو تقویم باید
#          روی یک مهر واقعی هم توافق کنند، نه فقط در کلیات.
#
# Usage / طرز استفاده:
#     python3 user_panel/tools/verify_page_calendar.py [--from-year 2000] [--to-year 2080]
#
# Exit code / کد خروج: 0 = every check passed, 1 = at least one check failed.

import argparse
import datetime
import os
import re
import subprocess
import sys
import tempfile

PASSED = 0
FAILED = 0
SKIPPED = 0

HERE = os.path.dirname(os.path.abspath(__file__))
MODULE = os.path.dirname(HERE)
APP_JS = os.path.join(MODULE, "web", "app.js")

# [EN] The panel's own clock anchor: the number the board's clock route answers
#      with, and the Jalali date both calendars must give for it.
# [FA] لنگر ساعت خود پنل: همان عددی که مسیر ساعت برد برایش جواب می‌دهد و تاریخی
#      که هر دو تقویم باید برایش بدهند.
ANCHOR_EPOCH = 1791331200
ANCHOR_TZ_MIN = 210
ANCHOR_DATE = "1405/07/15"

J_MONTH_LEN = 31  # [EN] longest Jalali month / [FA] بلندترین ماه شمسی


def step(title):
    """[EN] Print a section title. [FA] چاپ عنوان بخش."""
    print("\n== %s" % title)


def check(ok, what, detail=""):
    """[EN] Count one check and print it. [FA] یک بررسی را بشمار و چاپ کن."""
    global PASSED, FAILED
    if ok:
        PASSED += 1
        print("  ok   %s" % what)
    else:
        FAILED += 1
        print("  FAIL %s" % what)
        if detail:
            print("       %s" % detail)
    return ok


def fail(what, detail=""):
    """[EN] Count one failure. [FA] یک شکست را بشمار."""
    return check(False, what, detail)


def skip(what):
    """[EN] Count one skipped check. [FA] یک بررسی رد‌شده را بشمار."""
    global SKIPPED
    SKIPPED += 1
    print("  skip %s" % what)


def read_app_js():
    """[EN] Read the shipped page. [FA] صفحهٔ تحویلی را بخوان."""
    with open(APP_JS, "r", encoding="utf-8") as handle:
        return handle.read()


def lift_function(source, name):
    """[EN] Copy one top-level `function name(...) { ... }` out of the page.
       [FA] یک تابع سطح‌بالای هم‌نام را از دل صفحه بیرون بکش."""
    pattern = re.compile(r"(?ms)^function %s\(.*?\n}\n" % re.escape(name))
    match = pattern.search(source)
    if match is None:
        return None
    return match.group(0)


def build_harness(functions, from_year, to_year):
    """[EN] Assemble a node program that sweeps the years and writes two files.
       [FA] برنامه‌ای برای node بساز که سال‌ها را بپیماید و دو فایل بنویسد."""
    head = "\n".join(functions)
    body = """
const fs = require('fs');
const args = process.argv.slice(2);
const fromYear = Number(args[0]);
const toYear = Number(args[1]);
const outDir = args[2];
const gDaysInMonth = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
let forward = [];
for (let gy = fromYear; gy <= toYear; gy++) {
  for (let gm = 1; gm <= 12; gm++) {
    for (let gd = 1; gd <= gDaysInMonth[gm - 1]; gd++) {
      const j = jalaliOf(gy, gm, gd);
      forward.push(gy + '-' + gm + '-' + gd + ',' + j.y + '-' + j.m + '-' + j.d);
    }
  }
}
fs.writeFileSync(outDir + '/forward.csv', forward.join('\\n') + '\\n');
let back = [];
for (let jy = fromYear - 621; jy <= toYear - 621; jy++) {
  for (let jm = 1; jm <= 12; jm++) {
    for (let jd = 1; jd <= 31; jd++) {
      const g = gregorianOfJalali(jy, jm, jd);
      back.push(jy + '-' + jm + '-' + jd + ',' + g.y + '-' + g.m + '-' + g.d);
    }
  }
}
fs.writeFileSync(outDir + '/back.csv', back.join('\\n') + '\\n');
let roundtripBad = 0;
for (let gy = fromYear; gy <= toYear; gy++) {
  for (let gm = 1; gm <= 12; gm++) {
    for (let gd = 1; gd <= gDaysInMonth[gm - 1]; gd++) {
      const j = jalaliOf(gy, gm, gd);
      const g = gregorianOfJalali(j.y, j.m, j.d);
      if ((g.y !== gy) || (g.m !== gm) || (g.d !== gd)) { roundtripBad++; }
    }
  }
}
const parts = panelParts(""" + str(ANCHOR_EPOCH) + """, """ + str(ANCHOR_TZ_MIN) + """);
const anchorJ = jalaliOf(parts.y, parts.mo, parts.d);
console.log('roundtrip_bad=' + roundtripBad);
console.log('anchor=' + anchorJ.y + '/' +
            String(anchorJ.m).padStart(2, '0') + '/' + String(anchorJ.d).padStart(2, '0'));
console.log('anchor_source=' + parts.y + '-' + parts.mo + '-' + parts.d);
"""
    return head + body


def read_pairs(path):
    """[EN] Read the node output table. [FA] جدول خروجی node را بخوان."""
    pairs = {}
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            left, right = line.split(",")
            pairs[left] = right
    return pairs


def check_forward(pairs, oracle):
    """[EN] Every Gregorian day the page converts must match the oracle.
       [FA] هر روز میلادی که صفحه تبدیل می‌کند باید با مرجع بخواند."""
    checked = 0
    bad = []
    for key, got in pairs.items():
        gy, gm, gd = (int(part) for part in key.split("-"))
        want = oracle(gy, gm, gd)
        checked += 1
        if want != got:
            if len(bad) < 3:
                bad.append("%s: page=%s oracle=%s" % (key, got, want))
    ok = check(len(bad) == 0, "page → %d Gregorian days converted to Jalali, %d wrong"
               % (checked, len(bad)), "; ".join(bad))
    return ok


def check_reverse(pairs, calendar):
    """[EN] Every real Jalali day the page converts back must match the oracle.
       [FA] هر روز شمسی واقعی که صفحه برمی‌گرداند باید با مرجع بخواند."""
    checked = 0
    invented = 0
    bad = []
    for key, got in pairs.items():
        jy, jm, jd = (int(part) for part in key.split("-"))
        try:
            want_date = calendar(jy, jm, jd)
        except ValueError:
            invented += 1
            continue
        want = "%d-%d-%d" % (want_date.year, want_date.month, want_date.day)
        checked += 1
        if want != got:
            if len(bad) < 3:
                bad.append("%s: page=%s oracle=%s" % (key, got, want))
    ok = check(len(bad) == 0, "page ← %d Jalali days converted to Gregorian, %d wrong"
               % (checked, len(bad)), "; ".join(bad))
    if invented:
        print("       (%d swept days were Esfand 30 of a common year and do not exist - skipped)"
              % invented)
    return ok


def check_anchor(lines):
    """[EN] The page's chain must print the date the board prints.
       [FA] زنجیر صفحه باید همان تاریخی را چاپ کند که برد چاپ می‌کند."""
    anchor = None
    for line in lines:
        if line.startswith("anchor="):
            anchor = line.split("=", 1)[1].strip()
    return check(anchor == ANCHOR_DATE,
                 "the panel's clock anchor %d (+%d min) prints %s in the page"
                 % (ANCHOR_EPOCH, ANCHOR_TZ_MIN, ANCHOR_DATE),
                 "the page said %s" % anchor)


def check_roundtrip(lines):
    """[EN] Day → Jalali → day must return the same day, in the page's own code.
       [FA] روز ← شمسی ← روز باید همان روز را برگرداند، در کد خود صفحه."""
    bad = None
    for line in lines:
        if line.startswith("roundtrip_bad="):
            bad = int(line.split("=", 1)[1])
    return check(bad == 0, "the page round-trips every day it converts", "bad days: %s" % bad)


def main():
    """[EN] Lift the page's calendar out of app.js, run it, judge it.
       [FA] تقویم صفحه را از app.js بیرون بکش، بِدوانش، داوری کن."""
    parser = argparse.ArgumentParser(description="check the panel page's own Persian calendar")
    parser.add_argument("--from-year", type=int, default=2000)
    parser.add_argument("--to-year", type=int, default=2080)
    arguments = parser.parse_args()

    print("ChangeOver user panel - the page's own calendar / تقویم خود صفحه")

    try:
        import jdatetime
    except ImportError:
        skip("jdatetime is not installed, so the oracle cannot be asked")
        print("\n%d passed, %d failed, %d skipped" % (PASSED, FAILED, SKIPPED))
        return 0

    step("the shipped code / کد تحویلی")
    source = read_app_js()
    functions = []
    for name in ("jalaliOf", "gregorianOfJalali", "panelParts"):
        lifted = lift_function(source, name)
        if lifted is None:
            fail("web/app.js still has a function called %s()" % name,
                 "the page's calendar cannot be found; this check must not pass silently")
            print("\n%d passed, %d failed, %d skipped" % (PASSED, FAILED, SKIPPED))
            return 1
        functions.append(lifted)
    check(True, "the page's own calendar functions were found in web/app.js")

    step("running the page's calendar / اجرای تقویم صفحه")
    out_dir = tempfile.mkdtemp(prefix="up-cal-")
    harness = build_harness(functions, arguments.from_year, arguments.to_year)
    with open(os.path.join(out_dir, "sweep.js"), "w", encoding="utf-8") as handle:
        handle.write(harness)
    try:
        finished = subprocess.run(["node", os.path.join(out_dir, "sweep.js"),
                                   str(arguments.from_year), str(arguments.to_year), out_dir],
                                  capture_output=True, text=True, timeout=300, check=False)
    except FileNotFoundError:
        skip("node is not installed, so the page cannot be run")
        print("\n%d passed, %d failed, %d skipped" % (PASSED, FAILED, SKIPPED))
        return 0
    if finished.returncode != 0:
        fail("the page's calendar runs in node", finished.stderr.strip()[-400:])
        print("\n%d passed, %d failed, %d skipped" % (PASSED, FAILED, SKIPPED))
        return 1
    check(True, "the page's calendar ran over %d-%d without a runtime error"
          % (arguments.from_year, arguments.to_year))

    forward = read_pairs(os.path.join(out_dir, "forward.csv"))
    backward = read_pairs(os.path.join(out_dir, "back.csv"))
    check(len(forward) > 1000 and len(backward) > 1000,
          "the sweep produced a dense table of days",
          "forward %d rows, backward %d rows" % (len(forward), len(backward)))

    step("versus the oracle / در برابر مرجع")
    def oracle_forward(gy, gm, gd):
        """[EN] The oracle's Jalali date, in the same text shape the page uses.
           [FA] تاریخ شمسی مرجع، با همان شکل متنی که صفحه می‌دهد."""
        jalali = jdatetime.date.fromgregorian(date=datetime.date(gy, gm, gd))
        return "%d-%d-%d" % (jalali.year, jalali.month, jalali.day)

    check_forward(forward, oracle_forward)
    check_reverse(backward, lambda jy, jm, jd: jdatetime.date(jy, jm, jd).togregorian())

    step("the page's whole chain / کل زنجیر صفحه")
    lines = finished.stdout.splitlines()
    check_roundtrip(lines)
    check_anchor(lines)

    print("\n%d passed, %d failed, %d skipped" % (PASSED, FAILED, SKIPPED))
    if FAILED > 0:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
