/**
 * @file ui.h
 * @brief LED و بازر — فقط همین ماژول الگوی چشمک را می‌داند.
 *
 * Task فقط هر 100 ms صدای Ui_Run را می‌زند.
 * vTaskDelay این‌جا نیست؛ مال فایل FreeRTOS است.
 */

#ifndef UI_H
#define UI_H

typedef enum
{
    UI_PROFILE_OFF = 0,
    UI_PROFILE_SELFTEST,  /* بعد از روشن شدن: تست هر رنگ و یک بوق */
    UI_PROFILE_EVENT1,    /* سبز 500 روشن / 500 خاموش */
    UI_PROFILE_EVENT2     /* سبز 500 روشن / 1000 خاموش ، قرمز هر 500 چشمک */
} ui_profile_t;

void Ui_Init(void);
void Ui_SetProfile(ui_profile_t profile);
void Ui_Run(void);

#endif /* UI_H */
