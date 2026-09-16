/**
 * @file    task_ui.c
 * @brief   [EN] CMSIS-RTOS2 UI thread - selects LED scenarios from measured input voltage and a manual battery test voltage.
 *          [FA] تسک CMSIS-RTOS2 رابط کاربر - سناریوهای LED را از ولتاژ ورودی اندازه‌گیری‌شده و ولتاژ تست دستی باتری انتخاب می‌کند.
 *
 * @note    [EN] Input voltage comes from the Measurement module when its first frame is valid. Battery voltage remains a Live Expressions test input until its measurement stage is approved.
 *          CMSIS-RTOS2 simple: osDelay yields, other tasks run, MCU not locked. No HAL_Delay.
 *          [FA] ولتاژ ورودی پس از معتبرشدن اولین فریم از ماژول Measurement می‌آید. ولتاژ باتری تا تأیید مرحله خودش ورودی تست Live Expressions باقی می‌ماند.
 *          RTOS ساده است؛ osDelay اجازه اجرای تسک‌های دیگر را می‌دهد و HAL_Delay ممنوع است.
 */

#include "rtos_tasks.h"
#include "ui_led.h"
#include "app_config.h"
#include "modules_enable.h"
#include "cmsis_os2.h"
#include "rtos_time.h"


#if MODULE_MEASUREMENT
#include "measurement.h"
#endif

#include <stdint.h>

/* ==================== Global Test Inputs / ورودی‌های تست سراسری ==================== */

/* [EN] Battery voltage remains a manual Live Expressions test input until the battery measurement stage is approved.
 *      [FA] ولتاژ باتری تا زمان تأیید مرحله اندازه‌گیری باتری، ورودی تست دستی Live Expressions باقی می‌ماند. */
volatile uint32_t UINT32_T__G__BatteryVoltageMv = 25000u;

/* ==================== Task Ui / تسک UI ==================== */

/**
 * @brief  [EN] Run the UI task and select the scenario from measured input voltage and manual battery test voltage.
 *         [FA] تسک UI را اجرا می‌کند و سناریو را از ولتاژ ورودی اندازه‌گیری‌شده و ولتاژ تست دستی باتری انتخاب می‌کند.
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

#if MODULE_MEASUREMENT
        if (BOOL__G__MeasDataValid == true)
        {
            uint32_t__inputVoltageMv = UINT32_T__G__MeasInputVoltageMv;
        }
        else
        {
            /* [EN] No valid ADC frame is a safe disconnected-input result.
               [FA] نبود فریم معتبر ADC، ورودی قطع را به‌عنوان حالت امن نشان می‌دهد. */
            uint32_t__inputVoltageMv = 0u;
        }
#else
        /* [EN] Measurement is disabled, so the input is treated as disconnected safely.
           [FA] اگر Measurement خاموش باشد، ورودی برای ایمنی قطع در نظر گرفته می‌شود. */
        uint32_t__inputVoltageMv = 0u;
#endif

        uint32_t__batteryVoltageMv = UINT32_T__G__BatteryVoltageMv;

        /* [EN] The UI uses the measured input and the manual battery test value.
           [FA] UI از ورودی اندازه‌گیری‌شده و مقدار تست دستی باتری استفاده می‌کند. */
        func__Ui_Tick(uint32_t__inputVoltageMv, uint32_t__batteryVoltageMv);

        func__Rtos_DelayMilliseconds(UI_TICK_MS);
    }
}
