/**
 * @file    McuPowerPath_Implementation_Report_2026-09-17.md
 * @brief   [EN] Implementation report for Q1 (PB5) independent MCU supply with hysteresis 22000/21500.
 *          [FA] گزارش پیاده‌سازی تغذیهٔ مستقل MCU با Q1 (PB5) با هیسترزیس 22000/21500.
 */

# McuPowerPath Implementation Report — 2026-09-17 (Hysteresis Update)

## Summary / خلاصه

- Independent Q1 on PB5 (BSP_GPIO_BATTERY_SWITCH, active-low) owned exclusively by `Firmware/Modules/McuPowerPath`.
- Q17 on PB11 (BSP_GPIO_PROTECT_BATTERY) remains Changeover; no cross-wiring, `DOC/State.xlsx` untouched, `MODULE_PROTECTION=0`.
- Boot latch: `PIN_SAFE_BAT_SWITCH_HIGH=0u`, `MX_GPIO_Init()` → `PB5 Low` before output, `PB11 High` independent.
- PB4 both-edge EXTI intact; **hysteresis** on `v_in` (not `v_bat`):
  - `QUALIFY 22000u`: `v_in >=22000` continuous 5000ms → `PB5 High` (Q1 off)
  - `RECONNECT 21500u`: `v_in <21500` → `PB5 Low` (Q1 on), cancel timer (also applies after Q1 already off)
  - `21500..21999` dead-band → cancel pending timer, preserve Q1 (21.9V alone does **not** reconnect)
  - `HYSTERESIS 500u` = QUALIFY-RECONNECT
  - Falling PB4 still **immediate Low in ISR** without ADC wait, then `BSP_EXTI_INPUT_DETECT`.
- `Run()` in `TaskControl` ~10ms, no separate task, `MODULE_MCU_POWER_PATH=1`, `.cproject` & `check_firmware_syntax.sh` updated.
- Validation workbooks added, README updated, simulation tests for hysteresis added.

## Changed Files / فایل‌های تغییر یافته (2026-09-17)

| File | Change (hysteresis) |
|------|---------------------|
| `Firmware/Modules/McuPowerPath/mcu_power_path.h` | Added `MCU_POWER_INPUT_QUALIFY_MV 22000u`, `RECONNECT 21500u`, `HYSTERESIS (QUALIFY-RECONNECT)` with bilingual docs; kept `VALID` as alias to `QUALIFY`; updated brief/note for hysteresis. |
| `Firmware/Modules/McuPowerPath/mcu_power_path.c` | Rewrote `Run()` to 3-band hysteresis: `>=22000` → 5s timer → High, `21500..21999`/invalid → cancel+preserve, `<21500` → Low+cancel; time via `osKernelGetTickCount`+`TicksToMilliseconds`. ISR unchanged order Low→event. |
| `Firmware/Modules/McuPowerPath/README.md` | Updated وضعیت/فایل‌ها/توابع/پایه‌ها/پیش‌فرض امن with 22000/21500/500 and 3-band behavior, history row for hysteresis. |
| `Firmware/Modules/McuPowerPath/McuPowerPath_Validation.xlsx` **NEW** | Logic validation workbook (راهنما/برنامه تست/مراجع سناریو/ثبت ایراد/تأیید نهایی/فهرست‌ها) with hardware mapping, startup, 22V 5s, hysteresis 21.9/21.5/<21.5→Low, return 22V re-qualify, PB4 ISR, v_bat irrelevant, regression. |
| `Firmware/Modules/McuPowerPath/McuPowerPath_Board_Validation.xlsx` **NEW** | Board validation workbook (same sheets, board columns, measurement instrument, commit) mirrored for on-board tests. |
| (Previous) `board_pins.h` `main.c` `bsp_exti.c` `modules_enable.h` `rtos_app.c` `task_control.c` `.cproject` `check_firmware_syntax.sh` | Unchanged from 2026-09-17 base (PIN_SAFE 0, PB5 Low latch, ISR hook before event, MODULE_MCU_POWER_PATH 1, includes). |

**Not changed:** `changeover.c/.h` thresholds 21000/20800/21200 3000ms, `DOC/State.xlsx`, `MODULE_PROTECTION=0`.

## ISR Hook / هوک وقفه

- `bsp_exti.c` → `HAL_GPIO_EXTI_Callback(PB4)`:
  `#if MODULE_MCU_POWER_PATH func__McuPowerPath_OnInputIrq(); #endif` **before** `BSP_EXTI_INPUT_DETECT`.
- ISR: `Read PB4` low → `Write PB5 Low` + `BatteryConnected=true` + `TimerActive=false` (volatile only, no RTOS/ADC).

## Build / Syntax

- `bash tools/check_ai_rules.sh` → **ALL CHECKS PASSED** (McuPowerPath 7 sections OK, bilingual headers, `MODULE_MCU_POWER_PATH 1`, no HAL_Delay, markers, etc.)
- `bash tools/check_firmware_syntax.sh` → **HOST SYNTAX CHECK PASSED** (`gcc -fsyntax-only -std=c11 -Werror -DSTM32F103xB`)

## Simulation Tests (hysteresis) / تست شبیه‌سازی

Python host simulation mirrors `Run()` + `OnInputIrq` with 10ms tick:

| Test | Steps | Expected PB5 | Result |
|------|-------|--------------|--------|
| 22000mV 5s → High | `v_in=22000` from t0 every 10ms | Low until 4990ms, **High @5000ms** | PASS |
| 21900 after Q1 off → still High | After above High, set `v_in=21900` (dead-band) | **High preserved** (no reconnect) | PASS |
| 21400 after Q1 off → Low | `v_in=21400` (<21500) | **Immediate Low**, timer cancelled | PASS |
| PB4 falling before ADC threshold | Valid `22000` 2s → ISR `input_present=false` before Run sees <21500 | **ISR Low immediately**, timer cancelled | PASS |
| Return to 22000 → re-qualify 5s | After Low, `v_in=22000` again | Low 5s then **High @5000ms from return** | PASS |
| Additional: 21500 exact (dead-band) after off → still High | `v_in=21500` | **High preserved** | PASS |
| Additional: <21500 (21499) → Low | `v_in=21499` | **Low** | PASS |
| Startup without DC | `v_in invalid / 0V` | **Low forever** | PASS |
| v_bat change while v_in valid | v_bat 18V→24V fluctuation, v_in 23000 | **High preserved** | PASS |

Log excerpt: `Test 22000 5s PASS (Low 4990, High 5000), 21900 PASS (High), 21400 PASS (Low), ISR PASS, re-qualify PASS`.

## Board Test Items (Excel coverage)

Both workbooks contain 6 sheets. **برنامه تست** columns: شناسه/دسته/اولویت/پیش‌شرط/روش اجرای دقیق/ورودی‌ها/مدت/نتیجه مورد انتظار/معیار قبولی/نتیجه واقعی/وضعیت/شرح ایراد/مدرک/commit/تست‌کننده/تاریخ.

Mapped items:
- **Hardware mapping:** Q1 PB5 pin41 Low=connected High=disconnected, PB4 input interrupt, PB11/Q17 Changeover-exclusive.
- **Startup:** PB5 Low, MCU from battery.
- **Valid 24V:** >=22V, no cut <5s, cut @5s.
- **Hysteresis:** 21.9V preserve & no reconnect, 21.5V preserve, <21.5V Low, return >=22V re-qualify 5s (MP-008..014).
- **PB4 falling:** ISR Low <100µs, timer cancel, no reset (MP-015).
- **v_bat irrelevant:** v_bat sweep 19-24V while v_in 23V stable → Q1 stays High (MP-016).
- **Regression:** Changeover PB11 Q17 21 scenarios unchanged, no PB5 control (MP-017).

`DOC/State.xlsx` and Changeover logic not modified per boundary.

---
*Branch `arena/01a0aaca-changeover` @ commit 96980a3 base. Hysteresis update 2026-09-17 (Asia/Tehran).*
