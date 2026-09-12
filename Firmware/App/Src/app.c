#include "app.h"
#include "ui.h"
#include "rtos_app.h"

void App_Init(void)
{
    Ui_Init();
    Ui_SetProfile(UI_PROFILE_EVENT1);
}

void App_Start(void)
{
    App_Init();
    Rtos_Start();
}
