/**
 * @file    flash_lock.h
 * @brief   [EN] One-shot read-out lock for the shipped board: on the first boot
 *              of a build that has MODULE_FLASH_LOCK = 1, program RDP level 1
 *              and reset. Every later boot finds the part already locked and
 *              does nothing, so the lock is a one-time action and never a
 *              boot-time loop.
 * @note    [EN] Delivery state is MODULE_FLASH_LOCK = 1: the first boot programs
 *              RDP level 1 and resets; later boots leave protection untouched.
 *              Turning it on costs the developer SWD access on that part -
 *              removing protection for reflashing requires a full chip erase,
 *              which also wipes the parameter bank and the calibration table.
 *              That is why the switch lives in modules_enable.h next to the
 *              other compile-time decisions and not in a panel parameter.
 * @note    [EN] Hardware details live in bsp_flash_lock.h; this module owns the
 *              policy only (when to ask, and what counts as success).
 */
#ifndef FLASH_LOCK_H
#define FLASH_LOCK_H
#include <stdbool.h>
/* ==================== FlashLock Result ==================== */
/**
 * @brief  [EN] What func__FlashLock_ApplyOnce() found or did.
 */
typedef enum
{
    FLASH_LOCK_RESULT_ALREADY_PROTECTED = 0,
    FLASH_LOCK_RESULT_JUST_LOCKED = 1,
    FLASH_LOCK_RESULT_FAILED = 2
} flash_lock_result_t;
/* ==================== FlashLock_IsProtected ==================== */
/**
 * @brief  [EN] Read-only query of the current protection state.
* @return bool true when RDP level 1 is active / true RDP
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
 * @return flash_lock_result_t [EN] ALREADY_PROTECTED when nothing was needed,
 *         JUST_LOCKED when the reset was requested, FAILED when programming
 *         failed (the board then continues to boot unlocked) /
 */
flash_lock_result_t func__FlashLock_ApplyOnce(void);
#endif /* FLASH_LOCK_H */
