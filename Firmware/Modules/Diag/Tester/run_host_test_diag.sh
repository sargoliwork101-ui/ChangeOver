#!/usr/bin/env bash
# [EN] Host unit test for the Diag module: compiles the PRODUCTION diag.c
#      together with a fake kernel clock, a fake charger and two real flash
#      pages mapped at their hardware addresses. No board, no RTOS, no HAL.
# [FA] تست هاست ماژول Diag: کامپایل مستقیم کد محصول به‌همراه ساعت بدلی کرنل،
#      شارژر بدلی و دو صفحهٔ فلش واقعی که روی آدرس سخت‌افزاری خودشان نگاشت
#      شده‌اند. بدون برد و بدون RTOS و HAL.
# [EN] -Wno-int-to-pointer-cast: the module reads its record straight from the
#      flash address; on a 64-bit host that cast is the point of the test, not
#      a bug.
# [FA] ‎-Wno-int-to-pointer-cast‎: ماژول رکوردش را مستقیم از آدرس فلش می‌خواند؛
#      روی هاست ۶۴بیتی همین تبدیل هدف تست است، نه ایراد.
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../../.. && pwd)"

gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O0 -g \
    -D_GNU_SOURCE \
    -Wno-int-to-pointer-cast \
    -I "${ROOT}/Firmware/App/Inc" \
    -I "${ROOT}/Firmware/Bsp/Inc" \
    -I "${ROOT}/Firmware/Config/Inc" \
    -I "${ROOT}/Firmware/Rtos/Inc" \
    -I "${ROOT}/Firmware/Modules/Diag" \
    -I "${ROOT}/Firmware/Modules/Charger" \
    -I "${ROOT}/Firmware/Modules/Measurement" \
    -I "${ROOT}/CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2" \
    -I "${ROOT}/CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/include" \
    -I "${ROOT}/CubeIDE/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3" \
    -I "${ROOT}/CubeIDE/Drivers/CMSIS/Include" \
    -I "${ROOT}/CubeIDE/Drivers/CMSIS/Device/ST/STM32F1xx/Include" \
    -I "${ROOT}/CubeIDE/Core/Inc" \
    host_test_diag.c "${ROOT}/Firmware/Modules/Diag/diag.c" \
    -o host_test_diag

./host_test_diag
echo "DIAG HOST TEST: PASS"
