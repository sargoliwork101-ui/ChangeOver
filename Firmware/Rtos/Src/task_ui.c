/**
 * @file    task_ui.c
 * @brief   [EN] FreeRTOS UI task - fully RTOS simple & readable, vTaskDelay (does not lock MCU).
 *          Uses new Ui API with __ after type and func__ prefix, ui_config.h deleted.
 *          [FA] تسک UI کاملاً RTOS ساده و خوانا با vTaskDelay.
 *
 * @note    [EN] RTOS simple: vTaskDelay yields, other tasks run, MCU not locked. No HAL_Delay.
 *          Formulas in ui.c are non-linear broken into steps.
 *          [FA] RTOS ساده: vTaskDelay میکرو را قفل نمی‌کند.
 */

#include "rtos_tasks.h"
#include "ui_led.h"
#include "ui_buzzer.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <stdbool.h>

/* ==================== Global Test Inputs ==================== */

/* [EN] Manual test inputs for the UI stage, in mV. Change them live in the
 *      debugger (Live Expressions) without touching the board. 24000 = 24 V,
 *      25000 = 25 V (battery, 21 V=0% .. 28 V=100%).
 *      [FA] ورودی‌های تست دستی مرحلهٔ UI، بر حسب mV. بدون لمس برد، زنده در
 *      دیباگر (Live Expressions) عوض می‌شوند. 24000 = 24V، 25000 = 25V
 *      (باتری، ۲۱V=0٪ تا ۲۸V=100٪). */
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
