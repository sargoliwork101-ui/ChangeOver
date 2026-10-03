/**
 * @file    freertos_hooks.c
 * @brief   [EN] Static allocation hooks required by FreeRTOS (Idle/Timer) and stack overflow trap.
 *          [FA] هوک تخصیص استاتیک Idle/Timer و تله سرریز استک.
 *
 * @note    [EN] If CubeMX already generated these symbols, exclude this file from the build.
 *          [FA] اگر CubeMX همین توابع را ساخت، این فایل را از Build خارج کن.
 */

/* ==================== Includes ==================== */
#include "FreeRTOS.h"
#include "task.h"

/* [EN] Flash guard (2026-10-03). The image was 780 bytes too big for the 62K
   FLASH region, and the fix was switching software timers off: nothing in
   this firmware creates one, yet tasks.c starts the timer task regardless,
   which drags in timers.c and queue.c - about 3 KB that no call site reaches.
   CubeMX does not record configUSE_TIMERS in the .ioc, so regenerating the
   project silently puts it back. When that happens the linker says only
   "region FLASH overflowed", which tells nobody why. This says why.
   [FA] نگهبان فلش. ایمیج ۷۸۰ بایت از ناحیهٔ ۶۲ کیلوبایتی FLASH بزرگ‌تر شده بود و
   راه‌حل خاموش‌کردن تایمرهای نرم‌افزاری بود: هیچ‌جای این فرم‌ور تایمر نمی‌سازد،
   ولی tasks.c به‌هرحال تسک تایمر را راه می‌اندازد و همان timers.c و queue.c را
   می‌کشد - حدود ۳ کیلوبایت که هیچ فراخوانی به آن نمی‌رسد. CubeMX این تنظیم را
   در .ioc نگه نمی‌دارد، پس تولید دوبارهٔ پروژه بی‌صدا برش می‌گرداند و آن‌وقت
   لینکر فقط می‌گوید «FLASH سرریز کرد» که علت را به کسی نمی‌گوید. این می‌گوید. */
#if (configUSE_TIMERS != 0)
#error "configUSE_TIMERS must stay 0 (flash diet 2026-10-03): the image does not fit in the 62K FLASH region with the timer task. See the note in FreeRTOSConfig.h. / تایمرها باید خاموش بمانند وگرنه ایمیج در ۶۲ کیلوبایت فلش جا نمی‌شود."
#endif

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
