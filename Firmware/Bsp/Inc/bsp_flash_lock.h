/**
 * @file    bsp_flash_lock.h
 * @brief   [EN] Read-out protection boundary (RDP level 1) of the STM32F103.
 *              This is the only place in the tree allowed to touch the option
 *              bytes; the FlashLock module only asks "protected or not" and
 *              "lock it now".
 *          [FA] مرز قفل خواندن حافظه (RDP سطح ۱) روی STM32F103. تنها جایی در
 *              پروژه که به option byte دست می‌زند همین‌جاست؛ ماژول FlashLock
 *              فقط می‌پرسد «قفل است یا نه» و «همین حالا قفل کن».
 *
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
 *          [FA] واقعیت‌های سخت‌افزاری که این هدر پنهان می‌کند: بیت RDPRT در
 *              OBR همیشه خواندنی است؛ رسیدن از سطح ۰ به ۱ نیازمند پاک‌کردن
 *              کل بلوک option byte است (چون 0xAA به 0xA5 نیاز به یک‌کردن
 *              بیت دارد و برنامه‌ریزی فقط بیت را صفر می‌کند)؛ به همین دلیل
 *              بایت USER قبلش خوانده و بعدش دوباره نوشته می‌شود، وگرنه
 *              watchdog سخت‌افزاری بی‌صدا روشن می‌شد یا رفتار ریست عوض
 *              می‌شد. سطح تازه فقط با ریست اعمال می‌شود.
 */

#ifndef BSP_FLASH_LOCK_H
#define BSP_FLASH_LOCK_H

#include <stdbool.h>

/* ==================== Read protection level / سطح قفل خواندن ==================== */

/**
 * @brief  [EN] The two protection states this board can be in.
 *         [FA] دو حالتی که این برد می‌تواند داشته باشد.
 */
typedef enum
{
    BSP_FLASH_LOCK_LEVEL_OPEN = 0,      /* [EN] Level 0: SWD can read flash. / [FA] سطح ۰: SWD فلش را می‌خواند. */
    BSP_FLASH_LOCK_LEVEL_PROTECTED = 1  /* [EN] Level 1: SWD reads are refused. / [FA] سطح ۱: خواندن با SWD رد می‌شود. */
} bsp_flash_lock_level_t;

/* ==================== BspFlashLock_GetLevel ==================== */

/**
 * @brief  [EN] Current read-out protection level, read straight from OBR.
 *         Non-destructive: works at both levels and needs no unlock.
 *         [FA] سطح فعلی قفل خواندن، مستقیم از ثبات OBR. مخرب نیست: در هر دو
 *         سطح کار می‌کند و بازکردن قفل لازم ندارد.
 * @return bsp_flash_lock_level_t [EN] OPEN when RDPRT = 0, PROTECTED when 1 /
 *         [FA] OPEN وقتی RDPRT صفر است، PROTECTED وقتی یک است
 */
bsp_flash_lock_level_t func__BspFlashLock_GetLevel(void);

/* ==================== BspFlashLock_ApplyLevel1 ==================== */

/**
 * @brief  [EN] Program RDP level 1, preserving the USER option byte.
 *         Blocking, thread context only (a full option-byte erase stalls code
 *         fetch, exactly like a flash page erase). The new level is NOT active
 *         until the next system reset.
 *         [FA] نوشتن RDP سطح ۱ با حفظ بایت USER. مسدودکننده و فقط در بافت
 *         تسک (پاک‌کردن بلوک option byte هم مثل پاک‌کردن صفحهٔ فلش،
 *         fetch کد را نگه می‌دارد). سطح تازه تا ریست بعدی فعال نیست.
 * @return bool [EN] true when the option bytes were programmed, false when
 *         unlock or programming failed (the part then stays OPEN) /
 *         [FA] true وقتی option byte نوشته شد، false وقتی بازکردن قفل یا
 *         نوشتن شکست خورد (در این حالت قطعه OPEN می‌ماند)
 */
bool func__BspFlashLock_ApplyLevel1(void);

/* ==================== BspFlashLock_LaunchReset ==================== */

/**
 * @brief  [EN] Reload the option bytes and reset the MCU. Does not return.
 *         [FA] بارگذاری دوبارهٔ option byte و ریست میکرو. برنمی‌گردد.
 */
void func__BspFlashLock_LaunchReset(void);

#endif /* BSP_FLASH_LOCK_H */
