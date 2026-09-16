/**
 * @file    rtos_tasks.h
 * @brief   [EN] Prototypes of application CMSIS-RTOS2 thread entry functions.
 *          [FA] اعلان توابع ورود تسک‌های CMSIS-RTOS2 لایهٔ کاربرد.
 */

#ifndef RTOS_TASKS_H
#define RTOS_TASKS_H

/**
 * @brief  [EN] LED/buzzer thread. Active this stage.
 *         [FA] تسک LED و بازر. در این مرحله فعال است.
 * @param  void_ptr__argument [EN] CMSIS-RTOS2 thread argument, unused / آرگومان استفاده‌نشده
 */
void func__TaskUi(void *void_ptr__argument);

/**
 * @brief  [EN] ADC sampling thread.
 *         [FA] تسک نمونه‌برداری ADC.
 * @param  void_ptr__argument [EN] CMSIS-RTOS2 thread argument, unused / آرگومان استفاده‌نشده
 */
void func__TaskMeasurement(void *void_ptr__argument);

/**
 * @brief  [EN] Protection thread.
 *         [FA] تسک حفاظت.
 * @param  void_ptr__argument [EN] CMSIS-RTOS2 thread argument, unused / آرگومان استفاده‌نشده
 */
void func__TaskProtection(void *void_ptr__argument);

/**
 * @brief  [EN] Changeover/charger thread.
 *         [FA] تسک Changeover و شارژر.
 * @param  void_ptr__argument [EN] CMSIS-RTOS2 thread argument, unused / آرگومان استفاده‌نشده
 */
void func__TaskControl(void *void_ptr__argument);

/**
 * @brief  [EN] UART/ESP thread.
 *         [FA] تسک UART/ESP.
 * @param  void_ptr__argument [EN] CMSIS-RTOS2 thread argument, unused / آرگومان استفاده‌نشده
 */
void func__TaskComm(void *void_ptr__argument);

#endif /* RTOS_TASKS_H */
