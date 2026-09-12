/**
 * @file ui.h
 *
 * حالت 1 = رویداد 1
 * حالت 2 = رویداد 2
 *
 * بقیه برنامه فقط Ui_SetProfile را صدا می‌زند.
 * الگوی روشن/خاموش این‌جا است، نه در فایل FreeRTOS.
 */

#ifndef UI_H
#define UI_H

typedef enum
{
    UI_PROFILE_OFF = 0,
    UI_PROFILE_EVENT1, /* سبز 500 روشن، 500 خاموش */
    UI_PROFILE_EVENT2  /* سبز 500/1000 ، قرمز 500/500 */
} ui_profile_t;

void Ui_Init(void);
void Ui_SetProfile(ui_profile_t profile);
void Ui_Run(void);

#endif /* UI_H */
