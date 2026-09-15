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

# 9. Check func_ prefix for our own functions (not HAL, not FreeRTOS hooks)
echo ""
echo "[9] func_ prefix for our own functions (per new AI rule)"
# Count our functions without func_ prefix in Firmware (excluding vApplication, HAL, etc)
# We expect all our functions to have func_ prefix
FOUND_OLD=$(grep -R --include="*.c" --include="*.h" -E "^\s*(void|bool|uint8_t|uint16_t|uint32_t|int\d+_t|app_state_t|fault_mask_t|measurement_snapshot_t)\s+[A-Z][a-zA-Z_]*_(Init|Run|Write|Read|Set|Get|Clear|Any|Evaluate|Power|Start|BoardTest|BuzzerBeep|BatteryVoltageToPercent|Scenario)" "$ROOT/Firmware" | grep -v "func_" | grep -v "vApplication" | grep -v "HAL_" || true)
if [ -n "$FOUND_OLD" ]; then
  echo "  FAIL: Found our functions without func_ prefix:"
  echo "$FOUND_OLD" | head -n 20
  FAIL=1
else
  echo "  OK: All our functions have func_ prefix (system functions untouched)"
fi

# 10. Check full type naming for variables (uint32_t_, UINT32_T_G_, etc, not shorthand U32_G_)
echo ""
echo "[10] Full type naming for variables (not shorthand U32_G_ / u32_)"
# Shorthand that should NOT exist anymore (except in comments)
FOUND_SHORT=$(grep -R --include="*.c" --include="*.h" -E "U32_G_|U8_G_|U16_G_|s_\b" "$ROOT/Firmware/Modules/Ui" "$ROOT/Firmware/Rtos/Src/task_ui.c" 2>/dev/null | grep -v "//" | grep -v "U32_G_InputVoltageMv" | grep -v "U32_G_BatteryVoltageMv" | grep -v "U32_G_BeepCnt" | grep -v "UINT32_T_G_" | head -n 20 || true)
# Actually check for old shorthand U32_G_ that is not UINT32_T_G_
OLD_SHORT=$(grep -R --include="*.c" --include="*.h" "U32_G_\|U8_G_\|U16_G_" "$ROOT/Firmware" | grep -v "UINT32_T_G_\|UINT8_T_G_\|UINT16_T_G_\|UINT16_T_G_Raw\|UINT8_T_G_Flags" | head -n 20 || true)
if [ -n "$OLD_SHORT" ]; then
  echo "  FAIL: Found old shorthand variable naming (should be full type like UINT32_T_G_):"
  echo "$OLD_SHORT"
  FAIL=1
else
  echo "  OK: No old shorthand U32_G_/U8_G_ found, full type naming used"
fi

# 11. Check each function has @param for params (per new AI rule)
echo ""
echo "[11] Function param docs (@param) check"
# For each .h file, count functions vs @param
for f in $(find "$ROOT/Firmware" -type f -name "*.h" | head -n 20); do
  funcs=$(grep -c "func_" "$f" || true)
  params=$(grep -c "@param" "$f" || true)
  echo "  $(basename "$f"): funcs=$funcs @param=$params"
done
# Ensure ui.h has param docs for all functions with params
if ! grep -q "@param.*uint32_t" "$ROOT/Firmware/Modules/Ui/ui.h"; then
  echo "  FAIL: ui.h missing @param with full type"
  FAIL=1
else
  echo "  OK: ui.h has @param with full type"
fi

# 12. Check AI_CONTEXT has new rules
echo ""
echo "[12] AI_CONTEXT.md new rules (readability, full type, func_ prefix)"
if grep -q "سادگی و خوانایی توابع" "$ROOT/Firmware/AI_CONTEXT.md" && grep -q "نام‌گذاری متغیر" "$ROOT/Firmware/AI_CONTEXT.md" && grep -q "نام‌گذاری تابع" "$ROOT/Firmware/AI_CONTEXT.md" && grep -q "func_" "$ROOT/Firmware/AI_CONTEXT.md"; then
  echo "  OK: AI_CONTEXT has readability, full type, func_ prefix rules"
else
  echo "  FAIL: AI_CONTEXT missing new rules"
  FAIL=1
fi

# 13. Check memory management rules
echo ""
echo "[13] Memory management (no malloc/free, static allocation, const config, stack overflow hook)"
# No malloc/free in Firmware - check for actual calls malloc( free( etc, ignore comments about not using malloc
FOUND_MALLOC=$(grep -R --include="*.c" --include="*.h" -E "malloc\(|free\(|calloc\(|realloc\(" "$ROOT/Firmware" 2>/dev/null | grep -v "does not use malloc" | grep -v "heap_4" || true)
if [ -n "$FOUND_MALLOC" ]; then
  echo "  FAIL: Found malloc/free calls in Firmware (should use static allocation):"
  echo "$FOUND_MALLOC" | head -n 20
  FAIL=1
else
  echo "  OK: No malloc/free calls in Firmware (static allocation)"
fi

# Check xTaskCreateStatic used, not xTaskCreate
FOUND_XTASK=$(grep -R --include="*.c" "xTaskCreate(" "$ROOT/Firmware" 2>/dev/null | grep -v "xTaskCreateStatic" | grep -v "//" || true)
if [ -n "$FOUND_XTASK" ]; then
  echo "  FAIL: Found xTaskCreate (should use xTaskCreateStatic):"
  echo "$FOUND_XTASK" | head -n 5
  FAIL=1
else
  echo "  OK: Tasks use xTaskCreateStatic (no malloc)"
fi

# Check APP_CONFIG is const
if grep -q "const app_config_t APP_CONFIG" "$ROOT/Firmware/Config/Src/app_config.c"; then
  echo "  OK: APP_CONFIG is const (Flash, not RAM)"
else
  echo "  FAIL: APP_CONFIG not const"
  FAIL=1
fi

# Check stack sizes defined in rtos_config.h
if grep -q "TASK_STACK_UI" "$ROOT/Firmware/Config/Inc/rtos_config.h" && grep -q "TASK_STACK_MEASUREMENT" "$ROOT/Firmware/Config/Inc/rtos_config.h"; then
  echo "  OK: Stack sizes defined in rtos_config.h (words)"
else
  echo "  FAIL: Stack sizes not defined"
  FAIL=1
fi

# Check stack overflow hook exists
if grep -q "vApplicationStackOverflowHook" "$ROOT/Firmware/Rtos/Src/freertos_hooks.c"; then
  echo "  OK: Stack overflow hook exists"
else
  echo "  FAIL: Stack overflow hook missing"
  FAIL=1
fi

# Check AI_CONTEXT has memory management section
if grep -q "مدیریت حافظه" "$ROOT/Firmware/AI_CONTEXT.md"; then
  echo "  OK: AI_CONTEXT has memory management section"
else
  echo "  FAIL: AI_CONTEXT missing memory management"
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
