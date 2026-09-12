/**
 * @file    changeover.c
 * @brief   [EN] Input vs battery path state machine (placeholder).
 *          [FA] ماشین حالت مسیر ورودی یا باتری (اسکلت).
 *
 * @stage   Placeholder
 */

#include "changeover.h"

static app_state_t s_state = APP_STATE_BOOT;

void Changeover_Init(void)
{
    s_state = APP_STATE_BOOT;
}

app_state_t Changeover_Evaluate(const measurement_snapshot_t *snap, fault_mask_t faults)
{
    (void)snap;

    if (faults != FAULT_NONE)
    {
        s_state = APP_STATE_FAULT;
    }
    else
    {
        s_state = APP_STATE_IDLE;
    }

    return s_state;
}
