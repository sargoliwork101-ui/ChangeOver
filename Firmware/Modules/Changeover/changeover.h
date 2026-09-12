#ifndef CHANGEOVER_H
#define CHANGEOVER_H

#include "app_types.h"

void Changeover_Init(void);
app_state_t Changeover_Evaluate(const measurement_snapshot_t *snap, fault_mask_t faults);

#endif /* CHANGEOVER_H */
