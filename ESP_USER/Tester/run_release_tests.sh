#!/usr/bin/env bash
# Mandatory ESP regression gate: no optional skips and no persistent development server.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export NODE_PATH="${DIR}/node_modules${NODE_PATH:+:${NODE_PATH}}"
fail() { echo "FAIL: $*" >&2; exit 2; }
for tool in gcc g++ node python3 sha256sum; do command -v "$tool" >/dev/null || fail "Missing required tool: $tool"; done
[[ -n "${CHROMIUM_PATH:-}" && -x "$CHROMIUM_PATH" ]] || fail "Set CHROMIUM_PATH to a compatible Chromium executable"
[[ -n "${LITTLEFS_SOURCE:-}" && -d "$LITTLEFS_SOURCE" ]] || fail "Set LITTLEFS_SOURCE to upstream littlefs 6a53d76e90af33f0656333c1db09bd337fa75d23"
(cd "$DIR" && node -e "for(const [p,v] of Object.entries(require('./package.json').devDependencies)){if(require(p+'/package.json').version!==v)throw Error(p+': run npm ci in ESP_USER/Tester');}") || fail "Test dependencies missing or mismatched; run npm ci in ESP_USER/Tester"
# Reject silently testing a different flash implementation.
(cd "$LITTLEFS_SOURCE" && sha256sum --check --status <<'HASHES'
8c28c80018293820d5400759f4918359b09d869c1a1240959b0647cc844783b5  lfs.c
bece5357a4721c826c3d25ffab83962986a92f929fcf2d06bc9d66b5be2a3f3a  lfs.h
8e1376a90e923a2897388a54ebaa756ad3ea07a0e2a87e73c0045515c0a82685  lfs_util.c
825e70edbe8839dc814fb41ad950c191035e6fa5b697a90ee7fa2b287e360c33  lfs_util.h
HASHES
) || fail "LittleFS sources differ from the pinned version"
if [[ "${1:-}" == "--preflight" ]]; then echo "PREFLIGHT PASS (tests not run)"; exit 0; fi
[[ $# -eq 0 ]] || fail "Usage: run_release_tests.sh [--preflight]"
WORK="$(mktemp -d /tmp/changeover-release-tests-XXXXXX)"
trap 'rm -rf "$WORK"' EXIT
export SIMPLE_TEST_BUILD="$WORK/host"
python3 "$DIR/host_test_release_preflight.py"
python3 "$DIR/../make_simple_preview.py"
bash "$DIR/run_simple_tests.sh"
# The upstream flash profiles include concurrent logs/charts/history/pack settings.
SIMPLE_TEST_BUILD="$WORK/flash" bash "$DIR/run_storage_littlefs.sh"
g++ -std=gnu++17 -O1 -g -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable \
    -I "$DIR/../Simulator/stubinc" -I "$DIR/../Simulator" \
    -o "$WORK/sim_panel" "$DIR/../Simulator/main.cpp"
SIMPLE_SIM_BINARY="$WORK/sim_panel" node "$DIR/host_test_pack_browser.js"
echo "ALL MANDATORY ESP RELEASE TESTS PASSED (host, DOM, real LittleFS, power cuts, live Chromium)"
