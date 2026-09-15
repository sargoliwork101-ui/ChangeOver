/**
 * @file    task_measurement.c
 * @brief   [EN] FreeRTOS measurement task - fully RTOS, non-blocking, chunked, vTaskDelayUntil 10ms base.
 *          [FA] تسک اندازه‌گیری کاملاً RTOS غیربلوکه، تیکه‌ای.
 *
 * @note    [EN] No HAL_Delay, no long blocking. Uses vTaskDelayUntil which yields, not locks MCU.
 *          [FA] بدون delay قفل‌کن، فقط vTaskDelayUntil.
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"

#if MODULE_MEASUREMENT
#include "measurement.h"
#endif

/**
 * @brief  [EN] Measurement task - non-blocking periodic.
 *         [FA] تسک اندازه‌گیری - دوره‌ای غیربلوکه.
 * @param  void_ptr__argument [EN] FreeRTOS arg / آرگومان
 */
/* ==================== TaskMeasurement ==================== */

void func__TaskMeasurement(void *void_ptr__argument)
{
    TickType_t ticktype__lastWakeTick;
    TickType_t ticktype__periodTicks;

    (void)void_ptr__argument;

#if MODULE_MEASUREMENT
    ticktype__periodTicks = pdMS_TO_TICKS(APP_CONFIG.control_period_ms);
#else
    ticktype__periodTicks = pdMS_TO_TICKS(1000u);
#endif

    ticktype__lastWakeTick = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&ticktype__lastWakeTick, ticktype__periodTicks);

#if MODULE_MEASUREMENT
        /* [EN] One small chunk per tick, non-blocking
           [FA] هر تیکه یک کار کوچک، بدون قفل */
        func__Measurement_Run();
#endif
    }
}