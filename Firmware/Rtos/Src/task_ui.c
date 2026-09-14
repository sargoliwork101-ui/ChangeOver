/**
 * @file    task_ui.c
 * @brief   [EN] FreeRTOS task that runs one LED/buzzer scenario at a time, voltage-based.
 *          [FA] تسک FreeRTOS که هر بار یک سناریوی LED/بازر را با ولتاژ ورودی و باتری اجرا می‌کند.
 *
 * @note    [EN] Teaching & naming:
 *          - Global test vars use UPPERCASE type prefix (U32_G_...), locals use lowercase (u32_...).
 *          - Battery 0% = 21V (UI_BAT_V_MIN_MV), 100% = 28V (UI_BAT_V_MAX_MV). Input present if V_in >= 20V (UI_INPUT_THRESHOLD_MV).
 *          - Parameters are defined on top of this file AND on top of ui.c for easy tuning without touching whole program.
 *          - Scenarios: InputOk (green steady), BatteryRun (green blink + smart beep, yellow OFF), Charging (yellow blink remaining to full, green steady).
 *          [FA] آموزش و نام‌گذاری:
 *          - متغیرهای تست گلوبال با تایپ حروف بزرگ (U32_G_...)، داخلی با کوچک (u32_...).
 *          - باتری صفر درصد = ۲۱ ولت، فول = ۲۸ ولت. ورودی وصل اگر V_in >= ۲۰ ولت.
 *          - پارامترها بالای همین فایل و بالای ui.c تعریف شده‌اند تا تغییرشان نیاز به عوض کردن کل برنامه نداشته باشد.
 */

/* ==================== Tunable Parameters — Change Here ==================== */
#define UI_BAT_V_MIN_MV_TASK            21000u  /* [EN] 0% = 21V / صفر درصد */
#define UI_BAT_V_MAX_MV_TASK            28000u  /* [EN] 100% = 28V / فول */
#define UI_INPUT_THRESHOLD_MV_TASK      20000u  /* [EN] <20V = no input / زیر ۲۰ ولت ورودی نداریم */
#define UI_PERCENT_FULL_TASK            100u

/* ==================== Includes ==================== */
#include "rtos_tasks.h"
#include "ui.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <stdbool.h>

/* ==================== Global Test Inputs (volatile for Live Expressions) ==================== */
/* [EN] UPPERCASE type prefix for globals per user rule / تایپ با حروف بزرگ برای گلوبال */

volatile uint32_t U32_G_InputVoltageMv = 24000u;   /* [EN] Input voltage mV, 24V normal / ولتاژ ورودی */
volatile uint32_t U32_G_BatteryVoltageMv = 25000u; /* [EN] Battery voltage mV, 21V=0% 28V=100% / ولتاژ باتری */

/* ==================== Task Entry ==================== */

/**
 * @brief  [EN] UI task entry. One board test, then one scenario cycle at a time.
 *         Voltage-based decision: V_in >=20V = input present. Battery voltage maps to percent.
 *         Never returns.
 *         [FA] ورود تسک UI. یک‌بار تست برد، بعد هر نوبت یک سیکل سناریو بر اساس ولتاژ.
 *         برنمی‌گردد.
 * @param  argument  [EN] Required by FreeRTOS, unused.
 */
void TaskUi(void *argument)
{
    (void)argument;

    Ui_BoardTest();

    for (;;)
    {
        uint32_t u32_inputVoltageMv;
        uint32_t u32_batteryVoltageMv;
        uint8_t u8_batteryPercent;
        bool b_inputPresent;

        /* [EN] Copy volatile globals to locals (lowercase prefix) / کپی گلوبال به لوکال */
        u32_inputVoltageMv = U32_G_InputVoltageMv;
        u32_batteryVoltageMv = U32_G_BatteryVoltageMv;

        /* [EN] Clamp battery voltage to valid range for safety / محدود کردن ولتاژ باتری */
        if (u32_batteryVoltageMv < UI_BAT_V_MIN_MV_TASK)
        {
            /* [EN] Below min, keep as is for 0% handling, but not underflow / زیر حداقل */
        }
        if (u32_batteryVoltageMv > UI_BAT_V_MAX_MV_TASK)
        {
            u32_batteryVoltageMv = UI_BAT_V_MAX_MV_TASK;
        }

        /* [EN] Convert to percent using helper (21V=0%,28V=100%) / تبدیل به درصد */
        u8_batteryPercent = Ui_BatteryVoltageToPercent(u32_batteryVoltageMv);

        /* [EN] Input present if V_in >= 20V threshold / ورودی وصل اگر >=۲۰ ولت */
        b_inputPresent = (u32_inputVoltageMv >= UI_INPUT_THRESHOLD_MV_TASK);

        if (b_inputPresent == true)
        {
            /* [EN] Input present: if battery not full, charging scenario, else InputOk / ورودی وصل: اگر باتری فول نیست شارژ */
            if (u8_batteryPercent < UI_PERCENT_FULL_TASK)
            {
                Ui_ScenarioCharging(u32_batteryVoltageMv);
            }
            else
            {
                Ui_ScenarioInputOk();
            }
        }
        else
        {
            /* [EN] No input: BatteryRun with smart beep, yellow OFF / ورودی قطع: دشارژ با بوق هوشمند */
            Ui_ScenarioBatteryRun(u32_batteryVoltageMv);
        }
    }
}
