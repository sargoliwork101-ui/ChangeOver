/**
 * @file    mcu_power_path.h
 * @brief   [EN] MCU self-supply path via Q1 (PB5, active-low: Low = battery
 *              connected), independent from Changeover (PB11). Qualify
 *              v_in >= 22000 mV for 5000 ms => Q1 off; v_in < 21500 mV =>
 *              Q1 on; 21500..21999 = dead-band (preserve Q1); PB4 falling
 *              edge reconnects immediately in ISR.
 *          [FA] مسیر تغذیهٔ خود MCU با Q1 (PB5، ‎active-low)‎ مستقل از
 *              ‎Changeover (PB11): v_in >= 22000mV‎ به‌مدت 5000ms ← قطع؛
 *              ‎v_in < 21500mV‎ ← وصل؛ ۲۱۵۰۰..۲۱۹۹۹ نوار مرده؛ لبهٔ نزولی PB4
 *              بلافاصله در ISR وصل می‌کند.
 *
 * @note    [EN] Owns ONLY PB5; thresholds apply to v_in only (not v_bat24);
 *              Changeover thresholds are not used. No board_pins.h/HAL/
 *              float; ISR does only GPIO write + volatile flags; time via
 *              rtos_time.h only.
 *          [FA] فقط مالک PB5 است؛ آستانه‌ها فقط برای v_in هستند و آستانه‌های
 *              Changeover اینجا استفاده نمی‌شوند.
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
 * @brief  [EN] Input qualification threshold [mV] for the 5 s disconnect
 *         timer (v_in only; staying in the dead-band does NOT reconnect).
 *         [FA] آستانهٔ احراز ورودی [mV] برای تایمر ۵ ثانیهٔ قطع (فقط v_in؛
 *         ماندن در نوار مرده وصل نمی‌کند).
 */
#define MCU_POWER_INPUT_QUALIFY_MV      22000u

/**
 * @brief  [EN] Reconnect threshold [mV]: below it Q1 goes Low (battery
 *         connected) and the timer cancels; 21500..21999 preserves Q1.
 *         [FA] آستانهٔ اتصال مجدد [mV]: پایین‌تر از آن Q1 می‌شود Low و
 *         تایمر لغو می‌شود؛ ۲۱۵۰۰..۲۱۹۹۹ حالت Q1 را حفظ می‌کند.
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
 *              ‎v_in >=22000‎ برای 5 ثانیه → قطع (PB5 High)؛ ‎21500..21999‎ → لغو تایمر، حفظ Q1؛
 *              ‎v_in <21500‎ → وصل (PB5 Low).
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
