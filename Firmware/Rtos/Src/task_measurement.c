/**
 * @file    task_measurement.c
 * @brief   [EN] CMSIS-RTOS2 measurement thread. The board ADC backend supplies
 *              completed normalized frames; this task only converts the newest
 *              frame into the shared snapshot. Simple RTOS pattern: osDelayUntil,
 *              no HAL_Delay.
 *          [FA] تسک اندازه‌گیری CMSIS-RTOS2. backend ADC برد فریم‌های کامل
 *              استانداردشده را می‌دهد؛ این تسک فقط جدیدترین فریم را در snapshot
 *              مشترک تبدیل می‌کند. الگوی RTOS ساده: osDelayUntil، بدون HAL_Delay.
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
#include "cmsis_os2.h"
#include "rtos_time.h"


#if MODULE_MEASUREMENT
#include "bsp_adc.h"
#include "measurement.h"
#endif

/* ==================== Task Measurement ==================== */

/**
 * @brief  [EN] Entry of the measurement task: initialize the board ADC port,
 *              start its autonomous acquisition, zero the snapshot, then run a
 *              fixed-period loop that converts one completed frame per cycle.
 *         [FA] ورودی تسک اندازه‌گیری: پورت ADC برد را مقداردهی و دریافت مستقل
 *              آن را شروع می‌کند، snapshot را صفر می‌کند و سپس در یک حلقهٔ
 *              دوره‌ای ثابت هر بار یک فریم کامل را تبدیل می‌کند.
 * @param  void_ptr__argument [EN] Unused task argument / آرگومان تسک، استفاده
 *                                 نمی‌شود
 */
void func__TaskMeasurement(void *void_ptr__argument)
{
    (void)void_ptr__argument;

#if MODULE_MEASUREMENT
    uint32_t UINT32_T__lastWakeTime;

    /* [EN] One-time bring-up is delegated to the board BSP. The task does not
       know the MCU ADC handle or its peripheral name.
       [FA] راه‌اندازی یک‌بار به BSP برد سپرده می‌شود. این تسک هندل ADC
       یا نام پریفرال میکرو را نمی‌شناسد. */
    func__BspAdc_Init();
    func__Measurement_Init();

    if (func__BspAdc_Start() == false)
    {
        /* [EN] Keep measurement invalid and sleep if calibration/start fails;
           other RTOS tasks continue to run.
           [FA] اگر کالیبراسیون/شروع شکست خورد، measurement نامعتبر می‌ماند
           و فقط همین تسک می‌خوابد؛ بقیهٔ تسک‌های RTOS ادامه می‌دهند. */
        for (;;)
        {
            func__Rtos_DelayMilliseconds(1000u);
        }
    }

    /* [EN] Fixed-period loop (MEASUREMENT_PERIOD_MS, top of measurement.h).
       Only this task sleeps here; the MCU keeps running the other tasks.
       [FA] حلقهٔ دوره‌ی ثابت (MEASUREMENT_PERIOD_MS، بالای measurement.h).
       اینجا فقط همین تسک می‌خوابد؛ میکرو بقیهٔ تسک‌ها را اجرا می‌کند. */
    UINT32_T__lastWakeTime = osKernelGetTickCount();

    for (;;)
    {
        UINT32_T__lastWakeTime += func__Rtos_MillisecondsToTicks(MEASUREMENT_PERIOD_MS);
        (void)osDelayUntil(UINT32_T__lastWakeTime);
        func__Measurement_Run();
    }
#else
    /* [EN] Flag off: task body is a plain 1 s sleep.
       [FA] فلگ خاموش: بدنهٔ تسک فقط ۱ ثانیه خواب است. */
    for (;;)
    {
        func__Rtos_DelayMilliseconds(1000u);
    }
#endif
}
