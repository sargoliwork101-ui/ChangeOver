/**
 * @file    changeover.h
 * @brief   [EN] Input vs battery path state machine - battery protect via BSP logical API only.
 *          [FA] ماشین حالت مسیر ورودی یا باتری - فقط با API منطقی BSP.
 *
 * @note    [EN] This module uses ONLY: snapshot.valid, snapshot.v_bat24_mv,
 *              snapshot.input_present, fault_mask, BOOL__G__UiBatteryAlarmIssued.
 *              Time conversion uses rtos_time.h only, tick=1ms assumption is forbidden.
 *              This module owns the two logical power-path outputs of the board:
 *              BSP_GPIO_PROTECT_BATTERY (battery cut) and BSP_GPIO_BATTERY_SWITCH
 *              (MCU control PSU feed); no physical pin names enter this module.
 *          [FA] این ماژول فقط از valid، v_bat24_mv، input_present، fault_mask و فلگ UI استفاده می‌کند.
 *              تبدیل زمان فقط با rtos_time.h و بدون فرض tick=1ms. این ماژول مالک
 *              دو خروجی منطقی مسیر توان برد است: BSP_GPIO_PROTECT_BATTERY (قطع
 *              باتری) و BSP_GPIO_BATTERY_SWITCH (تغذیه PSU کنترل MCU)؛ هیچ نام
 *              پایهٔ فیزیکی وارد این ماژول نمی‌شود.
 */

#ifndef CHANGEOVER_H
#define CHANGEOVER_H

/* ==================== Includes ==================== */
#include "app_types.h"

/* ==================== Functions ==================== */

/**
 * @brief  [EN] Start in BOOT with timers inactive, mirror the BSP safe state on
 *              both power-path outputs (protect asserted = battery cut, battery
 *              switch off) and keep the software flags equal to those levels.
 *         [FA] از حالت BOOT با تایمرهای غیرفعال شروع می‌کند، وضعیت امن BSP را روی
 *              هر دو خروجی مسیر توان آینه می‌کند (حفاظت فعال = قطع باتری، کلید
 *              باتری خاموش) و پرچم‌های نرم‌افزار را برابر همان سطح‌ها نگه می‌دارد.
 */
void func__Changeover_Init(void);

/* ==================== Changeover_Evaluate Thresholds / آستانه‌های ارزیابی Changeover ==================== */

/**
 * @brief  [EN] Battery voltage below which the gated cut (with UI alarm) can trigger, in millivolts.
 *         Range 0..40000 mV; effect: v_bat24_mv < 21000 and UI flag true for 3000 ms -> battery cut.
 *         [FA] ولتاژ باتری که پایین‌تر از آن قطع با گیت آلارم UI ممکن است، بر حسب میلی‌ولت.
 */
#define CHANGEOVER_BAT_LOW_ALARM_CUT_MV   21000u

/**
 * @brief  [EN] Battery voltage below which the independent cut triggers regardless of UI flag, in millivolts.
 *         Range 0..40000 mV; effect: v_bat24_mv < 20800 for 3000 ms -> battery cut independent of flag.
 *         [FA] ولتاژ باتری که پایین‌تر از آن قطع مستقل بدون نیاز به فلگ UI رخ می‌دهد، بر حسب میلی‌ولت.
 */
#define CHANGEOVER_BAT_CRITICAL_CUT_MV    20800u

/**
 * @brief  [EN] Battery voltage at or above which a reconnect is allowed, in millivolts.
 *         Range 0..40000 mV; effect: input_present true and v_bat24_mv >= 21200 for 3000 ms -> reconnect.
 *         [FA] ولتاژ باتری که در آن یا بالاتر از آن وصل مجدد مجاز است، بر حسب میلی‌ولت.
 */
#define CHANGEOVER_BAT_RECONNECT_MV       21200u

/**
 * @brief  [EN] Continuous duration required for cut or reconnect, in milliseconds.
 *         Range 1..60000 ms; effect: condition must be continuously true for this duration.
 *         [FA] مدت پیوسته مورد نیاز برای قطع یا وصل مجدد، بر حسب میلی‌ثانیه.
 */
#define CHANGEOVER_DURATION_MS            3000u

/* ==================== Changeover_Evaluate / ارزیابی Changeover ==================== */

/**
 * @brief  [EN] Evaluate next system state from snapshot and faults, drive the
 *              protect output and apply the battery power switch policy for the
 *              evaluated state. Uses rtos_time for 3000ms.
 *         [FA] حالت بعدی سیستم را از نمونه و خطا حساب می‌کند، خروجی حفاظت را
 *              می‌راند و سیاست کلید توان باتری را برای حالت ارزیابی‌شده اعمال
 *              می‌کند. برای 3000ms از rtos_time استفاده می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Snapshot from Measurement, may be NULL / نمونه اندازه‌گیری
 * @param  fault_mask_t__faults [EN] Fault bits from Fault module / بیت‌های خطا
 * @return app_state_t [EN] Next system state / حالت بعدی
 */
app_state_t func__Changeover_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap, fault_mask_t fault_mask_t__faults);

#endif /* CHANGEOVER_H */
