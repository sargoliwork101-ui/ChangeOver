#ifndef FAULT_H
#define FAULT_H

#include "app_types.h"

void Fault_Init(void);
void Fault_Set(fault_mask_t bits);
void Fault_Clear(fault_mask_t bits);
fault_mask_t Fault_Get(void);
bool Fault_Any(void);

#endif /* FAULT_H */
