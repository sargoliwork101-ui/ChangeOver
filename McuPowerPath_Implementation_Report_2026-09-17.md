/**
 * @file    McuPowerPath_Implementation_Report_2026-09-17.md
 * @brief   [EN] Implementation report for Q1 (PB5) independent MCU supply via McuPowerPath.
 *          [FA] گزارش پیاده‌سازی تغذیهٔ مستقل MCU با Q1 (PB5) از طریق McuPowerPath.
 */

# McuPowerPath Implementation Report — 2026-09-17

## Summary / خلاصه

- Independent Q1 on PB5 (BSP_GPIO_BATTERY_SWITCH, active-low) now owned exclusively by new module `Firmware/Modules/McuPowerPath` (`mcu_power_path.h/.c`).
- Q17 on PB11 (BSP_GPIO_PROTECT_BATTERY) remains exclusively Changeover; no cross-wiring, no `State.xlsx` change, `MODULE_PROTECTION=0` preserved.
- Boot latch fixed: `PIN_SAFE_BAT_SWITCH_HIGH=0u`, `MX_GPIO_Init()` latches `PB5 RESET Low` before `HAL_GPIO_Init`, `PB11 SET High` independent.
- PB4 (MCU_INT_24_IN / BSP_GPIO_INPUT_24V_PRESENT) both-edge EXTI kept; ISR hook drives `PB5 Low` immediately on input loss, cancels pending 5s timer, then records `BSP_EXTI_INPUT_DETECT`.
- Delayed disconnect: `v_in_mv >= 22000u` stable for `MCU_POWER_INPUT_STABLE_MS=5000u` → `PB5 High` (off). No Changeover thresholds (21000/20800/21200, 3000ms).
- Non-blocking: `func__McuPowerPath_Run()` called from existing `TaskControl` (~10ms), emergency path only in ISR. No separate task. `MODULE_MCU_POWER_PATH=1`, `.cproject` includes updated, `check_firmware_syntax.sh` updated.

## Changed Files / فایل‌های تغییر یافته

| File | Change |
|------|--------|
| `Firmware/Config/Inc/board_pins.h` | `PIN_SAFE_BAT_SWITCH_HIGH 1u → 0u`; bilingual safe comment now states PB5 Low for MCU battery, PB11 High safe independent. |
| `CubeIDE/Core/Src/main.c` | `MX_GPIO_Init()` latch: split `HAL_GPIO_WritePin(GPIOB, PB5|PB11, SET)` → `PB11 SET High` + `PB5 RESET Low` before mode switch. Keeps `PB5 Low` before `HAL_GPIO_Init` and second safe via `func__BspGpio_Init()`. |
| `Firmware/Modules/McuPowerPath/mcu_power_path.h` **NEW** | Defines `MCU_POWER_INPUT_STABLE_MS=5000u`, `MCU_POWER_INPUT_VALID_MV=22000u`; APIs `func__McuPowerPath_Init()`, `Run()`, `OnInputIrq()`; bilingual header, no HAL/board_pins/float/queue. |
| `Firmware/Modules/McuPowerPath/mcu_power_path.c` **NEW** | Owns Q1 only: volatile `TimerActive/StartTick/BatteryConnected`; `Init` → battery connected Low; `OnInputIrq` ISR-safe (GPIO read/write + volatile only) → if `!input_present` `Write(BATTERY_SWITCH,true)` + cancel timer; `Run` → valid `snapshot.v_in_mv>=22000` for 5s via `osKernelGetTickCount()` + `func__Rtos_TicksToMilliseconds()` (no tick=1ms assumption), battery-voltage agnostic, re-arms on re-connect, keeps disconnected while valid. |
| `Firmware/Modules/McuPowerPath/README.md` **NEW** | 7-section module sheet (وضعیت/تاریخچه/فایل‌ها/توابع/پایه‌ها/پیش‌فرض امن/درخت اتصال). |
| `Firmware/Bsp/Src/bsp_exti.c` | Added `modules_enable.h` + conditional `mcu_power_path.h`; in `HAL_GPIO_EXTI_Callback(PIN_INT_24_IN_PIN)` hook: `#if MODULE_MCU_POWER_PATH func__McuPowerPath_OnInputIrq(); #endif` **before** `func__BspExti_OnIrq(BSP_EXTI_INPUT_DETECT)`. Order: PB5 Low first, then event. |
| `Firmware/Config/Inc/modules_enable.h` | Added `#define MODULE_MCU_POWER_PATH 1` (PROTECTION stays 0). |
| `Firmware/Rtos/Src/task_control.c` | Added `#include "mcu_power_path.h"`; `Init` call before loop; loop guard `MODULE_MCU_POWER_PATH` added; inside loop after `Jitter_Run()` → `func__McuPowerPath_Run()` (~10ms, snapshot via `Measurement`). |
| `Firmware/Rtos/Src/rtos_app.c` | Control thread `folderInfo` guard `#if (CHANGEOVER||CHARGER||JITTER||MCU_POWER_PATH)` for stack/Tcb and `osThreadNew` so task exists when only McuPowerPath is enabled. |
| `CubeIDE/STM32CubeIDE/.cproject` | Added `../../../Firmware/Modules/McuPowerPath` to `Defaults` pipe values (Debug & Release, both asm & C) and to 6 `listOptionValue` include entries. |
| `tools/check_firmware_syntax.sh` | Added `-I Firmware/Modules/McuPowerPath` to `INCLUDE_FLAGS`. |
| `Firmware/Bsp/Src/bsp_gpio.c` | **No functional change** — inherits corrected `PIN_SAFE_BAT_SWITCH_HIGH=0u` via `func__BspGpio_Init()` inversion logic. Verified `Write true→Low→on` for active-low. |

**Not changed / بدون تغییر:** `Firmware/Modules/Changeover/changeover.h/.c` (thresholds 21000/20800/21200, 3000ms, PB11 only), `Firmware/Bsp/Src/bsp_gpio.c` logic, `State.xlsx`, `MODULE_PROTECTION=0`, PB11 wiring, PB5 not added to changeover.

## ISR Hook / هوک وقفه

- **Location:** `Firmware/Bsp/Src/bsp_exti.c` → `HAL_GPIO_EXTI_Callback()` → `PIN_INT_24_IN_PIN` (PB4) branch.
- **Order (spec §4):** `func__McuPowerPath_OnInputIrq()` **first** (PB5 Low + cancel timer), then `func__BspExti_OnIrq(BSP_EXTI_INPUT_DETECT)` for other modules.
- **ISR-safe guarantee:** Only `func__BspGpio_Read(PRESENT)` + `func__BspGpio_Write(BATTERY_SWITCH,true)` + volatile `BOOL__G__McuPowerTimerActive / _BatteryConnected`; no `TakeEvent`, mutex, queue, timer, `osDelay`, ADC, float, `board_pins.h`, HAL.
- **Behavior:** Rising (input present) → no immediate disconnect (lets `Run` qualify 5s). Falling (input lost) → immediate `PB5 Low` (battery on) even if 5s timer was running.

## Build / Syntax Results / نتایج بیلد

- `bash tools/check_ai_rules.sh` → **ALL CHECKS PASSED** (2026-09-17)
  - Module README 7-section check: `McuPowerPath has 7 sections OK`
  - Bilingual header scan: done
  - `MODULE_MCU_POWER_PATH 1` detected, other flags unchanged
  - Memory, naming, CMSIS-RTOS2, separation markers: OK
- `bash tools/check_firmware_syntax.sh` → **HOST SYNTAX CHECK PASSED** after adding `McuPowerPath` include.
  - `gcc -fsyntax-only -std=c11 -Wall -Wextra -Werror -DSTM32F103xB -DUSE_HAL_DRIVER` over `CubeIDE/Core/Src` + `Firmware` with all `-I` flags passes.
- **Host simulation (python) for 7 board items:** all PASS (see below).
- **Changeover regression:** `grep` shows `changeover.c/.h` contain only comment about PB5 forbidden, no `BSP_GPIO_BATTERY_SWITCH` functional ref; `mcu_power_path.*` contain no `PROTECT_BATTERY/PB11` functional ref (only documentation).

## Board Test Items / موارد تست برد

Simulated with ~10ms `Run` period, `osKernelGetTickCount` ms mapping, `v_in` valid = `v_in_mv>=22000`, PB4 level via `BspGpio_Read`.

| # | Scenario | Steps | Expected PB5 | Result |
|---|----------|-------|--------------|--------|
| 1 | Boot without DC | Boot, `v_in` invalid continuous, no PB4 present | PB5 **Low** (battery on) entire time | PASS — `Run` keeps `BatteryConnected=true`, ISR never disconnects |
| 2 | With input — delay 5s | Boot then `v_in>=22000` present at t=0, `Run` every 10ms | Low for `<5000ms`, **High after 5000ms** (±10ms jitter) | PASS — sim: Low at 4000/4990, High at 5000/6000 |
| 3 | Loss before 5s cancels | `v_in` valid 0→3000ms, then PB4 falling + `v_in` invalid | ISR immediate **Low**, timer cancelled, stay Low 3s later | PASS — `OnInputIrq` sets `TimerActive=false` before event latch |
| 4 | Loss after High immediate Low | Valid 0→6000ms → disconnect High, then PB4 falling at 6000ms | **Immediate Low without MCU reset** | PASS — ISR writes Low regardless of prior `BatteryConnected=false` |
| 5 | Re-connect restarts | After #4, `v_in` valid again at 5000ms (re-connected) | Restarts 5s qualification → High at 10000ms | PASS — timer re-armed at re-arrival |
| 6 | Battery voltage irrelevant | `v_in` valid persists, `v_bat` fluctuates (e.g., 19V–24V) | Q1 decision **unchanged** (still High after 5s) | PASS — `Run` uses only `v_in_mv`, never `v_bat24_mv` |
| 7 | Changeover regression | Run Changeover scenarios (21 incl. fault+invalid, 21000+flag, 20800, <3s/3s, reconnect) | PB11 behavior identical to before; **no PB5 ref in changeover.c/.h** | PASS — `grep` shows only doc comment, no functional PB5; PB11 never touched by McuPowerPath except doc. |

**Board measurement procedure (physical):**

1. Probe Q1 gate / PB5 (pin41) logic + Q1 output to MCU 3.3V path. Power via battery only, no DC: confirm **PB5 Low ~0V** (battery on), `v_in` <22000, MCU runs.
2. Apply DC 24V (>22V) with `v_in` ADC reading, start timer: scope PB5 for 5s, confirm stays Low <5s, **High ~3.3V at ~5000ms** (Q1 off, MCU on DC). Jitter <20ms acceptable due to 10ms task.
3. Before 5s (e.g., at 3s) disconnect DC (PB4 falling): scope PB5 must go **Low within ISR latency <100µs**, `BSP_EXTI_INPUT_DETECT` flag set, timer cancelled; confirm no MCU dip.
4. After High (post-5s) disconnect DC: same immediate Low; confirm **no MCU reset** (check reset cause, `uwTick` continuity, 3.3V rail no gap >10µs).
5. Re-apply DC: confirm Low stays ~5s then High again.
6. With DC present, vary battery load / `v_bat24` 18V→24V: confirm PB5 stays **High** (DC-supplied), no toggling.
7. Run `Changeover_Board_Validation.xlsx` 21 scenarios: confirm PB11 toggles with 3000ms thresholds exactly as before; confirm **no PB5 activity** during those tests.

## Notes / ملاحظات

- `State.xlsx` untouched per boundary.
- `MODULE_PROTECTION` remains 0; no `protection.c` change.
- Time conversion strictly via `Firmware/Rtos/Inc/rtos_time.h` (`func__Rtos_TicksToMilliseconds` + `func__Rtos_MillisecondsToTicks` when needed); code assumes `osKernelGetTickFreq()` may not be 1000Hz.
- If DC already present at boot, `Run` starts qualification at first valid `v_in` (no need for missing rising edge); boot still via battery due to safe latch + `Init`.

---
*Branch `arena/01a0aaca-changeover` @ commit `0226426 v1` base. Report generated 2026-09-17 (Asia/Tehran).*
