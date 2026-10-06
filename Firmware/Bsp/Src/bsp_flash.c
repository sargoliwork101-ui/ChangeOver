/**
 * @file    bsp_flash.c
 * @brief   [EN] STM32F103C8T6 on-chip flash writer for parameter persistence.
 *          [FA] نگارندهٔ فلش رویتراشهٔ STM32F103C8T6 برای ماندگاری پارامترها.
 *
 * @note    [EN] Direct register sequences from RM0008 ch.3 (FPEC): unlock
 *              via KEYR, page erase PER+AR+STRT, halfword program PG,
 *              re-lock. Status polled from SR (BSY/EOP/PGERR/WRPRTERR)
 *              with a bounded spin - no interrupts, no RTOS, no HAL, so
 *              the code runs in any thread and pre-scheduler (record
 *              reads are plain memory reads).
 *          [FA] توالی‌های مستقیم رجیستر از فصل ۳ RM0008 (FPEC): بازکردن
 *              قفل با KEYR، پاک‌کردن صفحه با ‎PER+AR+STRT‎، نوشتن نیم‌کلمه
 *              با PG و قفل مجدد. وضعیت از SR با اسپین محدود خوانده می‌شود
 *              - بدون وقفه، بدون RTOS، بدون HAL، تا در هر تسک و قبل از
 *              زمان‌بند اجرا شود (خواندن رکورد خواندن سادهٔ حافظه است).
 */

#include "bsp_flash.h"

#include "stm32f1xx.h"

/* ==================== BspFlash private constants ==================== */

/* [EN] FPEC unlock keys (RM0008 3.5.2). / کلیدهای بازکردن FPEC. */
#define BSP_FLASH_KEY1                 0x45670123u
#define BSP_FLASH_KEY2                 0xCDEF89ABu

/* [EN] Generous busy-spin bound: a 1 KiB page erase is typ. 20 ms / max 40 ms
 *      at 72 MHz; the bound covers a slow part with margin and cannot hang
 *      forever.
 * [FA] سقف سخاوتمندانهٔ اسپین: پاک‌کردن صفحهٔ ۱KB معمولاً ۲۰ و حداکثر ۴۰ms
 *      در ۷۲MHz است؛ این سقف قطعهٔ کند را با حاشیه می‌پوشاند و بی‌نهایت
 *      نمی‌چرخد. */
#define BSP_FLASH_SPIN_LIMIT           5000000u

/* [EN] Largest single program burst accepted, in halfwords: one 2 KiB NVM
 *      bank. Bigger requests are a caller bug, not a long write.
 * [FA] بزرگ‌ترین نوشتن مجاز بر حسب نیم‌کلمه: یک بانک ۲KB. */
#define BSP_FLASH_PROGRAM_MAX_HALFWORDS 1024u

/* ==================== BspFlash private helpers ==================== */

/**
 * @brief  [EN] Unlock the FPEC when locked (idempotent).
 *         [FA] قفل FPEC را اگر بسته است باز می‌کند (همانی).
 */
static void func__BspFlash_Unlock(void)
{
    if ((FLASH->CR & FLASH_CR_LOCK) != 0u)
    {
        FLASH->KEYR = BSP_FLASH_KEY1;
        FLASH->KEYR = BSP_FLASH_KEY2;
    }
}

/**
 * @brief  [EN] Clear the sticky SR error/EOP flags (write-1-to-clear on F1).
 *         [FA] پرچم‌های چسبان خطا/EOP در SR را پاک می‌کند (نوشتن ۱ برای پاک).
 */
static void func__BspFlash_ClearFlags(void)
{
    FLASH->SR = (FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR);
}

/**
 * @brief  [EN] Bounded wait for BSY to drop; clears EOP when it fired.
 *         [FA] انتظار محدود برای پایین‌آمدن BSY؛ EOP را اگر زده شد پاک می‌کند.
 * @‎return bool [EN] true = idle and clean‎ / بی‌کار و پاک
 */
static bool func__BspFlash_WaitIdle(void)
{
    uint32_t uint32_t__spin = BSP_FLASH_SPIN_LIMIT;

    while ((FLASH->SR & FLASH_SR_BSY) != 0u)
    {
        uint32_t__spin--;
        if (uint32_t__spin == 0u)
        {
            return false;
        }
    }

    if ((FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0u)
    {
        func__BspFlash_ClearFlags();
        return false;
    }

    if ((FLASH->SR & FLASH_SR_EOP) != 0u)
    {
        func__BspFlash_ClearFlags();
    }

    return true;
}

/* ==================== BspFlash_ErasePage ==================== */

/**
 * @brief  [EN] Erase one 1 KiB main-flash page. The caller must pass a
 *              page-aligned address inside the writable NVM window. The
 *              sequence is the one the reference manual
 *              requires: unlock, wait for idle, set PER and the page
 *              address, strobe START, wait for the busy flag to drop, check
 *              the error flags, then lock again - the lock is restored on
 *              every exit path, so a failed erase can never leave the flash
 *              controller open. An address outside the region this NVM
 *              layout owns is refused before anything is touched.
 *         [FA] یک صفحهٔ یک‌کیلوبایتی فلش اصلی را پاک می‌کند. فراخواننده باید
 *              آدرس هم‌تراز صفحه را داخل پنجرهٔ قابل‌نوشتن NVM بدهد. ترتیب
 *              کار همانی است که رفرنس‌منوال می‌خواهد: باز کردن قفل، انتظار
 *              بیکاری، ست‌کردن PER و آدرس صفحه، زدن START، انتظار افتادن پرچم
 *              مشغول، بررسی پرچم‌های خطا و بعد قفل دوباره؛ قفل در همهٔ مسیرهای
 *              خروج برگردانده می‌شود تا پاک‌کردن ناموفق هرگز کنترلر فلش را باز
 *              رها نکند. آدرس بیرون از ناحیه‌ای که این چیدمان NVM مالکش است،
 *              پیش از هر دست‌زدنی رد می‌شود.
 * @param  uint32_t__pageAddress [EN] Page-aligned address inside the target
 *                                   page and NVM region /
 *                                   آدرس هم‌تراز صفحه داخل صفحهٔ هدف و ناحیهٔ
 *                                   NVM
 * @return bool [EN] true when the page was erased and no error flag was
 *                   raised, false on a rejected address or a controller
 *                   error / اگر صفحه پاک شد و پرچم خطایی بالا نرفت true،
 *                   و با آدرس ردشده یا خطای کنترلر false
 */
bool func__BspFlash_ErasePage(uint32_t uint32_t__pageAddress)
{
    bool bool__ok;

    /* [EN] Main-flash 1 KiB page granularity; this guard is intentionally
       narrower than the whole 64 KiB bank and covers only the NVM window.
       [FA] دانهٔ صفحهٔ فلش اصلی ۱KB است؛ این گارد عمداً از کل بنک ۶۴KB
       محدودتر است و فقط پنجرهٔ NVM را پوشش می‌دهد. */
    /* [EN] Range + alignment guards (full-program audits 2026-09-26/27):
       only the data window BSP_FLASH_STORAGE_BASE_ADDR..END (bsp_flash.h,
       the single source of truth) may be erased and the address must be
       page-aligned - F1 erases by AR
       content, not by masking, so an unaligned or out-of-range address
       would erase an unintended page. A caller bug must never erase the
       application area.
       [FA] گارد بازه و تراز: فقط پنجرهٔ دادهٔ ‎bsp_flash.h‎ پاک می‌شود و
       آدرس باید تراز باشد - F1 با محتوای AR پاک می‌کند نه
       با ماسک‌کردن، پس آدرس ناتراز یا خارج از بازه صفحهٔ اشتباه را پاک
       می‌کرد و باگ فراخواننده هرگز نباید برنامه را پاک کند. */
    if ((uint32_t__pageAddress < BSP_FLASH_STORAGE_BASE_ADDR) ||
        (uint32_t__pageAddress >
         (BSP_FLASH_STORAGE_END_ADDR - BSP_FLASH_PAGE_SIZE_BYTES)) ||
        ((uint32_t__pageAddress & (BSP_FLASH_PAGE_SIZE_BYTES - 1u)) != 0u))
    {
        return false;
    }

    func__BspFlash_Unlock();
    /* [EN] Do not start an erase while an earlier flash operation is still
       busy. This driver normally waits on every exit, but a bounded preflight
       also protects against a caller or boot-time operation outside this API.
       [FA] پاک‌کردن را تا وقتی عملیات قبلی مشغول است شروع نکن؛ هرچند این
       درایور در خروج منتظر می‌ماند، پیش‌بررسی در برابر عملیات بیرونی هم امن است. */
    if (func__BspFlash_WaitIdle() == false)
    {
        FLASH->CR |= FLASH_CR_LOCK;
        return false;
    }
    func__BspFlash_ClearFlags();

    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = uint32_t__pageAddress;
    FLASH->CR |= FLASH_CR_STRT;

    bool__ok = func__BspFlash_WaitIdle();

    FLASH->CR &= ~FLASH_CR_PER;
    FLASH->CR |= FLASH_CR_LOCK;

    return bool__ok;
}

/* ==================== BspFlash_ProgramHalfWords ==================== */

/**
 * @brief  [EN] Program a run of 16-bit halfwords into main flash. The
 *              STM32F1 flash controller can only be written a halfword at a
 *              time, which is why the public contract is in halfwords and
 *              not bytes. Each write unlocks, sets PG, stores the halfword,
 *              waits for busy to clear and checks the error flags; the
 *              first failure aborts the loop so a half-written record is
 *              never reported as success, and the controller is locked
 *              again on every exit path. Writes are range-checked against
 *              the writable NVM window this layout owns, so a wild address
 *              cannot reach program code.
 *         [FA] رشته‌ای از نیم‌کلمه‌های ۱۶ بیتی را در فلش اصلی می‌نویسد. کنترلر
 *              فلش ‎STM32F1‎ فقط نیم‌کلمه‌ای می‌نویسد و قرارداد عمومی هم به همین
 *              دلیل نیم‌کلمه‌ای است نه بایتی. هر نوشتن قفل را باز می‌کند، PG را
 *              ست می‌کند، نیم‌کلمه را می‌نویسد، منتظر پاک‌شدن پرچم مشغول می‌ماند
 *              و پرچم‌های خطا را می‌بیند؛ اولین شکست حلقه را می‌شکند تا رکورد
 *              نیمه‌نوشته هرگز موفق گزارش نشود و قفل در همهٔ مسیرهای خروج
 *              برمی‌گردد. آدرس در برابر پنجرهٔ قابل‌نوشتن NVM متعلق به این
 *              چیدمان بررسی می‌شود تا آدرس ولگرد به کد برنامه نرسد.
 * @param  uint32_t__address   [EN] Even start address inside the NVM
 *                                 region (halfword aligned) /
 *                                 آدرس شروع زوج داخل ناحیهٔ NVM
 * @param  uint16_t__A__Data   [EN] Source array of halfwords, read-only,
 *                                 must hold at least count entries /
 *                                 آرایهٔ مبدأ نیم‌کلمه‌ها، فقط-خواندنی، باید
 *                                 دست‌کم به تعداد count عضو داشته باشد
 * @param  uint32_t__count     [EN] Number of halfwords to program; zero
 *                                 performs no flash write but still requires
 *                                 a non-NULL source pointer /
 *                                 تعداد نیم‌کلمه‌ها؛ صفر هیچ نوشتنی انجام
 *                                 نمی‌دهد اما اشاره‌گر مبدأ باید NULL نباشد
 * @return bool [EN] true when every halfword was programmed without an
 *                   error flag, false on a rejected range or the first
 *                   controller error /
 *                   اگر همهٔ نیم‌کلمه‌ها بدون پرچم خطا نوشته شدند true، و با
 *                   بازهٔ ردشده یا نخستین خطای کنترلر false
 */
bool func__BspFlash_ProgramHalfWords(uint32_t uint32_t__address,
                                     const uint16_t *uint16_t__A__Data,
                                     uint32_t uint32_t__count)
{
    bool bool__ok = true;

    /* [EN] Range guard (full-program audit 2026-09-27): the NVM layout
       owns BSP_FLASH_STORAGE_BASE_ADDR..END (bsp_flash.h) - a caller bug
       must never program the application area. The upper-bound check is
       written as END - byte_count, not address + byte_count, so a wild
       address cannot wrap through zero and pass the guard.
       [FA] گارد بازه: چیدمان NVM مالک پنجرهٔ دادهٔ ‎bsp_flash.h‎ است - باگ
       فراخواننده هرگز نباید ناحیهٔ برنامه را بنویسد. حد بالایی به‌شکل
       END - تعدادبایت نوشته شده تا آدرس ولگرد با سرریز از صفر عبور نکند. */
    if ((uint16_t__A__Data == NULL) || ((uint32_t__address & 1u) != 0u) ||
        (uint32_t__count > BSP_FLASH_PROGRAM_MAX_HALFWORDS) ||
        (uint32_t__address < BSP_FLASH_STORAGE_BASE_ADDR) ||
        (uint32_t__address >
         (BSP_FLASH_STORAGE_END_ADDR - (uint32_t__count * 2u))))
    {
        return false;
    }

    func__BspFlash_Unlock();
    /* [EN] Refuse to queue a program operation behind an already-busy FPEC;
       the bounded wait keeps this path finite and makes the initial status
       check match the erase path.
       [FA] برنامه‌نویسی را پشت عملیات مشغول FPEC صف نکن؛ انتظار محدود هم
       مسیر را متناهی نگه می‌دارد و هم بررسی اولیه را با پاک‌کردن یکسان می‌کند. */
    if (func__BspFlash_WaitIdle() == false)
    {
        FLASH->CR |= FLASH_CR_LOCK;
        return false;
    }
    func__BspFlash_ClearFlags();

    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__count; uint32_t__i++)
    {
        FLASH->CR |= FLASH_CR_PG;
        *(volatile uint16_t *)(uint32_t__address +
                               (uint32_t__i * 2u)) = uint16_t__A__Data[uint32_t__i];

        if (func__BspFlash_WaitIdle() == false)
        {
            bool__ok = false;
            break;
        }
    }

    FLASH->CR &= ~FLASH_CR_PG;
    FLASH->CR |= FLASH_CR_LOCK;

    return bool__ok;
}
