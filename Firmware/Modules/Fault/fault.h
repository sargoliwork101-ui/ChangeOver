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
 * @brief  [EN] Battery-lost detection lives HERE centrally (user directive
 *         2026-09-19): the Fault module evaluates the measurement snapshot
 *         every control pass, latches FAULT_CHARGER_BAT_LOST on both battery-
 *         lost cases, and clears it after the settle time. Modules (Charger)
 *         only mirror the bit; no charger-side timers or thresholds.
 *
 *         Two cases, one shared fault bit:
 *         1) Charging active + battery wire cut: the flyback pulses pump the
 *            470 uF output cap above any real 12 V battery, so a half-battery
 *            voltage (v_bat_low / v_bat_high) above FAULT_BAT_DISCONNECT_MV
 *            for FAULT_BAT_DISCONNECT_DEBOUNCE_MS proves "battery gone".
 *            150 ms now (= 15 control passes, user directives
 *            2026-09-19): at peak current the node crosses 14.8 V -> 15.0 V
 *            within milliseconds and the charger's 15.0 V validity cut then
 *            kills the pump, so the over-band can NEVER stay 300 ms - a
 *            300 ms debounce silently swallowed every peak-current
 *            disconnect; and the pumped node floats ~0.5 s above 14.8 V, so
 *            150 ms still latches the real thing. False buzzers came from
 *            ADC spike bursts on the switching node crossing 14.8 V; the
 *            14.8 V threshold stays (15.0 V would collide with the validity
 *            cut - only ~0.1 s of float above 15.0 V), the voltage median
 *            prefilter went median-3 -> median-5 (2-frame bursts die too),
 *            and with real spikes mostly dead the debounce sits at 150 ms.
 *         2) Input present and in range but EITHER half below
 *            FAULT_BATTERY_BACK_MV (7 V - user rewrite 2026-09-19: "ALL
 *            halves absent" could never fire with a single lead cut since
 *            the other half stays ~13 V, and a parked/idle charger makes no
 *            pump signature either, so a cut battery went completely
 *            silent) for FAULT_BAT_ABSENT_DEBOUNCE_MS proves the wire is
 *            gone. 7 V is far above any real charge-time dip, so a deeply
 *            discharged-but-connected battery never trips it.
 *            symmetric with the recovery threshold on purpose.
 *
 *         Recovery (shared): EVERY half back inside
 *         [FAULT_BATTERY_BACK_MV, FAULT_BAT_DISCONNECT_MV] (>= 7 V on BOTH
 *         halves per user directive - 6 V could still mean "charging";
 *         2026-09-19: with one lead cut the other half still sits
 *         at ~13 V; the old "not all absent" test then cleared after ~1 s,
 *         the alarm died after a single burst, and the charger's 15 s
 *         connection-settle no longer re-pumps to re-latch it, so the cut
 *         cable went silent forever) for
 *         FAULT_BAT_RECOVER_MS => clear the bit; the charger then mirrors it
 *         back to OFF and the first pass soft-restarts BULK at 1% duty.
 *
 *         Why "ALL below" for case 2: an installed-but-missing pack reads
 *         ~0 V on both halves, while a bench setup with only one 12 V battery
 *         wired reads ~0 V on the uninstalled half; requiring ALL halves low
 *         keeps the single-battery bench test fault-free.
 *
 *         [FA] تشخیص قطع باتری به‌صورت متمرکز همین‌جاست (دستور کاربر): دو
 *         حالت با یک پرچم خطا پوشش داده می‌شوند - بالای ۱۴٫۸V به‌مدت ۱۵۰ms
 *         (۱۵ پاس پشت‌سر؛ امضای پمپ حین شارژ - قطع‌سخت ۱۵٫۰V پمپ را در حد
 *         میلی‌ثانیه می‌خواباند پس دبانس ۳۰۰ms قدیمی هیچ‌وقت پر نمی‌شد و
 *         گره ~۰٫۵s بالای ۱۴٫۸V شناور می‌ماند پس ۱۵۰ms هم می‌رسد؛ بوق فیک‌ها
 *         ناشی از برست اسپایک ADC بودند: مدین ولتاژ در measurement.c
 *         سه‌تایی → پنج‌تایی شد و آستانه ۱۴٫۸V عمداً ماند چون ۱۵٫۰V با قطع
 *         اعتبار تداخل دارد)
 *         یا پایین‌بودن «هرکدام» از نیم‌باتری‌ها زیر ۷V
 *         (`FAULT_BATTERY_BACK_MV`؛ بازنویسی دستور کاربر: «هر دو غایب» با یک
 *         سیمِ قطع هرگز فایر نمی‌شد چون نیمِ دیگر ~۱۳V است و شارژر پارک‌شده
 *         هم امضای پمپ ندارد - سکوت کامل!) به‌مدت یک ثانیه (با ورودی سالم).
 *         بازیابی مشترک: برگشت هردو نیم به بالای ۷V بدون پمپ، به‌مدت یک
 *         ثانیه → پاک‌شدن پرچم و رمپ نرم شارژ از ۱٪ (پس از گیت ۱۵ ثانیه‌ای
 *         ثبات اتصال).
 * @note   Case 2 is only evaluated while the input is present and inside
 *         [FAULT_INPUT_PRESENT_MIN_MV, FAULT_INPUT_PRESENT_MAX_MV]; with no
 *         input there is nothing to report (system runs on battery or off).
 */
#define FAULT_BAT_DISCONNECT_MV           14800u
#define FAULT_BAT_DISCONNECT_DEBOUNCE_MS    150u
/* [EN] FAULT_BAT_ABSENT_MV (6 V) was REMOVED 2026-09-19: rule 2 and the
 *      recovery window both use FAULT_BATTERY_BACK_MV now (one concept, one
 *      constant). Battery-TRULY-back threshold for the recovery window (user directive
 *      2026-09-19): >= 7 V on BOTH halves, NOT 6 V, because a half can sit
 *      near 6 V while CHARGING; 7 V keeps the boundary safely above any
 *      charge-time dip so the flag clears only on a genuinely reconnected
 *      battery.
 * [FA] آستانهٔ «باتری واقعاً برگشته» برای پاکسازی: روی هر دو نیم‌باتری
 *      >= ۷V باشد نه ۶V، چون نیم‌باتری حین شارژ ممکن است نزدیک ۶V بنشیند؛
 *      با ۷V مرز بالاتر از هر افتِ حین شارژ امن است. */
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
 * @brief  [EN] Evaluate battery-lost detection from the latest measurement
 *         snapshot. Called once per control pass from task_control, BEFORE
 *         func__Fault_Get(), so the pass consumes the fresh mask. Sets and
 *         clears FAULT_CHARGER_BAT_LOST; all other bits stay untouched.
 *         [FA] تشخیص قطع باتری را از آخرین snapshot ارزیابی می‌کند؛ هر پاس
 *         کنترل قبل از Get صدا زده می‌شود تا ماسک تازه مصرف شود.
 * @param  measurement_snapshot_t__snap [EN] Snapshot pointer, may be NULL / اشاره‌گر snapshot
 */
void func__Fault_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap);

#endif /* FAULT_H */
