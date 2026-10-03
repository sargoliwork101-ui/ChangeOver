#!/usr/bin/env bash
# [EN] Build and run the ESP host tests. The sketch itself is compiled here
#      (esp_link_panel.ino, unmodified, ESP8266 branch) against the Arduino
#      stubs in this directory, so a broken ESP build is caught on the PC
#      instead of at flashing time. -Werror: the board's compiler is the only
#      other reader this code ever gets.
# [FA] ساخت و اجرای تست‌های هاست ESP. خود اسکچ اینجا کامپایل می‌شود
#      (esp_link_panel.ino، دست‌نخورده، شاخهٔ ESP8266) روی استاب‌های Arduino
#      همین پوشه، تا خرابی بیلد ESP روی PC پیدا شود نه سر فلش کردن.
set -euo pipefail

SCRIPT_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPOSITORY_ROOT="$(cd "${SCRIPT_DIRECTORY}/../.." && pwd)"
cd "${REPOSITORY_ROOT}"

BUILD_DIRECTORY="${SCRIPT_DIRECTORY}/build"
mkdir -p "${BUILD_DIRECTORY}"
BINARY="${BUILD_DIRECTORY}/host_test_esp_link"

# [EN] The sanitizers are the point, not an extra. A write one past the
#      parameter table and a shift of 32 on a 32-bit word are both invisible
#      to an assertion - the first corrupts a neighbour, the second is
#      undefined behaviour that happens to look right on one compiler. ASan
#      and UBSan see them. On the ESP there is nothing to see them at all.
# [FA] سنیتایزرها اصل کارند نه اضافه. نوشتن یک خانه بعد از جدول پارامترها و
#      شیفت ۳۲ روی کلمهٔ ۳۲ بیتی هیچ‌کدام با assert دیده نمی‌شوند - اولی خانهٔ
#      همسایه را خراب می‌کند و دومی رفتار تعریف‌نشده‌ای است که روی یک کامپایلر
#      اتفاقاً درست به نظر می‌رسد. روی خود ESP هیچ‌چیز آن‌ها را نمی‌بیند.
echo "[1/2] compiling the ESP sketch for the host / کامپایل اسکچ ESP برای هاست"
g++ \
    -std=gnu++17 \
    -Wall \
    -Wextra \
    -Werror \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer \
    -g \
    -Wno-unused-parameter \
    -I "${SCRIPT_DIRECTORY}" \
    -I "${SCRIPT_DIRECTORY}/stubinc" \
    -o "${BINARY}" \
    "${SCRIPT_DIRECTORY}/host_test_esp_link.cpp"

echo "[2/2] running / اجرا"
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
export ASAN_OPTIONS="detect_leaks=0"
"${BINARY}"
