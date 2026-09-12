/**
 * @file    rtos_app.h
 * @brief   [EN] Starts application tasks and the FreeRTOS scheduler.
 *          [FA] تسک‌های برنامه و زمان‌بند FreeRTOS را راه می‌اندازد.
 */

#ifndef RTOS_APP_H
#define RTOS_APP_H

/**
 * @brief  [EN] Create static tasks then call vTaskStartScheduler(). Does not return.
 *         [FA] تسک‌ها را استاتیک می‌سازد و scheduler را روشن می‌کند. برنمی‌گردد.
 */
void Rtos_Start(void);

#endif /* RTOS_APP_H */
