/**
 * @file    protection.h
 * @brief   [EN] Lightweight supervisor: while the measurement snapshot is
 *               missing or invalid it holds FAULT_ADC as a LIVE (never
 *               latching) condition; over-current and low-battery supervision
 *               live in the charger alarm and fault detector paths. The
 *               module is compiled out by default (MODULE_PROTECTION = 0).
 *               Full type naming, func__ prefix.
 *          [FA] ناظر سبک: تا وقتی نمونهٔ اندازهگیری نامعتبر است ‎FAULT_ADC‎ را
 *               بهصورت وضعیت زنده (بدون قفل) نگه می‌دارد؛ نظارت اضافهجریان و باتری ضعیف در مسیر
 *               آلارم شارژر و آشکارسازهای فالت است. ماژول بهطور پیشفرض بیرون کامپایل
 *               می‌ماند ‎(MODULE_PROTECTION = 0)‎. نام تایپ کامل.
 * @‎note    [EN] Full-program audit‎ ۲۰۲۶-۱۰-۰۷: the stale "placeholder" label
 *               was removed and the "latch faults" wording corrected - the
 *               implementation deliberately keeps FAULT_ADC live instead of
 *               latching it (see the note inside protection.c).
 *          [FA] ممیزی کل برنامه ۲۰۲۶-۱۰-۰۷: برچسب کهنهٔ «اسکلت» برداشته شد و
 *               عبارت «خطا را قفل می‌کند» اصلاح شد - پیادهسازی عمداً ‎FAULT_ADC‎
 *               را زنده نگه می‌دارد نه قفلشده (یادداشت داخل ‎protection.c‎).
 */

#ifndef PROTECTION_H
#define PROTECTION_H

/* ==================== Includes ==================== */
#include "app_types.h"

/**
 * @brief  [EN] Init protection state.
 *         [FA] حالت حفاظت را Init می‌کند.
 */
/* ==================== Functions ==================== */
void func__Protection_Init(void);

/**
 * @brief  [EN] Hold FAULT_ADC while the snapshot is missing or invalid and
 *              clear it on the first valid snapshot; deliberately never
 *              latches (a boot-time latch would stick for the whole power
 *              cycle and park the safe states forever).
 *         [FA] تا نبودِ نمونهٔ معتبر ‎FAULT_ADC‎ را نگه می‌دارد و با اولین نمونهٔ
 *              معتبر پاکش می‌کند؛ عمداً هرگز قفل نمی‌کند (قفل زمان بوت تا پایان
 *              همین دورهٔ برق می‌ماند و حالتهای امن را بر همیشه پارک می‌کرد).
 * @param  measurement_snapshot_t__snap [EN] Snapshot pointer / اشاره‌گر نمونه
 */
void func__Protection_Run(const measurement_snapshot_t *measurement_snapshot_t__snap);

#endif /* PROTECTION_H */
