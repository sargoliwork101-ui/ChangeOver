/**
 * @file    task_ui.c
 * @brief   [EN] FreeRTOS task for LED and buzzer.
 *          [FA] تسک FreeRTOS برای LED و بازر.
 *
 * @details
 *   [EN] Runs board test once, then selects a scenario with UI_FLAG.
 *        Pattern code lives in ui.c; this file only starts it.
 *   [FA] اول تست برد، بعد با UI_FLAG سناریو را انتخاب می‌کند.
 *        خود الگو داخل ui.c است.
 */

#include "rtos_tasks.h"
#include "ui.h"

/** 1 = scenario 1, otherwise scenario 2 / ۱ سناریو ۱، غیر از آن سناریو ۲ */
#define UI_FLAG  1u

/**
 * @brief  [EN] UI task entry. Does not return.
 *         [FA] ورود تسک UI. برنمی‌گردد.
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
