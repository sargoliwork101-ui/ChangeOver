/**
 * @file    rtos_tasks.h
 * @brief   [EN] Prototypes of application FreeRTOS tasks.
 *          [FA] اعلان تسک‌های FreeRTOS لایه کاربرد.
 *
 * @stage   TaskUi is active. Other tasks are placeholders until the user
 *          enables that stage.
 *          تسک UI فعال است. بقیه تا مرحله بعد اسکلت می‌مانند.
 */

#ifndef RTOS_TASKS_H
#define RTOS_TASKS_H

void TaskUi(void *argument);
void TaskMeasurement(void *argument);
void TaskProtection(void *argument);
void TaskControl(void *argument);
void TaskComm(void *argument);

#endif /* RTOS_TASKS_H */
