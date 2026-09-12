/**
 * @file    task_ui.c
 * @brief   [EN] FreeRTOS task for LED and buzzer.
 *          [FA] تسک FreeRTOS برای LED و بازر.
 */

#include "rtos_tasks.h"
#include "ui.h"

/** 1 = scenario 1, otherwise scenario 2 / ۱ سناریو ۱، غیر از آن سناریو ۲ */
#define UI_FLAG  1u

/**
 * @brief  [EN] UI task entry. Board test once, then one scenario. Does not return.
 *         [FA] ورود تسک UI. یک‌بار تست برد، بعد یک سناریو. برنمی‌گردد.
 * @param  argument  [EN] Required by FreeRTOS, unused.
 *                   [FA] اجباری FreeRTOS، استفاده نمی‌شود.
 */
void TaskUi(void *argument)
{
    (void)argument;

    Ui_BoardTest();

    if (UI_FLAG == 1u)
    {
        Ui_Scenario1();
    }
    else
    {
        Ui_Scenario2();
    }
}
