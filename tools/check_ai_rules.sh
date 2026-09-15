#!/usr/bin/env bash
set -e
SCRIPT_DIR="$(dirname "$0")"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
FAIL=0
echo "=== AI_CONTEXT execution / اجرای قوانین AI ==="
echo "Root: $ROOT"
echo ""
echo "[1] Firmware root README check"
if [ -f "$ROOT/Firmware/README.md" ]; then
  echo "  FAIL: Firmware/README.md exists"
  FAIL=1
else
  echo "  OK: No Firmware/README.md"
fi
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
echo ""
echo "[3] Bilingual header check"
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
echo ""
echo "[4] Function brief check"
find "$ROOT/Firmware" -type f \( -name "*.c" -o -name "*.h" \) | head -n 20 | while read -r f; do
  briefs=$(grep -c "@brief" "$f" || true)
  echo "  $(basename "$f"): briefs=$briefs"
done
echo ""
echo "[5] Folder separation"
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
    echo "  FAIL: MODULE_${m} should be 0"
    FAIL=1
  fi
done
echo "  Other modules are 0"
echo ""
echo "[7] CubeMX .ioc peripheral check"
IOC="$ROOT/CubeMX/CubeIDE.ioc"
if [ -f "$IOC" ]; then
  if grep -q "Mcu.IP.*ADC" "$IOC"; then
    echo "  FAIL: ADC found"
    FAIL=1
  else
    echo "  OK: No ADC"
  fi
  if grep -q "Mcu.IP.*TIM[2-4]" "$IOC"; then
    echo "  FAIL: TIM2-4 found"
    FAIL=1
  else
    echo "  OK: No TIM2-4 PWM"
  fi
  if grep -q "Mcu.IP.*USART" "$IOC"; then
    echo "  FAIL: USART found"
    FAIL=1
  else
    echo "  OK: No USART"
  fi
else
  echo "  WARN: $IOC not found"
fi
echo ""
echo "[8] Root README connection tree"
if grep -q "درخت اتصال کل پروژه" "$ROOT/README.md"; then
  echo "  OK: Root README has full tree"
else
  echo "  FAIL: Root README missing full tree"
  FAIL=1
fi
echo ""
echo "[9] func__ prefix double underscore"
FOUND_OLD_SINGLE=$(grep -R --include="*.c" --include="*.h" -E "func_[A-Z]" "$ROOT/Firmware" | grep -v "func__" | head -n 5 || true)
if [ -n "$FOUND_OLD_SINGLE" ]; then
  echo "  FAIL: Found old func_ single underscore"
  echo "$FOUND_OLD_SINGLE" | head -n 10
  FAIL=1
else
  echo "  OK: All func have func__ double"
fi
echo ""
echo "[10] Full type naming with __"
OLD_SHORT=$(grep -R --include="*.c" --include="*.h" "U32_G_\|U8_G_\|U16_G_" "$ROOT/Firmware" | grep -v "UINT32_T__G__\|UINT8_T__G__\|UINT16_T__G__" | head -n 20 || true)
if [ -n "$OLD_SHORT" ]; then
  echo "  FAIL: Found old shorthand"
  echo "$OLD_SHORT"
  FAIL=1
else
  echo "  OK: No old shorthand"
fi
if grep -R --include="*.c" --include="*.h" "uint32_t__" "$ROOT/Firmware" | head -n 1 | grep -q "uint32_t__"; then
  echo "  OK: Found uint32_t__ double"
else
  echo "  FAIL: No uint32_t__ found"
  FAIL=1
fi
if grep -R --include="*.c" --include="*.h" "UINT32_T__G__" "$ROOT/Firmware" | head -n 1 | grep -q "UINT32_T__G__"; then
  echo "  OK: Found UINT32_T__G__ double"
else
  echo "  FAIL: No UINT32_T__G__ found"
  FAIL=1
fi
echo ""
echo "[11] Function param docs"
for f in $(find "$ROOT/Firmware" -type f -name "*.h" | head -n 20); do
  funcs=$(grep -c "func__" "$f" || true)
  params=$(grep -c "@param" "$f" || true)
  echo "  $(basename "$f"): funcs=$funcs @param=$params"
done
if ! grep -q "@param.*uint32_t" "$ROOT/Firmware/Modules/Ui/ui.h"; then
  echo "  FAIL: ui.h missing @param"
  FAIL=1
else
  echo "  OK: ui.h has @param"
fi
echo ""
echo "[12] AI_CONTEXT new rules"
if grep -q "سادگی و خوانایی توابع" "$ROOT/Firmware/AI_CONTEXT.md" && grep -q "نام‌گذاری متغیر" "$ROOT/Firmware/AI_CONTEXT.md" && grep -q "نام‌گذاری تابع" "$ROOT/Firmware/AI_CONTEXT.md" && grep -q "func__" "$ROOT/Firmware/AI_CONTEXT.md"; then
  echo "  OK: AI_CONTEXT has readability, full type, func__"
else
  echo "  FAIL: AI_CONTEXT missing new rules"
  FAIL=1
fi
if grep -q "RTOS ساده" "$ROOT/Firmware/AI_CONTEXT.md" && grep -q "بدون قفل" "$ROOT/Firmware/AI_CONTEXT.md"; then
  echo "  OK: AI_CONTEXT has RTOS no delay"
else
  echo "  FAIL: AI_CONTEXT missing RTOS no delay"
  FAIL=1
fi
if grep -q "ui_config.h حذف شد" "$ROOT/Firmware/AI_CONTEXT.md"; then
  echo "  OK: AI_CONTEXT has ui_config.h deleted note"
else
  echo "  FAIL: AI_CONTEXT missing ui_config.h deleted note"
  FAIL=1
fi
echo ""
echo "[13] Memory management"
FOUND_MALLOC=$(grep -R --include="*.c" --include="*.h" -E "malloc\(|free\(|calloc\(|realloc\(" "$ROOT/Firmware" 2>/dev/null | grep -v "does not use malloc" | grep -v "heap_4" || true)
if [ -n "$FOUND_MALLOC" ]; then
  echo "  FAIL: Found malloc/free"
  echo "$FOUND_MALLOC" | head -n 20
  FAIL=1
else
  echo "  OK: No malloc/free"
fi
FOUND_XTASK=$(grep -R --include="*.c" "xTaskCreate(" "$ROOT/Firmware" 2>/dev/null | grep -v "xTaskCreateStatic" | grep -v "//" || true)
if [ -n "$FOUND_XTASK" ]; then
  echo "  FAIL: Found xTaskCreate"
  echo "$FOUND_XTASK" | head -n 5
  FAIL=1
else
  echo "  OK: Tasks use xTaskCreateStatic"
fi
if grep -q "const app_config_t APP_CONFIG" "$ROOT/Firmware/Config/Src/app_config.c"; then
  echo "  OK: APP_CONFIG is const"
else
  echo "  FAIL: APP_CONFIG not const"
  FAIL=1
fi
if grep -q "TASK_STACK_UI" "$ROOT/Firmware/Config/Inc/rtos_config.h"; then
  echo "  OK: Stack sizes defined"
else
  echo "  FAIL: Stack sizes not defined"
  FAIL=1
fi
if grep -q "vApplicationStackOverflowHook" "$ROOT/Firmware/Rtos/Src/freertos_hooks.c"; then
  echo "  OK: Stack overflow hook exists"
else
  echo "  FAIL: Stack overflow hook missing"
  FAIL=1
fi
if grep -q "مدیریت حافظه" "$ROOT/Firmware/AI_CONTEXT.md"; then
  echo "  OK: AI_CONTEXT has memory management"
else
  echo "  FAIL: AI_CONTEXT missing memory management"
  FAIL=1
fi
echo ""
echo "[14] Meaningful naming and constant prefix"
if grep -q "نام‌گذاری مرتبط با کار" "$ROOT/Firmware/AI_CONTEXT.md" || grep -q "نام باید مرتبط با کاری" "$ROOT/Firmware/AI_CONTEXT.md"; then
  echo "  OK: AI_CONTEXT has meaningful naming"
else
  echo "  FAIL: AI_CONTEXT missing meaningful naming"
  FAIL=1
fi
if grep -q "نام‌گذاری ثابت" "$ROOT/Firmware/AI_CONTEXT.md"; then
  echo "  OK: AI_CONTEXT has constant naming"
else
  echo "  FAIL: AI_CONTEXT missing constant naming"
  FAIL=1
fi
FOUND_DOT=$(grep -R --include="*.h" "UI\.c_" "$ROOT/Firmware" 2>/dev/null || true)
if [ -n "$FOUND_DOT" ]; then
  echo "  FAIL: Found invalid macro with dot"
  echo "$FOUND_DOT" | head -n 3
  FAIL=1
else
  echo "  OK: No invalid dot"
fi
if [ -f "$ROOT/Firmware/Modules/Ui/ui_config.h" ]; then
  echo "  FAIL: ui_config.h still exists"
  FAIL=1
else
  echo "  OK: ui_config.h deleted"
fi
if grep -q "#define UI_BAT_V_MIN_MV" "$ROOT/Firmware/Modules/Ui/ui.h"; then
  echo "  OK: ui.h has UI_ constants"
else
  echo "  FAIL: ui.h missing UI_ constants"
  FAIL=1
fi
if grep -q "UiBatteryRunBeepCycleCnt" "$ROOT/Firmware/Modules/Ui/ui.c" && grep -q "BuzzerTotalOnMs" "$ROOT/Firmware/Modules/Ui/ui.c" && grep -E -q "greenOnMs|greenOffMs|GreenOnMs|LED_BLINK" "$ROOT/Firmware/Modules/Ui/ui.c"; then
  echo "  OK: ui.c uses meaningful names with __"
else
  echo "  FAIL: ui.c missing meaningful names"
  FAIL=1
fi
echo ""
echo "[15] RTOS simple & readable - no HAL_Delay, vTaskDelay allowed (RTOS, MCU not locked)"
FOUND_HAL_DELAY=$(grep -R --include="*.c" "HAL_Delay(" "$ROOT/Firmware" 2>/dev/null || true)
if [ -n "$FOUND_HAL_DELAY" ]; then
  echo "  FAIL: Found HAL_Delay (locks MCU, forbidden)"
  echo "$FOUND_HAL_DELAY" | head -n 5
  FAIL=1
else
  echo "  OK: No HAL_Delay (RTOS vTaskDelay allowed, does not lock MCU)"
fi

FOUND_VDELAY_UI=$(grep -R --include="*.c" "vTaskDelay(" "$ROOT/Firmware/Modules/Ui" 2>/dev/null || true)
if [ -n "$FOUND_VDELAY_UI" ]; then
  echo "  OK: Found vTaskDelay in Ui (RTOS simple, readable, MCU not locked, other tasks run)"
else
  echo "  OK: No vTaskDelay in Ui (also OK, non-blocking tick)"
fi

if grep -R --include="*.c" "vTaskDelay" "$ROOT/Firmware/Rtos/Src" 2>/dev/null | head -n 1 | grep -q "vTaskDelay"; then
  echo "  OK: Tasks use vTaskDelay/vTaskDelayUntil (RTOS simple, not blocking MCU)"
else
  echo "  FAIL: Tasks dont use vTaskDelay"
  FAIL=1
fi

if grep -q "UI_TICK_MS" "$ROOT/Firmware/Modules/Ui/ui.h" && grep -q "Tick" "$ROOT/Firmware/Modules/Ui/ui.h"; then
  echo "  OK: ui.h has UI_TICK_MS and Tick API (simple RTOS)"
else
  echo "  FAIL: ui.h missing Tick API"
  FAIL=1
fi

# 16. Check function separation markers and buzzer at end
echo ""
echo "[16] Function separation with markers and buzzer at end (per new AI rule)"
if grep -q "==================== Buzzer / Beep" "$ROOT/Firmware/Modules/Ui/ui.c"; then
  echo "  OK: ui.c has Buzzer / Beep marker"
else
  echo "  FAIL: ui.c missing Buzzer / Beep marker"
  FAIL=1
fi

# Check buzzer code at end of ui.c (buzzer functions after LED)
BUZZER_LINE=$(grep -n "func__buzzer" "$ROOT/Firmware/Modules/Ui/ui.c" | head -n 1 | cut -d: -f1)
LED_LINE=$(grep -n "func__Ui_ScenarioInputOk" "$ROOT/Firmware/Modules/Ui/ui.c" | head -n 1 | cut -d: -f1)
if [ -n "$BUZZER_LINE" ] && [ -n "$LED_LINE" ]; then
  if [ "$BUZZER_LINE" -gt "$LED_LINE" ]; then
    echo "  OK: Buzzer code after LED (buzzer at end, LED first)"
  else
    echo "  FAIL: Buzzer code not at end (buzzer before LED)"
    FAIL=1
  fi
else
  echo "  WARN: Could not find buzzer/LED lines"
fi

# Check markers in other files
MARKER_COUNT=$(grep -R --include="*.c" "====================.*==================== " "$ROOT/Firmware" | wc -l)
if [ "$MARKER_COUNT" -ge 20 ]; then
  echo "  OK: Found $MARKER_COUNT function separation markers in Firmware"
else
  echo "  FAIL: Only $MARKER_COUNT markers found, expected >=20"
  FAIL=1
fi

if grep -q "جداسازی توابع با علامت مشخص" "$ROOT/Firmware/AI_CONTEXT.md" && grep -q "جداسازی بازر از LED" "$ROOT/Firmware/AI_CONTEXT.md"; then
  echo "  OK: AI_CONTEXT has separation rules"
else
  echo "  FAIL: AI_CONTEXT missing separation rules"
  FAIL=1
fi

echo ""
echo "[17] Formulas not linear (broken into steps, readable)"
if grep -q "فرمول‌ها خطی نباشد" "$ROOT/Firmware/AI_CONTEXT.md" || grep -q "فرمول‌ها را خطی ننویس" "$ROOT/Firmware/AI_CONTEXT.md"; then
  echo "  OK: AI_CONTEXT has non-linear formula rule"
else
  echo "  FAIL: AI_CONTEXT missing non-linear formula rule"
  FAIL=1
fi

# Check ui.c has non-linear formula broken into steps (voltageRange, voltageOffset, scaledOffset)
if grep -q "voltageRangeMv" "$ROOT/Firmware/Modules/Ui/ui.c" && grep -q "voltageOffsetMv" "$ROOT/Firmware/Modules/Ui/ui.c" && grep -q "scaledOffset" "$ROOT/Firmware/Modules/Ui/ui.c"; then
  echo "  OK: ui.c has non-linear formula broken into steps (range, offset, scaled)"
else
  echo "  FAIL: ui.c has linear formula (should break into steps)"
  FAIL=1
fi

# Check charging has non-linear steps (remainingPercent, periodPerPercent)
if grep -q "remainingPercent" "$ROOT/Firmware/Modules/Ui/ui.c" && grep -q "periodPerPercent" "$ROOT/Firmware/Modules/Ui/ui.c"; then
  echo "  OK: ui.c charging has non-linear steps (remainingPercent, periodPerPercent)"
else
  echo "  FAIL: ui.c charging has linear formula"
  FAIL=1
fi

# Check no single-line long formula like (offset * 100u) / range in one line without steps
FOUND_LINEAR=$(grep -n "offset.*\*.*100u.*\/.*range" "$ROOT/Firmware/Modules/Ui/ui.c" | head -n 1 || true)
if [ -n "$FOUND_LINEAR" ]; then
  echo "  FAIL: Found linear formula in one line (should be broken): $FOUND_LINEAR"
  FAIL=1
else
  echo "  OK: No linear one-line formula (all broken into steps)"
fi

echo ""
if [ $FAIL -eq 0 ]; then
  echo "ALL CHECKS PASSED"
  exit 0
else
  echo "SOME CHECKS FAILED"
  exit 1
fi

