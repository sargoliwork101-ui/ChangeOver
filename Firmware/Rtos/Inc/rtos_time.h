/**
 * @file    rtos_time.h
 * @brief   [EN] Portable CMSIS-RTOS2 tick and millisecond conversions.
 *          [FA] تبدیل قابل‌حمل تیک و میلی‌ثانیه در CMSIS-RTOS2.
 */

#ifndef RTOS_TIME_H
#define RTOS_TIME_H

#include <stdint.h>

/**
 * @brief  [EN] Convert milliseconds to the active CMSIS-RTOS2 tick count.
 *         [FA] میلی‌ثانیه را به تعداد تیک فعال CMSIS-RTOS2 تبدیل می‌کند.
 * @param  uint32_t__milliseconds [EN] Duration in milliseconds / مدت بر حسب میلی‌ثانیه
 * @return uint32_t [EN] Rounded-up kernel ticks / تیک کرنل با گردکردن رو به بالا
 */
uint32_t func__Rtos_MillisecondsToTicks(uint32_t uint32_t__milliseconds);

/**
 * @brief  [EN] Convert an elapsed CMSIS-RTOS2 tick count to milliseconds.
 *         [FA] تعداد تیک سپری‌شده CMSIS-RTOS2 را به میلی‌ثانیه تبدیل می‌کند.
 * @param  uint32_t__ticks [EN] Elapsed kernel ticks / تیک سپری‌شده کرنل
 * @return uint32_t [EN] Elapsed milliseconds / میلی‌ثانیه سپری‌شده
 */
uint32_t func__Rtos_TicksToMilliseconds(uint32_t uint32_t__ticks);

/**
 * @brief  [EN] Delay the current CMSIS-RTOS2 thread for a duration in milliseconds.
 *         [FA] تسک فعلی CMSIS-RTOS2 را به مدت میلی‌ثانیه متوقف می‌کند.
 * @param  uint32_t__milliseconds [EN] Delay in milliseconds / تأخیر بر حسب میلی‌ثانیه
 */
void func__Rtos_DelayMilliseconds(uint32_t uint32_t__milliseconds);

#endif /* RTOS_TIME_H */
