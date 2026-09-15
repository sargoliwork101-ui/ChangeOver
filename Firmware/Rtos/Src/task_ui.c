/**
 * @file    task_ui.c
 * @brief   [EN] FreeRTOS UI task - selects LED scenarios from input and battery voltages.
 *          [FA] تسک FreeRTOS رابط کاربر - سناریوهای LED را از ولتاژ ورودی و باتری انتخاب می‌کند.
 *
 * @note    [EN] RTOS simple: vTaskDelay yields, other tasks run, MCU not locked. No HAL_Delay.
 *          [FA] RTOS ساده است؛ vTaskDelay اجازه اجرای تسک‌های دیگر را می‌دهد و HAL_Delay ممنوع است.
 */

#include "rtos_tasks.h"
#include "ui_led.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>

/* ==================== Global Test Inputs ==================== */

volatile uint32_t UINT32_T__G__InputVoltageMv = 24000u;
volatile uint32_t UINT32_T__G__BatteryVoltageMv = 25000u;

/* ==================== Task Ui ==================== */

/**
 * @brief  [EN] Run the UI task and select the scenario from input and battery voltage.
 *         [FA] تسک UI را اجرا می‌کند و سناریو را از ولتاژ ورودی و باتری انتخاب می‌کند.
 */
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

        /* [EN] The UI scenario reads both live test/measurement inputs.
           [FA] سناریوی UI هر دو ورودی زنده تست/اندازه‌گیری را می‌خواند. */
        func__Ui_Tick(uint32_t__inputVoltageMv, uint32_t__batteryVoltageMv);

        vTaskDelay(pdMS_TO_TICKS(UI_TICK_MS));
    }
}
