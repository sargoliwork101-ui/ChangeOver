/**
 * @file    task_comm.c
 * @brief   [EN] CMSIS-RTOS2 communication thread - simple RTOS with osDelay.
 */
#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "cmsis_os2.h"
#include "rtos_time.h"
#if MODULE_ESP
#include "esp_link.h"
#include "bsp_iwdg.h"
#endif
#if MODULE_MEASUREMENT
#include "measurement.h"
#endif
#if MODULE_FAULT
#include "fault.h"
#endif
/* ==================== Task Comm ==================== */
/**
 * @brief  [EN] Communication thread entry point. It owns the whole link
 *              lifecycle: before the periodic loop it brings up the UART
 *              backend, the frame parser and ESP power exactly once, then
 *              every COMM period it services the receive path, answers the
 *              panel, pushes the telemetry it owes and checks in with the
 *              watchdog so a dead link is visible to the supervisor. The
 *              body is a CMSIS-RTOS2 osDelayUntil cadence, never a busy
 *              wait, and the function never returns - returning from a
 *              thread would be a fault on this port.
 * @param  void_ptr__argument [EN] CMSIS-RTOS2 thread argument, unused here
 *                                and deliberately cast to void /
 */
void func__TaskComm(void *void_ptr__argument)
{
    (void)void_ptr__argument;
#if MODULE_ESP
    /*
 * [EN] The comm thread owns the link lifecycle: bring the UART backend,
 * parser and ESP power up once before the periodic loop (ESP panel,
 * user order 2026-09-22).
 */
    func__EspLink_Init();
#endif
    for (;;)
    {
#if MODULE_ESP
        {
            measurement_snapshot_t measurement_snapshot_t__snap;
            fault_mask_t fault_mask_t__faults = FAULT_NONE;
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
#if MODULE_FAULT
            fault_mask_t__faults = func__Fault_Get();
#endif
            func__EspLink_Run(&measurement_snapshot_t__snap, fault_mask_t__faults);
            func__BspIwdg_CheckIn(BSP_IWDG_SLOT_COMM,
                              func__Rtos_TicksToMilliseconds(osKernelGetTickCount()));
        }
        func__Rtos_DelayMilliseconds(APP_CONFIG.comm_period_ms);
#else
        func__Rtos_DelayMilliseconds(1000u);
#endif
    }
}
