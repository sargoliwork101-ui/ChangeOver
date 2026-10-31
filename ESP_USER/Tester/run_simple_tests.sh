#!/usr/bin/env bash
# run_simple_tests.sh - ساخت و اجرای تست‌های هاست پنل ساده
# فقط فارسی - مثل بقیهٔ این پوشه
#
# خود هدرهای برنامه (simple_config/simple_state/simple_link) اینجا کامپایل
# می‌شوند، نه کپی‌شان؛ پس هر تغییری در منطق صف یا فریم SET_PARAM روی PC
# دیده می‌شود. سنیتایزرها اصل کارند: یک نوشتن بیرون آرایه یا شیفت ۳۲ روی
# کلمهٔ ۳۲بیتی روی برد هیچ‌چیز آن‌ها را نمی‌بیند.
#
#   SIMPLE_TEST_BUILD  پوشهٔ ساخت (پیش‌فرض /tmp/changeover_simple_tests)
set -euo pipefail

DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIRECTORY="${SIMPLE_TEST_BUILD:-/tmp/changeover_simple_tests}"
BINARY="${BUILD_DIRECTORY}/host_test_latch"
mkdir -p "${BUILD_DIRECTORY}"

echo "[1/4] پارس اسکریپت خود پنل (نگهبان صفحهٔ سفید)"
# همان کاری که ممیزی برای پنل فنی می‌کند: قبل از هر چیز، جاوااسکریپت درون
# simple_panel.h به یک پارسر JS سپرده می‌شود. صفحه‌ای که اسکریپتش پارس نشود،
# روی برد سفید می‌ماند و هیچ تست متنی این را نمی‌بیند.
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

echo "[2/4] کامپایل تست هاست پنل ساده"
# -Wno-unused-function/-variable: هدرهای برنامه پر از تابع و جدول‌های static
# هستند که این تست آن‌ها را صدا نمی‌زند؛ خود برنامه روی برد همه را استفاده
# می‌کند و همان‌جا با -Wall -Wextra تمیز کامپایل می‌شود.
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

echo "[3/4] اجرا"
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
export ASAN_OPTIONS="detect_leaks=0"
"${BINARY}"

echo "[4/4] تست رفتاری آلارم اضافه‌ولتاژ (DOM)"
# [EN] Optional: needs node + jsdom + the generated preview. SKIP when absent.
# [FA] اختیاری: به node و jsdom و پیش‌نمایش ساخته‌شده نیاز دارد؛ نبودن یعنی SKIP.
if command -v node >/dev/null 2>&1 && [ -f "${DIRECTORY}/../simple_preview.html" ]; then
    if node -e "require('jsdom')" >/dev/null 2>&1; then
        node "${DIRECTORY}/host_test_ov.js" || exit 1
    else
        echo "  SKIP: jsdom نصب نیست"
    fi
else
    echo "  SKIP: node یا simple_preview.html موجود نیست"
fi
