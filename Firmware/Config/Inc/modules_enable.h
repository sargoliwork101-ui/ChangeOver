/**
 * @file    modules_enable.h
 * @brief   کلید روشن/خاموش هر کتابخانه.
 *
 * 1 یعنی App آن را Init می‌کند و در صورت نیاز Task ساخته می‌شود.
 * 0 یعنی آن قابلیت در این بیلد وجود ندارد.
 *
 * الان فقط UI=1. بقیه را تا پایان همین مرحله دست نزن.
 */

#ifndef MODULES_ENABLE_H
#define MODULES_ENABLE_H

#define MODULE_UI             1
#define MODULE_FAULT          0
#define MODULE_MEASUREMENT    0
#define MODULE_PROTECTION     0
#define MODULE_CHANGEOVER     0
#define MODULE_CHARGER        0
#define MODULE_JITTER         0
#define MODULE_ESP            0

#endif /* MODULES_ENABLE_H */
