/**
 * @file    jitter.h
 * @brief   [EN] Public interface of the LM393 over-current comparator trip
 *               flags. The two comparators are open-collector with pull-ups,
 *               so a trip is a FALLING edge captured by EXTI in hardware;
 *               this module only latches the software flag per channel and
 *               hands it to the charger, which owns the reaction.
 * @note    [EN] Full-program audit 2026-10-05: the stale "placeholder"
 *               label of the old header line was removed.
 */
#ifndef JITTER_H
#define JITTER_H
/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>
/**
 * @brief  [EN] Clear trip flags and EXTI software flags.
 */
/* ==================== Functions ==================== */
void func__Jitter_Init(void);
/**
 * @brief  [EN] Latch trip if an EXTI event was taken.
 */
void func__Jitter_Run(void);
/**
 * @brief  [EN] True if channel 1 or 2 has latched a trip.
* @param uint8_t__channel Channel 1 or 2 /
* @return bool true if tripped / true
 */
bool func__Jitter_ChannelTripped(uint8_t uint8_t__channel);
/**
 * @brief  [EN] Clear one channel after the relay has opened and the retry
 *              sequence is ready to re-arm the comparator.
* @param uint8_t__channel Channel number 1 or 2 /
 */
void func__Jitter_ClearChannel(uint8_t uint8_t__channel);
#endif /* JITTER_H */
