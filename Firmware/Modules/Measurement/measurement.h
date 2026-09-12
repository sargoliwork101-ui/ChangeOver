#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include "app_types.h"

void Measurement_Init(void);
void Measurement_Run(void);
bool Measurement_GetSnapshot(measurement_snapshot_t *out);

#endif /* MEASUREMENT_H */
