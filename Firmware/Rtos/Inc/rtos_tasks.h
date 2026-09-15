/**
 * @file    rtos_tasks.h
 * @brief   [EN] Prototypes of application FreeRTOS tasks. Full type naming, func__ prefix.
 *          [FA] اعلان تسک‌های FreeRTOS لایه کاربرد. نام تایپ کامل و پیشوند func__.
 */

#ifndef RTOS_TASKS_H
#define RTOS_TASKS_H

/**
 * @brief  [EN] LED/buzzer task. Active this stage.
 *         [FA] تسک LED و بازر. در این مرحله فعال است.
 * @param  void_ptr__argument [EN] Required by FreeRTOS, unused, type void* / آرگومان
 */
void func__TaskUi(void *void_ptr__argument);

/**
 * @brief  [EN] ADC sampling task. Placeholder.
 *         [FA] تسک نمونه‌برداری ADC. اسکلت.
 * @param  void_ptr__argument [EN] Required by FreeRTOS, unused / آرگومان
 */
void func__TaskMeasurement(void *void_ptr__argument);

/**
 * @brief  [EN] Protection task. Placeholder.
 *         [FA] تسک حفاظت. اسکلت.
 * @param  void_ptr__argument [EN] Required by FreeRTOS, unused / آرگومان
 */
void func__TaskProtection(void *void_ptr__argument);

/**
 * @brief  [EN] Changeover/charger task. Placeholder.
 *         [FA] تسک Changeover و شارژر. اسکلت.
 * @param  void_ptr__argument [EN] Required by FreeRTOS, unused / آرگومان
 */
void func__TaskControl(void *void_ptr__argument);

/**
 * @brief  [EN] UART/ESP task. Placeholder.
 *         [FA] تسک UART/ESP. اسکلت.
 * @param  void_ptr__argument [EN] Required by FreeRTOS, unused / آرگومان
 */
void func__TaskComm(void *void_ptr__argument);

#endif /* RTOS_TASKS_H */

