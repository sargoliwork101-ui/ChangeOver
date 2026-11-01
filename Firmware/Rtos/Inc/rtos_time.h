/**
 * @file    rtos_time.h
 * @brief   [EN] Portable CMSIS-RTOS2 tick and millisecond conversions.
 */
#ifndef RTOS_TIME_H
#define RTOS_TIME_H
#include <stdint.h>
/**
 * @brief  [EN] Convert milliseconds to the active CMSIS-RTOS2 tick count.
* @param uint32_t__milliseconds Duration in milliseconds /
* @return uint32_t Rounded-up kernel ticks /
 */
uint32_t func__Rtos_MillisecondsToTicks(uint32_t uint32_t__milliseconds);
/**
 * @brief  [EN] Convert an elapsed CMSIS-RTOS2 tick count to milliseconds.
* @param uint32_t__ticks Elapsed kernel ticks /
* @return uint32_t Elapsed milliseconds /
 */
uint32_t func__Rtos_TicksToMilliseconds(uint32_t uint32_t__ticks);
/**
 * @brief  [EN] Delay the current CMSIS-RTOS2 thread for a duration in milliseconds.
* @param uint32_t__milliseconds Delay in milliseconds /
 */
void func__Rtos_DelayMilliseconds(uint32_t uint32_t__milliseconds);
#endif /* RTOS_TIME_H */
