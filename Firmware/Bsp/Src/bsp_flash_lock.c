/**
 * @file    bsp_flash_lock.c
 * @brief   [EN] Option-byte access for read-out protection (RDP level 1) on the
 *              STM32F103C8T6. Uses the vendor HAL option-byte path because the
 *              sequence (unlock FLASH, unlock OPTKEYR, erase the option block,
 *              program RDP, program USER) is easy to get subtly wrong and the
 *              vendor code in this repository is the reference for it.
 *          [FA] دسترسی به option byte برای قفل خواندن (RDP سطح ۱) روی
 *              STM32F103C8T6. از مسیر option byte همان HAL استفاده می‌شود،
 *              چون ترتیب کار (بازکردن فلش، بازکردن OPTKEYR، پاک‌کردن بلوک
 *              option، نوشتن RDP، نوشتن USER) به‌راحتی اشتباه می‌شود و کد
 *              خود سازنده در همین مخزن مرجع آن است.
 *
 * @note    [EN] Why the USER byte is carried through: HAL's RDP path calls the
 *              static FLASH_OB_RDP_LevelConfig(), which erases the ENTIRE
 *              option-byte block before writing 0xA5 into the RDP byte. Erased
 *              option bytes are not "neutral" on F1 - they change IWDG source,
 *              nRST_STOP and nRST_STDBY. So the current USER value is read with
 *              HAL_FLASHEx_OBGetConfig() and programmed back in the same call.
 *              Order matters: HAL_FLASHEx_OBProgram handles RDP first (the
 *              erase) and USER second (the program), which is the only order
 *              that leaves the USER byte written.
 *          [FA] چرا بایت USER همراه برده می‌شود: مسیر RDP در HAL تابع
 *              داخلی FLASH_OB_RDP_LevelConfig را صدا می‌زند که پیش از نوشتن
 *              0xA5 کل بلوک option byte را پاک می‌کند. option byte پاک‌شده در
 *              F1 «بی‌اثر» نیست - منبع IWDG و nRST_STOP و nRST_STDBY را عوض
 *              می‌کند. پس مقدار فعلی USER با HAL_FLASHEx_OBGetConfig خوانده
 *              و در همان فراخوانی دوباره نوشته می‌شود. ترتیب مهم است: HAL اول
 *              RDP (همان پاک‌کردن) و بعد USER را می‌نویسد؛ تنها ترتیبی که
 *              بایت USER نوشته‌شده باقی می‌ماند همین است.
 *
 * @note    [EN] Not covered by any host test: the option-byte sequence cannot
 *              run on a PC. The host test of the FlashLock module stubs these
 *              three functions and proves one-time behaviour only; the real
 *              sequence must be proven on the bench (see the module workbook).
 *          [FA] این فایل تست هاست ندارد: ترتیب option byte روی کامپیوتر
 *              اجرا نمی‌شود. تست هاستِ ماژول FlashLock این سه تابع را بدل
 *              می‌کند و فقط «یک‌بارمصرف بودن» را ثابت می‌کند؛ خودِ ترتیب
 *              باید روی میز تست اثبات شود (کاربرگ همان ماژول).
 */

#include "bsp_flash_lock.h"

#include "stm32f1xx_hal.h"

/* ==================== BspFlashLock_GetLevel ==================== */

bsp_flash_lock_level_t func__BspFlashLock_GetLevel(void)
{
    /* [EN] RDPRT is readable at both levels and is the same bit the vendor
       code inspects, so no HAL call is needed for a read-only question. */
    /* [FA] بیت RDPRT در هر دو سطح خواندنی است و همان بیتی است که کد سازنده
       نگاه می‌کند؛ پس برای یک سؤال فقط‌خواندنی، فراخوانی HAL لازم نیست. */
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
        /* [FA] از قبل قفل است: دیگر به option byte دست نمی‌زنیم. */
        return true;
    }

    /* [EN] Remember the user options before the erase that the RDP path does. */
    /* [FA] ذخیرهٔ گزینه‌های کاربر پیش از پاک‌کردنی که مسیر RDP انجام می‌دهد. */
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
    /* [FA] option byte را دوباره بار می‌کند و ریست می‌دهد؛ کنترل برنمی‌گردد. */
    HAL_FLASH_OB_Launch();
}
