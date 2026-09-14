/**
 * @file    task_ui.c
 * @brief   [EN] FreeRTOS task that runs one LED/buzzer scenario at a time.
 *          [FA] تسک FreeRTOS که هر بار یک سناریوی LED/بازر را اجرا می‌کند.
 */

#include "rtos_tasks.h"
#include "ui.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <stdbool.h>

#define UI_PERCENT_FULL   100u  /* [EN] 100 % charge / باتری فول */

/*
 * Manual test inputs until the Measurement (ADC) module exists.
 * Change them live in the debugger "Live Expressions" window — no rebuild or
 * re-flash is needed. Later the task will feed real measurement values here.
 *
 * ورودی‌های دستی تست تا قبل از راه‌اندازی ماژول Measurement (ADC).
 * در دیباگر از پنجره Live Expressions زنده عوض شوند؛ بدون Build/فلش دوباره.
 * بعداً همین‌جا با مقادیر واقعی اندازه‌گیری پر می‌شود.
 */
volatile uint8_t ui_test_battery_percent = 100u;  /* [EN] 0..100 charge / درصد شارژ */
volatile uint8_t ui_test_input_present   = 1u;    /* [EN] 1 = mains on, 0 = lost / ورودی وصل یا قطع */

/**
 * @brief  [EN] UI task entry. One board test, then one scenario cycle at a time
 *         so the manual inputs can change the scenario between cycles. Never returns.
 *         [FA] ورود تسک UI. یک‌بار تست برد، بعد در هر نوبت یک سیکل سناریو تا ورودی
 *         دستی بتواند بین سیکل‌ها سناریو را عوض کند. برنمی‌گردد.
 * @note   [EN] Ui_Init() already ran once from App_Init() before the scheduler.
 *         [FA] Ui_Init یک‌بار قبل از زمان‌بند در App_Init اجرا شده است.
 * @param  argument  [EN] Required by FreeRTOS, unused.
 *                   [FA] اجباری FreeRTOS، استفاده نمی‌شود.
 */
void TaskUi(void *argument)
{
    (void)argument;

    Ui_BoardTest();

    for (;;)
    {
        uint8_t pct;
        bool input_present;

        pct = ui_test_battery_percent;
        if (pct > UI_PERCENT_FULL)
        {
            pct = UI_PERCENT_FULL;
        }
        input_present = (ui_test_input_present != 0u);

        if (input_present == true)
        {
            Ui_ScenarioInputOk();
        }
        else if (pct <= APP_CONFIG.ui_low_battery_percent)
        {
            Ui_ScenarioBatteryLow();
        }
        else
        {
            Ui_ScenarioBatteryRun(pct);
        }
    }
}
