/**
 * @file    bsp_iwdg.h
 * @brief   [EN] Independent watchdog (IWDG) + multi-task liveness monitor.
 * @note    [EN] Kicking from one task proves only that task. Every
 *              supervised task checks in each cycle and the control task
 *              refreshes the watchdog only when ALL expected tasks are
 *              fresh, so a hung measurement/comm/protection/UI task (or
 *              the control task itself) resets the MCU instead of running
 *              half-dead. The supervised set is fixed at compile time from
 *              modules_enable.h and always matches the threads rtos_app.c
 *              creates.
 * @note    [EN] Timing (user order 2026-09-27, ~1 s): /32 + reload 1250 =
 *              1.0 s nominal at 40 kHz LSI; the LSI spreads 30..60 kHz, so
 *              the real timeout is 0.67..1.33 s. Control kicks every pass
 *              (10 ms). A task is STALE after BSP_IWDG_STALE_MS (covers the
 *              1 s idle loops of the disabled-module combinations) -
 *              worst-case hang detection ~1.5 s + one timeout. DBG_IWDG_STOP
 *              is set (debugger halt does not reset); a thread-create
 *              failure sits in func__Rtos_Fatal() with no kicks - a
 *              fail-safe reset loop.
 *              fail-safe.
 */
#ifndef BSP_IWDG_H
#define BSP_IWDG_H
#include <stdint.h>
#define BSP_IWDG_STALE_MS 1500u
typedef enum
{
    BSP_IWDG_SLOT_MEASUREMENT = 0,
    BSP_IWDG_SLOT_COMM       = 1,
    BSP_IWDG_SLOT_PROTECTION = 2,
    BSP_IWDG_SLOT_UI         = 3,
    BSP_IWDG_SLOT_COUNT      = 4
} bsp_iwdg_slot_t;
/* ==================== BspIwdg_Init ==================== */
/**
 * @brief  [EN] Start the LSI, program /32 + reload 1250, freeze on core
 *              halt and start the watchdog. Called once from main() before
 *              the scheduler; the first kick must arrive within the timeout
 *              (control kicks every pass, first pass runs milliseconds
 *              after the scheduler starts). Without a control task in this
 *              module combination the watchdog is deliberately NOT started
 *              (nobody could kick it).
 */
void func__BspIwdg_Init(void);
/* ==================== BspIwdg_CheckIn ==================== */
/**
 * @brief  [EN] Record one proof-of-life of a supervised task. Calls for slots
 *              that are not expected in this module combination are ignored.
* @param bsp_iwdg_slot_t__slot Which task checks in /
* @param uint32_t__nowMs FreeRTOS tick (1 kHz = ms) /
 */
void func__BspIwdg_CheckIn(bsp_iwdg_slot_t bsp_iwdg_slot_t__slot,
                           uint32_t uint32_t__nowMs);
/* ==================== BspIwdg_PollKick ==================== */
/**
 * @brief  [EN] Refresh the watchdog if and only if every expected slot
 *              checked in within BSP_IWDG_STALE_MS. Called by the control
 *              task every pass (its own liveness is the call itself).
* @param uint32_t__nowMs FreeRTOS tick (1 kHz = ms) /
 */
void func__BspIwdg_PollKick(uint32_t uint32_t__nowMs);
#endif /* BSP_IWDG_H */
