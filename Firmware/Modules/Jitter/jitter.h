/**
 * @file    jitter.h
 * @brief   [EN] LM393 jitter trip flags (placeholder).
 *          [FA] پرچم تریپ جیتر LM393 (اسکلت).
 *
 * @stage   Placeholder
 */

#ifndef JITTER_H
#define JITTER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  [EN] Clear trip flags and EXTI software flags.
 *         [FA] پرچم تریپ و EXTI را پاک می‌کند.
 */
void Jitter_Init(void);

/**
 * @brief  [EN] Latch trip if an EXTI event was taken.
 *         [FA] اگر رویداد EXTI آمده باشد تریپ را قفل می‌کند.
 */
void Jitter_Run(void);

/**
 * @brief  [EN] True if channel 1 or 2 has latched a trip.
 *         [FA] اگر کانال ۱ یا ۲ تریپ قفل کرده باشد true.
 */
bool Jitter_ChannelTripped(uint8_t channel);

#endif /* JITTER_H */
