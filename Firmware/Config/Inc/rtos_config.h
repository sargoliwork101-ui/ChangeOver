/**
 * @file    rtos_config.h
 * @brief   [EN] CMSIS-RTOS2 thread priorities and stack sizes.
 *          [FA] اولویت و اندازهٔ استک تسک‌های CMSIS-RTOS2.
 *
 * @note    [EN] Stack sizes are expressed in words for the FreeRTOS backend;
 *          the CMSIS attributes convert them to bytes at the port boundary.
 *          [FA] اندازهٔ استک برای Backend فعلی بر حسب word است؛ ویژگی‌های CMSIS
 *          آن را در مرز پورت به بایت تبدیل می‌کنند.
 */

#ifndef RTOS_CONFIG_H
#define RTOS_CONFIG_H

/* ==================== Includes ==================== */
#include "cmsis_os2.h"

/* ==================== Defines ==================== */
#define TASK_PRIO_PROTECTION    (osPriorityLow3)
#define TASK_PRIO_MEASUREMENT   (osPriorityLow2)
#define TASK_PRIO_CONTROL       (osPriorityLow2)
#define TASK_PRIO_COMM          (osPriorityLow1)
#define TASK_PRIO_UI            (osPriorityLow)

#define TASK_STACK_UI           128u
#define TASK_STACK_MEASUREMENT  192u
#define TASK_STACK_PROTECTION   192u
#define TASK_STACK_CONTROL      256u
#define TASK_STACK_COMM         256u

#endif /* RTOS_CONFIG_H */
