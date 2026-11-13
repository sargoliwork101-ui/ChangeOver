/**
 * @file    flash_lock.c
 * @brief   [EN] One-time read-out protection policy. Three steps and no state:
 *              ask the BSP whether the part is open, ask it to program level 1,
 *              ask it to reload and reset. Nothing is stored, so a power cut
 *              between the steps leaves the part exactly as it was - the next
 *              boot simply repeats the same question.
 * @note    [EN] This module never runs unless App_Init calls it, and App_Init
 *              only calls it when MODULE_FLASH_LOCK = 1 (default 0).
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
    if (func__FlashLock_IsProtected())
    {
        return FLASH_LOCK_RESULT_ALREADY_PROTECTED;
    }
    /* [EN] Step 2: program the option bytes. A failure leaves the part open,
       which is the safe direction - the board must still boot. */
    if (!func__BspFlashLock_ApplyLevel1())
    {
        return FLASH_LOCK_RESULT_FAILED;
    }
    /* [EN] Step 3: reload the option bytes. This resets the MCU; the return
       below is reached only under the host stub. */
    func__BspFlashLock_LaunchReset();
    return FLASH_LOCK_RESULT_JUST_LOCKED;
}
