#!/usr/bin/env bash
#
# check_user_panel.sh - every check this module can run without a board.
#
# [EN] WHY THIS EXISTS
#      The user panel is one Arduino sketch, and the only machine that normally
#      compiles it is the Arduino IDE at flashing time. This script runs the
#      five checks that catch almost everything before a board is plugged in:
#        1. the embedded page is not stale (regenerate and compare),
#        2. the page's JavaScript parses,
#        3. the sketch compiles and its behaviour tests pass under
#           AddressSanitizer + UndefinedBehaviorSanitizer,
#        4. the page's API map and the sketch's route table agree,
#        5. every source file carries its bilingual header and the request path
#           stays allocation-free.
#      A green run here is NOT a substitute for the bench: no host test can see
#      a wrong pin, a weak Wi-Fi antenna or a flash partition that is too small.
#
#      [FA] چرا این وجود دارد
#      پنل کاربر یک اسکچ آردوینو است و معمولاً تنها ماشینی که آن را کامپایل
#      می‌کند Arduino IDE سرِ فلش‌کردن است. این اسکریپت همان پنج بررسی‌ای را
#      اجرا می‌کند که تقریباً همه‌چیز را پیش از وصل‌کردن برد می‌گیرد:
#        ۱. صفحهٔ توکار کهنه نباشد (از نو ساخته و مقایسه می‌شود)،
#        ۲. جاوااسکریپت صفحه پارس شود،
#        ۳. اسکچ کامپایل و تست‌های رفتاری‌اش زیر AddressSanitizer و
#           UndefinedBehaviorSanitizer سبز شوند،
#        ۴. نگاشت API صفحه و جدول مسیرهای اسکچ با هم بخوانند،
#        ۵. هر فایل منبع سرصفحهٔ دوزبانه داشته باشد و مسیر درخواست بدون
#           تخصیص حافظه بماند.
#      سبز بودن اینجا جای میز آزمایش را نمی‌گیرد: هیچ تست میزبان نمی‌تواند
#      پایهٔ اشتباه، آنتن ضعیف وای‌فای یا پارتیشن فلش کوچک را ببیند.
#
# Usage / طرز استفاده:
#     bash user_panel/tools/check_user_panel.sh          (from the repo root)

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
MODULE="$(cd "$HERE/.." && pwd)"
FAILED=0
PASSED=0

ok()   { PASSED=$((PASSED + 1)); printf '  ok   %s\n' "$1"; }
bad()  { FAILED=$((FAILED + 1)); printf '  FAIL %s\n' "$1"; }
skip_check() { printf '  ..   %s\n' "$1"; }
step() { printf '\n== %s\n' "$1"; }

# ---------------------------------------------------------------- 1. assets --
step "embedded page / صفحهٔ توکار"
TMP_HEADER="$(mktemp)"
trap 'rm -f "$TMP_HEADER" /tmp/up_host_test' EXIT

if python3 "$HERE/build_panel_header.py" --output "$TMP_HEADER" >/dev/null 2>&1; then
    if diff -q "$TMP_HEADER" "$MODULE/up_web.h" >/dev/null 2>&1; then
        ok "up_web.h matches web/ (index.html, app.css, app.js)"
    else
        bad "up_web.h is STALE - run: python3 user_panel/tools/build_panel_header.py"
    fi
else
    bad "build_panel_header.py failed"
fi

# [EN] The offline preview is a convenience, but a stale one is a lie: it
#      claims to show what the board serves. Regenerate and compare.
# [FA] پیش‌نمایش آفلاین یک راحتی است، ولی کهنه‌اش دروغ است: ادعا می‌کند همانی را
#      نشان می‌دهد که برد سرو می‌کند. از نو بسازید و مقایسه کنید.
if [ -f "$MODULE/preview/user_panel_preview.html" ]; then
    TMP_PREVIEW="$(mktemp)"
    if python3 "$HERE/build_preview.py" --output "$TMP_PREVIEW" >/dev/null 2>&1 &&
       diff -q "$TMP_PREVIEW" "$MODULE/preview/user_panel_preview.html" >/dev/null 2>&1; then
        ok "preview/user_panel_preview.html matches web/ and the embedded font"
    else
        bad "the preview is STALE - run: python3 user_panel/tools/build_preview.py"
    fi
    rm -f "$TMP_PREVIEW"
fi

for asset in index.html app.css app.js; do
    if [ -s "$MODULE/web/$asset" ]; then
        ok "web/$asset present ($(wc -c < "$MODULE/web/$asset") bytes)"
    else
        bad "web/$asset is missing or empty"
    fi
done

# [EN] The page has a fourth asset, /f.css: the Persian web font as base64. It
#      is not a file under web/ - it is generated once into up_font.h - so what
#      is checked is that the header still carries it.
# [FA] صفحه دارایی چهارمی هم دارد، /f.css: قلم وب فارسی به‌صورت base64. فایلی
#      زیر web/ نیست - یک‌بار در up_font.h ساخته شده - پس آنچه بررسی می‌شود این
#      است که هدر هنوز آن را دارد.
if grep -q 'UP_PANEL_FONT_CSS' "$MODULE/up_font.h"; then
    ok "the Persian web font is embedded (up_font.h, served as /f.css)"
else
    bad "up_font.h no longer defines UP_PANEL_FONT_CSS - /f.css would be empty"
fi

# ------------------------------------------------------------------ 2. JS ----
step "page JavaScript / جاوااسکریپت صفحه"
if command -v node >/dev/null 2>&1; then
    if node --check "$MODULE/web/app.js" >/dev/null 2>&1; then
        ok "web/app.js parses"
    else
        bad "web/app.js does not parse (run: node --check web/app.js)"
    fi
else
    printf '  ..   node not installed, JavaScript parse check skipped\n'
fi

# ------------------------------------------------- 3. compile and behaviour --
step "host compile and behaviour tests / کامپایل و تست رفتاری"
CXX="${CXX:-g++}"
if command -v "$CXX" >/dev/null 2>&1; then
    if "$CXX" -std=gnu++17 -Wall -Wextra -Werror -Wno-unused-parameter \
        -I "$MODULE/tools" -I "$MODULE/tools/stubinc" \
        -g -fsanitize=address,undefined -fno-omit-frame-pointer \
        "$HERE/host_test_user_panel.cpp" -o /tmp/up_host_test 2>/tmp/up_build.log; then
        ok "sketch compiles clean (-Wall -Wextra -Werror)"
        if ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 /tmp/up_host_test >/tmp/up_test.log 2>&1; then
            ok "behaviour tests: $(grep -E 'checks, [0-9]+ failures' /tmp/up_test.log | tail -1)"
        else
            bad "behaviour tests failed:"
            grep -E '^  FAIL' /tmp/up_test.log | sed 's/^/       /'
        fi
    else
        bad "compile failed:"
        sed 's/^/       /' /tmp/up_build.log | head -20
    fi
else
    bad "no C++ compiler found (set CXX)"
fi

# ---------------------------------------- 3b. the workbook, read by Python ----
# [EN] The host test writes two real workbooks (one with no range filter, one
#      with the one-day filter). A third reader - Python's zipfile, openpyxl and
#      jdatetime - then re-derives every sample's date from its monotonic stamp
#      and proves the range selector dropped rows instead of relabelling them.
#      Three readers agreeing is the whole point: the panel's own writer is not
#      the judge of its own output.
# [FA] تست میزبان دو کتاب واقعی می‌نویسد (یکی بدون فیلتر و یکی با فیلتر
#      یک‌روزه). خوانندهٔ سومی - zipfile و openpyxl و jdatetime پایتون - تاریخ
#      هر نمونه را از مهر یکنوایش بازمی‌سازد و ثابت می‌کند انتخاب بازه ردیف
#      انداخته و فقط برچسب را عوض نکرده است.
step "the workbook, read by Python / کتاب اکسل، خوانده‌شده با پایتون"
if [ -f /tmp/up_report.xlsx ] && [ -f /tmp/up_report_1day.xlsx ]; then
    if python3 "$HERE/verify_report_xlsx.py" >/tmp/up_xlsx.log 2>&1; then
        ok "$(tail -1 /tmp/up_xlsx.log)"
    else
        bad "the workbook failed the independent Python reading:"
        grep -E '^  FAIL|Error|Traceback' /tmp/up_xlsx.log | sed 's/^/       /'
    fi
else
    skip_check "no workbook on disk (the host test writes them)"
fi

# ------------------------------------------------- 4. routes vs the page map --
step "routes and the page's API map / مسیرها و نگاشت API صفحه"
MISMATCH=0
for path in $(grep -oE "'/api/[a-z/]+'" "$MODULE/web/app.js" | tr -d "'" | sort -u); do
    if ! grep -q "\"$path\"" "$MODULE/up_http.h"; then
        bad "the page calls $path but no route registers it"
        MISMATCH=1
    fi
done
for path in $(grep -oE 'on\("/api/[a-z/]+"' "$MODULE/up_http.h" | sed 's/on("//; s/"//' | sort -u); do
    if ! grep -q "'$path'" "$MODULE/web/app.js"; then
        printf '  ..   route %s is served but the page never calls it\n' "$path"
    fi
done
if [ "$MISMATCH" -eq 0 ]; then
    ok "every route the page calls exists on the device"
fi

# ------------------------------------------------------------ 5. house rules --
step "house rules / قواعد پروژه"

# [EN] The C sources, named one by one. up_web.h and up_font.h are excluded
#      from the code scans below on purpose: they are embedded page and embedded
#      font, so a scan for "new Date" or "String" would be reading JavaScript
#      and CSS, not C.
# [FA] منابع C، یکی‌یکی نام‌برده‌شده. up_web.h و up_font.h عمداً از بررسی‌های
#      کدی زیر کنار گذاشته شده‌اند: آن‌ها صفحهٔ جاسازی‌شده و قلم جاسازی‌شده‌اند،
#      پس جست‌وجوی «new Date» یا «String» در آن‌ها یعنی خواندن جاوااسکریپت و
#      CSS، نه C.
C_SOURCES="up_config.h up_state.h up_store.h up_auth.h up_history.h up_link.h up_http.h up_sha256.h up_calendar.h up_xlsx.h up_report.h user_panel.ino"

for file in "$MODULE"/up_*.h "$MODULE"/user_panel.ino; do
    name="$(basename "$file")"
    head -200 "$file" | grep -q '\[EN\]' || { bad "$name has no English header text"; continue; }
    head -200 "$file" | grep -q '\[FA\]' || { bad "$name has no Persian header text"; continue; }
    ok "$name is bilingual"
done

ALLOC_HITS=""
for name in $C_SOURCES; do
    if grep -nE '\b(malloc|calloc|realloc)\s*\(|(^|[^A-Za-z_])new[ \t]+[A-Za-z_][A-Za-z0-9_:<>]*[ \t]*[([]' "$MODULE/$name" >/dev/null 2>&1; then
        ALLOC_HITS="$ALLOC_HITS $name"
    fi
done
if [ -n "$ALLOC_HITS" ]; then
    bad "dynamic allocation found in the sketch (static allocation only)"
else
    ok "no dynamic allocation anywhere in the sketch"
fi

if grep -n 'String' "$MODULE/up_link.h" >/dev/null 2>&1; then
    bad "up_link.h uses String (the request path must not allocate)"
else
    ok "the board request path allocates nothing"
fi

DELAY_HITS=""
for name in $C_SOURCES; do
    if grep -n 'HAL_Delay\|vTaskDelay' "$MODULE/$name" >/dev/null 2>&1; then
        DELAY_HITS="$DELAY_HITS $name"
    fi
done
if [ -n "$DELAY_HITS" ]; then
    bad "a blocking HAL/FreeRTOS delay appears in the sketch"
else
    ok "no blocking delay in the sketch"
fi

printf '\n----------------------------------------\n'
printf '%d checks passed, %d failed\n' "$PASSED" "$FAILED"
[ "$FAILED" -eq 0 ] || exit 1
