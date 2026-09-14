#!/usr/bin/env bash
# @file    check_ai_rules.sh
# @brief   [EN] Execute AI_CONTEXT.md rules: header check, README template, folder separation, MODULE flags.
#          [FA] اجرای قوانین AI_CONTEXT: چک هدر دوزبانه، قالب README ماژول، جدایی پوشه‌ها، فلگ ماژول‌ها.

set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FAIL=0

echo "=== AI_CONTEXT execution / اجرای قوانین AI ==="
echo "Root: $ROOT"
echo ""

# 1. Check Firmware/README.md does NOT exist (only AI_CONTEXT.md allowed)
echo "[1] Firmware root README check"
if [ -f "$ROOT/Firmware/README.md" ]; then
  echo "  FAIL: Firmware/README.md exists, should not (only AI_CONTEXT.md)"
  FAIL=1
else
  echo "  OK: No Firmware/README.md"
fi

# 2. Check each Module README has 7 sections
echo ""
echo "[2] Module README template (7 sections)"
for d in "$ROOT"/Firmware/Modules/*; do
  [ -d "$d" ] || continue
  mod=$(basename "$d")
  readme="$d/README.md"
  if [ ! -f "$readme" ]; then
    echo "  FAIL: $mod missing README.md"
    FAIL=1
    continue
  fi
  for sec in "## وضعیت" "## تاریخچه" "## فایل‌ها" "## توابع" "## پایه‌ها" "## پیش‌فرض امن" "## درخت اتصال"; do
    if ! grep -qF "$sec" "$readme"; then
      echo "  FAIL: $mod README missing section: $sec"
      FAIL=1
    fi
  done
  echo "  OK: $mod has 7 sections"
done

# 3. Check .c/.h bilingual header
echo ""
echo "[3] Bilingual header check (.c/.h top comment)"
find "$ROOT/Firmware" -type f \( -name "*.c" -o -name "*.h" \) | while read -r f; do
  if ! grep -q "@file" "$f"; then
    echo "  FAIL: $f missing @file"
    FAIL=1
  fi
  if ! grep -q "\[EN\]" "$f"; then
    echo "  FAIL: $f missing [EN]"
    FAIL=1
  fi
  if ! grep -q "\[FA\]" "$f"; then
    echo "  FAIL: $f missing [FA]"
    FAIL=1
  fi
done
echo "  Bilingual header scan done"

# 4. Check function comments: at least each .c/.h has @brief before function
echo ""
echo "[4] Function brief check (heuristic)"
find "$ROOT/Firmware" -type f \( -name "*.c" -o -name "*.h" \) | head -n 20 | while read -r f; do
  briefs=$(grep -c "@brief" "$f" || true)
  echo "  $(basename "$f"): briefs=$briefs"
done

# 5. Check folder separation: CubeIDE and CubeMX should not contain Firmware copy
echo ""
echo "[5] Folder separation CubeMX/CubeIDE vs Firmware"
if [ -d "$ROOT/CubeIDE/Firmware" ] || [ -d "$ROOT/CubeMX/Firmware" ]; then
  echo "  FAIL: Firmware copied inside CubeIDE or CubeMX"
  FAIL=1
else
  echo "  OK: No Firmware inside CubeIDE/CubeMX"
fi
if [ -d "$ROOT/CubeIDE/Core/Inc/Firmware" ]; then
  echo "  FAIL: Firmware inside CubeIDE Core"
  FAIL=1
else
  echo "  OK: Core does not contain Firmware"
fi

# 6. Check modules_enable.h: only UI=1
echo ""
echo "[6] modules_enable.h flags"
cat "$ROOT/Firmware/Config/Inc/modules_enable.h"
if grep -q "#define MODULE_UI.*1" "$ROOT/Firmware/Config/Inc/modules_enable.h"; then
  echo "  OK: MODULE_UI=1"
else
  echo "  FAIL: MODULE_UI not 1"
  FAIL=1
fi
for m in FAULT MEASUREMENT PROTECTION CHANGEOVER CHARGER JITTER ESP; do
  if grep -q "#define MODULE_${m}.*1" "$ROOT/Firmware/Config/Inc/modules_enable.h"; then
    echo "  FAIL: MODULE_${m} should be 0 at this stage"
    FAIL=1
  fi
done
echo "  Other modules are 0 (expected for LED-only stage)"

# 7. Check .ioc for ADC/PWM/UART not enabled
echo ""
echo "[7] CubeMX .ioc peripheral check (ADC/PWM/UART should be OFF)"
IOC="$ROOT/CubeMX/CubeIDE.ioc"
if [ -f "$IOC" ]; then
  if grep -q "Mcu.IP.*ADC" "$IOC"; then
    echo "  FAIL: ADC found in .ioc, should be OFF for LED stage"
    FAIL=1
  else
    echo "  OK: No ADC in .ioc"
  fi
  if grep -q "Mcu.IP.*TIM[2-4]" "$IOC"; then
    echo "  FAIL: TIM2-4 found (PWM) should be OFF"
    FAIL=1
  else
    echo "  OK: No TIM2-4 PWM in .ioc"
  fi
  if grep -q "Mcu.IP.*USART" "$IOC"; then
    echo "  FAIL: USART found, should be OFF"
    FAIL=1
  else
    echo "  OK: No USART in .ioc"
  fi
else
  echo "  WARN: $IOC not found"
fi

# 8. Check root README has connection tree
echo ""
echo "[8] Root README connection tree"
if grep -q "درخت اتصال کل پروژه" "$ROOT/README.md"; then
  echo "  OK: Root README has full tree"
else
  echo "  FAIL: Root README missing full tree"
  FAIL=1
fi

echo ""
if [ $FAIL -eq 0 ]; then
  echo "ALL CHECKS PASSED / همه چک‌ها پاس شد"
  exit 0
else
  echo "SOME CHECKS FAILED / بعضی چک‌ها فیل شد"
  exit 1
fi
