#!/usr/bin/env bash
# [EN] Host unit test for the Imbalance module (scenario 6): compiles the
#      production imbalance.c with a synthetic-input harness (no board,
#      no RTOS) and runs the full gate checklist.
# [FA] تست هاست ماژول عدم‌توازن: کامپایل مستقیم کد محصول با هارنس زمان مصنوعی.
set -euo pipefail
cd "$(dirname "$0")"

gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -O0 \
    host_test_imbalance.c ../imbalance.c -o host_test_imbalance

./host_test_imbalance
echo "IMBALANCE HOST TEST: PASS"
