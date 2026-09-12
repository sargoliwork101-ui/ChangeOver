/**
 * @file    ui.h
 * @brief   LED و بازر. همه الگوی چشمک همین ماژول است.
 *
 * بقیه کد فقط می‌گوید کدام پروفایل؛ نمی‌گوید PB0 را High کن.
 * دلیل: فردا الگوی قرمز را عوض کردی، یک فایل را عوض می‌کنی نه ده جا.
 */

#ifndef UI_H
#define UI_H

typedef enum
{
    UI_PROFILE_OFF = 0,     /* همه خاموش */
    UI_PROFILE_SELFTEST,    /* قرمز، زرد، سبز، بوق — تست سیم‌کشی */
    UI_PROFILE_HEARTBEAT,   /* سبز چشمک: RTOS زنده است */
    UI_PROFILE_FAULT        /* قرمز چشمک — برای بعد، الان صدا زده نمی‌شود */
} ui_profile_t;

void Ui_Init(void);
void Ui_SetProfile(ui_profile_t profile);
void Ui_Run(void);

#endif /* UI_H */
