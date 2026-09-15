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

/* ==================== Task Ui ==================== */

void func__TaskUi(void *void_ptr__argument)
{
    (void)void_ptr__argument;

    func__Ui_Init();
    func__Ui_BoardTest_Start();

    for (;;)
    {
        uint32_t uint32_t__inputVoltageMv;
        uint32_t uint32_t__batteryVoltageMv;

        uint32_t__inputVoltageMv = UINT32_T__G__InputVoltageMv;
        uint32_t__batteryVoltageMv = UINT32_T__G__BatteryVoltageMv;

        /* [EN] Simple RTOS tick - readable, 10ms base, MCU not locked
           [FA] تیکه ساده RTOS - خوانا */
        func__Ui_Tick(uint32_t__inputVoltageMv, uint32_t__batteryVoltageMv);

        vTaskDelay(pdMS_TO_TICKS(UI_TICK_MS));
    }
}
