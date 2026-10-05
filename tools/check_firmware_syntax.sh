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
    -I Firmware/Modules/McuPowerPath
    -I Firmware/Modules/Imbalance
    -I Firmware/Modules/CalLut
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

# [EN] The ESP sketch is a second program in this repository and it was never
#      built by any gate: until now the only compiler that ever read it was the
#      Arduino IDE, at flashing time. It is C++, not C, so it gets its own step.
# [FA] اسکچ ESP برنامهٔ دوم این مخزن است و هیچ دروازه‌ای آن را نمی‌ساخت: تا امروز
#      تنها کامپایلری که آن را می‌خواند Arduino IDE بود، سر فلش کردن. چون ++C است
#      نه C، گام جداگانهٔ خودش را دارد.
echo "--- ESP sketch (C++) / اسکچ ESP ---"
./esp_link_panel/Tester/run_esp_tests.sh

echo "HOST SYNTAX CHECK PASSED / بررسی syntax سمت Host موفق بود"
