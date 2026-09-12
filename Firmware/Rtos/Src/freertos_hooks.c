/**
 * @file    freertos_hooks.c
 * @brief   [EN] Static allocation hooks required by FreeRTOS (Idle/Timer) and stack overflow trap.
 *          [FA] هوک تخصیص استاتیک Idle/Timer و تله سرریز استک.
 *
 * @note    [EN] If CubeMX already generated these symbols, exclude this file from the build.
 *          [FA] اگر CubeMX همین توابع را ساخت، این فایل را از Build خارج کن.
 */

#include "FreeRTOS.h"
#include "task.h"

static StaticTask_t s_idle_tcb;
static StackType_t s_idle_stack[configMINIMAL_STACK_SIZE];

/**
 * @brief  [EN] Provide RAM for the Idle task (static allocation).
 *         [FA] RAM تسک Idle را می‌دهد (تخصیص استاتیک).
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &s_idle_tcb;
    *ppxIdleTaskStackBuffer = s_idle_stack;
    *pulIdleTaskStackSize = (uint32_t)configMINIMAL_STACK_SIZE;
}

#if (configUSE_TIMERS == 1)
static StaticTask_t s_timer_tcb;
static StackType_t s_timer_stack[configTIMER_TASK_STACK_DEPTH];

/**
 * @brief  [EN] Provide RAM for the Timer service task.
 *         [FA] RAM تسک سرویس تایمر را می‌دهد.
 */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &s_timer_tcb;
    *ppxTimerTaskStackBuffer = s_timer_stack;
    *pulTimerTaskStackSize = (uint32_t)configTIMER_TASK_STACK_DEPTH;
}
#endif

/**
 * @brief  [EN] Called if a task overflows its stack. Halts here.
 *         [FA] اگر استک تسک پر شود صدا می‌شود. همین‌جا می‌ایستد.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
