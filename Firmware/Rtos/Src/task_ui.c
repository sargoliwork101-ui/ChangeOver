/**
 * @file    task_ui.c
 * @brief   Task چشمک. معادل loop() آردوینو، ولی فقط برای UI.
 *
 * FreeRTOS این تابع را برای همیشه صدا می‌زند.
 * الگوی for(;;) به‌جای while(1): در MISRA حلقه بی‌نهایت باید واضح باشد.
 * خروج از Task در این طراحی وجود ندارد.
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#if MODULE_UI
#include "ui.h"
#endif

void TaskUi(void *argument)
{
    uint32_t delay_ms;

    /* پارامتر FreeRTOS را استفاده نمی‌کنیم؛ صریحاً دور می‌ریزیم. */
    (void)argument;

    for (;;)
    {
#if MODULE_UI
        Ui_Run();
#endif

        delay_ms = APP_CONFIG.ui_period_ms;
        if (delay_ms == 0u)
        {
            delay_ms = 1u;
        }

        /* این‌جا صبر کن. CPU را به Idle Task بده. HAL_Delay نگذار. */
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}
