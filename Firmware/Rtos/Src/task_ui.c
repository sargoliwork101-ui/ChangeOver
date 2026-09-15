/**
 * @file    task_ui.c
 * @brief   [EN] FreeRTOS task that runs one LED/buzzer scenario at a time, voltage-based, full type naming, func_ prefix.
 *          [FA] تسک FreeRTOS که هر بار یک سناریوی LED/بازر را با ولتاژ اجرا می‌کند، نام‌گذاری تایپ کامل و پیشوند func_.
 *
 * @note    [EN] Naming per AI_CONTEXT.md:
 *          - Variables: full type first. Global UPPERCASE with G_: UINT32_T_G_..., BOOL_G_...
 *            Local lowercase: uint32_t_..., uint8_t_..., bool_...
 *          - Functions we write: func_ prefix, system functions untouched.
 *          - Battery 0%=21V (UI_BAT_V_MIN_MV), 100%=28V (UI_BAT_V_MAX_MV). Input present if V_in>=20V (UI_INPUT_THRESHOLD_MV).
 *          - Thresholds in ui_config.h single file.
 *          [FA] نام‌گذاری طبق AI:
 *          - متغیر: اول تایپ کامل. گلوبال حروف بزرگ با G_: UINT32_T_G_...
 *          - تابع خودمان: پیشوند func_.
 */

#include "rtos_tasks.h"
#include "ui.h"
#include "ui_config.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <stdbool.h>

/* ==================== Global Test Inputs (volatile for Live Expressions) ==================== */
/* [EN] Full type uppercase for globals / تایپ کامل حروف بزرگ برای گلوبال */

volatile uint32_t UINT32_T_G_InputVoltageMv = 24000u;   /* [EN] Input voltage mV, 24V normal / ولتاژ ورودی */
volatile uint32_t UINT32_T_G_BatteryVoltageMv = 25000u; /* [EN] Battery voltage mV, 21V=0% 28V=100% / ولتاژ باتری */

/**
 * @brief  [EN] UI task entry. One board test, then one scenario cycle at a time. Voltage-based decision.
 *         Never returns.
 *         [FA] ورود تسک UI. یک‌بار تست برد، بعد هر نوبت یک سیکل سناریو بر اساس ولتاژ. برنمی‌گردد.
 * @param  void_ptr_argument [EN] Required by FreeRTOS, unused, type void* / آرگومان FreeRTOS، استفاده نمی‌شود
 */
void func_TaskUi(void *void_ptr_argument)
{
    (void)void_ptr_argument;

    func_Ui_BoardTest();

    for (;;)
    {
        uint32_t uint32_t_inputVoltageMv;
        uint32_t uint32_t_batteryVoltageMv;
        uint8_t uint8_t_batteryPercent;
        bool bool_inputPresent;

        /* [EN] Copy volatile globals to locals (full type lowercase) / کپی گلوبال به لوکال */
        uint32_t_inputVoltageMv = UINT32_T_G_InputVoltageMv;
        uint32_t_batteryVoltageMv = UINT32_T_G_BatteryVoltageMv;

        /* [EN] Clamp battery voltage to max for safety / محدود کردن ولتاژ باتری به حداکثر */
        if (uint32_t_batteryVoltageMv > UI_BAT_V_MAX_MV)
        {
            uint32_t_batteryVoltageMv = UI_BAT_V_MAX_MV;
        }

        /* [EN] Convert to percent using helper (21V=0%,28V=100%) / تبدیل به درصد */
        uint8_t_batteryPercent = func_Ui_BatteryVoltageToPercent(uint32_t_batteryVoltageMv);

        /* [EN] Input present if V_in >=20V threshold / ورودی وصل اگر >=۲۰ ولت */
        bool_inputPresent = (uint32_t_inputVoltageMv >= UI_INPUT_THRESHOLD_MV);

        if (bool_inputPresent == true)
        {
            /* [EN] Input present: if battery not full, charging, else InputOk / ورودی وصل: اگر باتری فول نیست شارژ */
            if (uint8_t_batteryPercent < UI_PERCENT_FULL)
            {
                func_Ui_ScenarioCharging(uint32_t_batteryVoltageMv);
            }
            else
            {
                func_Ui_ScenarioInputOk();
            }
        }
        else
        {
            /* [EN] No input: BatteryRun with smart beep, yellow OFF / ورودی قطع: دشارژ با بوق هوشمند */
            func_Ui_ScenarioBatteryRun(uint32_t_batteryVoltageMv);
        }
    }
}
