/**
 * @file    app.c
 * @brief   [EN] Starts the application through the CMSIS-RTOS2 boundary.
 */
#include "app.h"
#include "rtos_app.h"
#include "modules_enable.h"
#include "esp_link_nvm.h"
#include "cal_lut.h"
#if MODULE_FLASH_LOCK
#include "flash_lock.h"
#endif
#if MODULE_DIAG
#include "diag.h"
#endif
/* ==================== App_Init ==================== */
/**
 * @brief  [EN] Application initialization hook; thread-owned module init runs in its thread.
 */
void func__App_Init(void)
{
#if MODULE_FLASH_LOCK
    /*
 * [EN] One-time read-out lock (user order 2026-10-20). Runs before the
 * scheduler so the option-byte erase cannot stall a running task; on
 * success the MCU resets inside this call and the boot repeats with the
 * part already locked, which makes the second pass a no-op. Delivery
 * builds enable MODULE_FLASH_LOCK = 1; development builds may disable it.
 */
    (void)func__FlashLock_ApplyOnce();
#endif
#if MODULE_ESP
    /*
 * [EN] v1.14 (user order 2026-09-25: panel values must survive power
 * loss): replay the newest CRC-valid flash record through the clamped
 * parameter setters BEFORE the scheduler starts. The module Init
 * functions that run later inside their own threads only reset channel
 * state and measurement outputs - never the settable statics - so the
 * loaded values survive them. A missing or corrupt record changes
 * nothing (compiled defaults stay).
 */
    func__EspLink_NvmInit();
    /*
 * [EN] v1.66 (user order 2026-10-05: push the calibration table straight
 * into the micro, on its OWN storage path): load the bench LUT from its
 * dedicated flash block. No valid record = the gain and offset line stays in
 * charge (there is no compiled table any more).
 */
    func__CalLut_Init();
#endif
#if MODULE_DIAG
    func__Diag_Init();
#endif
}
/* ==================== App_Start ==================== */
/**
 * @brief  [EN] Initialize the CMSIS-RTOS2 application and start its threads.
 */
void func__App_Start(void)
{
    func__App_Init();
    func__Rtos_Start();
}
