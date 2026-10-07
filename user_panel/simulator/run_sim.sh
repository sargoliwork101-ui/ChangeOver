#!/usr/bin/env bash
#
# run_sim.sh - build the simulator and run it, or run its selftest.
#
# [EN] WHY A SCRIPT
#      The simulator compiles the panel's own sketch with the host stubs, which
#      means three include paths and one translation unit. Nobody should have to
#      remember them, and a stale binary pretending to be the current panel is
#      worse than no binary at all - so this script always rebuilds.
#
#      [FA] چرا اسکریپت
#      شبیه‌ساز، خودِ اسکچ پنل را با استاب‌های میزبان کامپایل می‌کند: سه مسیر
#      include و یک واحد ترجمه. هیچ‌کس نباید مجبور باشد آن‌ها را به خاطر بسپارد،
#      و باینریِ کهنه‌ای که خودش را پنل امروز جا بزند از نبودنش بدتر است - پس این
#      اسکریپت همیشه از نو می‌سازد.
#
# Usage / طرز استفاده:
#     bash user_panel/simulator/run_sim.sh                 (panel on :8090)
#     bash user_panel/simulator/run_sim.sh --selftest      (no sockets)
#     bash user_panel/simulator/run_sim.sh --port 9000 --speed 30
#
# Exit codes / کدهای خروج: 0 = ok, 1 = the selftest failed, 2 = build failed.

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
MODULE="$(cd "$HERE/.." && pwd)"
CXX="${CXX:-g++}"
OUT="$HERE/build/up_sim"

mkdir -p "$HERE/build"

if ! "$CXX" -std=gnu++17 -Wall -Wextra -Werror -Wno-unused-parameter \
        -I "$MODULE/tools" -I "$MODULE/tools/stubinc" \
        -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
        "$HERE/sim_main.cpp" -o "$OUT" 2>"$HERE/build/build.log"; then
    echo "simulator build failed / ساخت شبیه‌ساز شکست خورد:"
    sed 's/^/  /' "$HERE/build/build.log" | head -40
    exit 2
fi

# [EN] The leak checker is off: the panel is one static translation unit and the
#      process lives until Ctrl-C, so a "leak" here is the program still running.
# [FA] بررسی نشت خاموش است: پنل یک واحد ترجمهٔ کاملاً ایستا است و فرآیند تا
#      Ctrl-C زنده می‌ماند، پس «نشت» یعنی برنامه هنوز در حال اجراست.
export ASAN_OPTIONS="detect_leaks=0"
export UBSAN_OPTIONS="halt_on_error=1"

if [ "${1:-}" = "--selftest" ]; then
    exec "$OUT" --selftest --page "$HERE/sim.html"
fi

exec "$OUT" --page "$HERE/sim.html" "$@"
