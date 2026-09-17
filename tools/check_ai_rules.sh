#!/usr/bin/env bash
set -e
SCRIPT_DIR="$(dirname "$0")"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
FAIL=0
echo "=== AI_AGENT_RULES execution / اجرای قوانین AI ==="
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
echo "[6] modules_enable.h flags (module stage: UI + MEASUREMENT; BSP peripherals may be enabled)"
cat "$ROOT/Firmware/Config/Inc/modules_enable.h"
if grep -q "#define MODULE_UI.*1" "$ROOT/Firmware/Config/Inc/modules_enable.h"; then
  echo "  OK: MODULE_UI=1"
else
  echo "  FAIL: MODULE_UI not 1"
  FAIL=1
fi
if grep -q "#define MODULE_MEASUREMENT.*1" "$ROOT/Firmware/Config/Inc/modules_enable.h"; then
  echo "  OK: MODULE_MEASUREMENT=1 (ADC stage)"
else
  echo "  FAIL: MODULE_MEASUREMENT not 1"
  FAIL=1
fi
for m in PROTECTION CHARGER JITTER ESP; do
  if grep -q "#define MODULE_${m}.*1" "$ROOT/Firmware/Config/Inc/modules_enable.h"; then
    echo "  FAIL: MODULE_${m} should be 0"
    FAIL=1
  fi
done
if grep -q "#define MODULE_FAULT.*1" "$ROOT/Firmware/Config/Inc/modules_enable.h"; then
  echo "  OK: MODULE_FAULT=1 (fault-mask validation build)"
elif grep -q "#define MODULE_FAULT.*0" "$ROOT/Firmware/Config/Inc/modules_enable.h"; then
  echo "  OK: MODULE_FAULT=0 (fault module disabled)"
else
  echo "  FAIL: MODULE_FAULT must be explicitly 0 or 1"
  FAIL=1
fi
if grep -q "#define MODULE_CHANGEOVER.*1" "$ROOT/Firmware/Config/Inc/modules_enable.h"; then
  echo "  OK: MODULE_CHANGEOVER=1 (real board validation build)"
elif grep -q "#define MODULE_CHANGEOVER.*0" "$ROOT/Firmware/Config/Inc/modules_enable.h"; then
  echo "  OK: MODULE_CHANGEOVER=0 (module disabled)"
else
  echo "  FAIL: MODULE_CHANGEOVER must be explicitly 0 or 1"
  FAIL=1
fi
echo "  Other non-validation modules are 0"
echo ""
echo "[7] CubeMX .ioc peripheral check (full schematic BSP contract)"
IOC="$ROOT/CubeMX/CubeIDE.ioc"
if [ -f "$IOC" ]; then
  if grep -q "Mcu.IP.*ADC" "$IOC"; then
    echo "  OK: ADC1 enabled (5-channel measurement backend)"
  else
    echo "  FAIL: ADC1 missing in .ioc"
    FAIL=1
  fi
  if grep -q "ADC1.NbrOfConversion=5" "$IOC" && grep -q "DMA1.Request1=ADC1" "$IOC"; then
    echo "  OK: ADC1 has 5 channels + circular DMA"
  else
    echo "  FAIL: ADC1 channels/DMA not configured"
    FAIL=1
  fi
  if grep -q "Mcu.IP.*TIM2" "$IOC" && grep -q "Mcu.IP.*TIM3" "$IOC" && \
     grep -q "TIM2.Channel-Output compare CH1=TIM_CHANNEL_1" "$IOC" && \
     grep -q "TIM3.Channel-Output compare CH1=TIM_CHANNEL_1" "$IOC"; then
    echo "  OK: TIM2_CH1 and TIM3_CH1 PWM backends are retained"
  else
    echo "  FAIL: Charger PWM timers/channels missing"
    FAIL=1
  fi
  if grep -q "Mcu.IP.*USART1" "$IOC" && grep -q "USART1.BaudRate=115200" "$IOC" && \
     grep -q "PA9.Signal=USART1_TX" "$IOC" && grep -q "PA10.Signal=USART1_RX" "$IOC"; then
    echo "  OK: USART1 ESP-Link UART backend is retained"
  else
    echo "  FAIL: USART1 UART backend missing"
    FAIL=1
  fi
  # JIT1/JIT2 must be single active edge (RISING provisional, scope-verified), PB4 keeps both edges.
  if grep -q "PB2.Mode=External_Interrupt_Mode_with_Rising" "$IOC" && \
     grep -q "PB4.Mode=External_Interrupt_Mode_with_Rising_Falling_edge_trigger_detection" "$IOC" && \
     grep -q "PB6.Mode=External_Interrupt_Mode_with_Rising" "$IOC"; then
    echo "  OK: JITTER1, 24V detect and JITTER2 EXTI lines are retained (JIT single edge)"
  else
    echo "  FAIL: Schematic EXTI lines missing"
    FAIL=1
  fi
  if grep -q "Mcu.PinsNb=26" "$IOC" && ! grep -q "PB9" "$IOC"; then
    echo "  OK: Pin inventory has 26 pins and no unsupported PB9 mapping"
  else
    echo "  FAIL: Pin inventory/PB9 cleanup is incorrect"
    FAIL=1
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
if (grep -q "@param.*uint32_t" "$ROOT/Firmware/Modules/Ui/ui.h" 2>/dev/null || grep -q "@param.*uint32_t" "$ROOT/Firmware/Modules/Ui/ui_led.h" 2>/dev/null || grep -q "@param.*uint32_t" "$ROOT/Firmware/Modules/Ui/ui_buzzer.h" 2>/dev/null); then
  echo "  OK: ui.h / ui_led.h / ui_buzzer.h has @param"
else
  echo "  FAIL: ui.h missing @param"
  FAIL=1
fi
echo ""
echo "[12] AI_AGENT_RULES new rules"
if grep -q "سادگی و خوانایی توابع" "$ROOT/AI_AGENT_RULES.md" && grep -q "نام‌گذاری متغیر" "$ROOT/AI_AGENT_RULES.md" && grep -q "نام‌گذاری تابع" "$ROOT/AI_AGENT_RULES.md" && grep -q "func__" "$ROOT/AI_AGENT_RULES.md"; then
  echo "  OK: AI_AGENT_RULES has readability, full type, func__"
else
  echo "  FAIL: AI_AGENT_RULES missing new rules"
  FAIL=1
fi
if grep -q "CMSIS-RTOS2 ساده" "$ROOT/AI_AGENT_RULES.md" && grep -q "بدون قفل" "$ROOT/AI_AGENT_RULES.md"; then
  echo "  OK: AI_AGENT_RULES has CMSIS-RTOS2 no delay"
else
  echo "  FAIL: AI_AGENT_RULES missing RTOS no delay"
  FAIL=1
fi
if grep -q "ui_config.h حذف شد" "$ROOT/AI_AGENT_RULES.md"; then
  echo "  OK: AI_AGENT_RULES has ui_config.h deleted note"
else
  echo "  FAIL: AI_AGENT_RULES missing ui_config.h deleted note"
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
FOUND_DYNAMIC_THREAD=$(grep -R --include="*.c" --include="*.h" -E "\bxTaskCreate\(|\bosThreadNew\([^;]*NULL[[:space:]]*\)" "$ROOT/Firmware" 2>/dev/null | grep -v "rtos_app.c" | grep -v "//" || true)
if [ -n "$FOUND_DYNAMIC_THREAD" ]; then
  echo "  FAIL: Found a thread creation path without the static CMSIS attributes"
  echo "$FOUND_DYNAMIC_THREAD" | head -n 5
  FAIL=1
elif grep -q "osThreadNew" "$ROOT/Firmware/Rtos/Src/rtos_app.c" && grep -q "cb_mem" "$ROOT/Firmware/Rtos/Src/rtos_app.c" && grep -q "stack_mem" "$ROOT/Firmware/Rtos/Src/rtos_app.c"; then
  echo "  OK: CMSIS-RTOS2 threads provide static control blocks and stacks"
else
  echo "  FAIL: CMSIS-RTOS2 static thread attributes not found"
  FAIL=1
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
if grep -q "مدیریت حافظه" "$ROOT/AI_AGENT_RULES.md"; then
  echo "  OK: AI_AGENT_RULES has memory management"
else
  echo "  FAIL: AI_AGENT_RULES missing memory management"
  FAIL=1
fi
echo ""
echo "[14] Meaningful naming and constant prefix"
if grep -q "نام‌گذاری مرتبط با کار" "$ROOT/AI_AGENT_RULES.md" || grep -q "نام باید مرتبط با کاری" "$ROOT/AI_AGENT_RULES.md"; then
  echo "  OK: AI_AGENT_RULES has meaningful naming"
else
  echo "  FAIL: AI_AGENT_RULES missing meaningful naming"
  FAIL=1
fi
if grep -q "نام‌گذاری ثابت" "$ROOT/AI_AGENT_RULES.md"; then
  echo "  OK: AI_AGENT_RULES has constant naming"
else
  echo "  FAIL: AI_AGENT_RULES missing constant naming"
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
if grep -q "#define UI_BAT_V_MIN_MV" "$ROOT/Firmware/Modules/Ui/ui.h" 2>/dev/null || grep -q "#define UI_BAT_V_MIN_MV" "$ROOT/Firmware/Modules/Ui/ui_led.h" 2>/dev/null; then
  echo "  OK: ui.h / ui_led.h has UI_ constants (LED) per user request constants in own header"
else
  echo "  FAIL: ui.h / ui_led.h missing UI_ constants"
  FAIL=1
fi
if (grep -E -q "dutyWindowMs|beepOnMs|periodTailMs" "$ROOT/Firmware/Modules/Ui/ui_buzzer.c" 2>/dev/null && grep -E -q "greenOnMs|greenOffMs" "$ROOT/Firmware/Modules/Ui/ui_led.c" 2>/dev/null); then
  echo "  OK: ui_led.c / ui_buzzer.c uses meaningful names with __"
else
  echo "  FAIL: UI files missing meaningful names"
  FAIL=1
fi

# Check for split files existence (LED and BUZZER per user request)
if [ -f "$ROOT/Firmware/Modules/Ui/ui_led.h" ] && [ -f "$ROOT/Firmware/Modules/Ui/ui_led.c" ] && [ -f "$ROOT/Firmware/Modules/Ui/ui_buzzer.h" ] && [ -f "$ROOT/Firmware/Modules/Ui/ui_buzzer.c" ]; then
  echo "  OK: UI split into LED and BUZZER (ui_led.h/c, ui_buzzer.h/c) exists per user request"
else
  echo "  WARN: UI split files not found (ui_led.h/c, ui_buzzer.h/c) - expected after split"
fi
echo ""
echo "[15] CMSIS-RTOS2 simple & readable - no HAL_Delay (RTOS, MCU not locked)"
FOUND_HAL_DELAY=$(grep -R --include="*.c" "HAL_Delay(" "$ROOT/Firmware" 2>/dev/null || true)
if [ -n "$FOUND_HAL_DELAY" ]; then
  echo "  FAIL: Found HAL_Delay (locks MCU, forbidden)"
  echo "$FOUND_HAL_DELAY" | head -n 5
  FAIL=1
else
  echo "  OK: No HAL_Delay (CMSIS-RTOS2 delay lets other threads run)"
fi

FOUND_OS_DELAY_UI=$(grep -R --include="*.c" -E "osDelay\(|func__Rtos_DelayMilliseconds\(" "$ROOT/Firmware/Modules/Ui" 2>/dev/null || true)
if [ -n "$FOUND_OS_DELAY_UI" ]; then
  echo "  OK: UI uses CMSIS-RTOS2-compatible delays"
else
  echo "  OK: UI is non-blocking and has no delay call"
fi

if grep -R --include="*.c" -E "osDelayUntil\(|func__Rtos_DelayMilliseconds\(" "$ROOT/Firmware/Rtos/Src" 2>/dev/null | head -n 1 | grep -Eq "osDelayUntil|func__Rtos_DelayMilliseconds"; then
  echo "  OK: Threads use CMSIS-RTOS2 delay APIs (other threads continue)"
else
  echo "  FAIL: CMSIS-RTOS2 delay API not found in threads"
  FAIL=1
fi

if (grep -q "UI_TICK_MS" "$ROOT/Firmware/Modules/Ui/ui.h" 2>/dev/null || grep -q "UI_TICK_MS" "$ROOT/Firmware/Modules/Ui/ui_led.h" 2>/dev/null || grep -q "UI_TICK_MS" "$ROOT/Firmware/Modules/Ui/ui_buzzer.h" 2>/dev/null) && (grep -q "Tick" "$ROOT/Firmware/Modules/Ui/ui.h" 2>/dev/null || grep -q "Tick" "$ROOT/Firmware/Modules/Ui/ui_led.h" 2>/dev/null || grep -q "Tick" "$ROOT/Firmware/Modules/Ui/ui_buzzer.h" 2>/dev/null); then
  echo "  OK: ui.h / ui_led.h / ui_buzzer.h has UI_TICK_MS and Tick API (simple RTOS, constants in own header)"
else
  echo "  FAIL: ui.h missing Tick API"
  FAIL=1
fi

# 16. Check function separation markers and buzzer at end
echo ""
echo "[16] Function separation with markers and buzzer at end (per new AI rule) - now split LED/BUZZER"
if grep -q "==================== Buzzer / Beep" "$ROOT/Firmware/Modules/Ui/ui.c" 2>/dev/null || grep -q "==================== Buzzer / Beep" "$ROOT/Firmware/Modules/Ui/ui_buzzer.c" 2>/dev/null; then
  echo "  OK: ui.c or ui_buzzer.c has Buzzer / Beep marker"
else
  echo "  FAIL: ui.c / ui_buzzer.c missing Buzzer / Beep marker"
  FAIL=1
fi

# Check split files: LED in ui_led.c, one buzzer service in ui_buzzer.c.
if [ -f "$ROOT/Firmware/Modules/Ui/ui_led.c" ] && [ -f "$ROOT/Firmware/Modules/Ui/ui_buzzer.c" ]; then
  if grep -q "func__Ui_ScenarioInputOk" "$ROOT/Firmware/Modules/Ui/ui_led.c" && grep -q "func__Ui_Buzzer_Tick" "$ROOT/Firmware/Modules/Ui/ui_buzzer.c"; then
    echo "  OK: Split LED and BUZZER: LED in ui_led.c, BUZZER in ui_buzzer.c (single buzzer service)"
  else
    echo "  WARN: Could not find buzzer/LED lines in split files"
  fi
else
  echo "  WARN: Could not find buzzer/LED split files"
fi

# Check markers in other files + UI split files have markers above each function
MARKER_COUNT=$(grep -R --include="*.c" "====================.*==================== " "$ROOT/Firmware" | wc -l)
if [ "$MARKER_COUNT" -ge 20 ]; then
  echo "  OK: Found $MARKER_COUNT function separation markers in Firmware"
else
  echo "  FAIL: Only $MARKER_COUNT markers found, expected >=20"
  FAIL=1
fi

# Check UI split files have markers above each function (per user request: بالای هر تابع این مدلی جدا بشه)
UI_LED_MARKERS=$(grep -c "====================.*==================== " "$ROOT/Firmware/Modules/Ui/ui_led.h" 2>/dev/null || echo 0)
UI_BUZZER_MARKERS=$(grep -c "====================.*==================== " "$ROOT/Firmware/Modules/Ui/ui_buzzer.h" 2>/dev/null || echo 0)
UI_LED_C_MARKERS=$(grep -c "====================.*==================== " "$ROOT/Firmware/Modules/Ui/ui_led.c" 2>/dev/null || echo 0)
UI_BUZZER_C_MARKERS=$(grep -c "====================.*==================== " "$ROOT/Firmware/Modules/Ui/ui_buzzer.c" 2>/dev/null || echo 0)
if [ "$UI_LED_MARKERS" -ge 5 ] && [ "$UI_BUZZER_MARKERS" -ge 3 ] && [ "$UI_LED_C_MARKERS" -ge 8 ] && [ "$UI_BUZZER_C_MARKERS" -ge 5 ]; then
  echo "  OK: UI split files have markers above each function (ui_led.h $UI_LED_MARKERS, ui_buzzer.h $UI_BUZZER_MARKERS, ui_led.c $UI_LED_C_MARKERS, ui_buzzer.c $UI_BUZZER_C_MARKERS)"
else
  echo "  FAIL: UI split files missing markers above each function (led.h $UI_LED_MARKERS, buzzer.h $UI_BUZZER_MARKERS, led.c $UI_LED_C_MARKERS, buzzer.c $UI_BUZZER_C_MARKERS)"
  FAIL=1
fi

if grep -q "جداسازی توابع با علامت مشخص" "$ROOT/AI_AGENT_RULES.md" && grep -q "جداسازی بازر از LED" "$ROOT/AI_AGENT_RULES.md"; then
  echo "  OK: AI_AGENT_RULES has separation rules"
else
  echo "  FAIL: AI_AGENT_RULES missing separation rules"
  FAIL=1
fi

echo ""
echo "[17] Formulas not linear (broken into steps, readable)"
if grep -q "فرمول‌ها خطی نباشد" "$ROOT/AI_AGENT_RULES.md" || grep -q "فرمول‌ها را خطی ننویس" "$ROOT/AI_AGENT_RULES.md"; then
  echo "  OK: AI_AGENT_RULES has non-linear formula rule"
else
  echo "  FAIL: AI_AGENT_RULES missing non-linear formula rule"
  FAIL=1
fi

# Check ui.c or ui_led.c has non-linear formula broken into steps (voltageRange, voltageOffset, scaledOffset)
if (grep -q "voltageRangeMv" "$ROOT/Firmware/Modules/Ui/ui.c" 2>/dev/null && grep -q "voltageOffsetMv" "$ROOT/Firmware/Modules/Ui/ui.c" 2>/dev/null && grep -q "scaledOffset" "$ROOT/Firmware/Modules/Ui/ui.c" 2>/dev/null) || \
   (grep -q "voltageRangeMv" "$ROOT/Firmware/Modules/Ui/ui_led.c" 2>/dev/null && grep -q "voltageOffsetMv" "$ROOT/Firmware/Modules/Ui/ui_led.c" 2>/dev/null && grep -q "scaledOffset" "$ROOT/Firmware/Modules/Ui/ui_led.c" 2>/dev/null); then
  echo "  OK: ui.c / ui_led.c has non-linear formula broken into steps (range, offset, scaled)"
else
  echo "  FAIL: ui.c has linear formula (should break into steps)"
  FAIL=1
fi

# Check charging has non-linear steps (remainingPercent, periodPerPercent)
if (grep -q "remainingPercent" "$ROOT/Firmware/Modules/Ui/ui.c" 2>/dev/null && grep -q "periodPerPercent" "$ROOT/Firmware/Modules/Ui/ui.c" 2>/dev/null) || \
   (grep -q "remainingPercent" "$ROOT/Firmware/Modules/Ui/ui_led.c" 2>/dev/null && grep -q "periodPerPercent" "$ROOT/Firmware/Modules/Ui/ui_led.c" 2>/dev/null); then
  echo "  OK: ui.c / ui_led.c charging has non-linear steps (remainingPercent, periodPerPercent)"
else
  echo "  FAIL: ui.c charging has linear formula"
  FAIL=1
fi

# Check no single-line long formula like (offset * 100u) / range in one line without steps
FOUND_LINEAR=$(grep -n "offset.*\*.*100u.*\/.*range" "$ROOT/Firmware/Modules/Ui/ui.c" 2>/dev/null | head -n 1 || true)
FOUND_LINEAR2=$(grep -n "offset.*\*.*100u.*\/.*range" "$ROOT/Firmware/Modules/Ui/ui_led.c" 2>/dev/null | head -n 1 || true)
if [ -n "$FOUND_LINEAR" ] || [ -n "$FOUND_LINEAR2" ]; then
  echo "  FAIL: Found linear formula in one line (should be broken): $FOUND_LINEAR $FOUND_LINEAR2"
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

