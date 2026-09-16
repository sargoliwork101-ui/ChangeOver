/**
 * @file    changeover.h
 * @brief   [EN] Input vs battery path state machine with battery protection.
 *          [FA] ماشین حالت مسیر ورودی و باتری با حفاظت باتری.
 */

#ifndef CHANGEOVER_H
#define CHANGEOVER_H

/* ==================== Includes / شامل‌ها ==================== */
#include "app_types.h"

/* ==================== Changeover functions / توابع Changeover ==================== */

/**
 * @brief  [EN] Initialize the Changeover state without driving a product pin.
 *         [FA] حالت Changeover را بدون تحریک پایهٔ محصول مقداردهی می‌کند.
 */
void func__Changeover_Init(void);

/**
 * @brief  [EN] Public battery-protection thresholds and persistence interval.
 *         [FA] آستانه‌های عمومی حفاظت باتری و بازهٔ پایدار ماندن شرط.
 *
 * [EN] The UI-assisted threshold is combined with the public UI alarm flag;
 *      the hard threshold is independent of UI. All thresholds are mV and the
 *      transition interval is milliseconds. These constants affect the
 *      decision made by func__Changeover_Evaluate.
 * [FA] آستانهٔ وابسته به UI با فلگ عمومی آلارم UI ترکیب می‌شود؛ آستانهٔ سخت
 *      مستقل از UI است. همهٔ آستانه‌ها بر حسب mV و زمان بر حسب میلی‌ثانیه‌اند.
 *      این ثابت‌ها روی تصمیم func__Changeover_Evaluate اثر می‌گذارند.
 */
#define CHANGEOVER_BATTERY_CUT_WITH_UI_THRESHOLD_MV 21000u
#define CHANGEOVER_BATTERY_CUT_HARD_THRESHOLD_MV     20800u
#define CHANGEOVER_BATTERY_RECONNECT_THRESHOLD_MV   21200u
#define CHANGEOVER_TRANSITION_PERSISTENCE_MS         3000u

/**
 * @brief  [EN] Evaluate one valid snapshot and update the logical battery path.
 *         [FA] یک snapshot معتبر را ارزیابی و مسیر منطقی باتری را به‌روز می‌کند.
 *
 * [EN] Invalid snapshots cause no decision and preserve state and pin. A
 *      non-zero fault mask enters FAULT without changing the pin. Battery
 *      cut/reconnect conditions must remain continuously true for the public
 *      persistence interval.
 * [FA] snapshot نامعتبر هیچ تصمیمی ایجاد نمی‌کند و state و پایه حفظ می‌شوند.
 *      ماسک خطای غیرصفر به FAULT می‌رود و پایه را تغییر نمی‌دهد. شرط قطع یا
 *      وصل مجدد باید به‌صورت پیوسته به‌اندازهٔ بازهٔ عمومی برقرار باشد.
 *
 * @param  measurement_snapshot_t__snap [EN] Measurement snapshot / snapshot اندازه‌گیری
 * @param  fault_mask_t__faults [EN] Current fault bits / بیت‌های خطای فعلی
 * @param  bool__uiBatteryAlarmIssued [EN] Valid UI low-battery alarm level / سطح معتبر آلارم باتری کم UI
 * @return app_state_t [EN] Current Changeover state / حالت فعلی Changeover
 */
app_state_t func__Changeover_Evaluate(
    const measurement_snapshot_t *measurement_snapshot_t__snap,
    fault_mask_t fault_mask_t__faults,
    bool bool__uiBatteryAlarmIssued);

#endif /* CHANGEOVER_H */
