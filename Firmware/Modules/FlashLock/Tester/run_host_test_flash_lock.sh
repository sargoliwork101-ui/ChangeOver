#!/usr/bin/env bash
# [EN] Host unit test for the FlashLock module: compiles the PRODUCTION
#      flash_lock.c together with a stub option-byte BSP and runs the
#      one-time-policy checklist. No board, no RTOS, no HAL.
# [FA] تست هاست ماژول FlashLock: کامپایل مستقیم کد محصول به‌همراه BSP بدلی
#      option byte و اجرای چک‌لیست سیاست یک‌بارمصرف. بدون برد و RTOS و HAL.
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../../.. && pwd)"

gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O0 \
    -I "${ROOT}/Firmware/Bsp/Inc" \
    -I "${ROOT}/Firmware/Config/Inc" \
    -I "${ROOT}/Firmware/Modules/FlashLock" \
    host_test_flash_lock.c "${ROOT}/Firmware/Modules/FlashLock/flash_lock.c" \
    -o host_test_flash_lock

./host_test_flash_lock
echo "FLASH_LOCK HOST TEST: PASS"
