/**
 * @file    task_ui.c
 * @brief   [EN] FreeRTOS UI task - fully RTOS simple & readable, vTaskDelay (does not lock MCU).
 *          Uses new Ui API with __ after type and func__ prefix, ui_config.h deleted.
 *          [FA] تسک UI کاملاً RTOS ساده و خوانا با vTaskDelay.
 *
 * @note    [EN] RTOS simple: vTaskDelay yields, other tasks run, MCU not locked. No HAL_Delay.
 *          Formulas in ui_led.c are non-linear broken into steps.
 *          [FA] RTOS ساده: vTaskDelay میکرو را قفل نمی‌کند.
 */

#include "rtos_tasks.h"
#include "ui_led.h"
#include "ui_buzzer.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <stdbool.h>

/* ==================== Global Test Inputs ==================== */

volatile uint32_t UINT32_T__G__InputVoltageMv = 24000u;
volatile uint32_t UINT32_T__G__BatteryVoltageMv = 25000u;

/* ==================== Manual Buzzer Test Configuration ==================== */

#define UI_TASK_BUZZER_MANUAL_TEST_ENABLE  1u     /* [EN] 1=test buzzer, 0=normal UI / یک تست بوق، صفر برنامه عادی */
#define UI_TASK_BUZZER_TEST_PERIOD_MS      10000u /* [EN] Test period / دوره تست */
#define UI_TASK_BUZZER_TEST_DUTY_PERCENT   10u    /* [EN] Test duty window / دیوتی تست */
#define UI_TASK_BUZZER_TEST_COUNT          2u     /* [EN] Test beep count / تعداد بوق تست */
#define UI_TASK_BUZZER_TEST_GAP_MS         100u   /* [EN] Test gap / گپ تست */

/* ==================== Task Ui ==================== */

void func__TaskUi(void *void_ptr__argument)
{
    (void)void_ptr__argument;

    func__Ui_Init();

    if (UI_TASK_BUZZER_MANUAL_TEST_ENABLE == 0u)
    {
        func__Ui_BoardTest_Start();
    }
    else
    {
        /* [EN] Keep the manual test free from the one-shot BoardTest beep.
           [FA] تست دستی را از بوق یک‌باره تست برد جدا نگه دار. */
        (void)func__Ui_Buzzer_Tick(0u, 0u, 0u, 0u);
    }

    for (;;)
    {
        uint32_t uint32_t__inputVoltageMv;
        uint32_t uint32_t__batteryVoltageMv;
        uint32_t uint32_t__taskDelayMs;
        int32_t int32_t__nextBuzzerCheckMs;

        uint32_t__inputVoltageMv = UINT32_T__G__InputVoltageMv;
        uint32_t__batteryVoltageMv = UINT32_T__G__BatteryVoltageMv;
        uint32_t__taskDelayMs = UI_TICK_MS;

        if (UI_TASK_BUZZER_MANUAL_TEST_ENABLE == 1u)
        {
            /* [EN] Manual test: do not call func__Ui_Tick() here. Its LED scenarios
               use RTOS delays and would make the buzzer service run too slowly.
               [FA] تست دستی: اینجا func__Ui_Tick() را صدا نزن؛ سناریوهای LED آن
               تأخیر RTOS دارند و باعث می‌شوند سرویس بوق دیر بررسی شود. */
            int32_t__nextBuzzerCheckMs = func__Ui_Buzzer_Tick(
                UI_TASK_BUZZER_TEST_PERIOD_MS,
                UI_TASK_BUZZER_TEST_DUTY_PERCENT,
                UI_TASK_BUZZER_TEST_COUNT,
                UI_TASK_BUZZER_TEST_GAP_MS);

            if (int32_t__nextBuzzerCheckMs > 0)
            {
                uint32_t__taskDelayMs = (uint32_t)int32_t__nextBuzzerCheckMs;
            }
        }
        else
        {
            /* [EN] Original application path. Set the test constant to 0 to use it.
               [FA] مسیر اصلی برنامه؛ برای فعال‌کردن آن ثابت تست را صفر کن. */
            func__Ui_Tick(uint32_t__inputVoltageMv, uint32_t__batteryVoltageMv);
        }

        vTaskDelay(pdMS_TO_TICKS(uint32_t__taskDelayMs));
    }
}
