/**
 * @file    app.c
 * @brief   سیم‌کشی ماژول‌ها. در این مرحله فقط UI.
 *
 * چرا این‌قدر خالی است؟
 *   چون عمداً رله و ADC را هنوز صدا نمی‌زنیم.
 *   پایه‌های قدرت اگر در CubeMX Output نشده باشند و این‌جا Write شوند،
 *   رفتار نامشخص است. پس تا خودت نگفتی «تمام شد»، همین فایل فقط LED است.
 */

#include "app.h"
#include "modules_enable.h"
#include "rtos_app.h"

#if MODULE_UI
#include "ui.h"
#endif

void App_Init(void)
{
#if MODULE_UI
    Ui_Init();
    /* فعلاً رویداد 1. برای رویداد 2 بگذار: Ui_SetProfile(UI_PROFILE_EVENT2); */
    Ui_SetProfile(UI_PROFILE_EVENT1);
#endif
}

void App_Start(void)
{
    App_Init();
    Rtos_Start();
}
