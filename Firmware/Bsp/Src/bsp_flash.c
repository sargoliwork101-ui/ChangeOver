/**
 * @file    bsp_flash.c
 * @brief   [EN] STM32F103C8T6 on-chip flash writer for parameter persistence.
 * @note    [EN] Direct register sequences from RM0008 ch.3 (FPEC): unlock
 *              via KEYR, page erase PER+AR+STRT, halfword program PG,
 *              re-lock. Status polled from SR (BSY/EOP/PGERR/WRPRTERR)
 *              with a bounded spin - no interrupts, no RTOS, no HAL, so
 *              the code runs in any thread and pre-scheduler (record
 *              reads are plain memory reads).
 */
#include "bsp_flash.h"
#include "stm32f1xx.h"
#include <stdint.h>   /* [EN] uintptr_t for the portable address->pointer cast. */
/* ==================== BspFlash private constants ==================== */
#define BSP_FLASH_KEY1                 0x45670123u
#define BSP_FLASH_KEY2                 0xCDEF89ABu
/*
 * [EN] Generous busy-spin bound: a 1 KiB page erase is typ. 20 ms / max 40 ms
 *      at 72 MHz; the bound covers a slow part with margin and cannot hang
 *      forever.
 */
#define BSP_FLASH_SPIN_LIMIT           5000000u
/*
 * [EN] Largest single program burst accepted, in halfwords: one 2 KiB NVM
 *      bank. Bigger requests are a caller bug, not a long write.
 */
#define BSP_FLASH_PROGRAM_MAX_HALFWORDS 1024u
/* ==================== BspFlash private helpers ==================== */
/**
 * @brief  [EN] Unlock the FPEC when locked (idempotent).
 */
static void func__BspFlash_Unlock(void)
{
    if ((FLASH->CR & FLASH_CR_LOCK) != 0u)
    {
        FLASH->KEYR = BSP_FLASH_KEY1;
        FLASH->KEYR = BSP_FLASH_KEY2;
    }
}
/* ==================== BspFlash_ClearFlags ==================== */
/**
 * @brief  [EN] Clear the sticky SR error/EOP flags (write-1-to-clear on F1).
 */
static void func__BspFlash_ClearFlags(void)
{
    FLASH->SR = (FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR);
}
/* ==================== BspFlash_WaitIdle ==================== */
/**
 * @brief  [EN] Bounded wait for BSY to drop; clears EOP when it fired.
* @return bool true = idle and clean /
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
 * @param  uint32_t__pageAddress [EN] Page-aligned address inside the target
 *                                   page and NVM region /
 *                                   NVM
 * @return bool [EN] true when the page was erased and no error flag was
 *                   raised, false on a rejected address or a controller
 */
bool func__BspFlash_ErasePage(uint32_t uint32_t__pageAddress)
{
    bool bool__ok;
    /*
 * [EN] Main-flash 1 KiB page granularity; this guard is intentionally
 * narrower than the whole 64 KiB bank and covers only the NVM window.
 */
    /*
 * [EN] Range + alignment guards (full-program audits 2026-09-26/27):
 * only the data window BSP_FLASH_STORAGE_BASE_ADDR..END (bsp_flash.h,
 * the single source of truth) may be erased and the address must be
 * page-aligned - F1 erases by AR
 * content, not by masking, so an unaligned or out-of-range address
 * would erase an unintended page. A caller bug must never erase the
 * application area.
 */
    if ((uint32_t__pageAddress < BSP_FLASH_STORAGE_BASE_ADDR) ||
        (uint32_t__pageAddress >
         (BSP_FLASH_STORAGE_END_ADDR - BSP_FLASH_PAGE_SIZE_BYTES)) ||
        ((uint32_t__pageAddress & (BSP_FLASH_PAGE_SIZE_BYTES - 1u)) != 0u))
    {
        return false;
    }
    func__BspFlash_Unlock();
    /*
 * [EN] Do not start an erase while an earlier flash operation is still
 * busy. This driver normally waits on every exit, but a bounded preflight
 * also protects against a caller or boot-time operation outside this API.
 */
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
    FLASH->CR &= (uint32_t)(~FLASH_CR_PER);
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
 * @param  uint32_t__address   [EN] Even start address inside the NVM
 *                                 region (halfword aligned) /
 * @param  uint16_t__A__Data   [EN] Source array of halfwords, read-only,
 *                                 must hold at least count entries /
 * @param  uint32_t__count     [EN] Number of halfwords to program; zero
 *                                 performs no flash write but still requires
 *                                 a non-NULL source pointer /
 * @return bool [EN] true when every halfword was programmed without an
 *                   error flag, false on a rejected range or the first
 *                   controller error /
 */
bool func__BspFlash_ProgramHalfWords(uint32_t uint32_t__address,
                                     const uint16_t *uint16_t__A__Data,
                                     uint32_t uint32_t__count)
{
    bool bool__ok = true;
    /*
 * [EN] Range guard (full-program audit 2026-09-27): the NVM layout
 * owns BSP_FLASH_STORAGE_BASE_ADDR..END (bsp_flash.h) - a caller bug
 * must never program the application area. The upper-bound check is
 * written as END - byte_count, not address + byte_count, so a wild
 * address cannot wrap through zero and pass the guard.
 */
    if ((uint16_t__A__Data == NULL) || ((uint32_t__address & 1u) != 0u) ||
        (uint32_t__count > BSP_FLASH_PROGRAM_MAX_HALFWORDS) ||
        (uint32_t__address < BSP_FLASH_STORAGE_BASE_ADDR) ||
        (uint32_t__address >
         (BSP_FLASH_STORAGE_END_ADDR - (uint32_t__count * 2u))))
    {
        return false;
    }
    func__BspFlash_Unlock();
    /*
 * [EN] Refuse to queue a program operation behind an already-busy FPEC;
 * the bounded wait keeps this path finite and makes the initial status
 * check match the erase path.
 */
    if (func__BspFlash_WaitIdle() == false)
    {
        FLASH->CR |= FLASH_CR_LOCK;
        return false;
    }
    func__BspFlash_ClearFlags();
    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__count; uint32_t__i++)
    {
        FLASH->CR |= FLASH_CR_PG;
        /*
 * [EN] uintptr_t keeps the flash-address -> pointer cast exact on
 * every width (32-bit target: identical code; wider host builds for
 * the module test suites: no size-mismatch warning).
 */
        *(volatile uint16_t *)(uintptr_t)(uint32_t__address +
                                          (uint32_t__i * 2u)) =
            uint16_t__A__Data[uint32_t__i];
        if (func__BspFlash_WaitIdle() == false)
        {
            bool__ok = false;
            break;
        }
    }
    FLASH->CR &= (uint32_t)(~FLASH_CR_PG);
    FLASH->CR |= FLASH_CR_LOCK;
    return bool__ok;
}
