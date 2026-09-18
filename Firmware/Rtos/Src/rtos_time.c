/**
 * @file    rtos_time.c
 * @brief   [EN] Portable CMSIS-RTOS2 tick and millisecond conversions.
 *          [FA] تبدیل قابل‌حمل تیک و میلی‌ثانیه در CMSIS-RTOS2.
 */

#include "rtos_time.h"
#include "cmsis_os2.h"

#include <stdint.h>

/**
 * @brief  [EN] Convert milliseconds to active kernel ticks without assuming a 1 kHz tick.
 *         [FA] میلی‌ثانیه را بدون فرض نرخ یک کیلوهرتز به تیک کرنل تبدیل می‌کند.
 * @param  uint32_t__milliseconds [EN] Duration in milliseconds / مدت بر حسب میلی‌ثانیه
 * @return uint32_t [EN] Rounded-up kernel ticks / تیک کرنل با گردکردن رو به بالا
 */
uint32_t func__Rtos_MillisecondsToTicks(uint32_t uint32_t__milliseconds)
{
    uint32_t uint32_t__tickFrequency;
    uint64_t uint64_t__ticks;

    uint32_t__tickFrequency = osKernelGetTickFreq();
    if (uint32_t__tickFrequency == 0u)
    {
        return 0u;
    }

    uint64_t__ticks = ((uint64_t)uint32_t__milliseconds * (uint64_t)uint32_t__tickFrequency) + 999u;
    uint64_t__ticks /= 1000u;

    if (uint64_t__ticks > UINT32_MAX)
    {
        return UINT32_MAX;
    }

    return (uint32_t)uint64_t__ticks;
}

/**
 * @brief  [EN] Convert elapsed kernel ticks to milliseconds without assuming a 1 kHz tick.
 *         [FA] تیک‌های سپری‌شده را بدون فرض نرخ یک کیلوهرتز به میلی‌ثانیه تبدیل می‌کند.
 * @param  uint32_t__ticks [EN] Elapsed kernel ticks / تیک سپری‌شده کرنل
 * @return uint32_t [EN] Elapsed milliseconds / میلی‌ثانیه سپری‌شده
 */
uint32_t func__Rtos_TicksToMilliseconds(uint32_t uint32_t__ticks)
{
    uint32_t uint32_t__tickFrequency;
    uint64_t uint64_t__milliseconds;

    uint32_t__tickFrequency = osKernelGetTickFreq();
    if (uint32_t__tickFrequency == 0u)
    {
        return 0u;
    }

    uint64_t__milliseconds = (uint64_t)uint32_t__ticks * 1000u;
    uint64_t__milliseconds /= (uint64_t)uint32_t__tickFrequency;

    if (uint64_t__milliseconds > UINT32_MAX)
    {
        return UINT32_MAX;
    }

    return (uint32_t)uint64_t__milliseconds;
}

/**
 * @brief  [EN] Delay the current CMSIS-RTOS2 thread for milliseconds.
 *         [FA] تسک فعلی CMSIS-RTOS2 را به مدت میلی‌ثانیه متوقف می‌کند.
 * @param  uint32_t__milliseconds [EN] Delay in milliseconds / تأخیر بر حسب میلی‌ثانیه
 */
void func__Rtos_DelayMilliseconds(uint32_t uint32_t__milliseconds)
{
    (void)osDelay(func__Rtos_MillisecondsToTicks(uint32_t__milliseconds));
}
