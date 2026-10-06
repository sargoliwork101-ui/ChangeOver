/**
 * @file    jitter.c
 * @brief   [EN] Implementation of the LM393 trip-flag latch: it polls the
 *               EXTI software flags of the board layer, latches one sticky
 *               boolean per channel and offers per-channel and global
 *               clears. No HAL and no pin numbers appear here - which EXTI
 *               line belongs to which comparator is the board port's job.
 *          [FA] پیاده‌سازی قفلِ پرچم تریپ LM393: پرچم‌های نرم‌افزاری EXTI لایهٔ
 *               برد را می‌خواند، برای هر کانال یک بولینِ چسبنده قفل می‌کند و
 *               پاک‌کردن تکی و کلی می‌دهد. هیچ HAL و شمارهٔ پایه‌ای اینجا نیست؛
 *               اینکه کدام خط EXTI به کدام مقایسه‌گر می‌رود کار پورت برد است.
 * @note    [EN] Full-program audit 2026-10-05: the stale "placeholder"
 *               label of the old header line was removed.
 *          [FA] ممیزی ۲۰۲۶-۱۰-۰۵: برچسب کهنهٔ «اسکلت» از سرخط قبلی برداشته شد.
 */

#include "jitter.h"
#include "bsp_exti.h"

static bool BOOL__G__Trip[2];

/**
 * @brief  [EN] Clear trip flags and EXTI software flags.
 *         [FA] پرچم تریپ و EXTI را پاک می‌کند.
 */
/* ==================== Jitter_Init ==================== */

void func__Jitter_Init(void)
{
    BOOL__G__Trip[0] = false;
    BOOL__G__Trip[1] = false;
    func__BspExti_Init();
}

/**
 * @brief  [EN] Latch trip if an EXTI event was taken.
 *         [FA] اگر رویداد EXTI آمده باشد تریپ را قفل می‌کند.
 */
/* ==================== Jitter_Run ==================== */

void func__Jitter_Run(void)
{
    if (func__BspExti_TakeEvent(BSP_EXTI_JITTER1) == true)
    {
        BOOL__G__Trip[0] = true;
    }
    if (func__BspExti_TakeEvent(BSP_EXTI_JITTER2) == true)
    {
        BOOL__G__Trip[1] = true;
    }
}

/**
 * @brief  [EN] True if channel 1 or 2 has latched a trip.
 *         [FA] اگر کانال ۱ یا ۲ تریپ قفل کرده باشد true.
 * @param  uint8_t__channel [EN] Channel number 1 or 2, other values return false / شماره کانال ۱ یا ۲
 * @return bool [EN] true if tripped / اگر تریپ کرده true
 */
/* ==================== Jitter_ChannelTripped ==================== */

bool func__Jitter_ChannelTripped(uint8_t uint8_t__channel)
{
    if (uint8_t__channel == 1u)
    {
        return BOOL__G__Trip[0];
    }
    if (uint8_t__channel == 2u)
    {
        return BOOL__G__Trip[1];
    }
    return false;
}

/**
 * @brief  [EN] Clear one latched channel so the next retry can observe a new
 *              comparator edge rather than the old event.
 *         [FA] تریپ قفل‌شدهٔ یک کانال را پاک می‌کند تا retry بعدی لبهٔ جدید
 *              comparator را ببیند، نه رویداد قبلی را.
 * @param  uint8_t__channel [EN] Channel number 1 or 2 / شماره کانال ۱ یا ۲
 */
/* ==================== Jitter_ClearChannel ==================== */

void func__Jitter_ClearChannel(uint8_t uint8_t__channel)
{
    if (uint8_t__channel == 1u)
    {
        BOOL__G__Trip[0] = false;
    }
    else if (uint8_t__channel == 2u)
    {
        BOOL__G__Trip[1] = false;
    }
    else
    {
        /* [EN] Invalid channel is intentionally ignored. */
        /* [FA] کانال نامعتبر عمداً نادیده گرفته می‌شود. */
    }
}
