/**
 * فایل ۲ از ۲ — همان برنامه‌ای که با هم گفتیم.
 *
 * این‌جا FreeRTOS است چون vTaskDelay مال FreeRTOS است.
 * الگوی چشمک هم این‌جا است تا یک فایل را از بالا به پایین بخوانی.
 *
 * mode = 1  رویداد 1
 * mode = 2  رویداد 2
 */

#include "rtos_tasks.h"
#include "ui.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>

static uint32_t mode = 1u;

void TaskUi(void *argument)
{
    (void)argument;

    for (;;)
    {
        if (mode == 1u)
        {
            /* رویداد 1: سبز هر 500 ms چشمک. قرمز خاموش. */
            green(true);
            red(false);
            vTaskDelay(pdMS_TO_TICKS(500u));

            green(false);
            red(false);
            vTaskDelay(pdMS_TO_TICKS(500u));
        }
        else
        {
            /* رویداد 2: سبز 500 روشن / 1000 خاموش ، قرمز هر 500 چشمک. */
            green(true);
            red(true);
            vTaskDelay(pdMS_TO_TICKS(500u));

            green(false);
            red(false);
            vTaskDelay(pdMS_TO_TICKS(500u));

            green(false);
            red(true);
            vTaskDelay(pdMS_TO_TICKS(500u));
        }
    }
}
