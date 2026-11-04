/**
 * @file    mcu_power_path.h
 * @brief   [EN] MCU self-supply path via Q1 (PB5, active-low: Low = battery
 *              connected), independent from Changeover (PB11). The voltage
 *              decision is the shared Fault input Schmitt permission (panel
 *              q33/q34 window + q169/q170 re-arm margins). While that
 *              permission stays true for 5000 ms => Q1 off; when it drops =>
 *              Q1 on. PB4 falling edge reconnects immediately in ISR.
 * @note    [EN] Owns ONLY PB5; thresholds apply to v_in only (not v_bat24);
 *              Changeover thresholds are not used. No board_pins.h/HAL/
 *              float; ISR does only GPIO write + volatile flags; time via
 *              rtos_time.h only.
 */
#ifndef MCU_POWER_PATH_H
#define MCU_POWER_PATH_H
/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>
/* ==================== McuPowerPath Constants / MCU ==================== */
/**
 * @brief  [EN] Continuous stable time required before disconnecting the MCU battery path, in milliseconds.
 *         Range 1..60000 ms; effect: input must stay valid for this duration before PB5 goes High.
 *         Used only for Q1 (PB5), not for Changeover Q17.
 */
#define MCU_POWER_INPUT_STABLE_MS       5000u
/**
 * @brief  [EN] Boot fallback for the shared healthy-input lower bound (panel q33).
 *              With MODULE_FAULT=1 the live q33/q34 pair is read atomically from
 *              Fault; this macro only mirrors the factory default for Fault-off.
 */
#define MCU_POWER_INPUT_DEFAULT_MIN_MV  21000u
/**
 * @brief  [EN] Boot fallback for the shared healthy-input upper bound (panel q34).
 */
#define MCU_POWER_INPUT_DEFAULT_MAX_MV  28000u
/**
 * @brief  [EN] Boot fallback Schmitt margin [mV] for MODULE_FAULT=0 only.
 *              With MODULE_FAULT=1 the live shared Fault permission (panel
 *              q169/q170 margins) decides; the 5 s timer runs while that
 *              permission is true. Clamped to span/4 on a narrow window.
 */
#define MCU_POWER_INPUT_QUALIFY_MARGIN_MV 1000u
/* ==================== McuPowerPath Init ==================== */
/**
 * @brief  [EN] Initialize Q1 path: battery connected (PB5 Low), timer inactive.
 */
void func__McuPowerPath_Init(void);
/* ==================== McuPowerPath Run ==================== */
/**
 * @brief  [EN] Periodic qualification: call from the control task every ~10 ms.
 *              Uses the shared Fault input permission: while it stays true
 *              for 5s → disconnect (PB5 High); when it drops → reconnect
 *              (PB5 Low), cancel timer. An invalid snapshot preserves Q1 and
 *              cancels the timer. Battery voltage does not affect Q1 while
 *              input is valid. PB4 falling edge still reconnects immediately
 *              in ISR.
 */
void func__McuPowerPath_Run(void);
/* ==================== McuPowerPath OnInputIrq ==================== */
/**
 * @brief  [EN] ISR-safe handler for PB4 input-detect edges. Call from the EXTI callback in ISR context.
 *              If PB4 indicates input loss, immediately drives PB5 Low (battery connected) and cancels pending timer.
 *              Must be ISR-safe: only GPIO write and volatile flags, no RTOS blocking calls.
 */
void func__McuPowerPath_OnInputIrq(void);
#endif /* MCU_POWER_PATH_H */
