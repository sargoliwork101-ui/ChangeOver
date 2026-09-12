#ifndef UI_H
#define UI_H

#include "app_types.h"

void Ui_Init(void);
void Ui_Show(app_state_t state, fault_mask_t faults);
void Ui_Run(void); /* periodic patterns, no blocking delay */

#endif /* UI_H */
