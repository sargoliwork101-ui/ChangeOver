/**
 * @file    protection.c
 * @brief   [EN] Over-current and low-battery checks (placeholder).
 *          [FA] بررسی اضافه جریان و باتری ضعیف (اسکلت).
 *
 * @stage   Placeholder
 */

#include "protection.h"
#include "app_config.h"
#include "fault.h"

void Protection_Init(void)
{
}

void Protection_Run(const measurement_snapshot_t *snap)
{
    if ((snap == 0) || (!snap->valid))
    {
        Fault_Set(FAULT_ADC);
        return;
    }

    (void)APP_CONFIG;
}
