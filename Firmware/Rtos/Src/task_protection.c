/**
 * @file    task_protection.c
 * @brief   [EN] CMSIS-RTOS2 protection thread - simple RTOS with osDelay.
 */
#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "cmsis_os2.h"
#include "rtos_time.h"
#if MODULE_PROTECTION
#include "protection.h"
#include "measurement.h"
#include "bsp_iwdg.h"
#endif
/* ==================== Task Protection ==================== */
/**
 * @brief  [EN] Protection thread entry point - the highest-priority task of
 *              the system, so a trip is evaluated before any lower task can
 *              keep the stage running. Each pass reads a measurement
 *              snapshot, hands it to the Protection module and checks in
 *              with the watchdog. While MODULE_PROTECTION is 0 (the shipped
 *              configuration, enforced by the rule checker) the body
 *              compiles down to the watchdog check-in and the periodic
 *              delay, which keeps the slot alive and the timing honest
 *              without the module having to exist.
 * @param  void_ptr__argument [EN] CMSIS-RTOS2 thread argument, unused here
 *                                and deliberately cast to void /
 */
void func__TaskProtection(void *void_ptr__argument)
{
    (void)void_ptr__argument;
    for (;;)
    {
#if MODULE_PROTECTION
        {
            measurement_snapshot_t measurement_snapshot_t__snap;
            /*
 * [EN] Full-program audit 2026-10-05: only .valid used to be
 * initialized here, so the other eight fields of this automatic
 * struct held stack garbage whenever the measurement module is
 * compiled out or the getter declines. Every consumer does gate
 * on .valid, so nothing was observably broken - but handing a
 * partly-indeterminate struct across a module boundary is the
 * kind of latent defect that only shows up after someone adds a
 * reader that trusts the payload. Zeroing every field costs a
 * handful of stores per pass and makes the contract true by
 * construction. task_ui.c already did exactly this.
 */
            measurement_snapshot_t__snap.v_in_mv       = 0u;
            measurement_snapshot_t__snap.v_bat24_mv    = 0u;
            measurement_snapshot_t__snap.v_bat12_mv    = 0u;
            measurement_snapshot_t__snap.v_bat_low_mv  = 0u;
            measurement_snapshot_t__snap.v_bat_high_mv = 0u;
            measurement_snapshot_t__snap.i_ch1_ma      = 0u;
            measurement_snapshot_t__snap.i_ch2_ma      = 0u;
            measurement_snapshot_t__snap.input_present = false;
            measurement_snapshot_t__snap.valid         = false;
#if MODULE_MEASUREMENT
            (void)func__Measurement_GetSnapshot(&measurement_snapshot_t__snap);
#endif
            func__Protection_Run(&measurement_snapshot_t__snap);
            func__BspIwdg_CheckIn(BSP_IWDG_SLOT_PROTECTION,
                                  func__Rtos_TicksToMilliseconds(osKernelGetTickCount()));
        }
        func__Rtos_DelayMilliseconds(APP_CONFIG.protection_period_ms);
#else
        func__Rtos_DelayMilliseconds(1000u);
#endif
    }
}
