/**
 * @file    rtos_tasks.h
 * @brief   [EN] Prototypes of application CMSIS-RTOS2 thread entry functions.
 */
#ifndef RTOS_TASKS_H
#define RTOS_TASKS_H
/**
 * @brief  [EN] LED/buzzer thread. Active this stage.
* @param void_ptr__argument CMSIS-RTOS2 thread argument, unused /
 */
void func__TaskUi(void *void_ptr__argument);
/**
 * @brief  [EN] ADC sampling thread.
* @param void_ptr__argument CMSIS-RTOS2 thread argument, unused /
 */
void func__TaskMeasurement(void *void_ptr__argument);
/**
 * @brief  [EN] Protection thread.
* @param void_ptr__argument CMSIS-RTOS2 thread argument, unused /
 */
void func__TaskProtection(void *void_ptr__argument);
/**
 * @brief  [EN] Changeover/charger thread.
* @param void_ptr__argument CMSIS-RTOS2 thread argument, unused /
 */
void func__TaskControl(void *void_ptr__argument);
/**
 * @brief  [EN] UART/ESP thread.
* @param void_ptr__argument CMSIS-RTOS2 thread argument, unused /
 */
void func__TaskComm(void *void_ptr__argument);
#endif /* RTOS_TASKS_H */
