/**
 * @file    task_ui.c
 * @brief   [EN] CMSIS-RTOS2 UI thread - selects LED scenarios from one valid Measurement snapshot.
 *          [FA] تسک CMSIS-RTOS2 رابط کاربر - سناریوهای LED را از یک snapshot معتبر Measurement انتخاب می‌کند.
 *
 * @note    [EN] Invalid snapshots keep the UI in safe-off; no manual battery value is used.
 *          CMSIS-RTOS2 simple: osDelay yields, other tasks run, MCU not locked. No HAL_Delay.
 *          [FA] snapshot نامعتبر UI را در خاموشی امن نگه می‌دارد و مقدار دستی باتری مصرف نمی‌شود.
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

#include <stdbool.h>
#include <stdint.h>

/* ==================== Task Ui / تسک UI ==================== */

/**
 * @brief  [EN] Run the UI task from one coherent Measurement snapshot.
 *         [FA] تسک UI را از یک snapshot منسجم Measurement اجرا می‌کند.
 */
void func__TaskUi(void *void_ptr__argument)
{
    (void)void_ptr__argument;

    func__Ui_Init();
    func__Ui_BoardTest_Start();

    for (;;)
    {
#if MODULE_MEASUREMENT
        measurement_snapshot_t measurement_snapshot_t__snap;
        bool bool__snapshotValid;

        /* [EN] Initialize every field because GetSnapshot returns false without
           copying an invalid snapshot. [FA] همهٔ فیلدها را مقداردهی می‌کند،
           چون در نمونهٔ نامعتبر GetSnapshot کپی انجام نمی‌دهد. */
        measurement_snapshot_t__snap.v_in_mv = 0u;
        measurement_snapshot_t__snap.v_bat24_mv = 0u;
        measurement_snapshot_t__snap.v_bat12_mv = 0u;
        measurement_snapshot_t__snap.i_ch1_ma = 0u;
        measurement_snapshot_t__snap.i_ch2_ma = 0u;
        measurement_snapshot_t__snap.input_present = false;
        measurement_snapshot_t__snap.valid = false;

        bool__snapshotValid = func__Measurement_GetSnapshot(&measurement_snapshot_t__snap);
        if (bool__snapshotValid == false)
        {
            measurement_snapshot_t__snap.valid = false;
        }

        func__Ui_Tick(
            measurement_snapshot_t__snap.v_in_mv,
            measurement_snapshot_t__snap.v_bat24_mv,
            measurement_snapshot_t__snap.input_present,
            measurement_snapshot_t__snap.valid);
#else
        /* [EN] Disabled Measurement keeps UI outputs safely off.
           [FA] با غیرفعال بودن Measurement خروجی‌های UI خاموش و امن می‌مانند. */
        func__Ui_Tick(0u, 0u, false, false);
#endif

        func__Rtos_DelayMilliseconds(UI_TICK_MS);
    }
}
