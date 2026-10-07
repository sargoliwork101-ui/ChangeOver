#!/usr/bin/env bash
#
# run_sim.sh - build and run the user panel simulator.
#
# [EN] WHY THIS EXISTS
#      The panel is normally compiled by the Arduino IDE and first seen on the
#      cabinet's screen. This script builds the SAME sketch - the same up_*.h,
#      the same page, the same Excel writer - against the host stubs plus a model
#      of the machine, and serves it on a port so a browser can look at it. Two
#      ways to run:
#        bash user_panel/simulator/run_sim.sh             # serve on port 8090
#        bash user_panel/simulator/run_sim.sh --selftest  # checks, no sockets
#      A green run is not a bench test: it cannot see a wrong pin, a weak
#      antenna or a flash chip that is slower than the model assumes.
#
#      [FA] چرا این وجود دارد
#      پنل معمولاً با Arduino IDE کامپایل می‌شود و اولین بار روی صفحهٔ تابلو دیده
#      می‌شود. این اسکریپت همان اسکچ را - همان up_*.hها، همان صفحه، همان
#      نویسندهٔ اکسل - در کنار استاب‌های میزبان و مدلی از ماشین می‌سازد و روی یک
#      پورت سرو می‌کند تا مرورگر ببیندش. دو حالت اجرا:
#        bash user_panel/simulator/run_sim.sh             # سرو روی پورت ۸۰۹۰
#        bash user_panel/simulator/run_sim.sh --selftest   # چک‌ها، بدون سوکت
#      اجرای سبز، تست میز نیست: پایهٔ اشتباه، آنتن ضعیف یا فلشی که از فرض مدل
#      کندتر است را نمی‌بیند.
#
# Usage / طرز استفاده:
#     bash user_panel/simulator/run_sim.sh [--port 8090] [--selftest]
#     (from anywhere; the paths are resolved from this file)

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
MODULE="$(cd "$HERE/.." && pwd)"
BUILD="$HERE/build"
BINARY="$BUILD/up_sim"
CXX="${CXX:-g++}"

if ! command -v "$CXX" >/dev/null 2>&1; then
    printf 'no C++ compiler found (set CXX)\n' >&2
    exit 2
fi

mkdir -p "$BUILD"

# [EN] Always rebuild: a simulator that quietly runs yesterday's binary is worse
#      than none, because it makes a fixed bug look unfixed and vice versa.
# [FA] همیشه از نو بساز: شبیه‌سازی که بی‌صدا باینری دیروز را اجرا کند از نبودنش
#      بدتر است، چون اشکال رفع‌شده را رفع‌نشده نشان می‌دهد و برعکس.
if ! "$CXX" -std=gnu++17 -Wall -Wextra -Werror -Wno-unused-parameter \
        -I "$MODULE/tools" -I "$MODULE/tools/stubinc" \
        -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
        "$HERE/sim_main.cpp" -o "$BINARY"; then
    printf 'build failed / ساخت نشد\n' >&2
    exit 1
fi

export ASAN_OPTIONS="detect_leaks=0"
export UBSAN_OPTIONS="halt_on_error=1"

exec "$BINARY" --page "$HERE/sim.html" "$@"
