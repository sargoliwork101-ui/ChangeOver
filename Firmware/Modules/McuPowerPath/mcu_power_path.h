/**
 * @file    mcu_power_path.h
 * @brief   [EN] MCU self-supply path via Q1 (PB5) - independent from Changeover Q17 (PB11).
 *              Q1 is the MCU battery switch (active-low): PB5 Low = battery connected, High = disconnected.
 *              Input presence is PB4 (BSP_GPIO_INPUT_24V_PRESENT) with both-edge EXTI;
 *              valid input for qualification is v_in >= 22000mV for 5000ms, reconnect is v_in < 21500mV (hysteresis 500mV).
 *              After continuous 5s qualification Q1 disconnects; hysteresis dead-band 21500-21999mV preserves Q1; falling PB4 reconnects immediately in ISR.
 *          [FA] مسیر تغذیهٔ خود MCU با Q1 (PB5) - مستقل از Changeover Q17 (PB11).
 *              Q1 کلید باتری MCU (active-low): PB5 Low = باتری وصل، High = باتری قطع.
 *              تشخیص ورودی PB4 با وقفه دو لبه؛ احراز ورودی معتبر v_in >= 22000mV برای 5000ms، اتصال مجدد v_in < 21500mV (هیسترزیس 500mV).
 *
 * @note    [EN] This module owns ONLY PB5 (BSP_GPIO_BATTERY_SWITCH). PB11 remains owned by Changeover.
 *              Changeover thresholds (21000/20800/21200, 3000ms) are not used here.
 *              Thresholds are for v_in only, not v_bat24. PB4 falling edge still reconnects in ISR without ADC wait.
 *              No board_pins.h, no HAL, no float/queue/mutex in ISR; ISR does only GPIO write and volatile flag.
 *              Time conversion ONLY via rtos_time.h; tick=1ms assumption is forbidden.
 *          [FA] این ماژول فقط مالک PB5 است؛ PB11 در اختیار Changeover می‌ماند.
 *              آستانه‌های Changeover در این ماژول استفاده نمی‌شوند. آستانه‌ها برای v_in هستند، نه v_bat24.
 */

#ifndef MCU_POWER_PATH_H
#define MCU_POWER_PATH_H

/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>

/* ==================== McuPowerPath Constants / ثابت‌های مسیر تغذیه MCU ==================== */

/**
 * @brief  [EN] Continuous stable time required before disconnecting the MCU battery path, in milliseconds.
 *         Range 1..60000 ms; effect: input must stay valid for this duration before PB5 goes High.
 *         Used only for Q1 (PB5), not for Changeover Q17.
 *         [FA] زمان پیوسته لازم پیش از قطع مسیر باتری MCU، بر حسب میلی‌ثانیه.
 */
#define MCU_POWER_INPUT_STABLE_MS       5000u

/**
 * @brief  [EN] Voltage at or above which DC input qualifies for the 5s disconnect timer, in millivolts.
 *         Range 0..40000 mV; effect: v_in_mv >= 22000 is required to start/continue the 5s qualification.
 *         If Q1 is already disconnected, staying in 21500..21999 does NOT reconnect; only <21500 reconnects.
 *         This threshold is independent from Changeover thresholds and is for v_in only.
 *         [FA] ولتاژی که از آن به بالا ورودی DC واجد شرایط تایمر 5 ثانیه قطع می‌شود، بر حسب میلی‌ولت.
 */
#define MCU_POWER_INPUT_QUALIFY_MV      22000u

/**
 * @brief  [EN] Voltage below which a previously disconnected battery path reconnects, in millivolts.
 *         Range 0..40000 mV; effect: v_in_mv < 21500 forces battery reconnect (PB5 Low) and cancels timer.
 *         Dead-band 21500..21999 preserves current Q1 state; 21.9V alone does not reconnect.
 *         For v_in only, not v_bat24.
 *         [FA] ولتاژی که پایین‌تر از آن مسیر باتری قبلاً قطع‌شده دوباره وصل می‌شود، بر حسب میلی‌ولت.
 */
#define MCU_POWER_INPUT_RECONNECT_MV    21500u

/**
 * @brief  [EN] Hysteresis between qualify and reconnect thresholds, in millivolts.
 *         Value = QUALIFY - RECONNECT (500mV); effect: prevents chattering around 22V.
 *         [FA] هیسترزیس بین آستانه احراز و اتصال مجدد، بر حسب میلی‌ولت.
 */
#define MCU_POWER_INPUT_HYSTERESIS_MV   (MCU_POWER_INPUT_QUALIFY_MV - MCU_POWER_INPUT_RECONNECT_MV)

/**
 * @brief  [EN] Legacy alias: valid input threshold equals qualify threshold. Prefer QUALIFY.
 *         [FA] نام قدیمی: آستانه معتبر برابر احراز است. از QUALIFY استفاده کنید.
 */
#define MCU_POWER_INPUT_VALID_MV        MCU_POWER_INPUT_QUALIFY_MV

/* ==================== Functions ==================== */

/**
 * @brief  [EN] Initialize Q1 path: battery connected (PB5 Low), timer inactive.
 *         [FA] مسیر Q1 را مقداردهی می‌کند: باتری وصل (PB5 Low)، تایمر غیرفعال.
 */
void func__McuPowerPath_Init(void);

/**
 * @brief  [EN] Periodic qualification: call from the control task every ~10 ms.
 *              v_in >=22000 for 5s → disconnect (PB5 High); v_in 21500..21999 → cancel timer, preserve Q1;
 *              v_in <21500 → reconnect (PB5 Low), cancel timer. Battery voltage does not affect Q1 while input valid.
 *         [FA] احراز دوره‌ای: از تسک کنترل هر حدود 10 میلی‌ثانیه صدا زده شود.
 *              v_in >=22000 برای 5 ثانیه → قطع (PB5 High)؛ 21500..21999 → لغو تایمر، حفظ Q1؛
 *              v_in <21500 → وصل (PB5 Low).
 */
void func__McuPowerPath_Run(void);

/**
 * @brief  [EN] ISR-safe handler for PB4 input-detect edges. Call from the EXTI callback in ISR context.
 *              If PB4 indicates input loss, immediately drives PB5 Low (battery connected) and cancels pending timer.
 *              Must be ISR-safe: only GPIO write and volatile flags, no RTOS blocking calls.
 *         [FA] تابع امن وقفه برای لبه‌های PB4. از callback وقفه صدا زده شود.
 *              اگر PB4 قطع ورودی را نشان داد، فوراً PB5 را Low و تایمر را لغو می‌کند.
 */
void func__McuPowerPath_OnInputIrq(void);

#endif /* MCU_POWER_PATH_H */
