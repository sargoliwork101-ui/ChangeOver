/**
 * @file    app.c
 * @brief   [EN] Wires modules for the current stage (UI only).
 *          [FA] سیم‌کشی ماژول‌ها برای مرحله فعلی (فقط UI).
 */

#include "app.h"
#include "ui.h"
#include "rtos_app.h"

/**
 * @brief  [EN] Initialise the UI outputs to a safe (all off) state.
 *         [FA] خروجی‌های UI را در حالت امن (همه خاموش) می‌گذارد.
 */
void App_Init(void)
{
    Ui_Init();
}

/**
 * @brief  [EN] Init then enter FreeRTOS. Called from main after MX_GPIO_Init.
 *         [FA] Init و ورود به FreeRTOS. از main بعد از MX_GPIO_Init صدا زده شود.
 */
void App_Start(void)
{
    App_Init();
    Rtos_Start();
}
