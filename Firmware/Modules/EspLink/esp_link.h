#ifndef ESP_LINK_H
#define ESP_LINK_H

#include "app_types.h"

void EspLink_Init(void);
void EspLink_Run(const measurement_snapshot_t *snap, app_state_t state, fault_mask_t faults);
void EspLink_Power(bool on);

#endif /* ESP_LINK_H */
