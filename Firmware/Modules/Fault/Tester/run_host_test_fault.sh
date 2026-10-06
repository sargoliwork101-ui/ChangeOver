#!/usr/bin/env bash
# [EN] Host unit test for the Fault module: compiles the PRODUCTION
#      fault.c together with a stub world (fake tick clock, fake BSP
#      GPIO, fake imbalance/dead-battery vetoes) and runs the state-machine
#      checklist. No board, no RTOS, no HAL.
# [FA] تست هاست ماژول Fault: کامپایل مستقیم کد محصول به‌همراه دنیای بدلی
#      (ساعت، GPIO، وتوها) و اجرای چک‌لیست ماشین حالت. بدون برد و RTOS.
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../../.. && pwd)"

gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O0 \
    -I "${ROOT}/Firmware/App/Inc" \
    -I "${ROOT}/Firmware/Bsp/Inc" \
    -I "${ROOT}/Firmware/Config/Inc" \
    -I "${ROOT}/Firmware/Rtos/Inc" \
    -I "${ROOT}/Firmware/Modules/Fault" \
    -I "${ROOT}/Firmware/Modules/Charger" \
    -I "${ROOT}/Firmware/Modules/Imbalance" \
    -I "${ROOT}/Firmware/Modules/Measurement" \
    -I "${ROOT}/CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2" \
    -I "${ROOT}/CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/include" \
    -I "${ROOT}/CubeIDE/Drivers/CMSIS/Include" \
    host_test_fault.c "${ROOT}/Firmware/Modules/Fault/fault.c" \
    -o host_test_fault

./host_test_fault
echo "FAULT HOST TEST: PASS"
