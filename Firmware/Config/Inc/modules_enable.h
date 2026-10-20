/**
 * @file    modules_enable.h
 * @brief   [EN] Compile-time on/off switches for each module.
 *          [FA] کلید زمان‌کامپایل روشن/خاموش هر ماژول.
 *
 * @note    [EN] 1 = init + task may run. 0 = code stays, App/Rtos will not start it.
 *          [FA] 1 یعنی Init و Task می‌توانند اجرا شوند. UI، MEASUREMENT، CHANGEOVER و FAULT برای تست برد روشن هستند.
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

/* [EN] One-time read-out lock (RDP level 1). 0 = module is compiled and host
 *      tested but App_Init never calls it, so the part stays open. Set to 1
 *      only in the build that is meant for the field: the FIRST boot then
 *      programs the option bytes and resets, and every later boot is a no-op.
 *      Cost of turning it on: SWD loses read access on that part, and every
 *      later flash session begins with a full chip erase (which also wipes the
 *      parameter bank at 0x0800E000 and the calibration table at 0x0800F000).
 * [FA] قفل یک‌بارمصرف خواندن حافظه (RDP سطح ۱). مقدار ۰ یعنی ماژول کامپایل و
 *      تست هاست می‌شود ولی App_Init صدایش نمی‌زند، پس قطعه باز می‌ماند. فقط
 *      در بیلدی که برای تحویل است ۱ بشود: اولین بوت option byte را می‌نویسد و
 *      ریست می‌دهد و هر بوت بعدی هیچ کاری نمی‌کند. هزینهٔ روشن‌کردنش: SWD
 *      دسترسی خواندن را از دست می‌دهد و هر فلش بعدی با پاک‌کردن کامل تراشه
 *      شروع می‌شود (که بانک پارامترها در ‎0x0800E000‎ و جدول کالیبراسیون در
 *      ‎0x0800F000‎ را هم می‌برد). */
#define MODULE_FLASH_LOCK     0

#endif /* MODULES_ENABLE_H */
