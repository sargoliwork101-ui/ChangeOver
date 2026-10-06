#!/usr/bin/env bash
# [EN] Host unit test for the Measurement module: compiles the PRODUCTION
#      measurement.c together with a stub world (fake tick clock, fake BSP
#      GPIO, fake imbalance/dead-battery vetoes) and runs the state-machine
#      checklist. No board, no RTOS, no HAL.
# [FA] تست هاست ماژول Measurement: کامپایل مستقیم کد محصول به‌همراه دنیای بدلی
#      (ساعت، GPIO، وتوها) و اجرای چک‌لیست ماشین حالت. بدون برد و RTOS.
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../../.. && pwd)"

gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O0 \
    -I "${ROOT}/Firmware/App/Inc" \
    -I "${ROOT}/Firmware/Bsp/Inc" \
    -I "${ROOT}/Firmware/Config/Inc" \
    -I "${ROOT}/Firmware/Rtos/Inc" \
    -I "${ROOT}/Firmware/Modules/Measurement" \
    -I "${ROOT}/Firmware/Modules/CalLut" \
    -I "${ROOT}/Firmware/Modules/Charger" \
    -I "${ROOT}/Firmware/Modules/Imbalance" \
    -I "${ROOT}/Firmware/Modules/Measurement" \
    -I "${ROOT}/Firmware/Modules/CalLut" \
    -I "${ROOT}/CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2" \
    -I "${ROOT}/CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/include" \
    -I "${ROOT}/CubeIDE/Drivers/CMSIS/Include" \
    host_test_measurement.c "${ROOT}/Firmware/Modules/Measurement/measurement.c" \
    -o host_test_measurement

./host_test_measurement
echo "MEASUREMENT HOST TEST: PASS"
