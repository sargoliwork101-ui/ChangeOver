/**
 * @file    freertos_hooks.c
 * @brief   [EN] Static allocation hooks required by FreeRTOS (Idle/Timer) and stack overflow trap.
 *          [FA] هوک تخصیص استاتیک ‎Idle/Timer‎ و تله سرریز استک.
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

/* [EN] Build-log marker. "region FLASH overflowed by 780 bytes" looks exactly
   the same whether the fix is missing or merely insufficient, and the only
   way to tell from the outside is to see whether the fix was compiled at all.
   This line prints in the CubeIDE build console, so the question is answered
   by looking, not by guessing. If it is absent from the log, the build did
   not use this source tree.
   [FA] نشانگر کنسول بیلد. پیام «سرریز ۷۸۰ بایت» چه وقتی اصلاح نرسیده باشد و چه
   وقتی کافی نبوده، دقیقاً یک‌شکل است؛ تنها راه تشخیص از بیرون این است که ببینیم
   اصلاً اصلاح کامپایل شده یا نه. این خط در کنسول بیلد CubeIDE چاپ می‌شود، پس
   جواب را با دیدن می‌گیریم نه با حدس. اگر در لاگ نبود، بیلد از این درخت سورس
   استفاده نکرده است. */
#pragma message("ChangeOver flash diet 2026-10-03 ACTIVE: configUSE_TIMERS=0, timers.c and queue.c are out (~3 KB)")

/* [EN] Clean-up 2026-10-06: these four statics were the only ones in the
 *      firmware still using the FreeRTOS "s_" style instead of the project's
 *      TYPE__G__Name rule. Renamed only - same storage, same linkage.
 * [FA] پاک‌سازی ۲۰۲۶-۱۰-۰۶: این چهار استاتیک تنها جاهایی بودند که به‌جای
 *      قاعدهٔ ‎TYPE__G__Name‎ پروژه از سبک ‎"s_"‎ فری‌آرتوس استفاده می‌کردند.
 *      فقط نام عوض شده؛ حافظه و لینکیج یکی است. */
static StaticTask_t STATICTASK_T__G__IdleTcb;
static StackType_t  STACKTYPE_T__G__A__IdleStack[configMINIMAL_STACK_SIZE];

/* [EN] Vector-table and kernel callback entry points have no header of their
 *      own in this tree; declaring them here documents the external linkage
 *      and keeps -Wmissing-prototypes quiet.
 * [FA] این نقاط ورود هدر اختصاصی ندارند؛ اعلان اینجا لینکیج بیرونی را مستند
 *      می‌کند و هشدار ‎-Wmissing-prototypes‎ را می‌بندد. */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize);
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);

/**
 * @brief  [EN] Provide RAM for the Idle task (static allocation).
 *         [FA] RAM تسک Idle را می‌دهد (تخصیص استاتیک).
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
 *         [FA] RAM تسک سرویس تایمر را می‌دهد.
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
