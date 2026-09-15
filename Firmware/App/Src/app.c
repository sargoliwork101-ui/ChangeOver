/**
 * @file    app.c
 * @brief   [EN] Wires modules for the current stage (UI only). Full type naming, func_ prefix.
 *          [FA] سیم‌کشی ماژول‌ها برای مرحله فعلی (فقط UI). نام تایپ کامل.
 */

#include "app.h"
#include "ui.h"
#include "rtos_app.h"

/**
 * @brief  [EN] Initialise the UI outputs to a safe (all off) state.
 *         [FA] خروجی‌های UI را در حالت امن (همه خاموش) می‌گذارد.
 */
void func_App_Init(void)
{
    func_Ui_Init();
}

/**
 * @brief  [EN] Init then enter FreeRTOS. Called from main after MX_GPIO_Init.
 *         [FA] Init و ورود به FreeRTOS. از main بعد از MX_GPIO_Init صدا زده شود.
 */
void func_App_Start(void)
{
    func_App_Init();
    func_Rtos_Start();
}

