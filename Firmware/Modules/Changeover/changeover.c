#include "changeover.h"
#include "actuator.h"

static app_state_t s_state = APP_STATE_BOOT;

void Changeover_Init(void)
{
    s_state = APP_STATE_BOOT;
}

app_state_t Changeover_Evaluate(const measurement_snapshot_t *snap, fault_mask_t faults)
{
    if (faults != FAULT_NONE)
    {
        s_state = APP_STATE_FAULT;
        Actuator_EnterSafeState();
        return s_state;
    }

    (void)snap;
    /* Step 6: observe input vs battery.
     * Hardware already steers the load path analog.
     * Software mainly drives PROTECT_BATT and reports state. */
    s_state = APP_STATE_IDLE;
    return s_state;
}
