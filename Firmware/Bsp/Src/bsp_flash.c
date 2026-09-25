/**
 * @file    bsp_flash.c
 * @brief   [EN] STM32F103C8T6 on-chip flash writer for parameter persistence.
 *          [FA] نگارندهٔ فلش رویتراشهٔ STM32F103C8T6 برای ماندگاری پارامترها.
 *
 * @note    [EN] Direct register sequences from RM0008 chapter 3 (FPEC):
 *              unlock via KEYR 0x45670123 / 0xCDEF89AB, page erase
 *              PER+AR+STRT, halfword program PG, then re-lock. Status is
 *              polled from SR (BSY / EOP / PGERR / WRPRTERR) with a bounded
 *              spin - no interrupts, no RTOS, no HAL dependency, so the same
 *              code runs in any thread and in the pre-scheduler boot phase
 *              (reads of the record are plain memory reads and need nothing
 *              from this file).
 *          [FA] توالی‌های مستقیم رجیستر از فصل ۳ RM0008 (FPEC): بازکردن قفل
 *              با KEYR، پاک‌کردن صفحه با PER+AR+STRT، نوشتن نیم‌کلمه با PG و
 *              قفل مجدد. وضعیت از SR خوانده می‌شود (BSY/EOP/PGERR/WRPRTERR)
 *              با اسپین محدود - بدون وقفه، بدون RTOS، بدون وابستگی به HAL،
 *              تا همین کد در هر تسک و در فاز بوتِ قبل از زمان‌بند اجرا شود
 *              (خواندن رکورد خواندن حافظهٔ ساده است و چیزی از این فایل
 *              نمی‌خواهد).
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
 * @return bool [EN] true = idle and clean / بی‌کار و پاک
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

bool func__BspFlash_ErasePage(uint32_t uint32_t__pageAddress)
{
    bool bool__ok;

    /* [EN] Main-flash 1 KiB page granularity, whole 64 KiB bank.
       [FA] اندازهٔ صفحهٔ فلش اصلی ۱KB است، کل بنک ۶۴KB. */
    if (uint32_t__pageAddress > 0x0800FC00u)
    {
        return false;
    }

    func__BspFlash_Unlock();
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

bool func__BspFlash_ProgramHalfWords(uint32_t uint32_t__address,
                                     const uint16_t *uint16_t__A__Data,
                                     uint32_t uint32_t__count)
{
    bool bool__ok = true;

    if ((uint16_t__A__Data == NULL) || ((uint32_t__address & 1u) != 0u))
    {
        return false;
    }

    func__BspFlash_Unlock();
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
