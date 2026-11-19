#!/usr/bin/env bash
#
#
set -euo pipefail
DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export NODE_PATH="${DIRECTORY}/node_modules${NODE_PATH:+:${NODE_PATH}}"
BUILD_DIRECTORY="${SIMPLE_TEST_BUILD:-/tmp/changeover_simple_tests}"
BINARY="${BUILD_DIRECTORY}/host_test_latch"
mkdir -p "${BUILD_DIRECTORY}"
echo "[1/8] پارس اسکریپت خود پنل (نگهبان صفحهٔ سفید)"
if command -v node >/dev/null 2>&1; then
    python3 - "${DIRECTORY}/../simple_panel.h" <<'PYJS'
import re
import pathlib
import sys

src = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
blocks = re.findall(r"<script>([\s\S]*?)</script>", src)
if not blocks:
    raise SystemExit("هیچ بلوک <script> در simple_panel.h پیدا نشد")
pathlib.Path("/tmp/simple_panel_inline_check.js").write_text(
    "\n;\n".join(blocks), encoding="utf-8")
print("      %d بلوک اسکریپت استخراج شد" % len(blocks))
PYJS
    node --check /tmp/simple_panel_inline_check.js || {
        echo "  ناموفق: جاوااسکریپت پنل پارس نمی‌شود (صفحهٔ سفید روی برد)"; exit 1; }
    echo "  موفق: جاوااسکریپت پنل پارس شد"
else
    echo "  هشدار: node پیدا نشد - بررسی پارس JS رد شد"
fi
echo "[2/8] کامپایل تست هاست پنل ساده"
g++ \
    -std=gnu++17 \
    -Wall \
    -Wextra \
    -Werror \
    -Wno-unused-function \
    -Wno-unused-variable \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer \
    -g \
    -I "${DIRECTORY}/../Simulator/stubinc" \
    -I "${DIRECTORY}/../Simulator" \
    -o "${BINARY}" \
    "${DIRECTORY}/host_test_latch.cpp"
echo "      کامپایل آزمون ساعت با setup/loop واقعی"
g++ -std=gnu++17 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "${DIRECTORY}/../Simulator/stubinc" -I "${DIRECTORY}/../Simulator" \
    -o "${BUILD_DIRECTORY}/host_test_clock_loop" "${DIRECTORY}/host_test_clock_loop.cpp"
echo "      کامپایل آزمون ثبت واقعی شارژ در ESP"
g++ -std=gnu++17 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "${DIRECTORY}/../Simulator/stubinc" -I "${DIRECTORY}/../Simulator" \
    -o "${BUILD_DIRECTORY}/host_test_charge_sessions" "${DIRECTORY}/host_test_charge_sessions.cpp"
g++ -std=gnu++17 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "${DIRECTORY}/../Simulator/stubinc" -I "${DIRECTORY}/../Simulator" \
    -o "${BUILD_DIRECTORY}/host_test_pack_review" "${DIRECTORY}/host_test_pack_review.cpp"
g++ -std=gnu++17 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "${DIRECTORY}/../Simulator/stubinc" -I "${DIRECTORY}/../Simulator" \
    -o "${BUILD_DIRECTORY}/host_test_pack_edges" "${DIRECTORY}/host_test_pack_edges.cpp"
echo "[3/8] اجرا"
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
export ASAN_OPTIONS="detect_leaks=0"
"${BINARY}"
"${BUILD_DIRECTORY}/host_test_clock_loop"
"${BUILD_DIRECTORY}/host_test_charge_sessions"
"${BUILD_DIRECTORY}/host_test_pack_review"
"${BUILD_DIRECTORY}/host_test_pack_edges"
echo "[4/8] تست رفتاری آلارم اضافه‌ولتاژ (DOM)"
# [EN] Optional: needs node + jsdom + the generated preview. SKIP when absent.
if command -v node >/dev/null 2>&1 && [ -f "${DIRECTORY}/../simple_preview.html" ]; then
    if node -e "require('jsdom')" >/dev/null 2>&1; then
        node "${DIRECTORY}/host_test_ov.js" || exit 1
    else
        echo "  SKIP: jsdom نصب نیست"
    fi
else
    echo "  SKIP: node یا simple_preview.html موجود نیست"
fi
echo "[5/8] تست ساعت زندهٔ کارت آمار (DOM)"
if command -v node >/dev/null 2>&1 && [ -f "${DIRECTORY}/../simple_preview.html" ]; then
    if node -e "require('jsdom')" >/dev/null 2>&1; then
        node "${DIRECTORY}/host_test_diag_clock.js" || exit 1
    else
        echo "  SKIP: jsdom نصب نیست"
    fi
else
    echo "  SKIP: node یا simple_preview.html موجود نیست"
fi
echo "[6/8] تست بازخورد و قفل دکمه‌ها (DOM)"
if command -v node >/dev/null 2>&1 && node -e "require('jsdom')" >/dev/null 2>&1; then
    node "${DIRECTORY}/host_test_panel_actions.js"
    node "${DIRECTORY}/host_test_chart_refresh.js"
    node "${DIRECTORY}/host_test_settings.js"
    node "${DIRECTORY}/host_test_battery_percent.js"
else
    echo "  SKIP: node یا jsdom نصب نیست"
fi
echo "[7/8] تست ورود واقعی پیش‌نمایش و فرمت حافظه (DOM)"
if command -v node >/dev/null 2>&1 && [ -f "${DIRECTORY}/../simple_preview.html" ] && node -e "require('jsdom')" >/dev/null 2>&1; then
    node "${DIRECTORY}/host_test_login_format.js"
    node "${DIRECTORY}/host_test_charge_calendar.js"
    node "${DIRECTORY}/host_test_about.js"
    node "${DIRECTORY}/host_test_battery_wear_preview.js"
    node "${DIRECTORY}/host_test_pack_review.js"
    node "${DIRECTORY}/host_test_pack_races.js"
else
    echo "  SKIP: node یا jsdom یا پیش‌نمایش موجود نیست"
fi
echo "[8/8] تست سیاست لاگ و ریست شبیه‌ساز پایتون"
python3 "${DIRECTORY}/host_test_lab_log_policy.py"
