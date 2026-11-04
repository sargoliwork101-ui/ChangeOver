/**
 * @file    protection.c
 * @brief   [EN] Implementation of the lightweight supervisor: FAULT_ADC is a
 *               LIVE (never latching) condition held while the measurement
 *               snapshot is missing or invalid. Over-current and low-battery
 *               supervision live in the charger alarm and fault detector
 *               paths; the module is compiled out by default
 *               (MODULE_PROTECTION = 0). Full type naming, func__ prefix.
 *               was removed and the "latch faults" wording corrected - see
 *               the design note inside func__Protection_Run for why
 *               FAULT_ADC must stay live instead of latching.
 */
#include "protection.h"
#include "fault.h"
#include <stddef.h>
/**
 * @brief  [EN] No state to initialize - the supervisor is stateless by
 *              design; the entry point is kept so the module lifecycle
 *              stays uniform with the rest of the firmware.
 */
/* ==================== Protection_Init ==================== */
void func__Protection_Init(void)
{
}
/**
 * @brief  [EN] Hold FAULT_ADC while the snapshot is missing or invalid and
 *              clear it on the first valid snapshot; deliberately never
 *              latches (a boot-time latch would stick for the whole power
 *              cycle and park the safe states forever).
* @param measurement_snapshot_t__snap Snapshot pointer, may be NULL, valid flag checked /
 */
/* ==================== Protection_Run ==================== */
void func__Protection_Run(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    if ((measurement_snapshot_t__snap == NULL) || (measurement_snapshot_t__snap->valid == false))
    {
        /*
 * [EN] ADC invalid = LIVE state, never a latch: at boot the first
 * passes run before measurement warm-up; a latched FAULT_ADC would
 * stick forever (nothing cleared it) and park Changeover/Charger in
 * safe states for the whole power cycle. Clears itself below as
 * soon as a valid snapshot arrives.
 */
        func__Fault_Set(FAULT_ADC);
        return;
    }
    func__Fault_Clear(FAULT_ADC);
}
