/**
 * @file    rtos_backend_memory.h
 * @brief   [EN] Static object types required by the selected CMSIS-RTOS2 backend.
 *          [FA] نوع حافظهٔ ثابت موردنیاز Backend انتخاب‌شدهٔ CMSIS-RTOS2.
 *
 * @note    [EN] Only this boundary knows that the current CMSIS-RTOS2 backend is FreeRTOS.
 *          [FA] فقط این مرز می‌داند که Backend فعلی CMSIS-RTOS2، FreeRTOS است.
 */

#ifndef RTOS_BACKEND_MEMORY_H
#define RTOS_BACKEND_MEMORY_H

#include "FreeRTOS.h"
#include "task.h"

typedef StackType_t rtos_stack_word_t;
typedef StaticTask_t rtos_thread_control_block_t;

#endif /* RTOS_BACKEND_MEMORY_H */
