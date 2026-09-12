#ifndef CHARGER_H
#define CHARGER_H

#include "app_types.h"

void Charger_Init(void);
void Charger_Evaluate(const measurement_snapshot_t *snap, app_state_t state);

#endif /* CHARGER_H */
