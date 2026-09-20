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
#define MODULE_CHARGER        1
#define MODULE_JITTER         1
#define MODULE_ESP            0

#endif /* MODULES_ENABLE_H */
