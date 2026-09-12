/**
 * @file    protection.c
 * @brief   [EN] Over-current and low-battery checks (placeholder).
 *          [FA] بررسی اضافه جریان و باتری ضعیف (اسکلت).
 */

#include "protection.h"
#include "app_config.h"
#include "fault.h"

#include <stddef.h>

/**
 * @brief  [EN] Init protection state.
 *         [FA] حالت حفاظت را Init می‌کند.
 */
void Protection_Init(void)
{
}

/**
 * @brief  [EN] Compare snapshot against limits; latch faults.
 *         [FA] نمونه را با حد مقایسه می‌کند و خطا را قفل می‌کند.
 */
void Protection_Run(const measurement_snapshot_t *snap)
{
    if ((snap == NULL) || (snap->valid == false))
    {
        Fault_Set(FAULT_ADC);
        return;
    }

    (void)APP_CONFIG;
}
