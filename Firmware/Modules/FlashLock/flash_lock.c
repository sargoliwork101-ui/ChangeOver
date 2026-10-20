/**
 * @file    flash_lock.c
 * @brief   [EN] One-time read-out protection policy. Three steps and no state:
 *              ask the BSP whether the part is open, ask it to program level 1,
 *              ask it to reload and reset. Nothing is stored, so a power cut
 *              between the steps leaves the part exactly as it was - the next
 *              boot simply repeats the same question.
 *          [FA] سیاست قفل یک‌بارمصرف خواندن حافظه. سه گام و بدون حالت:
 *              پرسیدن از BSP که قطعه باز است یا نه، درخواست نوشتن سطح ۱،
 *              درخواست بارگذاری و ریست. هیچ چیزی ذخیره نمی‌شود، پس قطع برق
 *              بین گام‌ها قطعه را همان‌طور که بود می‌گذارد و بوت بعدی همان
 *              پرسش را تکرار می‌کند.
 *
 * @note    [EN] This module never runs unless App_Init calls it, and App_Init
 *              only calls it when MODULE_FLASH_LOCK = 1 (default 0).
 *          [FA] این ماژول تا وقتی App_Init صدایش نزند اجرا نمی‌شود، و
 *              App_Init فقط وقتی صدایش می‌زند که ‎MODULE_FLASH_LOCK = 1‎ باشد
 *              (پیش‌فرض ۰).
 */

#include "flash_lock.h"

#include "bsp_flash_lock.h"

/* ==================== FlashLock_IsProtected ==================== */

bool func__FlashLock_IsProtected(void)
{
    return (BSP_FLASH_LOCK_LEVEL_PROTECTED == func__BspFlashLock_GetLevel());
}

/* ==================== FlashLock_ApplyOnce ==================== */

flash_lock_result_t func__FlashLock_ApplyOnce(void)
{
    /* [EN] Step 1: already locked means the one-time action is done forever. */
    /* [FA] گام ۱: قفل‌بودن یعنی اقدام یک‌باره برای همیشه انجام شده است. */
    if (func__FlashLock_IsProtected())
    {
        return FLASH_LOCK_RESULT_ALREADY_PROTECTED;
    }

    /* [EN] Step 2: program the option bytes. A failure leaves the part open,
       which is the safe direction - the board must still boot. */
    /* [FA] گام ۲: نوشتن option byte. شکست، قطعه را باز می‌گذارد؛ این جهت
       ایمن است - برد باید همچنان بالا بیاید. */
    if (!func__BspFlashLock_ApplyLevel1())
    {
        return FLASH_LOCK_RESULT_FAILED;
    }

    /* [EN] Step 3: reload the option bytes. This resets the MCU; the return
       below is reached only under the host stub. */
    /* [FA] گام ۳: بارگذاری دوبارهٔ option byte. این کار میکرو را ریست می‌کند؛
       رسیدن به return پایین فقط با بدل تست هاست ممکن است. */
    func__BspFlashLock_LaunchReset();

    return FLASH_LOCK_RESULT_JUST_LOCKED;
}
