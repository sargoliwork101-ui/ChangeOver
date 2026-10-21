#!/usr/bin/env bash
# [EN] Integration host test for the battery-swap reset: production Fault and
#      Imbalance code, plus a structural check of the TaskControl wire-up.
#      No board, RTOS, or HAL is required.
# [FA] تست هاست یکپارچهٔ ریست تعویض باتری: کد واقعی Fault و Imbalance به‌همراه
#      بررسی اتصال TaskControl؛ بدون برد، RTOS یا HAL.
set -euo pipefail

SCRIPT_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPOSITORY_ROOT="$(cd "${SCRIPT_DIRECTORY}/../../../.." && pwd)"
cd "${SCRIPT_DIRECTORY}"
HOST_TEST_BINARY="$(mktemp "${TMPDIR:-/tmp}/host_test_battery_swap.XXXXXX")"
trap 'rm -f "$HOST_TEST_BINARY"' EXIT

gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O0 \
    -I "${REPOSITORY_ROOT}/Firmware/App/Inc" \
    -I "${REPOSITORY_ROOT}/Firmware/Bsp/Inc" \
    -I "${REPOSITORY_ROOT}/Firmware/Config/Inc" \
    -I "${REPOSITORY_ROOT}/Firmware/Rtos/Inc" \
    -I "${REPOSITORY_ROOT}/Firmware/Modules/Fault" \
    -I "${REPOSITORY_ROOT}/Firmware/Modules/Charger" \
    -I "${REPOSITORY_ROOT}/Firmware/Modules/Imbalance" \
    -I "${REPOSITORY_ROOT}/Firmware/Modules/Measurement" \
    -I "${REPOSITORY_ROOT}/CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2" \
    -I "${REPOSITORY_ROOT}/CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/include" \
    -I "${REPOSITORY_ROOT}/CubeIDE/Drivers/CMSIS/Include" \
    host_test_battery_swap.c \
    "${REPOSITORY_ROOT}/Firmware/Modules/Fault/fault.c" \
    "${REPOSITORY_ROOT}/Firmware/Modules/Imbalance/imbalance.c" \
    -o "${HOST_TEST_BINARY}"

"${HOST_TEST_BINARY}"

python3 - "${REPOSITORY_ROOT}/Firmware/Rtos/Src/task_control.c" <<'PY'
import pathlib
import re
import sys

source = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
control_start = source.find("void func__TaskControl")
if control_start < 0:
    raise SystemExit("TaskControl function not found")
control_body = source[control_start:]

mapping = re.compile(
    r"#if\s+MODULE_FAULT\s+"
    r"imbalance_inputs_t__imbalanceInputs\.bool__batAbsent\s*=\s*"
    r"func__Fault_IsBatteryAbsent\s*\(\s*\)\s*;\s*"
    r"#else\s*"
    r"imbalance_inputs_t__imbalanceInputs\.bool__batAbsent\s*=\s*false\s*;\s*"
    r"#endif",
    re.S,
)
if not mapping.search(control_body):
    raise SystemExit("TaskControl must wire Fault's live signal, with a false MODULE_FAULT fallback")

fault_evaluate_index = control_body.find("func__Fault_Evaluate")
absence_getter_index = control_body.find("func__Fault_IsBatteryAbsent")
if (fault_evaluate_index < 0) or (absence_getter_index <= fault_evaluate_index):
    raise SystemExit("Fault_Evaluate must refresh the live signal before TaskControl reads it")

old_latch_mapping = re.compile(
    r"bool__batAbsent\s*=\s*[^;]*FAULT_CHARGER_BAT_LOST",
    re.S,
)
if old_latch_mapping.search(control_body):
    raise SystemExit("TaskControl must not derive reset timing from the shared latched fault bit")

print("TaskControl battery-absence mapping/order/fallback: PASS")
PY

echo "BATTERY SWAP INTEGRATION TEST: PASS"
