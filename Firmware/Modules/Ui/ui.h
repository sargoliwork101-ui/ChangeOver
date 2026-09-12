#ifndef UI_H
#define UI_H

typedef enum
{
    UI_PROFILE_EVENT1 = 1,
    UI_PROFILE_EVENT2 = 2
} ui_profile_t;

void Ui_Init(void);
void Ui_SetProfile(ui_profile_t profile);
void Ui_Run(void);

#endif /* UI_H */
