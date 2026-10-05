/**
 * @file    fault.h
 * @brief   [EN] Bit-mask of latched faults (placeholder). Full type naming, func__ prefix.
 *          [FA] بیت‌ماسک خطاهای قفل‌شده (اسکلت). نام تایپ کامل.
 */

#ifndef FAULT_H
#define FAULT_H

/* ==================== Includes ==================== */
#include "app_types.h"

/* ==================== Battery-lost detection constants / ثابت‌های تشخیص قطع باتری ==================== */

/**
 * @brief  [EN] Central battery-lost detection (Charger only mirrors the
 *         bit). Two cases, one shared bit FAULT_CHARGER_BAT_LOST:
 *         1) charging active + battery wire cut: a half-battery above
 *            FAULT_BAT_DISCONNECT_MV for FAULT_BAT_DISCONNECT_DEBOUNCE_MS
 *            (150 ms - the 15.0 V validity cut kills the pump within
 *            milliseconds, so a longer debounce can never fill; the node
 *            still floats ~0.5 s above 14.8 V). v1.21: armed PER HALF -
 *            each half is judged only while ITS OWN charger channel is
 *            pumping (a parked channel cannot fly its half up).
 *         2) input present and in range but EITHER half below
 *            FAULT_BAT_ABSENT_MV (6 V) for FAULT_BAT_ABSENT_DEBOUNCE_MS.
 *         Recovery: BOTH halves >= FAULT_BATTERY_BACK_MV (7 V, not 6 V - a
 *         half can sit near 6 V while charging) and no pump, for
 *         FAULT_BAT_RECOVER_MS => clear the bit.
 *         [FA] تشخیص متمرکز قطع باتری (شارژر فقط آینه است). دو حالت با یک
 *         بیت: (۱) شارژ فعال + سیم قطع: نیم‌باتری بالای ۱۴٫۸V به‌مدت ۱۵۰ms
 *         (قطع ۱۵٫۰V پمپ را در حد میلی‌ثانیه می‌خواباند؛ گره ~۰٫۵s بالای
 *         ۱۴٫۸V شناور می‌ماند)؛ v1.21: مسلح‌شدن به‌ازای هر نیم - هر نیم فقط
 *         وقتی کانال شارژر خودش پمپ می‌کند قضاوت می‌شود (کانال پارک‌شده
 *         نمی‌تواند نیمش را بالا بفرستد)؛ (۲) ورودی سالم ولی هر نیم زیر ۶V
 *         به‌مدت ۱s. بازیابی: هر دو نیم >= ۷V بدون پمپ به‌مدت ۱s ← پاک‌شدن
 *         بیت.
 * @note   [EN] Case 2 runs only with the input inside
 *         [FAULT_INPUT_PRESENT_MIN_MV, FAULT_INPUT_PRESENT_MAX_MV].
 *         [FA] حالت ۲ فقط با ورودی سالم ارزیابی می‌شود.
 */
/* [EN] The macros below are BOOT DEFAULTS only: the live values live in
 *      FAULT_ALARM_T__G__Alarm (fault.c) as panel ids 27..34, flash-persisted
 *      and clamped as a set on every write (func__Fault_ClampAlarms).
 * [FA] ماکروها فقط پیش‌فرض بوت‌اند: مقادیر زنده در
 *      FAULT_ALARM_T__G__Alarm (fault.c) با شناسه‌های ۲۷..۳۴ از پنل، ماندگار
 *      روی فلش و گیره‌شده به‌صورت مجموعه. */
#define FAULT_BAT_DISCONNECT_MV           14800u
#define FAULT_BAT_DISCONNECT_DEBOUNCE_MS    150u
/* [EN] Absence threshold, case 2. Recovery stays at the higher
 *      FAULT_BATTERY_BACK_MV: the 1 V gap is the set/clear hysteresis.
 * [FA] آستانهٔ غیبت (حالت ۲)؛ بازیابی روی ۷V بالاتر می‌ماند: فاصلهٔ ۱V
 *      هیسترزیس جفت ‎Set/Clear‎ است. */
#define FAULT_BAT_ABSENT_MV                6000u
/* [EN] Recovery threshold: >= 7 V on BOTH halves, not 6 V (a half can
 *      sit near 6 V while charging).
 * [FA] آستانهٔ بازیابی: هر دو نیم >= ۷V نه ۶V (حین شارژ ممکن است نزدیک
 *      ۶V بنشیند). */
#define FAULT_BATTERY_BACK_MV              7000u
#define FAULT_BAT_ABSENT_DEBOUNCE_MS      1000u
#define FAULT_BAT_RECOVER_MS              1000u
#define FAULT_INPUT_PRESENT_MIN_MV        21000u
#define FAULT_INPUT_PRESENT_MAX_MV        28000u

/**
 * @brief  [EN] Clear all fault bits.
 *         [FA] همه بیت‌های خطا را پاک می‌کند.
 */
/* ==================== Functions ==================== */
void func__Fault_Init(void);

/**
 * @brief  [EN] Latch bits (OR).
 *         [FA] بیت‌ها را قفل می‌کند (OR).
 * @param  fault_mask_t__bits [EN] Bits to set / بیت‌هایی که باید قفل شود
 */
void func__Fault_Set(fault_mask_t fault_mask_t__bits);

/**
 * @brief  [EN] Clear bits (AND NOT).
 *         [FA] بیت‌ها را پاک می‌کند.
 * @param  fault_mask_t__bits [EN] Bits to clear / بیت‌هایی که باید پاک شود
 */
void func__Fault_Clear(fault_mask_t fault_mask_t__bits);

/**
 * @brief  [EN] Return current mask.
 *         [FA] ماسک فعلی را برمی‌گرداند.
 * @return fault_mask_t [EN] Current mask / ماسک فعلی
 */
fault_mask_t func__Fault_Get(void);

/**
 * @brief  [EN] True if any bit is set.
 *         [FA] اگر هر بیتی روشن باشد true.
 * @return bool [EN] true if any fault / اگر خطایی باشد true
 */
bool func__Fault_Any(void);

/**
 * @brief  [EN] Evaluate battery-lost detection from the latest snapshot
 *         (once per control pass, BEFORE Fault_Get). Only installed halves
 *         participate; the pump rule is armed only while a channel really
 *         pumps. Sets/clears FAULT_CHARGER_BAT_LOST only.
 *         [FA] ارزیابی تشخیص قطع باتری از snapshot تازه (هر پاس کنترل، قبل
 *         از Fault_Get). فقط نیم‌های نصب‌شده شرکت می‌کنند؛ قانون پمپ فقط حین
 *         پمپ واقعی مسلح است. فقط بیت BAT_LOST را ست/پاک می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Snapshot pointer, may be NULL / اشاره‌گر snapshot
 */
void func__Fault_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap);

/* ==================== Runtime alarm thresholds (v1.15) ==================== */
/* [EN] The eight FAULT_* numbers above are runtime panel ids 27..34,
 *      flash-persisted and clamped as a set on every write; the macros
 *      stay boot defaults only.
 * [FA] هشت عدد FAULT_* بالا = شناسه‌های زمان‌اجرای ۲۷..۳۴ از پنل، ماندگار
 *      روی فلش و گیره‌شده به‌صورت مجموعه؛ ماکروها فقط پیش‌فرض بوت‌اند. */
#define FAULT_ALARM_PARAM_DISCONNECT_MV        27u  /* [EN] mV, 14000..15000, >= absorbOver+50, <= ovCutoff-100 / mV */
#define FAULT_ALARM_PARAM_DISCONNECT_DEB_MS    28u  /* [EN] ms, 50..1000 / ms */
#define FAULT_ALARM_PARAM_ABSENT_MV            29u  /* [EN] mV, 3000..8000, < back-500 / mV */
#define FAULT_ALARM_PARAM_BACK_MV              30u  /* [EN] mV, 4000..9000, > absent+500 / mV */
#define FAULT_ALARM_PARAM_ABSENT_DEB_MS        31u  /* [EN] ms, 100..5000 / ms */
#define FAULT_ALARM_PARAM_RECOVER_MS           32u  /* [EN] ms, 100..5000 / ms */
#define FAULT_ALARM_PARAM_INPUT_MIN_MV         33u  /* [EN] mV, 18000..24000, < max-1000 / mV */
#define FAULT_ALARM_PARAM_INPUT_MAX_MV         34u  /* [EN] mV, 24000..30000, > min+1000 / mV */

/**
 * @brief  [EN] Live alarm-threshold set (one struct, like the charger
 *              profile). Evaluate() reads these, never the macros.
 *         [FA] مجموعهٔ زندهٔ آستانه‌های آلارم (یک struct مثل پروفایل
 *              شارژر). Evaluate این‌ها را می‌خواند، نه ماکروها.
 */
typedef struct
{
    uint32_t uint32_t__disconnectMv;
    uint32_t uint32_t__disconnectDebMs;
    uint32_t uint32_t__absentMv;
    uint32_t uint32_t__backMv;
    uint32_t uint32_t__absentDebMs;
    uint32_t uint32_t__recoverMs;
    uint32_t uint32_t__inputMinMv;
    uint32_t uint32_t__inputMaxMv;
} fault_alarm_t;

/**
 * @brief  [EN] Write one alarm threshold (27..34): store, re-clamp the whole
 *              set, report the applied value.
 *         [FA] نوشتن یک آستانهٔ آلارم (۲۷..۳۴): ذخیره، گیرهٔ کل مجموعه،
 *              گزارش مقدار اعمال‌شده.
 * @‎param  uint8_t__paramId [EN] 27..34‎ / شناسه
 * @param  uint32_t__value [EN] Requested value / مقدار درخواستی
 * @param  uint32_t__appliedValue [EN] Applied value out / مقدار اعمال‌شده
 * @‎return bool [EN] true when the id is 27..34‎ / شناسه معتبر بود
 */
bool func__Fault_SetAlarmParam(uint8_t uint8_t__paramId,
                               uint32_t uint32_t__value,
                               uint32_t *uint32_t__appliedValue);

/**
 * @brief  [EN] Read one live alarm threshold (27..34).
 *         [FA] خواندن یک آستانهٔ زندهٔ آلارم (۲۷..۳۴).
 * @‎param  uint8_t__paramId [EN] 27..34‎ / شناسه
 * @param  uint32_t__value [EN] Value out / مقدار
 * @‎return bool [EN] true when the id is 27..34‎ / شناسه معتبر بود
 */
bool func__Fault_GetAlarmParam(uint8_t uint8_t__paramId,
                               uint32_t *uint32_t__value);

/**
 * @brief  [EN] Re-clamp the disconnect threshold after a profile/OV change
 *         (called from Charger_ClampProfile so the pump rule never strands
 *         above the OV cutoff or inside the charge band).
 *         [FA] گیرهٔ دوبارهٔ آستانهٔ قطع بعد از تغییر پروفایل/OV (از
 *              ClampProfile؛ قانون پمپ بالای قطع OV یا داخل باند شارژ گیر
 *              نمی‌کند).
 */
void func__Fault_OnSupervisionChange(void);

#endif /* FAULT_H */
