#ifndef RTOS_CONFIG_H
#define RTOS_CONFIG_H

#include "FreeRTOS.h"

/* Priorities: higher number = higher priority on FreeRTOS */
#define TASK_PRIO_PROTECTION    (tskIDLE_PRIORITY + 4u)
#define TASK_PRIO_MEASUREMENT   (tskIDLE_PRIORITY + 3u)
#define TASK_PRIO_CONTROL       (tskIDLE_PRIORITY + 3u)
#define TASK_PRIO_COMM          (tskIDLE_PRIORITY + 2u)
#define TASK_PRIO_UI            (tskIDLE_PRIORITY + 1u)

/* Stack in words. Raise after checking watermark. */
#define TASK_STACK_UI           128u
#define TASK_STACK_MEASUREMENT  192u
#define TASK_STACK_PROTECTION   192u
#define TASK_STACK_CONTROL      256u
#define TASK_STACK_COMM         256u

#endif /* RTOS_CONFIG_H */
