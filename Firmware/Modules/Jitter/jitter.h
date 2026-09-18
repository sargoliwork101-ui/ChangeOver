/**
 * @file    jitter.h
 * @brief   [EN] LM393 jitter trip flags (placeholder). Full type naming, func__ prefix.
 *          [FA] پرچم تریپ جیتر LM393 (اسکلت). نام تایپ کامل.
 */

#ifndef JITTER_H
#define JITTER_H

/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  [EN] Clear trip flags and EXTI software flags.
 *         [FA] پرچم تریپ و EXTI را پاک می‌کند.
 */
/* ==================== Functions ==================== */
void func__Jitter_Init(void);

/**
 * @brief  [EN] Latch trip if an EXTI event was taken.
 *         [FA] اگر رویداد EXTI آمده باشد تریپ را قفل می‌کند.
 */
void func__Jitter_Run(void);

/**
 * @brief  [EN] True if channel 1 or 2 has latched a trip.
 *         [FA] اگر کانال ۱ یا ۲ تریپ قفل کرده باشد true.
 * @param  uint8_t__channel [EN] Channel 1 or 2 / کانال ۱ یا ۲
 * @return bool [EN] true if tripped / اگر تریپ کرده true
 */
bool func__Jitter_ChannelTripped(uint8_t uint8_t__channel);

/**
 * @brief  [EN] Clear one channel after the relay has opened and the retry
 *              sequence is ready to re-arm the comparator.
 *         [FA] پس از بازشدن رله و آماده‌شدن retry، تریپ یک کانال را پاک می‌کند
 *              تا comparator دوباره arm شود.
 * @param  uint8_t__channel [EN] Channel number 1 or 2 / شماره کانال ۱ یا ۲
 */
void func__Jitter_ClearChannel(uint8_t uint8_t__channel);

#endif /* JITTER_H */
