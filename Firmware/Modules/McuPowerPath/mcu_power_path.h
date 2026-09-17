/**
 * @file    mcu_power_path.h
 * @brief   [EN] MCU self-supply path via Q1 (PB5) - independent from Changeover Q17 (PB11).
 *              Q1 is the MCU battery switch (active-low): PB5 Low = battery connected, High = disconnected.
 *              Input presence is PB4 (BSP_GPIO_INPUT_24V_PRESENT) with both-edge EXTI; valid input is v_in >= 22V.
 *              After a continuous 5s valid input, Q1 disconnects the battery; any input loss reconnects immediately in ISR.
 *          [FA] مسیر تغذیهٔ خود MCU با Q1 (PB5) - مستقل از Changeover Q17 (PB11).
 *              Q1 کلید باتری MCU (active-low): PB5 Low = باتری وصل، High = باتری قطع.
 *              تشخیص ورودی PB4 با وقفه دو لبه؛ ورودی معتبر v_in >= 22V پس از 5 ثانیه پیوسته Q1 را قطع می‌کند.
 *
 * @note    [EN] This module owns ONLY PB5 (BSP_GPIO_BATTERY_SWITCH). PB11 remains owned by Changeover.
 *              Changeover thresholds (21000/20800/21200, 3000ms) are not used here.
 *              No board_pins.h, no HAL, no float/queue/mutex in ISR; ISR does only GPIO write and volatile flag.
 *              Time conversion ONLY via rtos_time.h; tick=1ms assumption is forbidden.
 *          [FA] این ماژول فقط مالک PB5 است؛ PB11 در اختیار Changeover می‌ماند.
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
 * @brief  [EN] Input voltage at or above which the DC input is considered valid, in millivolts.
 *         Range 0..40000 mV; effect: v_in_mv >= 22000 is required to qualify.
 *         This threshold is independent from Changeover thresholds.
 *         [FA] ولتاژ ورودی که از آن به بالا ورودی DC معتبر محسوب می‌شود، بر حسب میلی‌ولت.
 */
#define MCU_POWER_INPUT_VALID_MV        22000u

/* ==================== Functions ==================== */

/**
 * @brief  [EN] Initialize Q1 path: battery connected (PB5 Low), timer inactive.
 *         [FA] مسیر Q1 را مقداردهی می‌کند: باتری وصل (PB5 Low)، تایمر غیرفعال.
 */
void func__McuPowerPath_Init(void);

/**
 * @brief  [EN] Periodic qualification: call from the control task every ~10 ms.
 *              Uses valid v_in_mv >= 22000 for 5s to disconnect; input loss keeps battery connected.
 *              Battery voltage does not affect the decision while valid input persists.
 *         [FA] احراز دوره‌ای: از تسک کنترل هر حدود 10 میلی‌ثانیه صدا زده شود.
 *              با v_in معتبر 5 ثانیه را می‌شمارد و سپس باتری را قطع می‌کند.
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
