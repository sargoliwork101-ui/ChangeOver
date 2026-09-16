/**
 * @file    task_measurement.c
 * @brief   [EN] FreeRTOS measurement task. ADC+DMA run autonomously (the
 *              hardware fills the buffer without CPU involvement); this task
 *              only converts the newest frame into the shared snapshot.
 *              Simple RTOS pattern: vTaskDelayUntil, no HAL_Delay.
 *          [FA] تسک اندازه‌گیری FreeRTOS. ADC+DMA به‌طور مستقل کار می‌کنند
 *              (سخت‌افزار بافر را بدون درگیری CPU پر می‌کند)؛ این تسک فقط
 *              آخرین فریم را در snapshot مشترک تبدیل می‌کند. الگوی RTOS
 *              ساده: vTaskDelayUntil، بدون HAL_Delay.
 *
 * @note    [EN] Period is MEASUREMENT_PERIOD_MS (top of measurement.h) -
 *              one line to change the sample rate. Stack: TASK_STACK_MEASUREMENT
 *              words in rtos_config.h (conversion locals only, no big arrays).
 *          [FA] دوره از MEASUREMENT_PERIOD_MS (بالای measurement.h) — برای
 *              تغییر نرخ فقط یک خط. استک: TASK_STACK_MEASUREMENT word در
 *              rtos_config.h (فقط محلی‌های تبدیل، بدون آرایه‌ی بزرگ).
 */

/* ==================== Includes ==================== */
#include "rtos_tasks.h"
#include "modules_enable.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

#if MODULE_MEASUREMENT
#include "bsp_adc.h"
#include "measurement.h"
#endif

/* ==================== Task Measurement ==================== */

/**
 * @brief  [EN] Entry of the measurement task: one-time bring-up (give the
 *              CubeMX handle to the BSP, start the autonomous ADC+DMA, zero
 *              the snapshot, wait for the first frame), then a fixed 10 ms
 *              loop that converts one frame per tick.
 *         [FA] ورودی تسک اندازه‌گیری: راه‌اندازی یک‌بار (دادن هندل مکعب به
 *              BSP، شروع ADC+DMA مستقل، صفر کردن snapshot، انتظار فریم اول)،
 *              بعد یک حلقهٔ ثابت ۱۰ms که در هر تیک یک فریم تبدیل می‌کند.
 * @param  void_ptr__argument [EN] Unused task argument / آرگومان تسک، استفاده
 *                                 نمی‌شود
 */
void func__TaskMeasurement(void *void_ptr__argument)
{
    (void)void_ptr__argument;

#if MODULE_MEASUREMENT
    TickType_t TickType_t__lastWakeTime;

    /* [EN] One-time bring-up: the CubeMX handle (hadc1, generated in main.c)
       is handed to the BSP, then the hardware takes over - ADC converts
       continuously and DMA wraps the buffer, no CPU, no interrupt.
       [FA] راه‌اندازی یک‌بار: هندل مکعب (hadc1، ساخته‌شده در main.c) به BSP
       داده می‌شود، بعد سخت‌افزار دست‌کار می‌شود — ADC مدام تبدیل و DMA بافر
       را دور می‌زند؛ بدون CPU و بدون قطع‌کننده. */
    func__BspAdc_Init(&hadc1);
    (void)func__BspAdc_Start();
    func__Measurement_Init();

    /* [EN] The first full DMA frame is ready ~0.1 ms after Start; the settle
       delay is 1 ms margin (MEASUREMENT_SETTLE_MS).
       [FA] اولین فریم کامل DMA حدود 0.1ms بعد از Start آماده است؛ تأخیر
       استقراری 1ms حاشیه است (MEASUREMENT_SETTLE_MS). */
    vTaskDelay(pdMS_TO_TICKS(MEASUREMENT_SETTLE_MS));

    /* [EN] Fixed-period loop (MEASUREMENT_PERIOD_MS, top of measurement.h).
       Only this task sleeps here; the MCU keeps running the other tasks.
       [FA] حلقهٔ دوره‌ی ثابت (MEASUREMENT_PERIOD_MS، بالای measurement.h).
       اینجا فقط همین تسک می‌خوابد؛ میکرو بقیهٔ تسک‌ها را اجرا می‌کند. */
    TickType_t__lastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&TickType_t__lastWakeTime,
                        pdMS_TO_TICKS(MEASUREMENT_PERIOD_MS));
        func__Measurement_Run();
    }
#else
    /* [EN] Flag off: task body is a plain 1 s sleep.
       [FA] فلگ خاموش: بدنهٔ تسک فقط ۱ ثانیه خواب است. */
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000u));
    }
#endif
}
