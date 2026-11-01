/**
 * @file    modules_enable.h
 * @brief   [EN] Compile-time on/off switches for each module.
 * @note    [EN] 1 = init + task may run. 0 = code stays, App/Rtos will not start it.
 */
#ifndef MODULES_ENABLE_H
#define MODULES_ENABLE_H
/* ==================== Defines ==================== */
#define MODULE_UI             1
#define MODULE_FAULT          1
#define MODULE_MEASUREMENT    1
#define MODULE_PROTECTION     0
#define MODULE_CHANGEOVER     1
#define MODULE_MCU_POWER_PATH 1
#define MODULE_CHARGER        1
#define MODULE_JITTER         1
#define MODULE_ESP            1
#define MODULE_IMBALANCE      1
#define MODULE_DIAG           1
/*
 * [EN] One-time read-out lock (RDP level 1). 0 = module is compiled and host
 *      tested but App_Init never calls it, so the part stays open. Set to 1
 *      only in the build that is meant for the field: the FIRST boot then
 *      programs the option bytes and resets, and every later boot is a no-op.
 *      Cost of turning it on: SWD loses read access on that part, and every
 *      later flash session begins with a full chip erase (which also wipes the
 *      parameter bank at 0x0800E000 and the calibration table at 0x0800F000).
 */
#define MODULE_FLASH_LOCK     0
#endif /* MODULES_ENABLE_H */
