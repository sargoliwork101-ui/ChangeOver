#!/usr/bin/env bash
#
#
set -euo pipefail
DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPOSITORY_ROOT="$(cd "${DIRECTORY}/../.." && pwd)"
BSP_SOURCE="${REPOSITORY_ROOT}/Firmware/Bsp/Src/bsp_measurement.c"
BUILD_DIRECTORY="${SIMPLE_SIM_BUILD:-/tmp/changeover_simple_sim}"
mkdir -p "${BUILD_DIRECTORY}"
DEFINES="$(python3 - "${BSP_SOURCE}" <<'PY'
import re
import sys
from pathlib import Path

source = Path(sys.argv[1])
if not source.exists():
    print("# bsp_measurement.c پیدا نشد؛ پیش‌فرض‌های sim_board.h استفاده می‌شود")
    raise SystemExit(0)

text = source.read_text(encoding="utf-8")


def value(name):
    found = re.search(r"#define\s+" + name + r"\s+(\d+)u?(?![0-9A-Za-z_])", text)
    return int(found.group(1)) if found else None


vref = value("BSP_MEASUREMENT_VREF_MV")
full = value("BSP_MEASUREMENT_ADC_FULL_SCALE")
bottom = value("BSP_MEASUREMENT_SENSE_SHUNT_OHMS")
top24 = value("BSP_MEASUREMENT_SENSE_TOP_24V_OHMS")
top12 = value("BSP_MEASUREMENT_DIV12_TOP_OHMS")
if None in (vref, full, bottom, top24, top12):
    print("# تعریف‌های تقسیم‌کنندهٔ BSP ناقص است؛ پیش‌فرض‌های sim_board.h استفاده می‌شود")
    raise SystemExit(0)

lsb = vref / full
print("-DSIM_MV_PER_COUNT_24=%.6f" % (lsb * (top24 + bottom) / bottom),
      "-DSIM_MV_PER_COUNT_12=%.6f" % (lsb * (top12 + bottom) / bottom),
      "-DSIM_MV_PER_COUNT_ADC=%.6f" % lsb,
      "-DSIM_VREFINT_COUNTS=%d" % round(1200 / vref * full),
      "-DSIM_VDDA_MV=%d" % vref)
PY
)"
echo "[1/2] کامپایل خود برنامهٔ ESP برای PC"
# shellcheck disable=SC2086
g++ \
    -std=gnu++17 \
    -Wall \
    -Wextra \
    -O1 \
    -g \
    -I "${DIRECTORY}/stubinc" \
    -I "${DIRECTORY}" \
    ${DEFINES} \
    -o "${BUILD_DIRECTORY}/sim_panel" \
    "${DIRECTORY}/main.cpp"
echo "[2/2] اجرا"
export SIMPLE_SIM_PORT="${SIMPLE_SIM_PORT:-3000}"
exec "${BUILD_DIRECTORY}/sim_panel"
