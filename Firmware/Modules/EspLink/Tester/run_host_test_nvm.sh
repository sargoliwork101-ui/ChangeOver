#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

CC="${CC:-gcc}"
COMMON=(-std=c11 -Wall -Wextra -Werror -Wno-int-to-pointer-cast -ffunction-sections -fdata-sections)
INCLUDES=(
  -I"$ROOT/Firmware/Modules/EspLink"
  -I"$ROOT/Firmware/Modules/Charger"
  -I"$ROOT/Firmware/Modules/Measurement"
  -I"$ROOT/Firmware/Modules/Fault"
  -I"$ROOT/Firmware/Modules/Imbalance"
  -I"$ROOT/Firmware/Modules/Jitter"
  -I"$ROOT/Firmware/Modules/Changeover"
  -I"$ROOT/Firmware/Modules/McuPowerPath"
  -I"$ROOT/Firmware/Modules/Ui"
  -I"$ROOT/Firmware/Config/Inc"
  -I"$ROOT/Firmware/Bsp/Inc"
  -I"$ROOT/Firmware/Rtos/Inc"
  -I"$ROOT/CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2"
)

"$CC" "${COMMON[@]}" "${INCLUDES[@]}" \
  "$ROOT/Firmware/Modules/EspLink/Tester/host_test_nvm.c" \
  "$ROOT/Firmware/Modules/EspLink/esp_link_nvm.c" \
  -Wl,--gc-sections -o "$BUILD_DIR/host_test_nvm"

"$BUILD_DIR/host_test_nvm"
