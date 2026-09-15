/**
 * @file    protection.c
 * @brief   [EN] Over-current and low-battery checks (placeholder). Full type naming, func_ prefix.
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
void func_Protection_Init(void)
{
}

/**
 * @brief  [EN] Compare snapshot against limits; latch faults.
 *         [FA] نمونه را با حد مقایسه می‌کند و خطا را قفل می‌کند.
 * @param  measurement_snapshot_t_snap [EN] Snapshot pointer, may be NULL, valid flag checked / اشاره‌گر نمونه
 */
void func_Protection_Run(const measurement_snapshot_t *measurement_snapshot_t_snap)
{
    if ((measurement_snapshot_t_snap == NULL) || (measurement_snapshot_t_snap->valid == false))
    {
        func_Fault_Set(FAULT_ADC);
        return;
    }

    (void)APP_CONFIG;
}
