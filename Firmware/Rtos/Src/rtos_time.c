/**
 * @file    rtos_time.c
 * @brief   [EN] Portable CMSIS-RTOS2 tick and millisecond conversions.
 */
#include "rtos_time.h"
#include "cmsis_os2.h"
#include <stdint.h>
/* ==================== Rtos_MillisecondsToTicks ==================== */
/**
 * @brief  [EN] Convert milliseconds to active kernel ticks without assuming a 1 kHz tick.
* @param uint32_t__milliseconds Duration in milliseconds /
* @return uint32_t Rounded-up kernel ticks /
 */
uint32_t func__Rtos_MillisecondsToTicks(uint32_t uint32_t__milliseconds)
{
    uint32_t uint32_t__tickFrequency;
    uint32_t uint32_t__wholeSeconds;
    uint32_t uint32_t__leftoverMs;
    uint32_t uint32_t__wholeTicks;
    uint32_t uint32_t__fractionalTicks;
    uint32_t__tickFrequency = osKernelGetTickFreq();
    if ((uint32_t__milliseconds == 0u) || (uint32_t__tickFrequency == 0u))
    {
        return 0u;
    }
    /*
 * [EN] Flash diet 2026-09-27: (ms*freq+999)/1000 WITHOUT u64 - split ms
 * into whole seconds + leftover: whole*freq + (left*freq+999)/1000 is
 * the identical quotient (whole*freq*1000 is divisible by 1000) and no
 * product can overflow: leftover <= 999 and rates above 1 MHz (1000x
 * any real RTOS tick) saturate. The old >UINT32_MAX trip is kept as
 * the whole-seconds guard; the final addition is checked separately so
 * the result saturates instead of wrapping at UINT32_MAX. The u64
 * division pulled
 * __aeabi_uldivmod (~1 KiB).
 */
    if (uint32_t__tickFrequency > 1000000u)
    {
        return UINT32_MAX;
    }
    uint32_t__wholeSeconds = uint32_t__milliseconds / 1000u;
    uint32_t__leftoverMs = uint32_t__milliseconds % 1000u;
    if (uint32_t__wholeSeconds > (UINT32_MAX / uint32_t__tickFrequency))
    {
        return UINT32_MAX;
    }
    uint32_t__wholeTicks = uint32_t__wholeSeconds * uint32_t__tickFrequency;
    uint32_t__fractionalTicks =
        ((uint32_t__leftoverMs * uint32_t__tickFrequency) + 999u) / 1000u;
    /*
 * [EN] The two individually safe terms can still overflow when added at
 * the upper boundary. Saturate instead of wrapping to a short delay.
 */
    if (uint32_t__fractionalTicks >
        (UINT32_MAX - uint32_t__wholeTicks))
    {
        return UINT32_MAX;
    }
    return uint32_t__wholeTicks + uint32_t__fractionalTicks;
}
/* ==================== Rtos_TicksToMilliseconds ==================== */
/**
 * @brief  [EN] Convert elapsed kernel ticks to milliseconds without assuming a 1 kHz tick.
* @param uint32_t__ticks Elapsed kernel ticks /
* @return uint32_t Elapsed milliseconds /
 */
uint32_t func__Rtos_TicksToMilliseconds(uint32_t uint32_t__ticks)
{
    uint32_t uint32_t__tickFrequency;
    uint32_t uint32_t__wholeQuotient;
    uint32_t uint32_t__remainderTicks;
    uint32_t uint32_t__wholeMilliseconds;
    uint32_t uint32_t__remainderMilliseconds;
    uint32_t__tickFrequency = osKernelGetTickFreq();
    if ((uint32_t__ticks == 0u) || (uint32_t__tickFrequency == 0u))
    {
        return 0u;
    }
    /*
 * [EN] Flash diet 2026-09-27: (ticks*1000)/freq WITHOUT u64 - split
 * ticks by the rate: whole*1000 + (rem*1000)/freq is the identical
 * quotient, rem*1000 fits (rem < freq <= 1 MHz guard above the real
 * 1000 Hz, bit-exact there). Saturation matches the old trip.
 */
    if (uint32_t__tickFrequency > 1000000u)
    {
        return UINT32_MAX;
    }
    uint32_t__wholeQuotient = uint32_t__ticks / uint32_t__tickFrequency;
    uint32_t__remainderTicks = uint32_t__ticks % uint32_t__tickFrequency;
    if (uint32_t__wholeQuotient > (UINT32_MAX / 1000u))
    {
        return UINT32_MAX;
    }
    uint32_t__wholeMilliseconds = uint32_t__wholeQuotient * 1000u;
    uint32_t__remainderMilliseconds =
        (uint32_t__remainderTicks * 1000u) / uint32_t__tickFrequency;
    /*
 * [EN] Protect the final addition from wrapping at UINT32_MAX.
 */
    if (uint32_t__remainderMilliseconds >
        (UINT32_MAX - uint32_t__wholeMilliseconds))
    {
        return UINT32_MAX;
    }
    return uint32_t__wholeMilliseconds + uint32_t__remainderMilliseconds;
}
/* ==================== Rtos_DelayMilliseconds ==================== */
/**
 * @brief  [EN] Delay the current CMSIS-RTOS2 thread for milliseconds.
* @param uint32_t__milliseconds Delay in milliseconds /
 */
void func__Rtos_DelayMilliseconds(uint32_t uint32_t__milliseconds)
{
    (void)osDelay(func__Rtos_MillisecondsToTicks(uint32_t__milliseconds));
}
