/**
 * @file    task_ui.c
 * @brief   [EN] CMSIS-RTOS2 UI thread - selects LED scenarios from the real Measurement snapshot.
 *          [FA] تسک CMSIS-RTOS2 رابط کاربر - سناریوهای LED را از snapshot واقعی Measurement انتخاب می‌کند.
 *
 * @note    [EN] Production battery voltage comes ONLY from snapshot.v_bat24_mv via
 *              func__Measurement_GetSnapshot(). The snapshot.valid flag is checked
 *              before any UI decision; invalid snapshot forces safe-off, clears the
 *              UI battery alarm flag and no stale/manual values are used.
 *              CMSIS-RTOS2 simple: osDelay yields, other tasks run, MCU not locked. No HAL_Delay.
 *          [FA] ولتاژ باتری تولید فقط از snapshot.v_bat24_mv می‌آید و قبل از هر تصمیم
 *          UI مقدار snapshot.valid بررسی می‌شود.
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
#include "app_types.h"

/* ==================== Task Ui / تسک UI ==================== */

/**
 * @brief  [EN] Run the UI task and select the scenario from the real Measurement snapshot.
 *         [FA] تسک UI را اجرا می‌کند و سناریو را از snapshot واقعی Measurement انتخاب می‌کند.
 * @param  void_ptr__argument [EN] CMSIS-RTOS2 thread argument, unused / آرگومان استفاده‌نشده
 */
void func__TaskUi(void *void_ptr__argument)
{
    (void)void_ptr__argument;

    func__Ui_Init();
    func__Ui_BoardTest_Start();

    for (;;)
    {
        measurement_snapshot_t measurement_snapshot_t__snap;

        measurement_snapshot_t__snap.valid = false;
        measurement_snapshot_t__snap.v_in_mv = 0u;
        measurement_snapshot_t__snap.v_bat24_mv = 0u;
        measurement_snapshot_t__snap.v_bat12_mv = 0u;
        measurement_snapshot_t__snap.v_bat_low_mv = 0u;
        measurement_snapshot_t__snap.v_bat_high_mv = 0u;
        measurement_snapshot_t__snap.i_ch1_ma = 0u;
        measurement_snapshot_t__snap.i_ch2_ma = 0u;
        measurement_snapshot_t__snap.input_present = false;

#if MODULE_MEASUREMENT
        (void)func__Measurement_GetSnapshot(&measurement_snapshot_t__snap);
#else
        measurement_snapshot_t__snap.valid = false;
#endif

        /* [EN] UI owns BOOL__G__UiBatteryAlarmIssued and decides it from snapshot.valid + v_bat24_mv only.
           Changeover reads the flag only.
           [FA] UI مالک BOOL__G__UiBatteryAlarmIssued است و آن را فقط از valid و v_bat24_mv می‌سازد. */
        func__Ui_Tick(&measurement_snapshot_t__snap);

        func__Rtos_DelayMilliseconds(UI_TICK_MS);
    }
}
