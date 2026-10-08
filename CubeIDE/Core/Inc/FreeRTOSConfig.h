/* USER CODE BEGIN Header */
/*
 * FreeRTOS Kernel V10.3.1
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */
/* USER CODE END Header */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * These parameters and more are described within the 'configuration' section of the
 * FreeRTOS API documentation available on the FreeRTOS.org web site.
 *
 * See http://www.freertos.org/a00110.html
 *----------------------------------------------------------*/

/* USER CODE BEGIN Includes */
/* Section where include file can be added */
/* USER CODE END Includes */

/* Ensure definitions are only used by the compiler, and not by the assembler. */
#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
  #include <stdint.h>
  extern uint32_t SystemCoreClock;
#endif
#ifndef CMSIS_device_header
#define CMSIS_device_header "stm32f1xx.h"
#endif /* CMSIS_device_header */

#define configUSE_PREEMPTION                     1
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         0
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configCPU_CLOCK_HZ                       ( SystemCoreClock )
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configMAX_PRIORITIES                     ( 56 )
#define configMINIMAL_STACK_SIZE                 ((uint16_t)128)
/* [EN] Stack-overflow trap (full-program audit 2026-09-26): method 2 checks
   the canary + the stack pointer at every switch and calls
   vApplicationStackOverflowHook (freertos_hooks.c). Without this a blown
   stack corrupts RAM silently; with it the board halts deterministically.
   [FA] تلهٔ سرریز استک (ممیزی کل برنامه): متد ۲ در هر سوییچ نگهبان و اشاره‌گر
   استک را چک می‌کند و هوک freertos_hooks.c را صدا می‌زند. */
#define configCHECK_FOR_STACK_OVERFLOW           2
#define configMAX_TASK_NAME_LEN                  ( 16 )
#define configUSE_TRACE_FACILITY                 1
#define configUSE_16_BIT_TICKS                   0
#define configUSE_MUTEXES                        1
#define configQUEUE_REGISTRY_SIZE                8
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES                    0
#define configMAX_CO_ROUTINE_PRIORITIES          ( 2 )

/* Software timer definitions. */
/* [EN] Timers OFF - flash diet 2026-10-03, after the linker reported
   "region FLASH overflowed by 780 bytes". That measurement was against the
   then-62 KiB application region; the region is 56K since v1.80 reserved the
   two-page parameter NVM banks, so this diet must NEVER be reverted.
   Nothing in this firmware creates a software timer. The only mention was
   the hook that hands the timer task its RAM, and that hook is already
   wrapped in #if (configUSE_TIMERS == 1).
   The note that used to sit here said OFF "breaks the compile", and named
   cmsis_os2's osTimer API. That was the wrong cause. Two walls existed and
   both are opt-outs, not laws:
     timers.c:41        #error, raised only because INCLUDE_xTimerPendFunctionCall
                        was 1. Nothing calls xTimerPendFunctionCall.
     freertos_os2.h:208 #error, raised only because the CMSIS-RTOS v2 Event
                        Flags API was enabled. Nothing calls osEventFlags*.
   The same note claimed --gc-sections drops the timer code anyway. It
   cannot: tasks.c:2022 calls xTimerCreateTimerTask() unconditionally while
   configUSE_TIMERS == 1, so timers.c is reachable from the scheduler itself
   and is always linked. Measured in a linked image it was 1376 bytes that no
   call site could reach, plus the timer task's 1 KiB stack in RAM.
   [FA] تایمرها خاموش شدند - رژیم فلش، پس از پیام لینکر «سرریز ۷۸۰ بایت».
   هیچ‌جای این فرم‌ور تایمر نرم‌افزاری نمی‌سازد؛ تنها اشاره، هوکی است که RAM
   تسک تایمر را می‌دهد و خودش با #if محافظت شده است.
   یادداشت قبلی اینجا می‌گفت خاموش‌کردن «کامپایل را می‌شکند» و علت را osTimer
   می‌دانست. علت اشتباه بود: دو سد وجود داشت و هر دو گزینهٔ قابل خاموش‌کردن
   بودند - یکی timers.c:41 به‌خاطر INCLUDE_xTimerPendFunctionCall و دیگری
   freertos_os2.h:208 به‌خاطر روشن‌ماندن Event Flags.
   همان یادداشت می‌گفت gc-sections خودش حذفش می‌کند؛ نمی‌تواند، چون
   tasks.c:2022 تا وقتی configUSE_TIMERS==1 باشد بی‌قید xTimerCreateTimerTask()
   را صدا می‌زند. در ایمیج لینک‌شده ۱۳۷۶ بایت بود که هیچ فراخوانی به آن
   نمی‌رسید، به‌علاوهٔ ۱ کیلوبایت استک تسک تایمر در RAM. */
#define configUSE_TIMERS                         0
#define configTIMER_TASK_PRIORITY                ( 2 )
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH             256

/* [EN] CMSIS-RTOS v2 surface, trimmed to what this firmware actually calls.
   The complete list it uses is: osKernelInitialize, osKernelStart,
   osKernelLock, osKernelRestoreLock, osKernelGetState, osKernelGetTickCount,
   osKernelGetTickFreq, osThreadNew, osDelay, osDelayUntil. Nothing else.
   Each switch below drops a wrapper family with no caller in this tree, and
   the Event Flags one is also what forced INCLUDE_xTimerPendFunctionCall to
   stay on. These are the shim's own documented opt-outs (freertos_os2.h
   lines 47..84), not edits to middleware.
   configUSE_COUNTING_SEMAPHORES is deliberately NOT switched off: the shim
   requires it even when no semaphore is created, and turning it off stops
   cmsis_os2.c from compiling. Measured, not assumed.
   [FA] سطح CMSIS-RTOS v2 به همان چیزی که این فرم‌ور واقعاً صدا می‌زند کوتاه شد؛
   کل مصرفش همان ده تابع بالاست. هر کلید زیر خانواده‌ای بدون‌فراخوان را برمی‌دارد
   و کلید Event Flags همان بود که INCLUDE_xTimerPendFunctionCall را روشن نگه
   می‌داشت. این‌ها گزینه‌های رسمی خود شیم‌اند (freertos_os2.h خطوط ۴۷ تا ۸۴)، نه
   دست‌کاری میان‌افزار. configUSE_COUNTING_SEMAPHORES عمداً خاموش نشد: شیم حتی
   بدون ساختن سمافور به آن نیاز دارد و خاموش‌کردنش کامپایل cmsis_os2.c را
   می‌شکند - این آزموده شده، نه فرض‌شده. */
/* [EN] Application policy, unchanged: every CMSIS-RTOS2 object supplies its
   own static memory. / [FA] سیاست برنامه، بدون تغییر: هر شیء CMSIS-RTOS2
   حافظهٔ ثابت خودش را می‌دهد. */
#define configUSE_OS2_THREAD_SUSPEND_RESUME      0
#define configUSE_OS2_THREAD_ENUMERATE           0
#define configUSE_OS2_EVENTFLAGS_FROM_ISR        0
#define configUSE_OS2_THREAD_FLAGS               0
#define configUSE_OS2_TIMER                      0
#define configUSE_OS2_MUTEX                      0


/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */
#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskCleanUpResources       0
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_xTimerPendFunctionCall      0
#define INCLUDE_xQueueGetMutexHolder        1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_eTaskGetState               1


/* Cortex-M specific definitions. */
#ifdef __NVIC_PRIO_BITS
 /* __BVIC_PRIO_BITS will be specified when CMSIS is being used. */
 #define configPRIO_BITS         __NVIC_PRIO_BITS
#else
 #define configPRIO_BITS         4
#endif

/* The lowest interrupt priority that can be used in a call to a "set priority"
function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY   15

/* The highest interrupt priority that can be used by any interrupt service
routine that makes calls to interrupt safe FreeRTOS API functions.  DO NOT CALL
INTERRUPT SAFE FREERTOS API FUNCTIONS FROM ANY INTERRUPT THAT HAS A HIGHER
PRIORITY THAN THIS! (higher priorities are lower numeric values. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/* Interrupt priorities used by the kernel port layer itself.  These are generic
to all Cortex-M ports, and do not rely on any particular library functions. */
#define configKERNEL_INTERRUPT_PRIORITY 		( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
/* !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 	( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* Normal assert() semantics without relying on the provision of an assert.h
header file. */
/* USER CODE BEGIN 1 */
#define configASSERT( x ) if ((x) == 0) {taskDISABLE_INTERRUPTS(); for( ;; );}
/* USER CODE END 1 */

/* Definitions that map the FreeRTOS port interrupt handlers to their CMSIS
standard names. */
#define vPortSVCHandler    SVC_Handler
#define xPortPendSVHandler PendSV_Handler

/* IMPORTANT: After 10.3.1 update, Systick_Handler comes from NVIC (if SYS timebase = systick), otherwise from cmsis_os2.c */

#define USE_CUSTOM_SYSTICK_HANDLER_IMPLEMENTATION 0

/* USER CODE BEGIN Defines */
/* Section where parameter definitions can be added (for instance, to override default ones in FreeRTOS.h) */
/* USER CODE END Defines */

#endif /* FREERTOS_CONFIG_H */
