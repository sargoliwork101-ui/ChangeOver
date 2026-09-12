#ifndef PROTECTION_H
#define PROTECTION_H

#include "app_types.h"

void Protection_Init(void);
void Protection_Run(const measurement_snapshot_t *snap);

#endif /* PROTECTION_H */
