/**
 * @file    bsp_iwdg.h
 * @brief   [EN] Independent watchdog (IWDG) + multi-task liveness monitor.
 *          [FA] واچ‌داگ مستقل (IWDG) + ناظر زنده‌بودن چندتسکی.
 *
 * @note    [EN] Kicking from one task proves only that task. Every
 *              supervised task checks in each cycle and the control task
 *              refreshes the watchdog only when ALL expected tasks are
 *              fresh, so a hung measurement/comm/protection/UI task (or
 *              the control task itself) resets the MCU instead of running
 *              half-dead. The supervised set is fixed at compile time from
 *              modules_enable.h and always matches the threads rtos_app.c
 *              creates.
 *          [FA] kick از یک تسک فقط زنده‌بودن همان تسک را ثابت می‌کند. هر
 *              تسک تحت‌نظر هر چرخه اعلام حضور می‌کند و کنترل فقط وقتی همه
 *              تازه باشند واچ‌داگ را تازه می‌کند، پس قفل هر تسک (یا خود
 *              کنترل) به ریست می‌انجامد نه اجرای نیمه‌جان. مجموعهٔ تحت‌نظر
 *              در کامپایل از modules_enable.h ساخته می‌شود و همیشه با
 *              threadهای rtos_app.c یکی است.
 *
 * @note    [EN] Timing (user order 2026-09-27, ~1 s): /32 + reload 1250 =
 *              1.0 s nominal at 40 kHz LSI; the LSI spreads 30..60 kHz, so
 *              the real timeout is 0.67..1.33 s. Control kicks every pass
 *              (10 ms). A task is STALE after BSP_IWDG_STALE_MS (covers the
 *              1 s idle loops of the disabled-module combinations) -
 *              worst-case hang detection ~1.5 s + one timeout. DBG_IWDG_STOP
 *              is set (debugger halt does not reset); a thread-create
 *              failure sits in func__Rtos_Fatal() with no kicks - a
 *              fail-safe reset loop.
 *          [FA] زمان‌بندی (دستور ۲۰۲۶-۰۹-۲۷، ~۱ ثانیه): ۳۲/ + ریلود ۱۲۵۰
 *              = ۱٫۰ ثانیه اسمی با LSI ۴۰kHz؛ پراکندگی LSI یعنی ۰٫۶۷..۱٫۳۳
 *              ثانیه واقعی؛ کنترل هر پاس (۱۰ms) kick می‌کند. تسک بعد از
 *              BSP_IWDG_STALE_MS کهنه است (حلقه‌های ۱ ثانیه‌ای ترکیب‌های
 *              غیرفعال را می‌پوشاند) - بدترین تشخیص ~۱٫۵ ثانیه + یک
 *              تایم‌اوت. DBG_IWDG_STOP ست است (توقف دیباگر ریست نمی‌کند)؛
 *              خطای ساخت thread در Fatal بدون kick می‌ماند - حلقهٔ ریست
 *              fail-safe.
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
 * @brief  [EN] Start the LSI, program /32 + reload 1250, freeze on core
 *              halt and start the watchdog. Called once from main() before
 *              the scheduler; the first kick must arrive within the timeout
 *              (control kicks every pass, first pass runs milliseconds
 *              after the scheduler starts). Without a control task in this
 *              module combination the watchdog is deliberately NOT started
 *              (nobody could kick it).
 *         [FA] راه‌اندازی LSI، تنظیم ۳۲/ + ۱۲۵۰، فریز با halt و شروع
 *              واچ‌داگ. یک‌بار از main قبل از زمان‌بند؛ اولین kick باید داخل
 *              تایم‌اوت برسد (اولین پاس کنترل میلی‌ثانیه‌ها بعد از شروع
 *              زمان‌بند است). بدون تسک کنترل در این ترکیب، واچ‌داگ عمداً
 *              شروع نمی‌شود (کسی نمی‌توانست kick کند).
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
