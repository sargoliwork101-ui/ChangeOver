/**
 * @file    protection.c
 * @brief   [EN] Over-current and low-battery checks (placeholder). Full type naming, func__ prefix.
 *          [FA] بررسی اضافه جریان و باتری ضعیف (اسکلت). نام تایپ کامل.
 */

#include "protection.h"
#include "app_config.h"
#include "fault.h"

#include <stddef.h>

/**
 * @brief  [EN] Init protection state.
 *         [FA] حالت حفاظت را Init می‌کند.
 */
/* ==================== Protection_Init ==================== */

void func__Protection_Init(void)
{
}

/**
 * @brief  [EN] Compare snapshot against limits; latch faults.
 *         [FA] نمونه را با حد مقایسه می‌کند و خطا را قفل می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Snapshot pointer, may be NULL, valid flag checked / اشاره‌گر نمونه
 */
/* ==================== Protection_Run ==================== */

void func__Protection_Run(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    if ((measurement_snapshot_t__snap == NULL) || (measurement_snapshot_t__snap->valid == false))
    {
        /* [EN] ADC not valid right now: report it, but as a LIVE state, not a
           permanent latch (full-program audit 2026-09-22). At boot the first
           protection passes run before the measurement warm-up completes; a
           latched FAULT_ADC would then stick forever (nothing cleared it),
           Changeover would sit in APP_STATE_FAULT and the charger would stay
           in safe-idle for the rest of the power cycle. The bit clears by
           itself in the branch below as soon as a valid snapshot arrives.
           [FA] ADC الان معتبر نیست: به‌صورت وضعیت لحظه‌ای گزارش شود، نه
           قفل دائمی (ممیزی کل برنامه ۲۰۲۶-۰۹-۲۲). در بوت، پاس‌های اول
           حفاظت قبل از تکمیل warm-up اندازه‌گیری اجرا می‌شوند؛ FAULT_ADC
           قفل‌شده تا ابد می‌ماند (هیچ‌جا پاک نمی‌شد)، Changeover در
           APP_STATE_FAULT می‌ماند و شارژر تا پایان سیکل تغذیه safe-idle
           می‌ماند. بیت در شاخهٔ پایین به‌محض رسیدن snapshot معتبر پاک
           می‌شود. */
        func__Fault_Set(FAULT_ADC);
        return;
    }

    func__Fault_Clear(FAULT_ADC);

    (void)APP_CONFIG;
}
