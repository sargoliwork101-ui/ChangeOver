/**
 * @file    task_ui.c
 * @brief   [EN] FreeRTOS task for LED/buzzer status indication.
 *          [FA] تسک FreeRTOS برای نمایش وضعیت با LED و بازر.
 */

#include "rtos_tasks.h"
#include "ui.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <stdbool.h>

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
 * @brief  [EN] UI task entry. One board test, then periodic indication steps. Never returns.
 *         [FA] ورود تسک UI. یک‌بار تست برد، بعد گام‌های تناوبی نمایش. برنمی‌گردد.
 * @param  argument  [EN] Required by FreeRTOS, unused.
 *                   [FA] اجباری FreeRTOS، استفاده نمی‌شود.
 */
void TaskUi(void *argument)
{
    (void)argument;

    Ui_Init();
    Ui_BoardTest();

    for (;;)
    {
        bool input_present;

        input_present = (ui_test_input_present != 0u);
        Ui_Indicate(input_present, ui_test_battery_percent);

        /* Short fixed period keeps the blink edges accurate without blocking
           other tasks. / دوره کوتاه ثابت تا لبه‌های چشمک دقیق بمانند و تسک‌های
           دیگر هم آزاد باشند. */
        vTaskDelay(pdMS_TO_TICKS(APP_CONFIG.ui_task_period_ms));
    }
}
