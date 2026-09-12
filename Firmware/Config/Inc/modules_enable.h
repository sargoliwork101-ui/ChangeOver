#ifndef MODULES_ENABLE_H
#define MODULES_ENABLE_H

/**
 * Compile-time feature switches.
 * 1 = init + task (if any) are built
 * 0 = code can stay in the project but App/Rtos will not call it
 *
 * Enable one step at a time. See docs/steps.md.
 */

#define MODULE_UI             1
#define MODULE_FAULT          1
#define MODULE_MEASUREMENT    0
#define MODULE_PROTECTION     0
#define MODULE_CHANGEOVER     0
#define MODULE_CHARGER        0
#define MODULE_JITTER         0
#define MODULE_ESP            0

#endif /* MODULES_ENABLE_H */
