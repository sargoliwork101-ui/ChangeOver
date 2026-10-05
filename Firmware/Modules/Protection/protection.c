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
        /* [EN] ADC invalid = LIVE state, never a latch: at boot the first
           passes run before measurement warm-up; a latched FAULT_ADC would
           stick forever (nothing cleared it) and park Changeover/Charger in
           safe states for the whole power cycle. Clears itself below as
           soon as a valid snapshot arrives.
           [FA] ADC نامعتبر = وضعیت لحظه‌ای، نه قفل: پاس‌های اول بوت قبل از
           ‎warm-up‎ اجرا می‌شوند و FAULT_ADC قفل‌شده تا ابد می‌ماند و
           Changeover/شارژر را در حالت امن قفل می‌کند. به‌محض snapshot
           معتبر در شاخهٔ پایین پاک می‌شود. */
        func__Fault_Set(FAULT_ADC);
        return;
    }

    func__Fault_Clear(FAULT_ADC);

    (void)APP_CONFIG;
}
