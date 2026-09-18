#!/usr/bin/env bash
# [EN] Host-side syntax gate for CubeIDE Core and Firmware sources.
# [FA] دروازهٔ بررسی syntax سمت Host برای سورس‌های Core و Firmware.
set -euo pipefail

SCRIPT_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPOSITORY_ROOT="$(cd "${SCRIPT_DIRECTORY}/.." && pwd)"
cd "${REPOSITORY_ROOT}"

INCLUDE_FLAGS=(
    -I CubeIDE/Core/Inc
    -I CubeIDE/Drivers/CMSIS/Include
    -I CubeIDE/Drivers/CMSIS/Device/ST/STM32F1xx/Include
    -I CubeIDE/Drivers/STM32F1xx_HAL_Driver/Inc
    -I CubeIDE/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
    -I CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/include
    -I CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    -I CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3
    -I Firmware/App/Inc
    -I Firmware/Bsp/Inc
    -I Firmware/Config/Inc
    -I Firmware/Rtos/Inc
    -I Firmware/Modules/Ui
    -I Firmware/Modules/Measurement
    -I Firmware/Modules/Protection
    -I Firmware/Modules/Changeover
    -I Firmware/Modules/Charger
    -I Firmware/Modules/Jitter
    -I Firmware/Modules/Fault
    -I Firmware/Modules/EspLink
)

SOURCE_FILES=(
    $(find CubeIDE/Core/Src Firmware -type f -name '*.c' | sort)
)

for SOURCE_FILE in "${SOURCE_FILES[@]}"; do
    gcc \
        -fsyntax-only \
        -std=c11 \
        -Wall \
        -Wextra \
        -Werror \
        -Wno-int-to-pointer-cast \
        -DSTM32F103xB \
        -DUSE_HAL_DRIVER \
        "${INCLUDE_FLAGS[@]}" \
        "${SOURCE_FILE}"
done

echo "HOST SYNTAX CHECK PASSED / بررسی syntax سمت Host موفق بود"

# [EN] Numerical host test of the board calibration port: compile the harness
#      together with bsp_measurement.c and run it; a changed divider, gain or
#      shunt constant fails here before it reaches the board.
# [FA] تست عددی Host برای پورت کالیبراسیون برد: هارنس همراه bsp_measurement.c
#      کامپایل و اجرا می‌شود؛ تغییر تقسیم، گین یا شانت پیش از رسیدن به برد
#      اینجا شکست می‌خورد.
MEASUREMENT_TEST_BINARY="$(mktemp -u /tmp/changeover_meas_test.XXXXXX)"
gcc \
    -std=c11 \
    -Wall \
    -Wextra \
    -I Firmware/Bsp/Inc \
    tools/host_test_bsp_measurement.c \
    Firmware/Bsp/Src/bsp_measurement.c \
    -o "${MEASUREMENT_TEST_BINARY}"
"${MEASUREMENT_TEST_BINARY}"
rm -f "${MEASUREMENT_TEST_BINARY}"
