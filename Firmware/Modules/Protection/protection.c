/**
 * @file    protection.c
 * @brief   [EN] Implementation of the lightweight supervisor: FAULT_ADC is a
 *               LIVE (never latching) condition held while the measurement
 *               snapshot is missing or invalid. Over-current and low-battery
 *               supervision live in the charger alarm and fault detector
 *               paths; the module is compiled out by default
 *               (MODULE_PROTECTION = 0). Full type naming, func__ prefix.
 *          [FA] پیادهسازی ناظر سبک: ‎FAULT_ADC‎ یک وضعیت زنده (بدون قفل) است که
 *               تا نامعتبربودن نمونهٔ اندازهگیری نگه داشته می‌شود. نظارت اضافهجریان و
 *               باتری ضعیف در مسیر آلارم شارژر و آشکارسازهای فالت است؛ ماژول بهطور
 *               پیشفرض بیرون کامپایل می‌ماند ‎(MODULE_PROTECTION = 0)‎. نام تایپ کامل.
 * @‎note    [EN] Full-program audit‎ ۲۰۲۶-۱۰-۰۷: the stale "placeholder" label
 *               was removed and the "latch faults" wording corrected - see
 *               the design note inside func__Protection_Run for why
 *               FAULT_ADC must stay live instead of latching.
 *          [FA] ممیزی کل برنامه ۲۰۲۶-۱۰-۰۷: برچسب کهنهٔ «اسکلت» برداشته شد و
 *               عبارت «خطا را قفل می‌کند» اصلاح شد - دلیل زندهبودن ‎FAULT_ADC‎ در
 *               یادداشت طراحی داخل ‎func__Protection_Run‎ آمده است.
 */

#include "protection.h"
#include "app_config.h"
#include "fault.h"

#include <stddef.h>

/**
 * @brief  [EN] No state to initialize - the supervisor is stateless by
 *              design; the entry point is kept so the module lifecycle
 *              stays uniform with the rest of the firmware.
 *         [FA] حالتی برای مقداردهی اولیه نیست - ناظر بهطراحی بی‌حالت است؛
 *              نقطهٔ ورود برای یکدستماندن چرخهٔ عمر ماژولها نگه داشته شده است.
 */
/* ==================== Protection_Init ==================== */

void func__Protection_Init(void)
{
}

/**
 * @brief  [EN] Hold FAULT_ADC while the snapshot is missing or invalid and
 *              clear it on the first valid snapshot; deliberately never
 *              latches (a boot-time latch would stick for the whole power
 *              cycle and park the safe states forever).
 *         [FA] تا نبودِ نمونهٔ معتبر ‎FAULT_ADC‎ را نگه می‌دارد و با اولین نمونهٔ
 *              معتبر پاکش می‌کند؛ عمداً هرگز قفل نمی‌کند (قفل زمان بوت تا پایان
 *              همین دورهٔ برق می‌ماند و حالتهای امن را بر همیشه پارک می‌کرد).
 * @param  measurement_snapshot_t__snap [EN] Snapshot pointer, may be NULL, valid flag checked / اشاره‌گر نمونه
 */
/* ==================== Protection_Run ==================== */

void func__Protection_Run(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    if ((measurement_snapshot_t__snap == NULL) || (measurement_snapshot_t__snap->valid == false))
    {
        /* [EN] ADC invalid = LIVE state, never a latch: at boot the first
           passes run before measurement warm-up; a latched FAULT_ADC would
           stick forever (nothing cleared it) and park Changeover/Charger in
           safe states for the whole power cycle. Clears itself below as
           soon as a valid snapshot arrives.
           [FA] ADC نامعتبر = وضعیت لحظه‌ای، نه قفل: پاس‌های اول بوت قبل از
           ‎warm-up‎ اجرا می‌شوند و FAULT_ADC قفل‌شده تا ابد می‌ماند و
           Changeover/شارژر را در حالت امن قفل می‌کند. به‌محض snapshot
           معتبر در شاخهٔ پایین پاک می‌شود. */
        func__Fault_Set(FAULT_ADC);
        return;
    }

    func__Fault_Clear(FAULT_ADC);

    (void)APP_CONFIG;
}
