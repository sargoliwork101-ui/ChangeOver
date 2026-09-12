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
