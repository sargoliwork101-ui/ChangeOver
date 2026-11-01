/**
 * @file    freertos_hooks.c
 * @brief   [EN] Static allocation hooks required by FreeRTOS (Idle/Timer) and stack overflow trap.
 * @note    [EN] If CubeMX already generated these symbols, exclude this file from the build.
 */
/* ==================== Includes ==================== */
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_pwm.h"
/*
 * [EN] Flash guard (2026-10-03). The image was 780 bytes too big for the
 * 62 KiB FLASH region of that day - after the later NVM blocks were reserved
 * the region is 56K today - and the fix was switching software timers off:
 * nothing in this firmware creates one, yet tasks.c starts the timer task
 * regardless, which drags in timers.c and queue.c - about 3 KB that no call
 * site reaches.
 * CubeMX does not record configUSE_TIMERS in the .ioc, so regenerating the
 * project silently puts it back. When that happens the linker says only
 * "region FLASH overflowed", which tells nobody why. This says why.
 */
#if (configUSE_TIMERS != 0)
#error "configUSE_TIMERS must stay 0 (flash diet 2026-10-03): the image does not fit in the 56K FLASH region of today (62 KiB when the fix was made) with the timer task. See the note in FreeRTOSConfig.h. / تایمرها باید خاموش بمانند وگرنه ایمیج در ۵۶ کیلوبایت فلش امروز (۶۲ کیلوبایت هنگام این اصلاح) جا نمی‌شود."
#endif
/*
 * [EN] Build-log marker. "region FLASH overflowed by 780 bytes" looks exactly
 * the same whether the fix is missing or merely insufficient, and the only
 * way to tell from the outside is to see whether the fix was compiled at all.
 * This line prints in the CubeIDE build console, so the question is answered
 * by looking, not by guessing. If it is absent from the log, the build did
 * not use this source tree.
 */
#pragma message("ChangeOver flash diet 2026-10-03 ACTIVE: configUSE_TIMERS=0, timers.c and queue.c are out (~3 KB)")
/*
 * [EN] Clean-up 2026-10-06: these four statics were the only ones in the
 *      firmware still using the FreeRTOS "s_" style instead of the project's
 *      TYPE__G__Name rule. Renamed only - same storage, same linkage.
 */
static StaticTask_t STATICTASK_T__G__IdleTcb;
static StackType_t  STACKTYPE_T__G__A__IdleStack[configMINIMAL_STACK_SIZE];
/*
 * [EN] Vector-table and kernel callback entry points have no header of their
 *      own in this tree; declaring them here documents the external linkage
 *      and keeps -Wmissing-prototypes quiet.
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize);
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);
/**
 * @brief  [EN] Provide RAM for the Idle task (static allocation).
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &STATICTASK_T__G__IdleTcb;
    *ppxIdleTaskStackBuffer = STACKTYPE_T__G__A__IdleStack;
    *pulIdleTaskStackSize = (uint32_t)configMINIMAL_STACK_SIZE;
}
#if (configUSE_TIMERS == 1)
static StaticTask_t STATICTASK_T__G__TimerTcb;
static StackType_t  STACKTYPE_T__G__A__TimerStack[configTIMER_TASK_STACK_DEPTH];
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize);
/**
 * @brief  [EN] Provide RAM for the Timer service task.
 */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &STATICTASK_T__G__TimerTcb;
    *ppxTimerTaskStackBuffer = STACKTYPE_T__G__A__TimerStack;
    *pulTimerTaskStackSize = (uint32_t)configTIMER_TASK_STACK_DEPTH;
}
#endif
/**
 * @brief  [EN] Called if a task overflows its stack. Halts here.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    /*
 * [EN] Gates first: the charger must not keep driving the battery while the
 * system halts and waits for the watchdog reset. Register writes only, so
 */
    func__BspPwm_StopAll();
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
