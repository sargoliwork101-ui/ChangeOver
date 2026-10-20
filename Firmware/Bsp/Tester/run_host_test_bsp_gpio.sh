#!/usr/bin/env bash
# [EN] Host test runner for the real board GPIO port. Compiles the PRODUCTION
#      Firmware/Bsp/Src/bsp_gpio.c against the real CMSIS/HAL headers with the
#      Cortex intrinsics neutralised (host_stub_cortex.h) and a two-function
#      GPIO fake, then runs the polarity checklist. No board, no RTOS.
#
#      WHY this exists: bsp_gpio.c was the one file that never ran anywhere but
#      the board, so a wrong input active level could only be caught in the
#      field. Audit 2026-10-20 added PIN_JITTER1/2_ACTIVE_HIGH = 0 (LM393
#      open-collector) and this test pins that fact down: flipping either macro
#      back to 1 makes the run red at the JITTER checks.
#
# [FA] اجراکنندهٔ تست هاست پورت واقعی GPIO برد. فایل محصول bsp_gpio.c را با
#      هدرهای واقعی CMSIS/HAL و intrinsic‌های بی‌اثرشدهٔ کورتکس و یک GPIO
#      بدلی دو-تابعی کامپایل می‌کند و چک‌لیست قطبیت را اجرا می‌کند. بدون برد
#      و بدون RTOS.
#
#      چرا این تست هست: bsp_gpio.c تنها فایلی بود که هیچ‌جا جز برد اجرا
#      نمی‌شد، پس سطح فعالِ غلط فقط سر میز پیدا می‌شد. ممیزی ۲۰۲۶-۱۰-۲۰
#      مقدار ‎PIN_JITTER1/2_ACTIVE_HIGH = 0‎ (خروجی open-collector LM393) را
#      اضافه کرد و این تست همان را میخ‌کوب می‌کند: برگرداندن هر یک از این دو
#      ماکرو به ۱ اجرا را دقیقاً روی چک‌های JITTER قرمز می‌کند.
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../.. && pwd)"

gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O0 \
    -DSTM32F103xB -DUSE_HAL_DRIVER \
    -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
    -I "${ROOT}/CubeIDE/Core/Inc" \
    -I "${ROOT}/CubeIDE/Drivers/CMSIS/Include" \
    -I "${ROOT}/CubeIDE/Drivers/CMSIS/Device/ST/STM32F1xx/Include" \
    -I "${ROOT}/CubeIDE/Drivers/STM32F1xx_HAL_Driver/Inc" \
    -I "${ROOT}/CubeIDE/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy" \
    -I "${ROOT}/Firmware/Bsp/Inc" \
    -I "${ROOT}/Firmware/Config/Inc" \
    -include "${ROOT}/Firmware/Bsp/Tester/host_stub_cortex.h" \
    host_test_bsp_gpio.c "${ROOT}/Firmware/Bsp/Src/bsp_gpio.c" \
    -o host_test_bsp_gpio

./host_test_bsp_gpio
echo "BSP GPIO HOST TEST: PASS"
