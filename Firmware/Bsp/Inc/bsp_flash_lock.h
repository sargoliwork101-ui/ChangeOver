/**
 * @file    bsp_flash_lock.h
 * @brief   [EN] Read-out protection boundary (RDP level 1) of the STM32F103.
 *              This is the only place in the tree allowed to touch the option
 *              bytes; the FlashLock module only asks "protected or not" and
 *              "lock it now".
 * @note    [EN] Hardware facts this header hides (verified in the CMSIS device
 *              header and the vendor HAL in this repository):
 *              1) OBR.RDPRT (bit 1) is readable at any protection level and
 *                 reads 1 when the part is protected.
 *              2) Reaching level 1 from level 0 REQUIRES erasing the whole
 *                 option-byte block first: 0xAA -> 0xA5 needs bit sets and
 *                 programming can only clear bits. That erase is why the
 *                 USER byte (IWDG source, nRST_STOP, nRST_STDBY) is read
 *                 before and written back after - losing it would silently
 *                 enable the hardware watchdog or change reset behaviour.
 *              3) The new level only takes effect after a system reset, so
 *                 the caller decides when to reset (func__...LaunchReset).
 *              Consequence on this part: once locked, every later SWD
 *              session starts with a full chip erase.
 */
#ifndef BSP_FLASH_LOCK_H
#define BSP_FLASH_LOCK_H
#include <stdbool.h>
/* ==================== Read protection level ==================== */
/**
 * @brief  [EN] The two protection states this board can be in.
 */
typedef enum
{
    BSP_FLASH_LOCK_LEVEL_OPEN = 0,
    BSP_FLASH_LOCK_LEVEL_PROTECTED = 1
} bsp_flash_lock_level_t;
/* ==================== BspFlashLock_GetLevel ==================== */
/**
 * @brief  [EN] Current read-out protection level, read straight from OBR.
 *         Non-destructive: works at both levels and needs no unlock.
 * @return bsp_flash_lock_level_t [EN] OPEN when RDPRT = 0, PROTECTED when 1 /
 */
bsp_flash_lock_level_t func__BspFlashLock_GetLevel(void);
/* ==================== BspFlashLock_ApplyLevel1 ==================== */
/**
 * @brief  [EN] Program RDP level 1, preserving the USER option byte.
 *         Blocking, thread context only (a full option-byte erase stalls code
 *         fetch, exactly like a flash page erase). The new level is NOT active
 *         until the next system reset.
 * @return bool [EN] true when the option bytes were programmed, false when
 *         unlock or programming failed (the part then stays OPEN) /
 */
bool func__BspFlashLock_ApplyLevel1(void);
/* ==================== BspFlashLock_LaunchReset ==================== */
/**
 * @brief  [EN] Reload the option bytes and reset the MCU. Does not return.
 */
void func__BspFlashLock_LaunchReset(void);
#endif /* BSP_FLASH_LOCK_H */
