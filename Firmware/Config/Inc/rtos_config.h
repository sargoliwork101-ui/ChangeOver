/**
 * @file    rtos_config.h
 * @brief   [EN] Task priority and stack sizes (words, not bytes).
 *          [FA] اولویت و اندازه استک تسک‌ها (واحد: word، نه بایت).
 *
 * @note    [EN] On Cortex-M3 one word is 4 bytes. 128 words = 512 bytes.
 *          [FA] روی Cortex-M3 هر word چهار بایت است.
 */

#ifndef RTOS_CONFIG_H
#define RTOS_CONFIG_H

#include "FreeRTOS.h"

#define TASK_PRIO_PROTECTION    (tskIDLE_PRIORITY + 4u)
#define TASK_PRIO_MEASUREMENT   (tskIDLE_PRIORITY + 3u)
#define TASK_PRIO_CONTROL       (tskIDLE_PRIORITY + 3u)
#define TASK_PRIO_COMM          (tskIDLE_PRIORITY + 2u)
#define TASK_PRIO_UI            (tskIDLE_PRIORITY + 1u)

#define TASK_STACK_UI           128u
#define TASK_STACK_MEASUREMENT  192u
#define TASK_STACK_PROTECTION   192u
#define TASK_STACK_CONTROL      256u
#define TASK_STACK_COMM         256u

#endif /* RTOS_CONFIG_H */
