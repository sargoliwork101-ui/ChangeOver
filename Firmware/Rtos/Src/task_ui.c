/**
 * @file    task_ui.c
 * @brief   [EN] FreeRTOS UI task - fully RTOS, no delay inside modules, non-blocking tick every 10ms via vTaskDelayUntil.
 *          Uses new Ui API with __ after type and func__ prefix, ui_config.h deleted (now in ui.h).
 *          [FA] تسک UI کاملاً RTOS بدون delay داخل ماژول، تیکه ۱۰ms با vTaskDelayUntil.
 *
 * @note    [EN] Naming: type__name, TYPE__G__Name, func__ prefix. No HAL_Delay, no vTaskDelay inside ui.c.
 *          Long tasks broken into 10ms chunks.
 *          [FA] نام‌گذاری با __ و بدون delay.
 */

#include "rtos_tasks.h"
#include "ui.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <stdbool.h>

/* ==================== Global Test Inputs (volatile for Live Expressions) ==================== */

volatile uint32_t UINT32_T__G__InputVoltageMv = 24000u;   /* [EN] Input voltage mV / ولتاژ ورودی */
volatile uint32_t UINT32_T__G__BatteryVoltageMv = 25000u; /* [EN] Battery voltage mV / ولتاژ باتری */

/**
 * @brief  [EN] UI task entry - fully RTOS non-blocking.
 *         Board test runs non-blocking, then Ui tick every 10ms.
 *         [FA] ورود تسک UI - کاملاً RTOS غیربلوکه.
 * @param  void_ptr__argument [EN] FreeRTOS arg unused / آرگومان
 */
/* ==================== TaskUi ==================== */

void func__TaskUi(void *void_ptr__argument)
{
    TickType_t ticktype__lastWakeTick;
    const TickType_t ticktype__tickPeriod = pdMS_TO_TICKS(UI_TICK_MS);
    bool bool__boardTestDone;

    (void)void_ptr__argument;

    func__Ui_Init();
    func__Ui_BoardTest_Start();
    bool__boardTestDone = false;
    ticktype__lastWakeTick = xTaskGetTickCount();

    for (;;)
    {
        uint32_t uint32_t__inputVoltageMv;
        uint32_t uint32_t__batteryVoltageMv;

        /* [EN] Non-blocking tick every 10ms via vTaskDelayUntil (does not lock MCU)
           [FA] تیکه ۱۰ms با vTaskDelayUntil، میکرو قفل نمی‌شود */
        vTaskDelayUntil(&ticktype__lastWakeTick, ticktype__tickPeriod);

        if (bool__boardTestDone == false)
        {
            bool__boardTestDone = (func__Ui_BoardTest_Tick() == false);
            if (bool__boardTestDone == false)
            {
                continue;
            }
        }

        /* [EN] Copy volatile globals to locals - meaningful names related to work
           [FA] کپی گلوبال به لوکال با نام مرتبط */
        uint32_t__inputVoltageMv = UINT32_T__G__InputVoltageMv;
        uint32_t__batteryVoltageMv = UINT32_T__G__BatteryVoltageMv;

        /* [EN] Ui main tick - non-blocking, decides scenario, no delay inside
           [FA] تیکه اصلی UI - بدون delay */
        func__Ui_Tick(uint32_t__inputVoltageMv, uint32_t__batteryVoltageMv);
    }
}