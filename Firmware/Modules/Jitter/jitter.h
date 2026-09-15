/**
 * @file    jitter.h
 * @brief   [EN] LM393 jitter trip flags (placeholder). Full type naming, func_ prefix.
 *          [FA] پرچم تریپ جیتر LM393 (اسکلت). نام تایپ کامل.
 */

#ifndef JITTER_H
#define JITTER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  [EN] Clear trip flags and EXTI software flags.
 *         [FA] پرچم تریپ و EXTI را پاک می‌کند.
 */
void func_Jitter_Init(void);

/**
 * @brief  [EN] Latch trip if an EXTI event was taken.
 *         [FA] اگر رویداد EXTI آمده باشد تریپ را قفل می‌کند.
 */
void func_Jitter_Run(void);

/**
 * @brief  [EN] True if channel 1 or 2 has latched a trip.
 *         [FA] اگر کانال ۱ یا ۲ تریپ قفل کرده باشد true.
 * @param  uint8_t_channel [EN] Channel 1 or 2 / کانال ۱ یا ۲
 * @return bool [EN] true if tripped / اگر تریپ کرده true
 */
bool func_Jitter_ChannelTripped(uint8_t uint8_t_channel);

#endif /* JITTER_H */
