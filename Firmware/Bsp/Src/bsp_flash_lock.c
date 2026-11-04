/**
 * @file    bsp_flash_lock.c
 * @brief   [EN] Option-byte access for read-out protection (RDP level 1) on the
 *              STM32F103C8T6. Uses the vendor HAL option-byte path because the
 *              sequence (unlock FLASH, unlock OPTKEYR, erase the option block,
 *              program RDP, program USER) is easy to get subtly wrong and the
 *              vendor code in this repository is the reference for it.
 * @note    [EN] Why the USER byte is carried through: HAL's RDP path calls the
 *              static FLASH_OB_RDP_LevelConfig(), which erases the ENTIRE
 *              option-byte block before writing 0xA5 into the RDP byte. Erased
 *              option bytes are not "neutral" on F1 - they change IWDG source,
 *              nRST_STOP and nRST_STDBY. So the current USER value is read with
 *              HAL_FLASHEx_OBGetConfig() and programmed back in the same call.
 *              Order matters: HAL_FLASHEx_OBProgram handles RDP first (the
 *              erase) and USER second (the program), which is the only order
 *              that leaves the USER byte written.
 * @note    [EN] Not covered by any host test: the option-byte sequence cannot
 *              run on a PC. The host test of the FlashLock module stubs these
 *              three functions and proves one-time behaviour only; the real
 *              sequence must be proven on the bench (see the module workbook).
 */
#include "bsp_flash_lock.h"
#include "stm32f1xx_hal.h"
/* ==================== BspFlashLock_GetLevel ==================== */
bsp_flash_lock_level_t func__BspFlashLock_GetLevel(void)
{
    /* [EN] RDPRT is readable at both levels and is the same bit the vendor
       code inspects, so no HAL call is needed for a read-only question. */
    if ((FLASH->OBR & FLASH_OBR_RDPRT) != 0u)
    {
        return BSP_FLASH_LOCK_LEVEL_PROTECTED;
    }
    return BSP_FLASH_LOCK_LEVEL_OPEN;
}
/* ==================== BspFlashLock_ApplyLevel1 ==================== */
bool func__BspFlashLock_ApplyLevel1(void)
{
    FLASH_OBProgramInitTypeDef flash_ob;
    HAL_StatusTypeDef hal_status;
    uint8_t uint8_t__userConfigBefore;
    if (BSP_FLASH_LOCK_LEVEL_PROTECTED == func__BspFlashLock_GetLevel())
    {
        /* [EN] Already locked: never touch the option bytes again. */
        return true;
    }
    /* [EN] Remember the user options before the erase that the RDP path does. */
    HAL_FLASHEx_OBGetConfig(&flash_ob);
    uint8_t__userConfigBefore = flash_ob.USERConfig;
    if (HAL_OK != HAL_FLASH_Unlock())
    {
        return false;
    }
    if (HAL_OK != HAL_FLASH_OB_Unlock())
    {
        (void)HAL_FLASH_Lock();
        return false;
    }
    flash_ob.OptionType = OPTIONBYTE_RDP | OPTIONBYTE_USER;
    flash_ob.RDPLevel = OB_RDP_LEVEL_1;
    flash_ob.USERConfig = uint8_t__userConfigBefore;
    hal_status = HAL_FLASHEx_OBProgram(&flash_ob);
    (void)HAL_FLASH_OB_Lock();
    (void)HAL_FLASH_Lock();
    return (HAL_OK == hal_status);
}
/* ==================== BspFlashLock_LaunchReset ==================== */
void func__BspFlashLock_LaunchReset(void)
{
    /* [EN] Reloads option bytes and resets; control never returns here. */
    HAL_FLASH_OB_Launch();
}
