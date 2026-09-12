#include "protection.h"
#include "app_config.h"
#include "fault.h"
#include "actuator.h"

void Protection_Init(void)
{
}

void Protection_Run(const measurement_snapshot_t *snap)
{
    if ((snap == 0) || (!snap->valid))
    {
        Fault_Set(FAULT_ADC);
        Actuator_EnterSafeState();
        return;
    }

    /* Step 4: compare snap with APP_CONFIG thresholds, set faults, safe-state */
    (void)APP_CONFIG;
}
