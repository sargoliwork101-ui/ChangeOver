/**
 * @file    changeover.h
 * @brief   [EN] Input vs battery path state machine - battery protect via BSP logical API only.
 *          [FA] ماشین حالت مسیر ورودی یا باتری - فقط با API منطقی BSP.
 *
 * @note    [EN] This module uses ONLY: snapshot.valid, snapshot.v_bat24_mv,
 *              snapshot.input_present, fault_mask, BOOL__G__UiBatteryAlarmIssued.
 *              Time conversion uses rtos_time.h only, tick=1ms assumption is forbidden.
 *              Only BSP_GPIO_PROTECT_BATTERY is allowed; PB5/PB7 are forbidden.
 *          [FA] این ماژول فقط از valid، v_bat24_mv، input_present، fault_mask و فلگ UI استفاده می‌کند.
 */

#ifndef CHANGEOVER_H
#define CHANGEOVER_H

/* ==================== Includes ==================== */
#include "app_types.h"

/* ==================== Changeover Thresholds / آستانه‌های Changeover ==================== */

/**
 * @brief  [EN] Battery voltage below which the gated cut (with UI alarm) can trigger, in mV.
 *         [FA] ولتاژی که پایین‌تر از آن قطع با گیت آلارم UI ممکن است.
 */
#define CHANGEOVER_BAT_LOW_ALARM_CUT_MV   21000u

/**
 * @brief  [EN] Battery voltage below which the independent cut triggers regardless of UI flag, in mV.
 *         [FA] ولتاژی که پایین‌تر از آن قطع مستقل بدون نیاز به فلگ UI رخ می‌دهد.
 */
#define CHANGEOVER_BAT_CRITICAL_CUT_MV    20800u

/**
 * @brief  [EN] Battery voltage at or above which a reconnect is allowed, in mV.
 *         [FA] ولتاژی که در آن یا بالاتر از آن وصل مجدد مجاز است.
 */
#define CHANGEOVER_BAT_RECONNECT_MV       21200u

/**
 * @brief  [EN] Continuous duration required for cut or reconnect, in milliseconds.
 *         [FA] مدت پیوسته مورد نیاز برای قطع یا وصل مجدد، بر حسب میلی‌ثانیه.
 */
#define CHANGEOVER_DURATION_MS            3000u

/* ==================== Functions ==================== */

/**
 * @brief  [EN] Start in BOOT, timers inactive, protect deasserted (safe).
 *         [FA] از حالت BOOT شروع می‌کند.
 */
void func__Changeover_Init(void);

/**
 * @brief  [EN] Evaluate next system state from snapshot and faults and drive
 *              BSP_GPIO_PROTECT_BATTERY only. Uses rtos_time for 3000ms.
 *         [FA] حالت بعدی سیستم را از نمونه و خطا حساب و فقط پایه منطقی باتری را می‌زند.
 * @param  measurement_snapshot_t__snap [EN] Snapshot from Measurement, may be NULL / نمونه اندازه‌گیری
 * @param  fault_mask_t__faults [EN] Fault bits from Fault module / بیت‌های خطا
 * @return app_state_t [EN] Next system state / حالت بعدی
 */
app_state_t func__Changeover_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap, fault_mask_t fault_mask_t__faults);

#endif /* CHANGEOVER_H */
