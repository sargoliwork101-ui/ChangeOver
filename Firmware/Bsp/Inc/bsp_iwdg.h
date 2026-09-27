/**
 * @file    bsp_iwdg.h
 * @brief   [EN] Independent watchdog (IWDG) + multi-task liveness monitor.
 *          [FA] واچ‌داگ مستقل (IWDG) + ناظر زند‌بودن چندتسکی.
 *
 * @note    [EN] Why a monitor, not a plain kick: kicking the IWDG from one
 *              task only proves THAT task is alive. Here every supervised
 *              task checks in each cycle and the control task refreshes the
 *              watchdog only when ALL expected tasks are fresh - a hung
 *              measurement/comm/protection/UI task (or a hung control task
 *              itself) therefore resets the MCU instead of running half-dead.
 *              Supervised set is fixed at compile time from modules_enable.h
 *              and always matches the threads rtos_app.c creates, including
 *              the "task does not exist" combinations (a disabled module
 *              means no thread, so no slot to go stale).
 *          [FA] چرا ناظر و نه فقط kick ساده: kick از یک تسک فقط زنده‌بودن
 *              همان تسک را ثابت می‌کند. اینجا هر تسک تحت‌نظر هر چرخه اعلام
 *              حضور می‌کند و تسک کنترل فقط وقتی واچ‌داگ را تازه می‌کند که
 *              همهٔ تسک‌های موردانتظار تازه باشند - پس قفل‌کردن هر تسک
 *              (یا خود کنترل) به ریست می‌انجامد نه اجرای نیمه‌جان. مجموعهٔ
 *              تحت‌نظر در کامپایل از modules_enable.h ساخته می‌شود و همیشه
 *              با threadهای rtos_app.c یکی است.
 *
 * @note    [EN] Timing (user order 2026-09-27: IWDG enabled, ~1 s timeout):
 *              prescaler /32, reload 1250 -> nominal 1.0 s at the 40 kHz LSI.
 *              The F1 LSI spreads 30..60 kHz, so the real timeout is
 *              0.67..1.33 s; the control task kicks every pass (10 ms), more
 *              than 60x inside the worst case. A task is STALE after
 *              BSP_IWDG_STALE_MS without a check-in; the value covers the
 *              1 s idle loops of the not-yet-enabled-module combinations, so
 *              worst-case hang detection is ~1.5 s + one IWDG timeout.
 *          [FA] زمان‌بندی (دستور کاربر ۲۰۲۶-۰۹-۲۷: IWDG فعال، ~۱ ثانیه):
 *              پری‌اسکیلر ۳۲/، ریلود ۱۲۵۰ → اسمی ۱٫۰ ثانیه با LSI چهل‌کیلوهرتز.
 *              LSI در عمل ۳۰..۶۰kHz است پس تایم‌اوت واقعی ۰٫۶۷..۱٫۳۳ ثانیه؛
 *              کنترل هر پاس (۱۰ms) kick می‌کند، بیش از ۶۰ برابر داخل بدترین
 *              حالت. تسک پس از BSP_IWDG_STALE_MS بدون اعلام حضور کهنه است؛
 *              این مقدار حلقه‌های ۱ ثانیه‌ای ترکیب‌های غیرفعال را هم پوشش
 *              می‌دهد، پس بدترین تشخیص قفل ~۱٫۵ ثانیه + یک تایم‌اوت IWDG است.
 *
 * @note    [EN] Debugging: the core-halt freeze bit (DBG_IWDG_STOP) is set,
 *              so a debugger halt does not reset the board. A thread-create
 *              failure lands in func__Rtos_Fatal()'s infinite loop with no
 *              kicks, i.e. a fail-safe reset loop instead of silent half-life.
 *          [FA] دیباگ: بیت توقف با halt ست می‌شود تا توقف دیباگر برد را ریست
 *              نکند. خطای ساخت thread در حلقهٔ Fatal بدون kick می‌ماند یعنی
 *              حلقهٔ ریست fail-safe به‌جای نیمه‌جانِ ساکت.
 */

#ifndef BSP_IWDG_H
#define BSP_IWDG_H

#include <stdint.h>

/* [EN] A task that did not check in for this long vetoes the kick / تسکی که
   این‌قدر اعلام حضور نکرده kick را وتو می‌کند. */
#define BSP_IWDG_STALE_MS 1500u

typedef enum
{
    BSP_IWDG_SLOT_MEASUREMENT = 0,
    BSP_IWDG_SLOT_COMM       = 1,
    BSP_IWDG_SLOT_PROTECTION = 2,
    BSP_IWDG_SLOT_UI         = 3,
    BSP_IWDG_SLOT_COUNT      = 4
} bsp_iwdg_slot_t;

/**
 * @brief  [EN] Start the LSI, program prescaler /32 + reload 1250, freeze the
 *              IWDG on core halt, and start the watchdog. Called once from
 *              main() before the scheduler starts; the first kick must arrive
 *              within the timeout (control task kicks every pass, first pass
 *              runs milliseconds after the scheduler starts). If the control
 *              task does not exist in this module combination, the watchdog
 *              is deliberately NOT started (nobody could kick it).
 *         [FA] راه‌اندازی LSI، پری‌اسکیلر ۳۲/ + ریلود ۱۲۵۰، فریز با halt و
 *              شروع واچ‌داگ. یک‌بار از main پیش از شروع زمان‌بند؛ اولین kick
 *              باید داخل تایم‌اوت برسد (کنترل هر پاس kick می‌کند و اولین پاس
 *              میلی‌ثانیه بعد از شروع زمان‌بند است). اگر در این ترکیب ماژول‌ها
 *              تسک کنترل وجود ندارد واچ‌داگ عمداً شروع نمی‌شود.
 */
void func__BspIwdg_Init(void);

/**
 * @brief  [EN] Record one proof-of-life of a supervised task. Calls for slots
 *              that are not expected in this module combination are ignored.
 *         [FA] ثبت یک اعلام حضور تسک تحت‌نظر. فراخوانی برای شکاف‌های
 *              ناموردانتظار این ترکیب نادیده گرفته می‌شود.
 * @param  bsp_iwdg_slot_t__slot [EN] Which task checks in / کدام تسک
 * @param  uint32_t__nowMs [EN] FreeRTOS tick (1 kHz = ms) / تیک فری‌آرتاس
 */
void func__BspIwdg_CheckIn(bsp_iwdg_slot_t bsp_iwdg_slot_t__slot,
                           uint32_t uint32_t__nowMs);

/**
 * @brief  [EN] Refresh the watchdog if and only if every expected slot
 *              checked in within BSP_IWDG_STALE_MS. Called by the control
 *              task every pass (its own liveness is the call itself).
 *         [FA] تازه‌کردن واچ‌داگ اگر و فقط اگر همهٔ شکاف‌های موردانتظار داخل
 *              BSP_IWDG_STALE_MS اعلام حضور کرده باشند. تسک کنترل هر پاس صدا
 *              می‌زند (زنده‌بودن خودش همان صدازدن است).
 * @param  uint32_t__nowMs [EN] FreeRTOS tick (1 kHz = ms) / تیک فری‌آرتاس
 */
void func__BspIwdg_PollKick(uint32_t uint32_t__nowMs);

#endif /* BSP_IWDG_H */
