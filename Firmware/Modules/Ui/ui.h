#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    UI_PROFILE_EVENT1 = 1,
    UI_PROFILE_EVENT2 = 2
} ui_profile_t;

void Ui_Init(void);
void Ui_SetProfile(ui_profile_t profile);

/* یک خط از الگو را می‌زند و می‌گوید Task چند ms بخوابد. */
uint32_t Ui_Run(void);

#endif /* UI_H */
