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
 *            Why only 50 ms (= 5 control passes, user directives 2026-09-19):
 *            at peak current the node crosses 14.8 V -> 15.0 V within
 *            milliseconds and the charger's 15.0 V validity cut then kills
 *            the pump, so the over-band can NEVER stay 300 ms - a 300 ms
 *            debounce silently swallowed every peak-current disconnect. 5
 *            consecutive passes is still fast enough to survive the cut
 *            (the pumped node decays above 14.8 V for ~0.5 s) but thick
 *            enough to ignore one-off measurement blips, after the bench
 *            showed spontaneous over-band hops at 30 ms.
 *         2) Input present and in range but no battery wired: the divider
 *            pulls the node to ~0 V, so ALL half voltages below
 *            FAULT_BAT_ABSENT_MV (6 V, user choice - every real 12 V battery
 *            sits far above) for FAULT_BAT_ABSENT_DEBOUNCE_MS proves it.
 *
 *         Recovery (shared): every half back inside
 *         [FAULT_BAT_ABSENT_MV, FAULT_BAT_DISCONNECT_MV] for
 *         FAULT_BAT_RECOVER_MS => clear the bit; the charger then mirrors it
 *         back to OFF and the first pass soft-restarts BULK at 1% duty.
 *
 *         Why "ALL below" for case 2: an installed-but-missing pack reads
 *         ~0 V on both halves, while a bench setup with only one 12 V battery
 *         wired reads ~0 V on the uninstalled half; requiring ALL halves low
 *         keeps the single-battery bench test fault-free.
 *
 *         [FA] تشخیص قطع باتری به‌صورت متمرکز همین‌جاست (دستور کاربر): دو
 *         حالت با یک پرچم خطا پوشش داده می‌شوند - بالای ۱۴٫۸V به‌مدت ۵۰ms
 *         (۵ پاس پشت‌سر؛ امضای پمپ حین شارژ - چون قطع‌سخت ۱۵٫۰V پمپ را در
 *         حد میلی‌ثانیه می‌خواباند، دبانس ۳۰۰ms قدیمی هیچ‌وقت پر نمی‌شد،
 *         و ۳۰ms بر اساس مشاهده بنچ گاهی خودبه‌خود می‌پرید)
 *         یا پایین‌بودن هر دو نیم‌باتری از ۶V به‌مدت
 *         یک ثانیه (با ورودی سالم). بازیابی مشترک: برگشت به پنجره سالم و
 *         پایدارماندن یک ثانیه → پاک‌شدن پرچم و رمپ نرم شارژ از ۱٪.
 * @note   Case 2 is only evaluated while the input is present and inside
 *         [FAULT_INPUT_PRESENT_MIN_MV, FAULT_INPUT_PRESENT_MAX_MV]; with no
 *         input there is nothing to report (system runs on battery or off).
 */
#define FAULT_BAT_DISCONNECT_MV           14800u
#define FAULT_BAT_DISCONNECT_DEBOUNCE_MS     50u
#define FAULT_BAT_ABSENT_MV                6000u
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
