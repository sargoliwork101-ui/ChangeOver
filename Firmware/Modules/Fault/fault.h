/**
 * @file    fault.h
 * @brief   [EN] Central latched fault bits + battery-lost detection. The
 *               Fault module OWNS battery-lost evaluation (user directive
 *               2026-09-19); the charger only MIRRORS
 *               FAULT_CHARGER_BAT_LOST. Full type naming, func__ prefix.
 *          [FA] بیت‌های خطای قفل‌شدهٔ مرکزی + تشخیص قطع باتری. ارزیابی قطع
 *               باتری در مالکیت این ماژول است (دستور کاربر)؛ شارژر فقط بیت
 *               FAULT_CHARGER_BAT_LOST را آینه می‌کند. نام تایپ کامل.
 */

#ifndef FAULT_H
#define FAULT_H

/* ==================== Includes ==================== */
#include "app_types.h"

/* ==================== Battery-Lost Defines ==================== */
/* [EN] Rule 1 (pump): a charging half above 14.8 V means the battery side
 *      is open (flyback heading for runaway). Threshold stays 14.8 V, NOT
 *      15.0 V: 15.0 would collide with the overshoot fast-down validity cut
 *      (~0.1 s float vs ~0.5 s at 14.8). Debounce is 150 ms = 15 control
 *      passes - armed-absorb false trips still happened at 50 ms, while a
 *      real pump float lasts ~0.5 s, so 150 ms still catches it fast.
 * [FA] قاعدهٔ ۱ (پمپ): نیم‌سل بالای ۱۴٫۸V یعنی باتری جدا شده؛ آستانه ۱۴٫۸V
 *      است نه ۱۵٫۰V (با کات اعتبار اورشوت تداخل می‌کرد). ضدتکانه ۱۵۰ms -
 *      ۵۰ms هنوز خطای کاذب می‌داد و شناور واقعی ~۰٫۵s طول می‌کشد. */
#define FAULT_BAT_DISCONNECT_MV          14800u
#define FAULT_BAT_DISCONNECT_DEBOUNCE_MS   150u
/* [EN] Rule 2 (absent): with a valid 24 V input, EITHER half below
 *      FAULT_BATTERY_BACK_MV (7 V) means the battery is disconnected
 *      (rewritten per user: the old ALL-below-6-V rule stayed silent on a
 *      single cut lead). Debounce 1000 ms.
 * [FA] قاعدهٔ ۲ (غیبت): با ورودی معتبر، «هرکدام» از نیم‌سل‌ها زیر ۷V یعنی
 *      باتری قطع است (بازنویسی به دستور کاربر: ALL-زیر-۶V قدیمی با یک سیم
 *      بریده سکوت می‌کرد). ضدتکانه ۱۰۰۰ms. */
#define FAULT_BATTERY_BACK_MV             7000u
#define FAULT_BAT_ABSENT_DEBOUNCE_MS      1000u
/* [EN] Recovery: BOTH halves at/above 7 V continuously for 1000 ms before
 *      the bit clears (one-burst bug: a brief high frame used to clear the
 *      alarm while a cut lead still held its half low).
 * [FA] بازیابی: «هر دو» نیم‌سل ≥۷V به‌مدت پیوستهٔ ۱۰۰۰ms تا بیت پاک شود
 *      (باگ یک‌ریزش: فریم کوتاه بالا آلارم را می‌بست درحالی‌که سیم بریده
 *      هنوز پایین بود). */
#define FAULT_BAT_RECOVER_MS              1000u
/* [EN] The absent rule only runs while the 24 V input sits inside the sane
 *      window 21..28 V (USER window): below that there is nothing meaningful
 *      to charge, above it the measurement is outside the calibrated range.
 * [FA] قاعدهٔ غیبت فقط وقتی ورودی ۲۴V داخل پنجرهٔ ۲۱..۲۸V است اجرا می‌شود. */
#define FAULT_INPUT_PRESENT_MIN_MV       21000u
#define FAULT_INPUT_PRESENT_MAX_MV       28000u

/* ==================== Functions ==================== */

/**
 * @brief  [EN] Clear all fault bits.
 *         [FA] همه بیت‌های خطا را پاک می‌کند.
 */
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

/* ==================== Fault Evaluate ==================== */

/**
 * @brief  [EN] Central battery-lost evaluation, run once per control pass by
 *              task_control BEFORE func__Fault_Get()/func__Charger_Evaluate:
 *              EITHER rule (pump >14.8 V sustained 150 ms, or either half
 *              <7 V sustained 1000 ms with valid 21..28 V input) latches
 *              FAULT_CHARGER_BAT_LOST; the bit clears only after BOTH halves
 *              hold >=7 V for 1000 ms straight. NULL / invalid snapshots are
 *              ignored.
 *         [FA] ارزیابی مرکزی قطع باتری؛ هر پاس کنترلی توسط task_control
 *              پیش از خواندن ماسک اجرا می‌شود: «هرکدام» از دو قاعده بیت را
 *              قفل می‌کند و پاک‌شدن فقط با ۱۰۰۰ms پایداریِ «هر دو» نیم‌سل
 *              ≥۷V انجام می‌شود. snapshot نامعتبر/NULL نادیده گرفته می‌شود.
 * @param  measurement_snapshot_t__snap [EN] Latest shared snapshot /
 *                                      آخرین snapshot مشترک
 */
void func__Fault_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap);

#endif /* FAULT_H */
