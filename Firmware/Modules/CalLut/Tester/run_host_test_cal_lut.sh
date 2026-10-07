#!/usr/bin/env bash
# [EN] Host unit test for the CalLut module: compiles the PRODUCTION
#      cal_lut.c together with a stub world (fake tick clock, fake BSP
#      GPIO, fake imbalance/dead-battery vetoes) and runs the state-machine
#      checklist. No board, no RTOS, no HAL.
# [FA] تست هاست ماژول CalLut: کامپایل مستقیم کد محصول به‌همراه دنیای بدلی
#      (ساعت، GPIO، وتوها) و اجرای چک‌لیست ماشین حالت. بدون برد و RTOS.
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../../.. && pwd)"

gcc -std=gnu11 -DSTM32F103xB -DUSE_HAL_DRIVER -DESPLINK_HOST_TEST -Wall -Wextra -Wpedantic -Werror -Wno-unused-function -Wno-int-to-pointer-cast -O0 -ffunction-sections -fdata-sections \
    -I "${ROOT}/Firmware/App/Inc" \
    -I "${ROOT}/Firmware/Bsp/Inc" \
    -I "${ROOT}/Firmware/Config/Inc" \
    -I "${ROOT}/Firmware/Rtos/Inc" \
    -I "${ROOT}/Firmware/Modules/CalLut" \
    -I . \
    -I "${ROOT}/Firmware/Modules/Charger" \
    -I "${ROOT}/Firmware/Modules/Imbalance" \
    -I "${ROOT}/Firmware/Modules/Measurement" \
    -I "${ROOT}/Firmware/Modules/EspLink" \
    -I "${ROOT}/Firmware/Modules/Fault" \
    -I "${ROOT}/Firmware/Modules/Ui" \
    -I "${ROOT}/Firmware/Modules/Jitter" \
    -I "${ROOT}/Firmware/Modules/Changeover" \
    -I "${ROOT}/Firmware/Modules/McuPowerPath" \
    -I "${ROOT}/CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2" \
    -I "${ROOT}/CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/include" \
    -I "${ROOT}/CubeIDE/Drivers/CMSIS/Include" \
    host_test_cal_lut.c "${ROOT}/Firmware/Modules/CalLut/cal_lut.c" \
    "${ROOT}/Firmware/Modules/EspLink/esp_link.c" \
    -Wl,--gc-sections -o host_test_cal_lut

./host_test_cal_lut
echo "CAL_LUT HOST TEST: PASS"
