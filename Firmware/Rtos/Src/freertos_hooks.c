/**
 * @file    freertos_hooks.c
 * @brief   حافظه Idle Task وقتی Static Allocation روشن است.
 *
 * FreeRTOS علاوه بر Task تو، یک Task داخلی Idle دارد.
 * اگر malloc ممنوع باشد، باید بافر Idle را هم خودت بدهی.
 *
 * اگر CubeMX همین توابع را ساخت و لینکر گفت duplicate،
 * این فایل را از Build خارج کن. توضیح در docs/01-led-buzzer-cubeide.md
 */

#include "FreeRTOS.h"
#include "task.h"

static StaticTask_t s_idle_tcb;
static StackType_t s_idle_stack[configMINIMAL_STACK_SIZE];

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

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &s_timer_tcb;
    *ppxTimerTaskStackBuffer = s_timer_stack;
    *pulTimerTaskStackSize = (uint32_t)configTIMER_TASK_STACK_DEPTH;
}
#endif

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    /* استک Task پر شده. در LED اگر این‌جا آمدی TASK_STACK_UI را زیاد کن. */
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
