/**
 * @file    flash_lock.h
 * @brief   [EN] One-shot read-out lock for the shipped board: on the first boot
 *              of a build that has MODULE_FLASH_LOCK = 1, program RDP level 1
 *              and reset. Every later boot finds the part already locked and
 *              does nothing, so the lock is a one-time action and never a
 *              boot-time loop.
 *          [FA] قفل یک‌بارمصرف خواندن حافظه برای برد تحویلی: در اولین بوت
 *              بیلدی که ‎MODULE_FLASH_LOCK = 1‎ دارد، RDP سطح ۱ نوشته و برد
 *              ریست می‌شود. هر بوت بعدی قطعه را قفل می‌بیند و کاری نمی‌کند؛
 *              پس قفل یک اقدام یک‌باره است، نه حلقهٔ هر بوت.
 *
 * @note    [EN] Default state in the tree is MODULE_FLASH_LOCK = 0: the module
 *              compiles, the host test runs, but nothing is ever programmed.
 *              Turning it on costs the developer SWD access on that part -
 *              after locking, every SWD session begins with a full chip erase,
 *              which also wipes the parameter bank and the calibration table.
 *              That is why the switch lives in modules_enable.h next to the
 *              other compile-time decisions and not in a panel parameter.
 *          [FA] حالت پیش‌فرض درخت ‎MODULE_FLASH_LOCK = 0‎ است: ماژول کامپایل
 *              می‌شود و تست هاست اجرا می‌شود، ولی هیچ چیز نوشته نمی‌شود.
 *              روشن‌کردنش یعنی از دست دادن دسترسی SWD روی همان قطعه - بعد از
 *              قفل، هر نشست SWD با پاک‌کردن کامل تراشه شروع می‌شود که بانک
 *              پارامترها و جدول کالیبراسیون را هم می‌برد. به همین دلیل کلید
 *              در modules_enable.h کنار بقیهٔ تصمیم‌های زمان‌کامپایل است،
 *              نه یک پارامتر پنل.
 *
 * @note    [EN] Hardware details live in bsp_flash_lock.h; this module owns the
 *              policy only (when to ask, and what counts as success).
 *          [FA] جزئیات سخت‌افزار در bsp_flash_lock.h است؛ این ماژول فقط
 *              سیاست را دارد (چه وقت بپرسد و چه چیزی موفقیت است).
 */

#ifndef FLASH_LOCK_H
#define FLASH_LOCK_H

#include <stdbool.h>

/* ==================== FlashLock Result / نتیجهٔ قفل ==================== */

/**
 * @brief  [EN] What func__FlashLock_ApplyOnce() found or did.
 *         [FA] چیزی که func__FlashLock_ApplyOnce() دید یا انجام داد.
 */
typedef enum
{
    /* [EN] Part was already protected; nothing was written this boot. / [FA] قطعه از قبل قفل بود؛ این بوت چیزی نوشته نشد. */
    FLASH_LOCK_RESULT_ALREADY_PROTECTED = 0,

    /* [EN] Option bytes were programmed and a reset was requested. / [FA] option byte نوشته شد و ریست درخواست شد. */
    FLASH_LOCK_RESULT_JUST_LOCKED = 1,

    /* [EN] Programming failed; the part is still OPEN and boot continues. / [FA] نوشتن شکست خورد؛ قطعه هنوز OPEN است و بوت ادامه دارد. */
    FLASH_LOCK_RESULT_FAILED = 2
} flash_lock_result_t;

/* ==================== FlashLock_IsProtected ==================== */

/**
 * @brief  [EN] Read-only query of the current protection state.
 *         [FA] پرسش فقط‌خواندنی وضعیت فعلی قفل.
 * @‎return bool [EN] true when RDP level 1 is active / [FA] true‎ وقتی RDP سطح ۱ فعال است
 */
bool func__FlashLock_IsProtected(void);

/* ==================== FlashLock_ApplyOnce ==================== */

/**
 * @brief  [EN] Apply the one-time lock if the part is still open.
 *              Called once from App_Init, before the scheduler starts, so the
 *              option-byte erase cannot disturb a running task. On success the
 *              MCU resets inside the BSP launch call and this function does not
 *              return; the return value exists for the host test and for the
 *              failure path.
 *         [FA] اعمال قفل یک‌بارمصرف اگر قطعه هنوز باز است. یک‌بار از
 *              App_Init و پیش از شروع زمان‌بند صدا زده می‌شود تا پاک‌کردن
 *              option byte به تسک در حال اجرا آسیب نزند. در موفقیت، میکرو
 *              داخل فراخوانی BSP ریست می‌شود و تابع برنمی‌گردد؛ مقدار برگشتی
 *              برای تست هاست و مسیر شکست است.
 * @return flash_lock_result_t [EN] ALREADY_PROTECTED when nothing was needed,
 *         JUST_LOCKED when the reset was requested, FAILED when programming
 *         failed (the board then continues to boot unlocked) /
 *         [FA] ALREADY_PROTECTED وقتی کاری لازم نبود، JUST_LOCKED وقتی ریست
 *         درخواست شد، FAILED وقتی نوشتن شکست خورد (برد باز و بدون قفل بالا
 *         می‌آید)
 */
flash_lock_result_t func__FlashLock_ApplyOnce(void);

#endif /* FLASH_LOCK_H */
