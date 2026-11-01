/**
 * @file    protection.h
 * @brief   [EN] Lightweight supervisor: while the measurement snapshot is
 *               missing or invalid it holds FAULT_ADC as a LIVE (never
 *               latching) condition; over-current and low-battery supervision
 *               live in the charger alarm and fault detector paths. The
 *               module is compiled out by default (MODULE_PROTECTION = 0).
 *               Full type naming, func__ prefix.
 *               was removed and the "latch faults" wording corrected - the
 *               implementation deliberately keeps FAULT_ADC live instead of
 *               latching it (see the note inside protection.c).
 */
#ifndef PROTECTION_H
#define PROTECTION_H
/* ==================== Includes ==================== */
#include "app_types.h"
/**
 * @brief  [EN] Init protection state.
 */
/* ==================== Functions ==================== */
void func__Protection_Init(void);
/**
 * @brief  [EN] Hold FAULT_ADC while the snapshot is missing or invalid and
 *              clear it on the first valid snapshot; deliberately never
 *              latches (a boot-time latch would stick for the whole power
 *              cycle and park the safe states forever).
* @param measurement_snapshot_t__snap Snapshot pointer /
 */
void func__Protection_Run(const measurement_snapshot_t *measurement_snapshot_t__snap);
#endif /* PROTECTION_H */
