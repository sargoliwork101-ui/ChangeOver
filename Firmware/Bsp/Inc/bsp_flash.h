/**
 * @file    bsp_flash.h
 * @brief   [EN] STM32F103C8T6 on-chip flash writer for parameter persistence.
 * @note    [EN] Direct F1 flash-register driver (KEYR/CR/SR/AR), no HAL.
 *              A page erase on the single-bank F1 stalls every code fetch
 *              from flash for typ. 20..40 ms - callers must be in THREAD
 *              context, never an ISR (user order 2026-09-25: panel values
 *              must survive power loss; the save runs in the comm task
 *              after the 1.5 s debounce, so one regulation hiccup is
 *              acceptable).
 */
#ifndef BSP_FLASH_H
#define BSP_FLASH_H
#include <stdint.h>
#include <stdbool.h>
/* ==================== Writable storage window ==================== */
/*
 * [EN] SINGLE SOURCE OF TRUTH for every byte of flash this driver is allowed
 *      to erase or program: the data area above the application image, i.e.
 *      0x0800E000..0x0800FFFF (the last 8 KiB of the 64 KiB part, matching
 *      the NVM + LUTNVM + SPARE regions of STM32F103C8TX_FLASH.ld). The two
 *      owners - EspLink parameter records and the CalLut bench table - sit
 *      inside it and each one _Static_asserts that it does, so moving a bank
 *      without widening this window breaks the BUILD.
 *      Bug fixed 2026-10-05: the guard used to be the hard-coded pair
 *      0x0800F800/0x0800FC00. When v1.80 moved the parameter bank down to
 *      0x0800E000 the guard stayed put, so every erase was refused and the
 *      save silently failed on the board - invisible to the host tests,
 *      which emulate flash in RAM. Deriving the window instead of retyping
 *      an address is what makes that class of drift impossible.
 */
#define BSP_FLASH_STORAGE_BASE_ADDR    0x0800E000u
/*
 * [EN] One past the last writable byte (end of the 64 KiB part).
 */
#define BSP_FLASH_STORAGE_END_ADDR     0x08010000u
/*
 * [EN] Hardware erase granularity of the F1 medium-density part: 1 KiB.
 */
#define BSP_FLASH_PAGE_SIZE_BYTES      0x400u
/**
 * @brief  [EN] Erase one 1 KiB flash page (page-aligned address inside the
 *         writable NVM window). Blocking; thread context only.
 * @param  uint32_t__pageAddress [EN] Page-aligned address inside the target
* @return bool true on success /
 */
bool func__BspFlash_ErasePage(uint32_t uint32_t__pageAddress);
/**
 * @brief  [EN] Program a run of halfwords to main flash. The complete range
 *         must stay inside the writable NVM window; the caller owns page
 *         erase ordering and unaligned addresses are refused.
* @param uint32_t__address Even start address /
* @param uint16_t__A__Data Halfword source array /
 * @param  uint32_t__count [EN] Halfword count; zero performs no write, but
* @return bool true on success /
 */
bool func__BspFlash_ProgramHalfWords(uint32_t uint32_t__address,
                                     const uint16_t *uint16_t__A__Data,
                                     uint32_t uint32_t__count);
#endif /* BSP_FLASH_H */
