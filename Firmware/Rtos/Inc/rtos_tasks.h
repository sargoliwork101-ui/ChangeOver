/**
 * @file    rtos_tasks.h
 * @brief   [EN] Prototypes of application FreeRTOS tasks.
 *          [FA] اعلان تسک‌های FreeRTOS لایه کاربرد.
 */

#ifndef RTOS_TASKS_H
#define RTOS_TASKS_H

/**
 * @brief  [EN] LED/buzzer task. Active this stage.
 *         [FA] تسک LED و بازر. در این مرحله فعال است.
 */
void TaskUi(void *argument);

/**
 * @brief  [EN] ADC sampling task. Placeholder.
 *         [FA] تسک نمونه‌برداری ADC. اسکلت.
 */
void TaskMeasurement(void *argument);

/**
 * @brief  [EN] Protection task. Placeholder.
 *         [FA] تسک حفاظت. اسکلت.
 */
void TaskProtection(void *argument);

/**
 * @brief  [EN] Changeover/charger task. Placeholder.
 *         [FA] تسک Changeover و شارژر. اسکلت.
 */
void TaskControl(void *argument);

/**
 * @brief  [EN] UART/ESP task. Placeholder.
 *         [FA] تسک UART/ESP. اسکلت.
 */
void TaskComm(void *argument);

#endif /* RTOS_TASKS_H */
