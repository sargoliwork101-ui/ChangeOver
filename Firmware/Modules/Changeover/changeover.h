/**
 * @file    changeover.h
 * @brief   [EN] Input vs battery path state machine - battery protect via BSP logical API only.
 *          [FA] ماشین حالت مسیر ورودی یا باتری - فقط با API منطقی BSP.
 *
 * @note    [EN] This module uses ONLY: snapshot.valid, snapshot.v_bat24_mv,
 *              snapshot.input_present, fault_mask, an internal low-battery latch
 *              and (MODULE_IMBALANCE) the func__Imbalance_GetOutputs() veto
 *              flag, plus (MODULE_CHARGER) func__Charger_DeadBlocksOutput()
 *              for the condemned-pack veto - both vetos assert the battery
 *              protect line and sit in SAFE. A recovered pack may reconnect
 *              with no mains after CHANGEOVER_HEALTHY_HOLD_MS of unbroken
 *              healthy voltage, guarded by CHANGEOVER_CYCLE_GUARD_MS.
 *              Time conversion uses rtos_time.h only, tick=1ms
 *              assumption is forbidden.
 *              Only BSP_GPIO_PROTECT_BATTERY is allowed; PB5/PB7 are forbidden.
 *          [FA] این ماژول فقط از valid، v_bat24_mv، input_present، fault_mask،
 *              قفل داخلی باتری کم و (با ماژول عدم‌توازن) پرچم وتوی GetOutputs
 *              به‌علاوهٔ (با ماژول شارژر) وتوی باتری خرابِ DeadBlocksOutput
 *              استفاده می‌کند - هر دو وتو خط محافظ باتری را فعال و به SAFE
 *              می‌روند. باتریِ بازیابی‌شده هم می‌تواند بعد از یک پنجرهٔ سالمِ
 *              پیوسته بدون برق شهر وصل شود، با محافظ چرخه.
 */

#ifndef CHANGEOVER_H
#define CHANGEOVER_H

/* ==================== Includes ==================== */
#include "app_types.h"

/* ==================== Functions ==================== */

/**
 * @brief  [EN] Start protected in BOOT; wait for a valid, non-blocked sample
 *              at or above the existing 21200 mV reconnect threshold.
 *         [FA] در BOOT حفاظت فعال می‌ماند؛ تا نمونهٔ معتبر و بدون خطا با
 *              ولتاژ حداقل ۲۱۲۰۰ میلی‌ولت وصل نمی‌کند.
 */
void func__Changeover_Init(void);

/* ==================== Changeover_Evaluate Thresholds / آستانه‌های ارزیابی Changeover ==================== */

/**
 * @brief  [EN] Battery voltage below which the internal low-battery latch sets, in millivolts.
 *         Range 0..40000 mV; effect: v_bat24_mv < 21000 sets the latch; with the latch set for 3000 ms -> battery cut.
 *         [FA] ولتاژ باتری که پایین‌تر از آن قفل داخلی باتری کم بسته می‌شود، بر حسب میلی‌ولت.
 */
#define CHANGEOVER_BAT_LOW_ALARM_CUT_MV   21000u

/**
 * @brief  [EN] Battery voltage at or above which the internal low-battery latch clears, in millivolts.
 *         Range 0..40000 mV; effect: the latch set below 21000 mV stays set until v_bat24_mv >= 21200.
 *         v1.74: this window used to be UI link parameters 72/73; the user ordered it into
 *         Changeover as a fixed constant because this module is its only consumer and it must
 *         not be settable from the panel.
 *         [FA] ولتاژی که در آن یا بالاتر از آن قفلِ داخلیِ باتری کم باز می‌شود، بر حسب میلی‌ولت.
 *         این پنجره قبلاً پارامترهای ۷۲/۷۳ پنل بود و طبق دستور کاربر به‌صورت ثابت داخل همین
 *         ماژول آمد، چون تنها مصرف‌کننده‌اش همین‌جاست و نباید از پنل تنظیم شود.
 */
#define CHANGEOVER_BAT_LOW_ALARM_CLEAR_MV 21200u

/**
 * @brief  [EN] Battery voltage below which the independent (critical) cut triggers without waiting for the latch, in millivolts.
 *         Range 0..40000 mV; effect: v_bat24_mv < 20800 for 3000 ms -> battery cut.
 *         [FA] ولتاژ باتری که پایین‌تر از آن قطع بحرانی، بدون انتظار برای قفل، رخ می‌دهد، بر حسب میلی‌ولت.
 */
#define CHANGEOVER_BAT_CRITICAL_CUT_MV    20800u

/**
 * @brief  [EN] Battery voltage at or above which a reconnect is allowed, in millivolts.
 *         Range 0..40000 mV; effect: (input_present true with 30 s stable input, or a
 *         healthy hold of CHANGEOVER_HEALTHY_HOLD_MS with mains absent) and
 *         v_bat24_mv >= 21200 for 3000 ms -> reconnect.
 *         [FA] ولتاژ باتری که در آن یا بالاتر از آن وصل مجدد مجاز است، بر حسب میلی‌ولت.
 */
#define CHANGEOVER_BAT_RECONNECT_MV       21200u

/**
 * @brief  [EN] Continuous duration required for cut or reconnect, in milliseconds.
 *         Range 1..60000 ms; effect: condition must be continuously true for this duration.
 *         [FA] مدت پیوسته مورد نیاز برای قطع یا وصل مجدد، بر حسب میلی‌ثانیه.
 */
#define CHANGEOVER_DURATION_MS            3000u

/**
 * @brief  [EN] Extra stable input time before reconnect allowed, in milliseconds.
 *         After a cut, input_present must stay true for this long in addition
 *         to the 3s voltage condition. Prevents immediate re-connect on a
 *         flickering input.
 *         [FA] زمان پایداری اضافه ورودی قبل از وصل مجدد، بر حسب میلی‌ثانیه.
 *         بعد از قطع، ورودی باید این مدت پایدار بماند علاوه بر شرط ۳ ثانیه ولتاژ.
 */
#define CHANGEOVER_INPUT_STABLE_MS        30000u

/**
 * @brief  [EN] Unbroken healthy-voltage time that allows a reconnect with no mains, in milliseconds.
 *         Range 1..600000 ms; effect: v_bat24_mv must stay at or above
 *         CHANGEOVER_BAT_RECONNECT_MV without any dip for this long; then the
 *         normal 3 s reconnect filter runs. One dip restarts the window.
 *         [FA] مدت پیوستهٔ ولتاژ سالم که وصل مجدد بدون برق شهر را مجاز می‌کند، بر حسب میلی‌ثانیه.
 *         ولتاژ باید این مدت بدون هیچ افت در آستانهٔ وصل یا بالاتر بماند؛ بعد
 *         فیلتر ۳ ثانیه‌ای وصل اجرا می‌شود. هر افت پنجره را از صفر شروع می‌کند.
 */
#define CHANGEOVER_HEALTHY_HOLD_MS        10000u

/**
 * @brief  [EN] Cycle-guard window after a mains-free reconnect, in milliseconds.
 *         Range 1..3600000 ms; effect: if the pack cuts again inside this
 *         window, mains-free auto-reconnect is latched off until the mains
 *         returns (a really weak pack must not reconnect every few seconds).
 *         [FA] پنجرهٔ محافظ چرخه بعد از وصل بدون برق شهر، بر حسب میلی‌ثانیه.
 *         اگر باتری داخل این پنجره دوباره قطع شود، وصل خودکار بدون برق شهر
 *         قفل می‌شود تا برگشت برق شهر (باتری واقعاً ضعیف نباید هر چند ثانیه وصل شود).
 */
#define CHANGEOVER_CYCLE_GUARD_MS         60000u

/* ==================== Changeover_Evaluate / ارزیابی Changeover ==================== */

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
