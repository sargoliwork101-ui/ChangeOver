#include "charger.h"
#include "app_config.h"
#include "actuator.h"

void Charger_Init(void)
{
    Actuator_RequestRelay(false);
    Actuator_RequestPwm1Permille(0u);
    Actuator_RequestPwm2Permille(0u);
}

void Charger_Evaluate(const measurement_snapshot_t *snap, app_state_t state)
{
    (void)snap;
    (void)state;
    (void)APP_CONFIG;
    /* Step 8: CC/CV later. Until then keep relay off and duty 0. */
}
